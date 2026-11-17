/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* I2S output (ALNK0 -> external codec) and the
 * audio ISR: per half buffer, blocks of CTL samples: mix_block (fx.c: events ->
 * each synth part -> dist -> level / pan -> sends -> buses -> master) -> USB audio (uac_tap) -> the metronome's
 * click (click.c, the DAC only) -> 24-bit stereo. */
/* registers: hal/fm1_audio.h */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define DAC_TICKS 544u            /* TIMER4 ticks per I2S frame (24 MHz / 44,117.6 Hz) */
#define CPU_AVG (4096u / HALF_FRAMES)   /* halves the load meter averages: ~93 ms whatever the half */
#define OUT_SHIFT 8               /* Q15 -> 24-bit at full scale (B13: was 7, a -6 dBFS ceiling from the bring-up, "M0f ran clean at -18 dBFS"; the master's knee keeps the mix under it) */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

/* B15 THE RING (AUDIO_RING; owner 17 Nov: +2.9 ms latency approved; work/b15/cpu/cores/DESIGN.md). The interrupt renders one half AHEAD: at its entry it copies the half it
 * rendered last time (rbuf) into the DMA's free half and acknowledges at once, then renders the next half into rbuf. A half is then late only when its interrupt ENTERS a
 * whole half after it came due (the copy is the deadline, not the render's end), so a render over 100 % borrows from the next half and a lighter one pays it back (Deluge's
 * output ring, research #1). The "debt" is how late an entry is: ring_due is when this interrupt came due (the entry of an on-time one, else the last due + a half: the DMA and
 * TIMER4 run from the same 24 MHz clock, 544 ticks a frame). The ease ladder and the in-buffer guard read the debt the half leaves (ring_eff, guard_block), not the half's own
 * length: a spike the ring absorbs eases and sheds nothing (owner: no note cutting). The flash duck needs three silent halves (rbuf and both DMA halves). Off (0): the old
 * ping-pong, bit for bit (the host tests' default) */
#ifndef AUDIO_RING
#define AUDIO_RING 0
#endif
#if AUDIO_RING
#define RING_HALF_TICKS (HALF_FRAMES * DAC_TICKS * FM1_TICKS_PER_US / 24u)   /* the DMA's period in fm1_ticks: 69,632 on the unit (DAC_TICKS is 24 MHz ticks a frame) */
#define RING_JIT_TICKS (4u * FM1_TICKS_PER_US)         /* an entry this close to its expected time is on time: ring_due follows it (the entry's own jitter, and any drift, never adds up) */
#define RING_COPY_TICKS (8u * FM1_TICKS_PER_US)        /* the copy at the entry and the interrupt's own start: a debt within this of a half is already a miss */
static int32_t rbuf[HALF_WORDS] __attribute__((aligned(4)));   /* the half rendered ahead (zero at power-on: the first copy is silence) */
static uint32_t ring_due;                              /* when the interrupt now running came due (fm1_ticks) */
static uint8_t ring_run;                               /* ring_due is valid (an interrupt ran before) */
static uint32_t ring_debt, ring_debt_pk;               /* this entry's lateness (ticks; 0 .. a half), and the largest since the console last read it */
#define DUCK_SILENT_HALVES 3u                          /* rbuf and both halves of abuf */
#else
#define DUCK_SILENT_HALVES 2u                          /* both halves of abuf */
#endif

/* diagnostics, kept across resets and UBOOT entry: read with `fm1t memr` */
#define DBG_MAGIC 0x44424731u                       /* "DBG1" */
struct felucca_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots;
    uint32_t stage, page, home;           /* where the main loop is (breadcrumbs); home: always 0 since 0.2.14 (the OVERVIEW screen is gone; the field keeps the layout) */
    uint32_t prev_stage, prev_page, prev_home, prev_rst, prev_frames;   /* as found at boot */
    uint32_t erase_max_us, erase_last_us, prog_max_us, erases;   /* DUELL 0.2.11: the flash driver's own times (storage_hw.c), since boot */
} felucca_dbg __attribute__((section(".noinit")));
static volatile uint32_t audio_halves, audio_max_us;
static volatile uint32_t t5_nested_ticks;              /* TIMER4 ticks TIMER5 spent nested in this ISR (main.c) */
static uint32_t audio_cpu_rem;                         /* keep the fractional IIR step: no low-load bias */

/* DUELL CPU load meter (MENU > SYSTEM > TEST, ABOUT: ui_menu.c cpu_meter_poll): what the audio ISR keeps per I2S half, three adds and
 * a compare, no division. cpu_sum/cpu_n: the sum of the halves' render times (us) and how many; cpu_pk: the longest half
 * (us; ui_menu.c cpu_snap_poll reads and clears it) and cpu_el_pk: the longest half's ELAPSED time (us, TIMER5's nested work included: the
 * DMA deadline); cpu_miss: DMA periods missed (a half's elapsed time over 1, 2 or 3 periods = 1, 2, 3; a half the DMA overtook that was
 * not over its time = 1), counted since power-on. The time is fm1_ticks() across the half minus TIMER5's nested ticks (the key scan),
 * i.e. the audio's own work. Emulator and host: FELUCCA_METER_EST replaces it by a model (below): there is no clock there */
/* (the counters and CPU_BUDGET_US: core.h) */
#ifdef FELUCCA_METER_EST
/* an ESTIMATE, not a measurement: 16 % for the buses, plus 10 % a sounding voice (a voice in its release too), of the half's
 * time. 8 voices read 96 %, so the warning colours can be seen in the emulator; only the device measures */
static uint32_t meter_est_us(void)
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NLIVE; p++)
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active != 0;
    return CPU_BUDGET_US * (16u + 10u * n) / 100u;
}
#endif

/* MASTER, the DAC's volume (DUELL 0.2.25, owner 2026-10-20: "the master volume glitches when I rotate it, in the headphone"). The codec has no volume
 * register the firmware writes (hal/fm1_audio.h: only its enable lines), so this is the only volume there is. mix_block makes the mix at the fixed level
 * MASTER_USB_REF and USB took it; here out (Q15) becomes Q15 x 2^OUT_SHIFT (the 24-bit stream's scale) times song.master_q12 / MASTER_USB_REF, with two
 * differences from the stage that was here (usb_fixed_dac: out * m >> 11 per block, in Q15):
 *  - the gain glides: mdac_g (Q24: Q12 x 2^12) follows the knob's level with a one-pole step every sample (1/512 of the way: tau 512 samples = 11.6 ms), so a
 *    knob step (the level is read every block and changes every main-loop pass) is a glide of a few ms, not a step of up to 6 dB at the low end of the
 *    knob (a step of s dB is a jump of (10^(s/20) - 1) x the signal at the block seam: a click). The step is rounded away from the target, so the gain
 *    reaches it exactly (settled, the result is the old stage's bit for bit at the same level and 2^OUT_SHIFT times its Q15 value);
 *  - the result keeps OUT_SHIFT fractional bits of Q15 (it used to be cut to Q15 first): a quiet knob position (gain 1/30) no longer loses 5 bits of the
 *    signal to truncation (distortion and grain in headphones at a low volume).
 * (0.2.30: the knob has no boost any more, its top is unity; the saturation stays as a safety.) A gain above 1 saturates at the old ceiling, never wraps. mdac_g < 0: not started (power-on: the first block takes the
 * knob's level, no fade-in).
 * B13 (owner 2026-11-15: "very quiet compared to my other synths"; work/b13/level/REPORT.md): the DAC gets 8 dB more than before at every knob position, all of it after the limiter
 * (the mix, the limiter and USB unchanged): OUT_SHIFT 8 (+6 dB: the 16-bit scale is the DAC's full scale, not -6 dBFS of it) and the target x 1.2588 (+2 dB); fx.c's knee holds the
 * mix to 21900 (-3.5 dBFS), so the DAC's loudest sample is -1.5 dBFS and the saturation is never reached by the music (the click, added after, can). */
#ifndef MDAC_SHIFT
#define MDAC_SHIFT 9                                   /* the glide: 1/512 of the way a sample (tau 512 samples = 11.6 ms) */
#endif
/* Saturation to the 24-bit scale's 16-bit-Q15 range: +4,194,303 .. -4,194,304 (the old stage clipped at +4,194,176 .. -4,194,304). Written as a test of the bits above and the sign
 * mask instead of two limits, because a limit of -4,194,304 as a literal is an address in the chip ROM to build.py's "no reference to the ROM" check (0xFFC00000..) */
static inline int32_t mdac_sat(int32_t v)
{
    int32_t s = v >> 31;
    return (v >> (15 + OUT_SHIFT)) != s ? s ^ ((1 << (15 + OUT_SHIFT)) - 1) : v;
}
static int32_t mdac_g = -1;
static inline int32_t mdac_apply(int32_t v, int32_t k, uint32_t s)   /* v (Q15) x the gain (k = g >> s) -> Q15 x 2^OUT_SHIFT, saturated */
{
    return mdac_sat((v * k) >> (23u - s - OUT_SHIFT));
}
static __attribute__((noinline)) void master_dac(int32_t *out, uint32_t n)   /* audio ISR, after uac_tap */
{
    uint32_t i, s, q = song.master_q12, p = master_g16;
    int32_t t = (int32_t)((p >> 4) == q ? p << 8 : q << 12), g;   /* (the target, Q24) */
    t += (t >> 2) + (t >> 7) + (t >> 10);              /* B13: the DAC level +2.0 dB (x 1.2588; shifts, once a block): fx.c's knee ceiling (-3.5 dBFS) lands at -1.5 dBFS of the DAC; |out| <= 21900, k < 2^15 */
    g = mdac_g < 0 ? t : mdac_g;
    for (i = 0; i < n && g != t; i++) {                /* gliding: the gain moves a step every sample */
        int32_t d = t - g, k;
        g += d > 0 ? (d + ((1 << MDAC_SHIFT) - 1)) >> MDAC_SHIFT : d >> MDAC_SHIFT;
        s = g < (1 << 21) ? 6u : 9u;                   /* (m below 512: 3 more bits of the gain, where a whole step of it is a large part of it; |out| <= 32768, k < 2^15: fits int32) */
        k = g >> s;
        out[2u * i] = mdac_apply(out[2u * i], k, s);
        out[2u * i + 1u] = mdac_apply(out[2u * i + 1u], k, s);
    }
    s = g < (1 << 21) ? 6u : 9u;                       /* at rest (the rest of the block, or all of it): one gain */
    for (; i < n; i++) {
        out[2u * i] = mdac_apply(out[2u * i], g >> s, s);
        out[2u * i + 1u] = mdac_apply(out[2u * i + 1u], g >> s, s);
    }
    mdac_g = g;
}

static void audio_post(int32_t *out, uint32_t n)        /* after the block's master: USB, the DAC's level, the click */
{
#if FELUCCA_UAC
    uac_tap(out, n);                                    /* the USB audio input: the same master output, at the fixed level */
#endif
    master_dac(out, n);                                 /* MASTER's volume for the DAC (speaker, headphones): out is Q15 x 2^OUT_SHIFT from here */
#if defined(FELUCCA_HOST_SCOPE) && FELUCCA_HOST_SCOPE
    if (!scope_mix)                                     /* host OUT capture: music before the click */
        for (uint32_t i = 1; i < n; i += 2u)             /* (MIX: fx.c mix_block took it) */
            scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)(out[2u * i] >> OUT_SHIFT);
#endif
    click_render(out, n, OUT_SHIFT);                    /* the metronome (click.c): the DAC only, not USB */
}
#if MPJ_ON
/* B15 MPJ (twocore_run.c): a block whose master went to core 1 gets its DAC steps after the next block's mix (its master is closed by then: mix_gather waited for it), in block
 * order; the click's request is the one the block had at its own end (the next block's events may have set a new one, which waits) */
static struct { int32_t *out; uint8_t creq, on; } mpj_q;
static void mpj_fin(void)
{
    uint8_t r;
    if (!mpj_q.on)
        return;
    MPJ_SYNC();
    r = click_req;
    click_req = mpj_q.creq;
    audio_post(mpj_q.out, CTL);
    click_req = r;
    mpj_q.on = 0;
}
#endif
static void audio_block(int32_t *out, uint32_t n)       /* mix (fx.c), then Q15 -> 24 bit */
{
#if MPJ_ON
#if FELUCCA_UAC
    mpj.io_busy = uac.feed != 0;                        /* (USB audio reads the block's output at once: its master stays on core 0) */
#endif
    mix_block(out, n);
    mpj_fin();                                          /* (the block before's, if its master was on core 1) */
    if (mpj.posted_now) {
        mpj_q.out = out;
        mpj_q.creq = click_req;
        click_req = 0;
        mpj_q.on = 1;
        return;
    }
    audio_post(out, n);
#else
    mix_block(out, n);
    audio_post(out, n);
#endif
}

/* DUELL 0.2.11, the flash duck (SPEC 6.3): a flash erase keeps the interrupts off for its whole time (~40 ms; the device's own
 * figure: felucca_dbg.erase_max_us, the TEST screen), and the DMA loops abuf meanwhile, so the buffer must hold silence. Before
 * an erase (and a long program burst) the main loop asks for a duck (audio_duck): the ISR fades the DAC output to 0 over one half
 * (2.9 ms, linear), then renders on in silence (voices, envelopes and effects run as ever: what sounded keeps sounding after) and
 * counts the silent halves; after two, both halves of abuf are 0 and the erase may stop the interrupts. audio_unduck: the next
 * half fades back in over one half. A soft dip instead of the old step to silence and back (two clicks). USB (uac_tap) is not ducked */
#define DUCK_STEP (32768u / HALF_FRAMES)               /* Q15 a frame: 1 .. 0 over one half */
static volatile uint8_t duck_on;                       /* main loop: 1 fade out and stay silent, 0 fade back in */
static volatile uint8_t duck_zero;                     /* ISR: whole silent halves since the fade ended (up to 2: all of abuf) */
static uint32_t duck_g = 32768u;                       /* ISR: the gain, Q15 */
static __attribute__((noinline)) void duck_half(int32_t *o)   /* the half just rendered (24 bit) */
{
    uint32_t i, g0 = duck_g;
    for (i = 0; i < 2u * HALF_FRAMES; i += 2u) {
        if (duck_on)
            duck_g = duck_g > DUCK_STEP ? duck_g - DUCK_STEP : 0u;
        else
            duck_g = duck_g + DUCK_STEP < 32768u ? duck_g + DUCK_STEP : 32768u;
        o[i] = (int32_t)((int64_t)o[i] * duck_g >> 15);
        o[i + 1u] = (int32_t)((int64_t)o[i + 1u] * duck_g >> 15);
    }
    if (!duck_on)
        duck_zero = 0;
    else if (!g0 && duck_zero < DUCK_SILENT_HALVES)
        duck_zero++;
}
#ifndef AUDIO_DUCK_IDLE
#define AUDIO_DUCK_IDLE() ((void)0)                    /* (host tests: their simulated ISR runs here) */
#endif
#define DUCK_WAIT_TICKS (20000u * FM1_TICKS_PER_US)    /* 20 ms at most (the audio not running yet, the IRQs off: go on as before) */
static void audio_duck(void)                           /* main loop: fade out, wait until both halves of abuf are silent */
{
    uint32_t t0 = fm1_ticks();
    duck_on = 1;
    while (duck_zero < DUCK_SILENT_HALVES && audio_halves && fm1_ticks() - t0 < DUCK_WAIT_TICKS)
        AUDIO_DUCK_IDLE();
}
static void audio_unduck(void) { duck_on = 0; }       /* the next half fades back in */

/* Overload: two halves in a row above 85 % (the render plus TIMER5 nested in it), fade one voice over the
 * next block, and one more each half while it lasts. Keep each part's
 * lowest POLY note and MONO / LEGATO / UNISON lead, as the shared budget does.
 * DUELL 0.2.11 (SPEC 6.3): a single half over 100 % (it missed) sheds at once, without waiting for a second one: the
 * SHED_FAST_N oldest released voices (their tails) fade over the next block; held notes only by the rule above */
#define SHED_HELD 1u                                   /* shed_req bits: the 85 % rule (shed_voice) */
#define SHED_FAST 2u                                   /* .. a half over 100 %: released voices at once (shed_released) */
#define SHED_FAST_N 2u
static volatile uint8_t shed_req;
static uint8_t shed_over;                              /* bit k: the half k halves ago was over 85 % */
#define SHED_TICKS ((HALF_FRAMES * 1000000u / FS) * 85u / 100u * FM1_TICKS_PER_US)   /* 85 % of a half */
#define SHED_FULL_TICKS ((HALF_FRAMES * 1000000u / FS) * FM1_TICKS_PER_US)                 /* the whole half */
static uint32_t shed_count;

/* K3 THE EASE LADDER (TIDE 1.0.0-b4, CPU audit K3; core.h ease, EASE_*): in place of the two rules below (kill a voice after two halves over 85 %, LIGHT FX the
 * same), one level that eases the cheapest, least audible work first and cuts sound only when a half really missed. Driven by the ELAPSED half (all: TIMER5's
 * nested work included, what the DMA deadline sees):
 *  - up: two halves in a row over EASE_UP_PCT (85 %): one level up (at most one a half); a half over its whole time (a miss): to EASE_TAILS at least;
 *  - down, one level, the first of two rules to hold (both counts then start again; an up step restarts them too):
 *    a) PEAK: no half over EASE_DOWN_PCT (80 %) for EASE_DOWN_HALVES (~1 s) below LIGHT FX, for EASE_DOWN_HALVES_FX (~3 s) from LIGHT FX up. The meter's
 *       average alone would not do: the NOTES budget plans up to BUDGET_LIMIT on average (72 % in b1 .. b5; b6 N1's steps 76 / 80 / 84, budget.h DUELL_NOTES_STEP),
 *       so a scene that sits at 70 .. 85 % would never step down. (b6 N1: at 72 the busiest factory multis of the unit's GLIDE sweep already average 69 .. 70 % with a
 *       per-second elapsed peak of 86 .. 91 %; at a higher step neither release may hold in those scenes, so the ladder can stay at level 1 or 2 there: nothing is
 *       cut, but at level 2 a new note takes over a fading tail instead of a free voice while it lasts. The console's `cpu` shows the entries per level,
 *       ease_n1 .. ease_n5.) LIGHT FX saves
 *       5.5 .. 8.8 points, more than the 5-point gap between 85 and 80 %, so leaving it takes longer; and if level 3 is entered again within those 3 s
 *       of leaving it (a scene that needs it: 87 % without, 78 % with), the next release from 3 up waits 3 x as long (ease_fxl): at most one LIGHT FX
 *       blip in ~10 s instead of one every 3 s;
 *    b) AVERAGE: the meter's load (song.cpu_q8) under 70 % for ~3 s (OVL_CALM_HALVES, 0.2.25's LIGHT FX rule): a single half over 80 % every second
 *       would otherwise hold a level for good at a 50 % average;
 *  - EASE_TAILS fades the SHED_FAST_N oldest released tails on the half it is entered from below and on each half the overload lasts; EASE_HELD fades a held
 *    note (shed_voice) only on a half that missed. Levels 1 .. 3 fade nothing. The ISR's own shedding code (shed_req) is unchanged.
 * ease_step runs every half (range Z: tools/ram_fns_z.py); ease_set, a level change, is in the flash */
#define EASE_UP_PCT 85u
#define EASE_DOWN_PCT 80u
#define EASE_DOWN_HALVES ((FS + HALF_FRAMES / 2u) / HALF_FRAMES)   /* ~1 s of halves (345) */
#define EASE_DOWN_HALVES_FX OVL_CALM_HALVES            /* ~3 s (1034): from LIGHT FX up */
#define EASE_UP_TICKS SHED_TICKS                       /* (= EASE_UP_PCT of a half: the shedding rule's own threshold) */
#define EASE_DOWN_TICKS ((HALF_FRAMES * 1000000u / FS) * EASE_DOWN_PCT / 100u * FM1_TICKS_PER_US)
_Static_assert(EASE_UP_PCT == 85u, "audio.c: SHED_TICKS is 85 % of a half");
static uint16_t ease_calm, ease_avg;                   /* halves in a row with no half over EASE_DOWN_PCT, and with the meter under 70 %, while eased */
static uint16_t ease_since = 0xFFFFu;                  /* halves since the level last fell below LIGHT FX (saturating) */
static uint8_t ease_fxl;                               /* LIGHT FX was entered again within EASE_DOWN_HALVES_FX of leaving it: its peak release waits 3 x */
static uint32_t ease_n[EASE_MAX];                      /* entries into levels 1 .. 5 since power-on (the console's `cpu`); [EASE_LIGHT - 1] is LIGHT FX's count */
static __attribute__((noinline)) void ease_set(uint32_t n)
{
    uint32_t e;
    if (n >= EASE_LIGHT && ease < EASE_LIGHT)
        ease_fxl = ease_since < EASE_DOWN_HALVES_FX;
    if (n < EASE_LIGHT && ease >= EASE_LIGHT)
        ease_since = 0;
    for (e = ease; e < n; e++)                          /* (each level includes the ones below it: a jump enters each) */
        ease_n[e]++;
    ovl_lite = n >= EASE_LIGHT;
    ease = (uint8_t)n;
    ease_calm = ease_avg = 0;
}
static __attribute__((noinline)) uint32_t ease_step(uint32_t all)
{
    uint32_t e = ease, n = e, miss = all > SHED_FULL_TICKS, over;
    shed_over = (uint8_t)(shed_over << 1 | (all > EASE_UP_TICKS));
    over = (shed_over & 3u) == 3u;
    ease_since += ease_since != 0xFFFFu;
    if (over | miss) {
        ease_calm = ease_avg = 0;
        n += over && n < EASE_MAX;
        n = miss && n < EASE_TAILS ? EASE_TAILS : n;
        if (n >= EASE_TAILS)
            shed_req |= SHED_FAST;
        if (miss && n == EASE_HELD)
            shed_req |= SHED_HELD;
    } else if (n) {
        ease_calm = all > EASE_DOWN_TICKS ? 0u : (uint16_t)(ease_calm + 1u);
        ease_avg = song.cpu_q8 >= OVL_CALM_Q8 ? 0u : (uint16_t)(ease_avg + 1u);
        if (ease_calm >= (n < EASE_LIGHT ? EASE_DOWN_HALVES : ease_fxl ? 3u * EASE_DOWN_HALVES_FX : EASE_DOWN_HALVES_FX) || ease_avg >= (n >= EASE_LIGHT && ease_fxl ? 3u * OVL_CALM_HALVES : OVL_CALM_HALVES))   /* the quick-return back-off holds the average rule too */
            n--;
    }
    if (n != e)
        ease_set(n);
    return (all - t5_nested_ticks) / FM1_TICKS_PER_US;
}

static uint32_t shed_released(uint32_t n)             /* the n oldest released voices fade out; returns how many */
{
    uint32_t p, i, k;
    for (k = 0; k < n; k++) {
        voice_t *best = 0;
        for (p = 0; p < NPART; p++)
            for (i = 0; i < NVOICE; i++) {
                voice_t *v = &trk[p].v[i];
                if (v->active && !v->gate && v->stage != 4u && (!best || v->age < best->age))
                    best = v;
            }
        if (!best)
            break;
        voice_kill(best);
        shed_count++;
    }
    return k;
}

/* THE IN-BUFFER GUARD (b11, ported from DUELL 1.2 by OSCILL/R, GPL-3.0: audio.c guard_block). 1.2's bench: the missed halves at a chord's start ran only 10 to 30 us over (the half's
 * first block runs slow: its code reloads after the main loop), and the ladder above acts only after two halves over 85 % or after a miss: the first half over is already lost (TIDE's
 * 13 Nov sweep: 9 of the 14 seconds over 100 % were the first second after a load, PLAY or a glide's start). Here, after every block but the last: the time used so far (TIMER5's nested
 * work included, as the DMA deadline sees it) plus the blocks still to come at the running mean of a later block; when that would end past GUARD_TICKS, the GUARD_N oldest released
 * voices (keys let go: their tails; shed_released) fade now (voice_kill: over the next block), so the half's later blocks have them no more. A held note is never touched. Nothing
 * happens below the limit, so the sound changes only where a half would have been missed. A guard fade is not an ease-ladder step: the ladder (ease_step) reads only the halves'
 * elapsed times, as before; the fades count in shed_count (the console's) and guard_count */
#define GUARD_TICKS (SHED_FULL_TICKS * 96u / 100u)       /* 96 %: room for the interrupt's own late start */
#define GUARD_N 2u                                      /* tails faded at one check */
#ifndef AUDIO_BLK_TICKS
#define AUDIO_BLK_TICKS() fm1_ticks()                   /* (the block clock; host tests that model a half's time as one step at its end read it without a step: tests/audio_test.c) */
#endif
static uint32_t guard_q;                                /* the running mean of a later block (blocks 1 .. 3 of a half), ticks x 8 */
static uint32_t guard_count;                            /* checks that faded tails */
#if AUDIO_RING
static uint32_t guard_lim = GUARD_TICKS;                /* B15 ring: the half may end up to half a half past its time (the debt it leaves is then at most half the ring), less the debt it came in with */
#define GUARD_LIM guard_lim
#else
#define GUARD_LIM GUARD_TICKS
#endif
static inline void guard_block(uint32_t t0, uint32_t tb, uint32_t k)
{
    const uint32_t te = AUDIO_BLK_TICKS(), left = HALF_FRAMES / CTL - 1u - k;
    if (k)
        guard_q += (te - tb) - (guard_q >> 3);
    if (left && (te - t0) + left * (guard_q >> 3) > GUARD_LIM && shed_released(GUARD_N))
        guard_count++;
}
#if AUDIO_RING
/* B15 ring: what the ease ladder reads for a half of length `all` that came in `debt` late. With no debt left after it: its length, capped at EASE_UP_TICKS (never "over":
 * a spike the ring absorbed eases nothing; under the cap the ladder's way down works as before). With a debt left: just over EASE_UP_TICKS for a small one, rising to the
 * whole half (a "miss" to the ladder: EASE_TAILS) when the debt left reaches half the ring. A real miss is counted at the next entry (a debt of a whole half) */
static uint32_t ring_eff(uint32_t all, uint32_t debt)
{
    uint32_t nd = debt + all > RING_HALF_TICKS ? debt + all - RING_HALF_TICKS : 0u;
    if (!nd)
        return all < EASE_UP_TICKS ? all : EASE_UP_TICKS;
    return EASE_UP_TICKS + 1u + (uint32_t)((uint64_t)nd * (SHED_FULL_TICKS - EASE_UP_TICKS) / (RING_HALF_TICKS / 2u));
}
#endif

/* the level a released voice is ranked by: its envelope, or on an engine with its own envelopes (FM6: v->env is forced to full) the loudest carrier as the block left it */
static int32_t shed_level(track_t *t, voice_t *v)
{
    return ENGINES[t->engine]->ownenv ? fm6_sub_follow(t, v) << 9 : v->env;
}

static void shed_voice(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    int32_t low = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !v->gate && v->stage != 4u) {
                int32_t l = shed_level(&trk[p], v);
                if (!best || l < low)
                    best = v, low = l;
            }
        }
    if (best) {
        voice_kill(best);
        shed_count++;
        return;
    }
    for (p = 0; p < NPART; p++) {
        const track_t *t = &trk[p];
        uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && v->gate && v->stage != 4u &&
                ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u))) {
                if (!best || v->age < best->age)
                    best = v;
            }
        }
    }
    if (best) {
        voice_kill(best);
        shed_count++;
    }
}

void fm1_alnk0_irq(void)                       /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    felucca_dbg.in_audio = 1;                   /* first: TIMER5 nests from here on (main.c) */
    p = fm1_audio_pending();
#if DUELL_BENCH
    if (bench_tl_on && (p & FM1_AUDIO_HALF))            /* (bench builds only, bench suite stage 1: the timeline's due events into the MIDI-in ring, */
        bench_half();                                   /* before the clock starts: the half's own figure does not count it; src/bench.c) */
#endif
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, us, late, all, el, mp;
#if AUDIO_RING
        int32_t *o = rbuf;                              /* B15: this half is rendered ahead, into rbuf; the one rendered last time goes to the DMA now */
        uint32_t ring_miss = 0;
        fm1_audio_ack_half();                           /* (at once: a half that comes due while this one renders keeps its pending flag, and the next entry follows this one) */
        {
            uint32_t exp = ring_due + RING_HALF_TICKS, i;
            int32_t *d = &abuf[half * HALF_WORDS];
            if (!ring_run || (int32_t)(t0 - exp) < (int32_t)RING_JIT_TICKS) {
                ring_due = t0;                          /* on time (or the first entry): the due time follows the entry */
                ring_debt = 0;
            } else {
                ring_due = exp;
                ring_debt = t0 - exp;
                if (ring_debt + RING_COPY_TICKS >= RING_HALF_TICKS) {   /* a whole half late: the DMA played a stale half (one per half of lateness); start again from this entry */
                    if (!(duck_on && duck_zero >= DUCK_SILENT_HALVES))   /* (not after a flash erase: the interrupts were off on purpose and the DMA looped silence, as the old code counted none) */
                        ring_miss = (ring_debt + RING_COPY_TICKS) / RING_HALF_TICKS;
                    ring_due = t0;
                    ring_debt = 0;
                }
            }
            ring_run = 1;
            if (ring_debt > ring_debt_pk)
                ring_debt_pk = ring_debt;
            for (i = 0; i < HALF_WORDS; i++)
                d[i] = rbuf[i];
            guard_lim = GUARD_TICKS + RING_HALF_TICKS / 2u > ring_debt ? GUARD_TICKS + RING_HALF_TICKS / 2u - ring_debt : 0u;
        }
#else
        int32_t *o = &abuf[half * HALF_WORDS];
#endif
        if (shed_req) {
            uint32_t r = shed_req;
            shed_req = 0;
            if (!((r & SHED_FAST) && shed_released(SHED_FAST_N)) && (r & SHED_HELD))
                shed_voice();
        }
#if FELUCCA_UAC
        uac_render_start();
#endif
#if DUELL_TWOCORE
        tc_half_begin(t0);                              /* (the half's clock for core 0's deadlines: twocore_run.c) */
#endif
        for (b = 0; b < HALF_FRAMES; b += CTL) {
            const uint32_t tb = AUDIO_BLK_TICKS();
#ifdef FM1_INPUT_LAT
#if AUDIO_RING
            kb_out_tick = ring_due + (2u * HALF_FRAMES + b) * DAC_TICKS;   /* when this block plays (seq.c kb_lat): one half later with the ring */
#else
            kb_out_tick = t0 + (HALF_FRAMES + b) * DAC_TICKS;   /* when this block plays (seq.c kb_lat) */
#endif
#endif
            audio_block(o + 2u * b, CTL);
            guard_block(t0, tb, b / CTL);               /* b11: the in-buffer guard (above) */
        }
#if MPJ_ON
        mpj_fin();                                      /* (none waits here: the half's last block keeps its master on core 0; a safety net) */
#endif
        if (duck_on || duck_g < 32768u)                 /* (the flash duck: only around a flash write) */
            duck_half(o);
#if !AUDIO_RING
        fm1_audio_ack_half();
#endif
        audio_halves++;
        all = fm1_ticks() - t0;
#if AUDIO_RING
        (void)ease_step(ring_eff(all, ring_debt));      /* (B15: the ladder reads the debt this half leaves) */
        us = (all - t5_nested_ticks) / FM1_TICKS_PER_US;
#else
        us = ease_step(all);
#endif
        el = all / FM1_TICKS_PER_US;                    /* (K2: what the deadline saw, TIMER5 nested in it included) */
#if DUELL_BENCH
        cpu_el_last = el;                               /* (the bench's timeline window) */
#endif
        if (el > cpu_el_pk)
            cpu_el_pk = el;
#ifdef FELUCCA_METER_EST
        us = meter_est_us();
#endif
        if (us > audio_max_us)
            audio_max_us = us;
        cpu_sum += us;                                  /* the meter: no division here */
        cpu_n++;
#if DUELL_TEST_MODE
        if (us > cpu_pk)                                /* (the console's probe only) */
            cpu_pk = us;
#endif
        {
            uint32_t load = song.cpu_q8 * (CPU_AVG - 1u) + (us * 256u) / (HALF_FRAMES * 1000000u / FS) + audio_cpu_rem;
            song.cpu_q8 = load / CPU_AVG;
            audio_cpu_rem = load % CPU_AVG;
        }
#if AUDIO_RING
        late = ring_miss != 0;                          /* (B15: a miss is an entry a whole half late: the DMA replayed a half; a long render alone is not one) */
        mp = 0;
        cpu_miss += ring_miss;
#else
        late = fm1_audio_free_half() != half;
        mp = (el > CPU_BUDGET_US) + (el > 2u * CPU_BUDGET_US) + (el > 3u * CPU_BUDGET_US);   /* (K2: DMA periods missed: the half's elapsed time over 1, 2, 3 periods; the chip has no period counter, only the half selector) */
        cpu_miss += mp ? mp : late;                     /* (or one period: the DMA moved on while a half no longer than its time was rendered: a late entry) */
#endif
        if (late)
            felucca_dbg.late++;                         /* the DMA moved on while we rendered */
        felucca_dbg.halves++;
        felucca_dbg.last_us = us;
        if (us > felucca_dbg.max_us)
            felucca_dbg.max_us = us;
        felucca_dbg.cpu_q8 = song.cpu_q8;
    }
    felucca_dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)                   /* codec and ALNK0 bring-up (fm1_audio.h) */
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}
