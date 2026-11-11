/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Keyboard, scale, arpeggiator, sequencer and transport.
 * Everything here runs in the audio ISR, once per CTL-sample block, and
 * ends in trk_note_on / trk_note_off: engines never see where a note came from.
 * Four tracks, one transport: every track's pattern loops on its own LEN / DIV /
 * SWING / GATE (polymeter). The keys play the selected track; MIDI IN by MENU > MIDI > MIDI IN (G_ROUTE):
 * PARTS (0) the parts' own MIDI channels (GLO > MIDI: default 1 and 2; midi_pch) play parts 1 and 2, the other channels are ignored (midi_control.c midi_event); SEL (1)
 * every channel the selected track. Their CC1 (mod wheel), CC2 (breath controller), CC4 (foot controller), CC11 (expression) and channel aftertouch go to
 * the same track's modulation matrix (mod.c; the selected track: the one selected then).
 * A note into an armed track (song.rec) while
 * the transport runs is recorded into its pattern, quantised to its (swung) steps, with its
 * held length as TIE steps (rec_note, rec_hold, rec_release). With the ARP on, the notes the arp
 * plays are recorded, not the keys held (what plays back is what was heard); TIDE's ARP SEQ part is the exception: its keys sound as played and are
 * recorded into the pattern the arp plays (rec_arp_note). A key or MIDI note plays a
 * chord when the track's CHRD is on (chord.c): its notes go through the same input as so many keys. */
static const uint16_t SCALE_MASK[] = {
    0xFFF,                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                          /* PEN */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         /* MPEN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HARM */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHRY */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MEL (ascending) */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),              /* BLUES (minor) */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),              /* WHOLE */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10), /* DIMHW */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 11), /* DIMWH */
};

#define KB_SILENT 255u
static uint32_t kb_prev;
static volatile uint8_t kb_boot_hold;    /* #137: set from power-on to the end of the splash (main.c): the keys sound nothing,
                                          * and one held across its end stays silent until it is let go and pressed again */
static uint8_t kb_note[27], kb_trk[27];  /* per key: the note it started and on which track */
static uint8_t in_root;                  /* the root key_on built the chord of that part on (its own TRN): arp_add's held_dt is relative to it */
static uint8_t in_key;                   /* the key (+ 1) whose notes go into input_on now (key_on), 0: none (MIDI IN) */
#if DUELL_VELO
#define KEY_VEL(k) sense_vel(k)                  /* the velocity of key k's note: sense.c, after Velo's method (100 while the setting is OFF, and for the top 7 keys' fixed one) */
#define KEY_VELU(k) ((uint32_t)sense_vel(k))
#else
#define KEY_VEL(k) 100                           /* every key plays velocity 100 */
#define KEY_VELU(k) 100u
#endif
/* #191: where each track's held[i] came from, so the arp maps it again as it plays (arp_list: TRN and the global ROOT, SCALE, OFF-SCALE as
 * they are now). held_key: the key + 1 (bits 0..4) and the octave buttons then + 3 (bits 5..7); 0: not a key's own
 * mapping (MIDI IN), held[i] plays as it is. held_dt: held[i] above the key's
 * note (a CHRD chord's other notes). Out of track_t: its size stays as the audio ISR's trk[] indexing wants it */
static uint8_t held_key[NTRK][16];
static int8_t held_dt[NTRK][16];
static uint8_t arp_key_vel[NTRK];        /* the velocity of the newest key the track's arp was given (0: none yet; the arp plays 100 then) */
static volatile uint8_t kb_asleep;       /* MENU > SCREEN OFF (ui.c): the screen dark or waking, a key pressed now only
                                          * wakes it (silent, no layer, its release nothing) */
static uint8_t last_note = 60;
static volatile uint8_t transport_req;   /* 1 start, 2 stop, 3 restart, 4 STOP ALL (from the UI) */
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */
static volatile uint8_t panic_keep;      /* bit per track: that panic is a patch load (ui.c apply_preset_to, upreset.c up_load): with MODULES ARP on KEEPS RUNNING the arp's note list and phase stay */
static volatile uint8_t midi_hint;       /* MIDI IN played track n - 1, which is not the selected one (the UI says so) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

/* The parts' MIDI channels (0..15): GLO > MIDI (PART 1 CH, PART 2 CH), kept with the settings (ui.c ui_midi_p1 / ui_midi_p2, synced
 * by ui_input.c midi_ch_sync); the defaults are part 1 = channel 1, part 2 = channel 2. They are what each part RECEIVES (notes,
 * bend, CCs, PROGRAM CHANGE) and what the keys SEND (MIDI out). MENU > MIDI > MIDI IN (G_ROUTE): 0 PARTS (these channels), 1 SEL (every
 * channel plays the selected part); 2..4 (CH5-8 .. CH13-16 up to DUELL step 6) now read as PARTS. If both parts are on one channel part 1
 * takes it. LAYER: part 1's channel plays both parts, part 2's is not heard (midi_event) */
static volatile uint8_t midi_pch[2] = {0, 1};
/* the part (0 / 1) channel ch plays, -1 none */
static int32_t midi_part_of(uint32_t ch) { return ch == midi_pch[0] ? 0 : ch == midi_pch[1] ? 1 : -1; }
/* MIDI channel 0..15 of track i (keys -> MIDI out): its part's channel (the hidden parts: part 1's / 2's alternately, never played) */
static uint32_t trk_midi_ch(uint32_t i) { return midi_pch[i & 1u]; }

/* MIDI OUT of the ARP and the SEQUENCER (DUELL 1.1): what they play goes to the MIDI output (USB; the unit has no DIN output) like a played key
 * (key_on), on the part's MIDI channel, velocity as played. mo_bits[part] is a bit per note the arp / sequencer has sounding on MIDI out, mo_ch[part] the
 * channel those went out on: a note-on of a note already sounding ends it first (never two note-ons), a note-off of one not sounding is not sent, and
 * mo_flush (arp_forget: stop, panic, patch / mode change) ends what is left, on the channel it started on (no hanging notes if the channel is changed).
 * LAYER: only part 1 sends, as its keys do (part 2's channel is not used there). A played key with the ARP on sends nothing (key_on): the arp's notes are what sounds */
static uint32_t kb_mo;                         /* a bit per key: its note-ons went to MIDI out (key_on), so its note-offs do (key_off) */
static uint32_t mo_bits[NSHOWN][4];
static uint8_t mo_ch[NSHOWN];
static void mo_flush(track_t *t)
{
    uint32_t i = (uint32_t)(t - trk), w, b;
    if (i >= NSHOWN)
        return;
    for (w = 0; w < 4u; w++) {
        for (b = 0; mo_bits[i][w] && b < 32u; b++)
            if (((mo_bits[i][w] >> b) & 1u) && midi_out_event(0x08u | (0x80u | mo_ch[i]) << 8 | (w * 32u + b) << 16))
                mo_bits[i][w] &= ~(1u << b);        /* (an Off the ring dropped stays marked: the note is still on) */
    }
}
static void mo_on(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i = (uint32_t)(t - trk), mc;
    if (i >= NSHOWN || note > 127u || (duell_mode == DM_LAYER && i))
        return;
    mc = trk_midi_ch(i);
    if (mc != mo_ch[i]) {
        mo_flush(t);
        mo_ch[i] = (uint8_t)mc;
    }
    if (((mo_bits[i][note >> 5] >> (note & 31u)) & 1u) && !midi_out_event(0x08u | (0x80u | mc) << 8 | note << 16))
        return;                                     /* (the sounding note cannot be ended: the ring is full) */
    if (midi_out_event(0x09u | (0x90u | mc) << 8 | note << 16 | (vel < 1u ? 1u : vel > 127u ? 127u : vel) << 24))
        mo_bits[i][note >> 5] |= 1u << (note & 31u);   /* (marked only when the On went out: a dropped On gets no Off later) */
    else
        mo_bits[i][note >> 5] &= ~(1u << (note & 31u));
}
static void mo_off(track_t *t, uint32_t note)
{
    uint32_t i = (uint32_t)(t - trk);
    if (i >= NSHOWN || note > 127u || !((mo_bits[i][note >> 5] >> (note & 31u)) & 1u))
        return;
    if (midi_out_event(0x08u | (0x80u | mo_ch[i]) << 8 | note << 16))
        mo_bits[i][note >> 5] &= ~(1u << (note & 31u));   /* (an Off the ring dropped keeps the note marked) */
}
/* the arp's / sequencer's own notes: the voice and MIDI out together */
static void arp_note_on(track_t *t, uint32_t note, uint32_t vel) { trk_note_on(t, note, vel); mo_on(t, note, vel); }
static void arp_note_off(track_t *t, uint32_t note) { trk_note_off(t, note); mo_off(t, note); }

/* the 27 keys from F: black or white, and the key's place among the keys of its colour (white 0..15, black
 * 0..10) */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
/* the steps a track plays: always its own pattern (DUELL is a jam instrument: no song mode, PLAY loops each part's
 * pattern, P_SLEN steps, until STOP) */
static const step_t *seq_steps(const track_t *t) { return t->step; }

static uint32_t scale_mask(const track_t *t)
{
    (void)t;
    return SCALE_MASK[clamp(scale_g, 0, sizeof SCALE_MASK / sizeof SCALE_MASK[0] - 1)];
}

/* note n moved DOWN onto the global scale (ROOT, SCALE): the SEQUENCER arp's offset notes and the diatonic chords' root rule */
static int32_t scale_snap(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12;
    while (guard-- && !((mask >> (uint32_t)((n - root_g + 120) % 12)) & 1u))
        n--;
    return n;
}

/* OFF-SCALE KEYS: a key's or MIDI note's note n -> the note that sounds. SNAP (the default): the NEAREST note of the global scale, a tie
 * goes DOWN (C major: C# gives C, F# gives F); PLAY, or CHROMATIC (nothing is outside it): n as it is. Every source goes through this
 * (kb_pitch for the keys, chord_make for any note that starts a chord or a single note); each part maps a key with its own TRN (key_on) */
static int32_t note_snap_to(const track_t *t, int32_t n, uint32_t force)
{
    uint32_t mask = scale_mask(t), d;
    if ((offscale_g != OS_SNAP && !force) || mask == 0xFFFu)
        return n;
    for (d = 0; d <= 6u; d++) {
        if ((mask >> (uint32_t)((n - (int32_t)d - root_g + 240) % 12)) & 1u)
            return n - (int32_t)d;                      /* (down first: a tie goes down) */
        if ((mask >> (uint32_t)((n + (int32_t)d - root_g + 240) % 12)) & 1u)
            return n + (int32_t)d;
    }
    return n;
}
static int32_t note_snap(const track_t *t, int32_t n) { return note_snap_to(t, n, 0u); }

/* key k's note with the octave buttons at oct: TRANSPOSE (TRN, P_TRANS, semitones; DUELL 1.1: the patch's own, it comes with a patch load; each part's own, so in a LAYER part 2's key_on
 * maps with trk[1]), then the global OFF-SCALE rule (note_snap). The keys (kb_map) and the arp's latched
 * keys (arp_list, #191: as the keys play now, the octave they were pressed in) */
static uint32_t kb_pitch(const track_t *t, uint32_t k, int32_t oct)
{
    int32_t n = 53 + (int32_t)k + 12 * oct + t->p[P_TRANS];
    /* DUELL 0.2.14: in PERFORM the black keys (the only keys that play) snap to the global scale whatever OFF-SCALE says (SCALE CHROMATIC: unchanged) */
    return (uint32_t)clamp(note_snap_to(t, n, perf_keys && key_black(k)), 0, 127);
}

static uint32_t kb_map(const track_t *t, uint32_t k)
{
    return kb_pitch(t, k, song.octave);
}

#include "chord.c"

/* ------------------------------------------------------------- arp --- */
/* ARP TARGET (SPEC 2.3), HOLD: the root voices of the latched keys (let go, still sounding) end */
static __attribute__((noinline)) void atg_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++)
        trk_note_off(t, t->held[i]);
    t->atg_lat = 0;
}

/* HOLD ALL (P_AHOLD 2) with the arp off: the keys' own voices are the latched chord (held[]) */
static int hold_all_keys(const track_t *t) { return arp_hold(t) == 2 && !arp_mode(t); }   /* (0.2.14: as they act now, MODULES' ARP / SEQ OFF: core.h) */

static int in_held(const track_t *t, uint32_t note)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return 1;
    return 0;
}

static uint32_t arp_wait(const track_t *t);        /* (below) */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (arp_hold(t) && t->arp_phys == 0u) {
        if (t->atg_lat)
            atg_release(t);                         /* ARP TARGET: the latched root voices end */
        if (hold_all_keys(t))
            for (i = 0; i < t->nheld; i++)
                trk_note_off(t, t->held[i]);        /* HOLD ALL, arp off: the old chord's voices end */
        t->nheld = 0;                               /* new chord replaces the latched one */
    }
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u) {
        /* #191: a key the track's scale keyboard maps is kept as that
         * key, so TRN and the global ROOT, SCALE and OFF-SCALE reach the notes the arp holds (arp_list); the octave stays the one it was
         * pressed in. Anything else (MIDI IN) plays as it came, as before */
        uint8_t *hk = held_key[trk_index(t)];
        hk[t->nheld] = in_key
                           ? (uint8_t)(in_key | (uint32_t)(song.octave + 3) << 5) : 0u;
        held_dt[trk_index(t)][t->nheld] = (int8_t)(hk[t->nheld] ? (int32_t)note - (int32_t)in_root : 0);
        t->held[t->nheld++] = (uint8_t)note;
    }
    if (t->nheld == 1u) {
        t->arp_pos = 0xFFFFFFFu - arp_wait(t);      /* the first step: at once, or while PLAY runs on the next RATE boundary of the transport */
        t->arp_idx = 0xFFFFFFFFu;
        t->atg_note = t->atg_vel = 0;               /* ARP TARGET: a new phrase starts on the root */
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    uint8_t *hk = held_key[trk_index(t)];
    int8_t *dt = held_dt[trk_index(t)];
    if (t->arp_phys)
        t->arp_phys--;
    if (arp_hold(t))
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note) {
            hk[k] = hk[i];
            dt[k] = dt[i];
            t->held[k++] = t->held[i];
        }
    t->nheld = (uint8_t)k;
}

#define ND_NONE 0u                      /* track_t nd_late / nd_early: nothing latched (seq_tick; else its seq_pos + 1) */
static void rec_note(track_t *t, uint32_t note, uint32_t vel);
static void rec_release(track_t *t, uint32_t note);
static void rec_hold(track_t *t, uint32_t idx, uint32_t len);
#if DUELL_TIDE
static volatile uint8_t tide_rec_replace;       /* TIDE SETTINGS REC: 0 ADD (the default), 1 REPLACE (tide_ui.c TS_REC) */
static uint32_t rec_key[NSHOWN][4];             /* the keys of an armed ARP SEQ part that went straight to the voices (rec_arp_keys): their key-up skips the arp */
#endif
static int rec_on(const track_t *t) { return ((song.rec >> trk_index(t)) & 1u) && song.playing; }
/* the arp records what it plays, except on an ARP SEQ part: there it plays the pattern recording would write into (it climbed to 127 in
 * three loops, audit liverec L2). Raw P_AMODE, as seq_play reads it, so MODULES' SEQ OFF does not change which path records */
#if DUELL_TIDE
static int rec_arp_on(const track_t *t) { return rec_on(t) && t->p[P_AMODE] != AM_SEQ_ID; }
#else
#define rec_arp_on(t) rec_on(t)                         /* (plain DUELL: the arp records what it plays, ARP SEQ included) */
#endif

static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);   /* (below; motion.c) */
#include "motion.c"

/* the ARP's modes (P_AMODE). Append-only: a stored value keeps its meaning. ORD (1.2, it played as UP before) plays
 * the held notes in the order they were pressed, whatever ARP 2's ORD switch says (that switch orders the list of the
 * other modes). 1.2 (Discussion #95), over the note list (the held notes low to high, or as played,
 * across OCT octaves), C E G held: DNUP G E C E; UP+8 C E G C' (the first note again an octave above the top);
 * CONV C G E (outside in); DIVG E G C (inside out); PINKY C G E G (the top note between the others); THUMB
 * C E C G (the bottom one); WALK a random step up or down the list (bouncing off its ends); CHORD all the held
 * notes together (the first 4), an octave higher each step through OCT */
enum { AM_OFF, AM_UP, AM_DN, AM_UPDN, AM_RND, AM_ORD, AM_REPEAT, AM_DNUP, AM_UP8, AM_CONV, AM_DIVG, AM_PINKY,
       AM_THUMB, AM_WALK, AM_CHORD, AM_SEQ };
_Static_assert(AM_SEQ == AM_SEQ_ID, "core.h arp_mode: the SEQUENCER mode's value");
/* SEQ (DUELL): the track's step pattern (P_SLEN steps, SEQ button) as the arp's rhythm: each arp tick plays the next
 * step; empty / rest / tie step = rest; a NOTE step = all the held notes (the first 4, lowest first) together, moved by
 * step.note[0] - 60 semitones, onto the scale when SCALE is not CHR; ACCENT = velocity 127; gate = AGATE; OCT: one
 * octave up per pass through the pattern */

#include "midi_grid.c"       /* external clock: exact progress in musical units and fractional carries (fix A) */

static uint32_t arp_vel(const track_t *t) { return arp_key_vel[trk_index(t)] ? arp_key_vel[trk_index(t)] : 100u; }

/* an arp note 0 (C-1) is a sounding note: its value reads as none, arp_nz says it sounds (bit 0 arp_note, 1..3 arp_ch) */
#define ARP_UP(t) ((t)->arp_note | ((t)->arp_nz & 1u))
#define ARP_CH_UP(t) ((t)->arp_ch[0] | (t)->arp_ch[1] | (t)->arp_ch[2] | ((t)->arp_nz & 14u))
static __attribute__((noinline)) void arp_put(track_t *t, uint32_t i, uint32_t nt)   /* slot i (0 arp_note, 1.. arp_ch) plays nt */
{
    if (i)
        t->arp_ch[i - 1u] = (uint8_t)nt;
    else
        t->arp_note = (uint8_t)nt;
    t->arp_nz = (uint8_t)((t->arp_nz & ~(1u << i)) | (!nt << i));
}

static void arp_forget(track_t *t)               /* (its notes ended otherwise: trk_all_off) */
{
    mo_flush(t);
    t->arp_note = t->arp_ch[0] = t->arp_ch[1] = t->arp_ch[2] = t->arp_nz = 0;
    t->atg_note = t->atg_vel = t->atg_lat = 0;
    t->arp_ph = 0;
}

/* CHORD's notes. Not inlined: they run once an arp step, and kept out of the audio ISR's loop over the tracks
 * (tests/target_budget.py) */
static __attribute__((noinline)) void arp_chord_off(track_t *t, int rec)   /* the other notes end */
{
    uint32_t i;
    for (i = 0; i < 3u; i++)
        if (t->arp_ch[i] || ((t->arp_nz >> (i + 1u)) & 1u)) {
            if (rec)
                rec_release(t, t->arp_ch[i]);
            arp_note_off(t, t->arp_ch[i]);
            t->arp_ch[i] = 0;
            t->arp_nz &= (uint8_t)~(2u << i);
        }
}

static __attribute__((noinline)) void arp_chord(track_t *t, const uint32_t *list, uint32_t n)
{
    uint32_t i, nt, top = 0, o = t->arp_idx % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
    if (n > 4u)
        n = 4u;
    for (i = 0; i < n; i++) {                       /* the held notes (sorted or as played), o octaves up */
        nt = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
        if (nt == 127u && top++)
            continue;                               /* (notes clamped onto the top one: once) */
        arp_put(t, i, nt);
        if (rec_arp_on(t))
            rec_note(t, nt, arp_vel(t));
        arp_note_on(t, nt, arp_vel(t));
    }
}

/* the notes SEQ's step arp_idx plays (out, their number returned; 0: a rest), *vel their velocity */
static uint32_t arp_seq_notes(track_t *t, const uint32_t *list, uint32_t n, uint8_t *out, uint32_t *vel)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, k = t->arp_idx, i, j, m = 0, nt, lo = 127, oc;
    const step_t *s = TIDE_PLAY_STEP(t, TIDE_ARP_IDX(t, k, len));   /* TIDE hooks (H2, H3b) */
    int32_t off;
    if (s->time != ST_NOTE || !s->n)
        return 0;                                   /* a rest */
    oc = (k / len) % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
    off = (int32_t)s->note[0] - 60;
    for (i = 0; i < n; i++)
        lo = list[i] < lo ? list[i] : lo;
    for (i = 0; i < n; i++) {                       /* (lo: the root; every held note moves by the same offset) */
        int32_t x = (int32_t)list[i] + off + 12 * (int32_t)oc;
        if (scale_of(t))
            x = scale_snap(t, x);
        nt = (uint32_t)clamp(x, 0, 127);
        for (j = 0; j < m && out[j] != nt; j++)
            ;
        if (j == m && m < 4u)
            out[m++] = (uint8_t)nt;
    }
    *vel = (s->flags & SF_ACCENT) ? 127u : arp_vel(t);
    return TIDE_ARP_OUT(t, out, m, vel);             /* TIDE hook (D1, MR4): its velocity with the dynamics, a thinned chord's two voices */
}

/* whether SEQ's step k is a TIE (it holds the note before it: arp_seq lengthens that note, arp_fire leaves it sounding) */
static uint32_t arp_seq_tie(track_t *t, uint32_t k)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    return TIDE_PLAY_STEP(t, TIDE_ARP_IDX(t, k, len))->time == ST_TIE;
}

static __attribute__((noinline)) void arp_seq(track_t *t, const uint32_t *list, uint32_t n, uint32_t period)
{
    uint8_t out[4];
    uint32_t vel, i, ties = 0, m = arp_seq_notes(t, list, n, out, &vel);
    if (!m)
        return;                                     /* a rest */
    while (ties < 31u && arp_seq_tie(t, t->arp_idx + ties + 1u))
        ties++;                                     /* the TIE steps after it hold the note (audit G07: they were rests, every note a short stab) */
    for (i = 0; i < m; i++) {
        arp_put(t, i, out[i]);
        if (rec_arp_on(t))
            rec_note(t, out[i], vel);
        arp_note_on(t, out[i], vel);
    }
    t->arp_off = period * ties + period * (uint32_t)t->p[P_AGATE] / 128u;
}

static void arp_silence(track_t *t)              /* the sounding arp note(s) end */
{
    if (ARP_CH_UP(t))
        arp_chord_off(t, 0);
    arp_note_off(t, t->arp_note);
    t->arp_note = 0;
    t->arp_nz &= (uint8_t)~1u;
}

static void arp_off(track_t *t)                  /* the sounding arp note(s) end (a recorded one too) */
{
    if (ARP_CH_UP(t))
        arp_chord_off(t, rec_arp_on(t));
    arp_note_off(t, t->arp_note);
    if (rec_arp_on(t))
        rec_release(t, t->arp_note);
    t->arp_note = 0;
    t->arp_nz &= (uint8_t)~1u;
}

/* the place in a note list of len notes that step i of an arp in MODE m plays (i counts from 0, the first note played); walk: WALK's place (kept between calls, any start); rnd: the source of
 * RANDOM's and WALK's numbers. One copy for the arp (arp_pick: rng) and for the ARP pages' picture (ui_graph.c graph_arp: a fixed sequence), so a new MODE added here shows in the picture
 * (audit U06: the picture had its own copy, which had drifted: it started at step 1, so the first column showed the second note) */
static __attribute__((noinline)) uint32_t arp_index(uint32_t m, uint32_t i, uint32_t len, uint32_t *walk, uint32_t (*rnd)(void))
{
    uint32_t cyc = len > 1u ? 2u * len - 2u : 1u, k = i % cyc, w;
    switch (m) {
    case AM_DN:
        return len - 1u - i % len;
    case AM_UPDN:
        return k < len ? k : cyc - k;
    case AM_DNUP:
        return k < len ? len - 1u - k : k - (len - 1u);
    case AM_RND:
        return rnd() % len;
    case AM_CONV:
    case AM_DIVG:
        k = i % len;
        if (m == AM_DIVG)
            k = len - 1u - k;
        return (k & 1u) ? len - 1u - k / 2u : k / 2u;
    case AM_PINKY:
        return len < 2u ? 0u : (k & 1u) ? len - 1u : k / 2u;
    case AM_THUMB:
        return len < 2u ? 0u : (k & 1u) ? 1u + k / 2u : 0u;
    case AM_WALK:
        w = *walk;
        if (!i)
            w = 0;                                  /* (from the bottom) */
        else if (w >= len)
            w = len - 1u;                           /* (notes let go) */
        else if (len > 1u)
            w = !w ? 1u : w == len - 1u ? w - 1u : (rnd() >> 7) & 1u ? w + 1u : w - 1u;
        *walk = w;
        return w;
    default:
        return i % len;
    }
}
/* the place in the note list (len notes) the step arp_idx plays (not inlined, as arp_chord); UP+8 adds its note */
static __attribute__((noinline)) uint32_t arp_pick(track_t *t, uint32_t *list, uint32_t len)
{
    uint32_t w = t->arp_walk, r;
    if (t->p[P_AMODE] == AM_UP8 && len < 64u)       /* UP+8: the first note again over the top */
        list[len++] = (uint32_t)clamp((int32_t)list[0] + 12 * t->p[P_AOCT], 0, 127);
    r = arp_index((uint32_t)t->p[P_AMODE], t->arp_idx, len, &w, rng);
    t->arp_walk = (uint8_t)w;
    return r;
}

/* n: samples the arp advances by (the external clock's while it runs, 0 stopped); gate_n: samples its
 * gate counts down by (real time when the external transport stops: a tapped or latched note still ends) */
static volatile uint32_t beat_pos, beat_n;      /* samples into the beat, the beat of the bar (0..3) */

/* the metronome (click.c) and the count-in (1.1, Discussion #131). The click follows the song tempo's quarter notes
 * (BPM, or the external clock's 24 pulses), 4 to the bar, the first accented, whatever a track's DIV or SWING: a beat is
 * four 1/16 steps as the sequencer counts them (click_beat_len: 4 x div_samples(1/16), which may be a few samples
 * shorter than beat_samples, so the click never drifts from the 1/16 tracks it is recorded against). clk_pos and
 * clk_step count the 1/16 steps as seq_tick counts a 1/16 track's (and midi_clock_pulse rescales clk_pos with them when
 * an external clock's tempo moves): CLK_START at seq_start clicks the first beat in the block step 0 starts in, and
 * beat k in the block a 1/16 track's step 4k starts in (every 4th step; step 0 of 16, the bar's first, accented).
 * The count-in: with MENU > COUNT-IN on, PLAY from stop with a track armed (REC while stopped starts PLAY too) on
 * the internal clock counts cin_total beats first (1 or 2 bars, clicked whatever CLICK is), then
 * starts the sequencer at step 1 exactly that many beats later (song.playing stays 0 until then: nothing plays or
 * records; the keys sound). A note into an armed track in the count-in's last eighth (the second half of its last
 * beat) is recorded onto step 1 when it starts (cin_keep, cin_flush), held on or let go as it was. An external clock
 * never counts in (its Start starts). STOP (seq_stop) cancels it. */
#define CLK_START 0xFFFFFFFFu
static uint32_t clk_pos = CLK_START, clk_step;  /* samples into the 1/16 step, the step of the bar (0..15) */
static volatile uint8_t cin_left, cin_total;    /* the count-in's beats still to come (0: none), of how many */
static volatile uint32_t cin_pos;               /* samples into its beat playing */
static uint8_t cin_flush;                       /* the count-in ended in this block: record what was kept */
static uint8_t cin_note[NTRK][4], cin_vel[NTRK][4], cin_n[NTRK], cin_up[NTRK];   /* kept notes; cin_up bit k: let go */
static int seq_counting(void) { return cin_left != 0u; }
static uint32_t click_beat_len(void) { return 4u * div_samples(2); }   /* DIV 2: 1/16 */
static int click_wanted(void)                  /* CLICK REC: while a track is armed (not in a song: no recording) */
{
    return click_mode == CLICK_ON || (click_mode == CLICK_REC && song.rec);
}
/* the arp's note list (list, its length returned; 0: nothing to play): the held notes (sorted or as played) over OCT
 * octaves, *cnt of them. #191: the notes as they play now, a key's mapped again through kb_pitch (TRN, ROOT, SCALE,
 * OFF-SCALE now, the octave it was pressed in; a chord's note as far above it), the rest (MIDI IN) as they came; each once
 * (two keys may meet on one note now), a key silent now (KB_SILENT) left out. With nothing changed
 * since the keys were pressed, exactly the list of held[] as before */
static uint32_t arp_list(const track_t *t, uint32_t *list, uint32_t *cnt)
{
    uint32_t i, j, n = 0, x, s, last = 0, len = 0, o;
    const uint8_t *hk = held_key[trk_index(t)];
    const int8_t *dt = held_dt[trk_index(t)];
    for (i = 0; i < t->nheld; i++) {
        x = t->held[i];
        if ((s = hk[i]) != 0u) {
            x = kb_pitch(t, (s & 31u) - 1u, (int32_t)(s >> 5) - 3);
            if (x == KB_SILENT)
                continue;
            x = (uint32_t)clamp((int32_t)x + dt[i], 0, 127);
        }
        last = x;
        for (j = 0; j < n && list[j] != x; j++)
            ;
        if (j == n)
            list[n++] = x;
    }
    n = TIDE_SEQ_CHORD(t, list, n);                 /* TIDE hook (H2b) */
    *cnt = n;
    if (!t->p[P_AORDER] && t->p[P_AMODE] != AM_ORD)   /* (ORD: as pressed, held[] keeps that order) */
        for (i = 1; i < n; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                x = list[j];
                list[j] = list[j - 1];
                list[j - 1] = x;
            }
    for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
        for (i = 0; i < n && len < 64u; i++)
            list[len++] = clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (n && t->p[P_AMODE] == AM_REPEAT) { list[0] = last; len = 1; } /* REPEAT: last played note, no octave traversal */
    return len;
}

/* the arp's PROB roll: 0 never plays, 127 always (the random draw is still taken, so the sequence of draws is the same) */
static __attribute__((noinline)) int arp_roll(const track_t *t)
{
    uint32_t r = (uint32_t)(rng() & 127u);
    return r < (uint32_t)t->p[P_APROB] || t->p[P_APROB] >= 127;
}

/* ARP TARGET (SPEC 2.3): with units masked (params.c atgt_bits) an arp step plays no note: the held keys sound as notes
 * (input_on), and the step sets the note their masked units go to (atg_note: the note the classic arp would play; SEQ
 * and CHORD, which play several, their lowest) and the gate on those units' level (atg_vel: the step's velocity, 100,
 * accent 127, for GATE of the step; voice.c atg_voice). A rest (SEQ's empty step, a step PROB skips) closes the gate; the
 * units stay on the last note */
static __attribute__((noinline)) void atg_fire(track_t *t, uint32_t *list, uint32_t cnt, uint32_t len, uint32_t period)
{
    uint32_t nt = 128, vel = arp_vel(t), i, m, o, ties = 0;
    uint8_t out[4];
    if (t->p[P_AMODE] == AM_SEQ && arp_seq_tie(t, t->arp_idx))
        return;                                     /* a TIE: the step before goes on (its gate was lengthened with it; audit G07, as arp_fire) */
    if (ARP_UP(t))
        arp_off(t);                                 /* (a classic step's note: the mask just changed) */
    if (t->p[P_AMODE] == AM_SEQ) {
        if (arp_roll(t))
            for (i = 0, m = arp_seq_notes(t, list, cnt, out, &vel); i < m; i++)
                nt = out[i] < nt ? out[i] : nt;
        while (ties < 31u && arp_seq_tie(t, t->arp_idx + ties + 1u))
            ties++;                                 /* the TIE steps after it hold it */
    } else if (t->p[P_AMODE] == AM_CHORD) {         /* CHORD: its lowest note, an octave up each step through OCT */
        o = t->arp_idx % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
        for (i = 0; i < cnt; i++)
            nt = list[i] < nt ? list[i] : nt;
        nt = (uint32_t)clamp((int32_t)nt + 12 * (int32_t)o, 0, 127);
        if (!arp_roll(t))
            nt = 128;
    } else {
        i = arp_pick(t, list, len);
        if (arp_roll(t))
            nt = list[i];
    }
    if (nt > 127u || !nt) {
        t->atg_vel = 0;                             /* a rest: the gate closes, the pitch stays */
        return;
    }
    t->atg_note = (uint8_t)nt;
    t->atg_vel = (uint8_t)vel;
    t->arp_off = period * ties + period * (uint32_t)t->p[P_AGATE] / 128u;
}

#if DUELL_TIDE
static int arp_rec_fire(track_t *t);
#endif
/* an arp step starts (period: its length): the next note of the list. Not inlined: once a step, kept out of the audio
 * ISR's loop over the tracks (tests/target_budget.py) */
static __attribute__((noinline)) void arp_fire(track_t *t, uint32_t period)
{
    uint32_t cnt, list[64], len, j;
    if (!(len = arp_list(t, list, &cnt))) {        /* (every key silent now) */
        if (ARP_UP(t))
            arp_off(t);
        return;
    }
    t->arp_idx++;
    TIDE_ARP_TICK(t);                               /* TIDE hook (H3b) */
#if DUELL_TIDE
    if (t->p[P_AMODE] == AM_SEQ && !(mods_off & MODS_SEQ))   /* b7 (review B2): the step's locks and automation on the step the arp plays (seq_play leaves them) */
        motion_step(t, TIDE_ARP_IDX(t, t->arp_idx, t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u), &motion, 1u);
    if (t->p[P_AMODE] == AM_SEQ && arp_rec_fire(t))
        return;                                     /* live recording put a key into this step already: it sounds, the step does not play again */
#endif
    if (atgt_bits(t)) {                             /* ARP TARGET: the units move, no note (atg_fire) */
        atg_fire(t, list, cnt, len, period);
        return;
    }
    t->atg_vel = 0;                                 /* (arp_off is the note's again) */
    if (t->p[P_AMODE] == AM_SEQ) {                  /* SEQ: the step pattern (chance: the arp's PROB) */
        if (arp_seq_tie(t, t->arp_idx))
            return;                                 /* a TIE: the note before it goes on (its length was set with it) */
        if (ARP_UP(t))
            arp_off(t);
        if (TIDE_APROB(t, arp_roll(t)))   /* TIDE hook (H3c) */
            arp_seq(t, list, cnt, period);
        return;
    }
    j = t->p[P_AMODE] == AM_CHORD ? 0u : arp_pick(t, list, len);
    if (ARP_UP(t))
        arp_off(t);
    if (arp_roll(t)) {
        t->arp_off = period * (uint32_t)t->p[P_AGATE] / 128u;
        if (t->p[P_AMODE] == AM_CHORD) {            /* CHORD: the first 4 notes, an octave up each step */
            arp_chord(t, list, cnt);
            return;
        }
        arp_put(t, 0, list[j]);
        if (rec_arp_on(t))
            rec_note(t, t->arp_note, arp_vel(t));    /* recording: what the arp plays */
        arp_note_on(t, t->arp_note, arp_vel(t));
    }
}

/* SWING is stored and shown as 50 .. 75 % (50 = straight, owner 2026-10-28): the percentage of a pair of steps (two periods) that the first
 * step takes, so the offbeat lands where the display says (audit S03; it used to land at 70 % for 75, 62.9 for 66).
 * sw_ex: the part of a SWG over 50 (0 .. 25); sw_len: the samples the even step is longer by, the odd one shorter, for the sum of such parts
 * (the track's SWG and the global one add, at most 25 = 75 %): the offbeat is at period + ex * period / 50 = (50 + 2 * ex) % of the pair */
/* a triplet division (N_DIV 4 = 8T, 5 = 16T, 10 = 4T, 11 = 2T, 12 = 32T) is not swung: its steps are already the third of a beat, and a swung pair measured 70 % (audit S14; MPC and Elektron leave triplets straight) */
#define DIV_TRIPLET(d) ((0x1C30u >> ((d) & 31u)) & 1u)
static inline int32_t sw_ex(int32_t pct) { return pct <= 50 ? 0 : pct >= 75 ? 25 : pct - 50; }
static inline int32_t sw_len(int32_t ex, uint32_t period) { return (ex > 25 ? 25 : ex) * (int32_t)period / 50; }
/* the arp's swing in samples for a step of `period` (ARP RATE, the part's ARP SWING + the global TEMPO SWING as the sequencer adds them (b4 UI round 2, R4); none on a triplet): the one formula, arp_step's and arp_dur's */
static inline int32_t arp_sw(const track_t *t, uint32_t period) { return DIV_TRIPLET(t->p[P_ARATE]) ? 0 : sw_len(sw_ex(t->p[P_ASWING]) + sw_ex(song.g[G_SWING]), period); }
static void arp_step(track_t *t, uint32_t n, uint32_t gate_n)
{
    uint32_t period, duration;
    int32_t sw;
    if (ARP_UP(t)) {
        if (t->arp_off <= gate_n)
            arp_off(t);
        else
            t->arp_off -= gate_n;
    }
    if (t->atg_vel) {                               /* ARP TARGET: the step's gate */
        if (t->arp_off <= gate_n)
            t->atg_vel = 0;
        else
            t->arp_off -= gate_n;
    }
    if (TIDE_ARP_PAUSED(t)) {                       /* TIDE hook (H2c): a SEQ arp on a latched chord waits for PLAY */
        t->arp_ph = 0;
        if (ARP_UP(t))
            arp_off(t);
        return;
    }
    if (!arp_mode(t) || (!t->nheld && !TIDE_SEQ_GHOST(t))) {   /* (0.2.14: MODULES' ARP OFF / SEQ OFF: as the arp's MODE OFF; TIDE hook H2b) */
        t->arp_ph = 0;                              /* (a new press starts a new phrase) */
        if (!t->nheld && ARP_UP(t))
            arp_off(t);
        return;
    }
    period = div_samples((uint32_t)t->p[P_ARATE]);
    sw = arp_sw(t, period);
    if (t->arp_pos >= 0x8000000u) {                 /* a new phrase waits for its first step (0xFFFFFFF less the samples still to wait) */
        uint32_t w = 0xFFFFFFFu - t->arp_pos;
        if (n < w) {
            t->arp_pos += n;
            return;
        }
        if (clock_src()) midi_arp_grid[trk_index(t)].remainder = 0;
        t->arp_pos = w ? n - w : 0u;                /* (the overshoot of a wait is carried: the next step stays on the grid) */
        arp_fire(t, period);
        if (t->arp_pos && ARP_UP(t)) {
            if (t->arp_off <= t->arp_pos) arp_off(t);
            else t->arp_off -= t->arp_pos;
        }
    } else {
        t->arp_pos += n;
        duration = period + (uint32_t)((t->arp_idx & 1u) ? -sw : sw);   /* (after an even step the wait is longer: the offbeat lands late, as the sequencer's) */
        if (t->arp_pos < duration)
            return;
        do {                                        /* the overshoot is carried, so no step is rounded up to a block; on the external clock every step due fires */
            t->arp_pos -= duration;
            arp_fire(t, period);
            duration = period + (uint32_t)((t->arp_idx & 1u) ? -sw : sw);
        } while (t->arp_pos >= duration && clock_src());
        if (t->arp_pos >= duration)                 /* (the internal clock: a faster RATE left more than a step; it starts afresh) */
            t->arp_pos = 0;
        if (ARP_UP(t)) {
            if (t->arp_off <= t->arp_pos) arp_off(t);
            else t->arp_off -= t->arp_pos;
        }
    }
    if (ARP_UP(t))
        t->arp_ph = 1;                              /* (the next step's note-on continues this phrase: voice.c) */
}

static void arp_tick(track_t *t, uint32_t n) { arp_step(t, n, n); }

/* -------------------------------------------------------- note input --- */
/* the length of step idx in samples: SWING (the track's + the global, at most 75 %) makes the even steps longer
 * and the odd ones shorter, so every odd step starts late */
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    int32_t sw = DIV_TRIPLET(t->p[P_SDIV]) ? 0 : sw_len(sw_ex(t->p[P_SSWING]) + sw_ex(song.g[G_SWING]), period);   /* the track's + the global, at most 75 % (not on a triplet) */
    return period + (uint32_t)(((idx ^ TIDE_SWING_PAR(t)) & 1u) ? -sw : sw);   /* TIDE hook (H3b): POLY swings on the full loop's parity */
}

/* NUDGE (1.2, #162): a note `off` samples from the start of the step it goes into, in 1/NUDGE_DIV of the step (period),
 * rounded (step_set_nudge keeps -8..+7): the first note of a fresh step sets it; QUANTIZE (P_SQNT) decides whether it
 * plays (not inlined: its divide stays out of the audio ISR's loops) */
static __attribute__((noinline)) int32_t rec_nudge(int32_t off, uint32_t period)
{
    return (off * NUDGE_DIV + (off < 0 ? -(int32_t)period / 2 : (int32_t)period / 2)) / (int32_t)(period ? period : 1u);
}

#if DUELL_TIDE
static uint32_t arp_dur(const track_t *t)           /* the samples of the arp step playing (arp_step's duration) */
{
    uint32_t period = div_samples((uint32_t)t->p[P_ARATE]);
    int32_t sw = arp_sw(t, period);
    return period + (uint32_t)((t->arp_idx & 1u) ? -sw : sw);
}
/* the note `stored` into step idx, `note` the key held (tie bookkeeping). The step starts afresh (its old notes go) when it has no notes, on a part that
 * keeps one note per step (MONO / LEGATO / UNISON), or (REC = REPLACE) for the first note recorded into it in this pass: a chord played together, and the
 * notes of a hold, add. An ARP SEQ part (R3) always starts afresh: its arp plays note[0] only, so a key sets the step's note whatever REC says (a step holds one
 * note there). The pass: rec_wr marks the steps written, the playhead clears the mark two steps behind it (rec_hold), so a pattern of 1 or 2
 * steps replaces a chord note by note. REC = ADD (the default; plain DUELL's rec_note) is the rule before REPLACE. A note is added unless it is there, the last of
 * 4 replaced; the accent and the loudest velocity; a new hold (one in another step ends), a chord held until its last key is up */
static void rec_put(track_t *t, uint32_t idx, uint32_t stored, uint32_t vel, int32_t off, uint32_t note)
{
    step_t *s = &t->step[idx];
    uint32_t *w = &t->rec_wr[(idx >> 5) & 1u], bit = 1u << (idx & 31u), k;
    if (s->time != ST_NOTE || !s->n || trk_vmode(t) != V_POLY || arp_mode(t) == AM_SEQ_ID || (tide_rec_replace && !(*w & bit))) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
        step_set_nudge(s, off);
    }
    *w |= bit;
    for (k = 0; k < s->n && s->note[k] != stored; k++)
        ;
    if (k == s->n) {
        if (s->n < 4u)
            s->n++;
        s->note[s->n - 1u] = (uint8_t)stored;
    }
    s->time = ST_NOTE;
    if (vel > 110)
        s->flags |= SF_ACCENT;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    t->seq_active = 1;
    if (!t->rh_n || t->rh_start != idx) {
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;
}
#endif

/* live recording: the note goes into the nearest step, as swung (the one playing, or
 * the next one when it is past the middle of the playing one). Overdub: a step that
 * holds notes gets this one added (a chord of up to 4; when full, the last note is
 * replaced); MONO / LEGATO / UNISON parts keep one note per step, as step entry does.
 * Held on (synth parts): each further step the sequencer enters while the note is
 * held becomes a TIE (rec_hold), up to the pattern length; a release before the middle
 * of the last one puts that step back (rec_release), so a short note stays one step.
 * A note recorded into another step ends the hold before (the step model ties the
 * notes of one step only). */
static void rec_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len;
#if !DUELL_TIDE
    uint32_t k;
#endif
    uint32_t period = div_samples((uint32_t)t->p[P_SDIV]), cur = step_samples(t, period, t->seq_idx);
    uint32_t next = t->seq_pos > cur / 2u, sounds;
    int32_t off = rec_nudge(t->seq_pos >= cur ? 0 : next ? -(int32_t)(cur - t->seq_pos) : (int32_t)t->seq_pos, period);
    step_t *s;
    if (next)
        idx = (idx + 1u) % len;
    s = &t->step[idx];
    /* the step's notes play later in this pass (it is the next one, not played early yet, or the one playing whose
     * nudge has not come yet): what sounds now must not play again there (rskip) */
    sounds = next ? t->nd_fired != idx + 1u : t->nd_late != ND_NONE;
#if DUELL_TIDE
    rec_put(t, idx, note, vel, off, note);
#else
    if (s->time != ST_NOTE || !s->n || trk_vmode(t) != V_POLY) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
        step_set_nudge(s, off);
    }
    for (k = 0; k < s->n && s->note[k] != note; k++)
        ;
    if (k == s->n) {
        if (s->n < 4u)
            s->n++;
        s->note[s->n - 1u] = (uint8_t)note;
    }
    s->time = ST_NOTE;
    if (vel > 110)
        s->flags |= SF_ACCENT;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    t->seq_active = 1;
#endif
    if (sounds) {                                   /* it sounds now: the step must not trigger it again */
        if (t->rskip_idx != idx)
            t->rskip_n = 0;
        t->rskip_idx = (uint8_t)idx;
        if (t->rskip_n < 4u)
            t->rskip[t->rskip_n++] = (uint8_t)note;
    }
#if !DUELL_TIDE
    if (!t->rh_n || t->rh_start != idx) {           /* a new hold (one in another step ends) */
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;      /* a chord: held until its last key is up */
#endif
}

/* the sequencer enters step idx (before playing it): a recorded note still held ties into it */
static void rec_hold(track_t *t, uint32_t idx, uint32_t len)
{
    step_t *s;
    uint32_t k;
#if DUELL_TIDE
    k = (idx + 2u * len - 2u) % len;                /* REPLACE's pass: the playhead enters step idx, the mark of the step two behind it goes (rec_put) */
    t->rec_wr[(k >> 5) & 1u] &= ~(1u << (k & 31u));
#endif
    if (!t->rh_n)
        return;
    if (!((song.rec >> trk_index(t)) & 1u) || t->rh_ties + 1u >= len) {
        t->rh_n = 0;                                /* disarmed, or the whole pattern is this note */
        return;
    }
    if (idx == t->rh_start)
        return;                                     /* (recorded ahead into the step now starting) */
    s = &t->step[idx];
    t->rh_bak = *s;
    t->rh_last = (uint8_t)idx;
    t->rh_ties++;
    for (k = 0; k < 4u; k++)
        s->note[k] = 0;
    s->n = 0;
    s->time = ST_TIE;
    s->flags = 0;
    s->vel = 0;
}

/* a key of a recorded note is up: the hold ends with the last one */
static void rec_release(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    for (i = 0; i < t->rh_n; i++)
        if (t->rh_note[i] != note)
            t->rh_note[k++] = t->rh_note[i];
    if (k == t->rh_n || (t->rh_n = (uint8_t)k))
        return;                                     /* not one of them, or others still held */
#if DUELL_TIDE
    if (arp_mode(t) == AM_SEQ_ID) {                 /* an ARP SEQ part's hold runs on the arp's steps (rec_arp_note) */
        if (t->rh_ties && t->arp_pos < 0x8000000u && TIDE_ARP_IDX(t, t->arp_idx, t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u) == t->rh_last &&
            t->arp_pos < arp_dur(t) / 2u)
            t->step[t->rh_last] = t->rh_bak;
        return;
    }
#endif
    if (t->rh_ties && t->seq_idx == t->rh_last &&
        t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u)
        t->step[t->rh_last] = t->rh_bak;            /* released early in it: not held into this step */
}

#if DUELL_TIDE
/* ARP SEQ, live recording (audit liverec L2, R2; owner 2026-11-08): the pattern keeps playing from its stand-in, a key sounds directly (no transpose, the arp
 * does not hear it) and goes into the arp's step nearest to it, as a step offset from the stand-in's root, so the pattern plays it back as played.
 * The step is the one the arp plays (tide_arp_idx), the next one when the key came past the middle of the playing one. REC = ADD / REPLACE as rec_note;
 * a key always sets the step's note, in both modes and in POLY too (ARP SEQ plays note[0] only: a step holds one note, rec_put) */
static int rec_arp_keys(const track_t *t)
{
    return rec_on(t) && !cin_left && trk_index(t) < NSHOWN && arp_mode(t) == AM_SEQ_ID;
}
static uint32_t rec_key_set(const track_t *t, uint32_t note, uint32_t on)   /* the key's mark set to `on` (a key rec_arp_keys sent to the voices); what it was */
{
    uint32_t pi = trk_index(t), *w, bit = 1u << (note & 31u), was;
    if (pi >= NSHOWN)
        return 0;
    w = &rec_key[pi][(note >> 5) & 3u];
    was = *w & bit;
    *w = on ? *w | bit : *w & ~bit;
    return was;
}
static __attribute__((noinline)) void rec_arp_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, list[64], cnt = 0, root = 127, i, k = t->arp_idx, idx, oc;
    if (t->arp_pos >= 0x8000000u || t->arp_pos > arp_dur(t) / 2u)
        k++;                                        /* the next arp step (or the first, which has not started yet) */
    idx = TIDE_ARP_IDX(t, k, len);
    if (arp_list(t, list, &cnt) && cnt) {
        for (i = 0; i < cnt; i++)
            root = list[i] < root ? list[i] : root; /* the lowest note of the chord the stand-in plays (arp_seq_notes moves every note by the same offset) */
    } else
        root = 60;
    oc = (k / len) % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);   /* (arp_seq_notes raises each pass through the pattern by an octave) */
    rec_put(t, idx, (uint32_t)clamp((int32_t)note - (int32_t)root + 60 - 12 * (int32_t)oc, 0, 127), vel, 0, note);
    if (k != t->arp_idx)
        t->arp_skip = k + 1u;                       /* it sounds now: the arp does not play that step again in this pass (arp_rec_fire) */
}
/* an ARP SEQ part's arp step starts: the pass's marks, the hold's ties, and (returns 1) the step a key was put into already, left out */
static __attribute__((noinline)) int arp_rec_fire(track_t *t)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = TIDE_ARP_IDX(t, t->arp_idx, len), hit = t->arp_skip == t->arp_idx + 1u;
    if (t->arp_skip && (t->arp_skip <= t->arp_idx + 1u || !rec_on(t)))   /* (the step it was put into has come, or is past, or REC is off) */
        t->arp_skip = 0;
    rec_hold(t, idx, tide_play_len(t));             /* (seq_tick leaves an ARP SEQ part's hold to the arp; the loop it walks is POLY's, when a stretch shortens it) */
    if (!hit)
        return 0;
    if (ARP_UP(t))
        arp_off(t);
    return 1;
}
#endif

/* a note into armed track t in the count-in's last eighth: kept for step 1 (cin_flush); once each, 4 per track */
static __attribute__((noinline)) void cin_keep(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i = trk_index(t), k;
    if (cin_left != 1u || cin_pos < click_beat_len() / 2u || !((song.rec >> i) & 1u) || arp_mode(t))
        return;
    for (k = 0; k < cin_n[i] && cin_note[i][k] != note; k++)
        ;
    if (k == cin_n[i]) {
        if (k == 4u)
            return;
        cin_n[i]++;
    }
    cin_note[i][k] = (uint8_t)note;
    cin_vel[i][k] = (uint8_t)vel;
    cin_up[i] &= (uint8_t)~(1u << k);
}

static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    last_note = (uint8_t)note;
#if DUELL_TIDE
    tide_ghost_real(t);                             /* b7 (MULTIS): a real note ends the AUTO TIDE ghost chord on this part before it starts */
    if (rec_arp_keys(t)) {                          /* R2: the key sounds as played and goes into the pattern the part plays (the stand-in runs on) */
        rec_key_set(t, note, 1);
        trk_note_on(t, note, vel);
        rec_arp_note(t, note, vel);
        return;
    }
    rec_key_set(t, note, 0);
#endif
    arp_key_vel[trk_index(t)] = (uint8_t)vel;       /* (the arp plays the newest key's velocity) */
    if (cin_left)
        cin_keep(t, note, vel);
    if (rec_on(t) && !arp_mode(t))                 /* (ARP on: arp_tick records its notes) */
        rec_note(t, note, vel);
    if (arp_mode(t)) {                              /* (0.2.14: the arp as it plays now: MODULES' ARP OFF, SEQ OFF: the keys play directly) */
        arp_add(t, note);
        if (atgt_bits(t))                           /* ARP TARGET: the key sounds, the arp moves its units */
            trk_note_on(t, note, vel);
    } else {
        if (arp_hold(t) == 2)
            arp_add(t, note);                       /* HOLD ALL: latch the chord (the next one replaces it) */
        trk_note_on(t, note, vel);
    }
}

/* LAYER (core.h mix_xf, mix_split): XFADE's gain of a note of velocity v for part 1 (part 0) or part 2 (part 1), Q15, 0 = none
 * (XFADE 0): equal power cos / sin of v / 127 over a quarter turn, blended to 1 by XFADE (soft favours part 1, hard part 2) */
static uint32_t xf_gain(uint32_t part, uint32_t v)
{
    int32_t x = mix_xf, e, g;
    if (!x)
        return 0;
    v = v > 127u ? 127u : v;
    e = sine_i(v * 8454143u + (part ? 0u : 0x40000000u));     /* 2^30 / 127 per step: a quarter period at 127 */
    g = 32767 + (e - 32767) * x / 100;
    return (uint32_t)(g < 1 ? 1 : g > 32767 ? 32767 : g);
}
/* the key as played: the octave buttons, no TRN, no scale (what the SPLIT note is compared with, whatever each part's TRN) */
static uint32_t kb_raw(uint32_t k)
{
    return (uint32_t)clamp(53 + (int32_t)k + 12 * song.octave, 0, 127);
}
/* SPLIT as a stored code (0 OFF, else the note + 1): MIDDLE (1.0.0-b7) is the middle key's note as played now, the 14th of the 27 keys (k 13: F#4 = 66 at octave 0),
 * so OCT- / OCT+ move it with the keys and MIDI notes split where that key sounds */
#define SPLIT_KEY 13u
static __attribute__((noinline)) uint32_t split_note(void)   /* (one copy in the flash: a note's or a key's rate, tools/ram_allow.py) */
{
    uint32_t s = mix_split;
    return s == SPLIT_MIDDLE ? kb_raw(SPLIT_KEY) + 1u : s > 128u ? 0u : s;
}
/* LAYER with SPLIT set: which parts play note n (bit 0 part 1 below the split, bit 1 part 2 at or above it); 3 both */
static uint32_t lay_route(uint32_t n)
{
    uint32_t s = split_note();
    return !s ? 3u : n + 1u < s ? 1u : 2u;
}
/* the part key k plays when it goes down (seq.c keyboard_block): LAYER part 1 (key_on gives part 2 its own copy); TIDE_PLAYSEL: STEREO / DUEL with the part selection on BOTH
 * and SPLIT set, the split's side of the key (the played key against the split, as LAYER); else the selected part */
static uint32_t kb_part(uint32_t k)
{
    if (duell_mode == DM_LAYER)
        return 0u;
#if TIDE_PLAYSEL
    if (song.both && split_note())
        return lay_route(kb_raw(k)) == 1u ? 0u : 1u;
#endif
    return song.sel;
}
/* input_on of a LAYER note: the part's XFADE share rides with the voices it starts (an arp's later notes keep it) */
static void lay_input(track_t *t, uint32_t note, uint32_t vel)
{
    t->lay_xg = (uint16_t)xf_gain(t != trk, vel);
    input_on(t, note, vel);
    if (!arp_mode(t))
        t->lay_xg = 0;
}

static int midi_note_held(const track_t *t, uint32_t note);
static int midi_local_held(const track_t *t, uint32_t note);
static void input_off(track_t *t, uint32_t note)
{
    if (midi_note_held(t, note))
        return;
    if (cin_left) {                                 /* a kept note let go before step 1: recorded, then let go */
        uint32_t i = trk_index(t), k;
        for (k = 0; k < cin_n[i]; k++)
            if (cin_note[i][k] == note)
                cin_up[i] |= (uint8_t)(1u << k);
    }
    rec_release(t, note);
#if DUELL_TIDE
    if (rec_key_set(t, note, 0)) {                  /* (it never reached the arp: no arp_remove) */
        trk_note_off(t, note);
        return;
    }
#endif
    arp_remove(t, note);                            /* both: the note may have started in the */
    if (hold_all_keys(t) && in_held(t, note))
        return;                                     /* HOLD ALL: the latched chord keeps sounding */
    if (arp_hold(t) && atgt_bits(t))                /* ARP TARGET, HOLD: the root voice stays with the latched key */
        t->atg_lat = 1;                             /* (atg_release ends it) */
    else
        trk_note_off(t, note);                      /* other mode (ARP switched while held) */
}

/* key k's tone i of its chord on track t (key_on, and STRUM's later tones: strum_block): played unless another key holds that note already (then it
 * sounds once; nor sent to MIDI OUT twice); kb_chn[k] counts it as started either way (its release ends exactly the started ones) */
static void key_tone(uint32_t k, track_t *t, uint32_t i, uint32_t vel)
{
    uint32_t x = kb_chord[k][i], mc = trk_midi_ch(trk_index(t));
#if DUELL_TIDE
    if (midi_local_held(t, x) || midi_note_held(t, x))   /* review FR8 (A4): a MIDI note holds it too (as midi_play): not counted twice by the arp */
#else
    if (midi_local_held(t, x))
#endif
        return;
    if (duell_mode == DM_LAYER && t == trk)
        lay_input(t, x, vel);
    else
        input_on(t, x, vel);
    #if DUELL_TIDE
    if (!(arp_mode(t) && !atgt_bits(t)) || rec_arp_keys(t)) {   /* (ARP on: the arp's notes are what sounds and goes out: arp_note_on; but an armed ARP SEQ part's key went to the voices by input_on, so it goes out here, and kb_mo ends it) */
#else
    if (!(arp_mode(t) && !atgt_bits(t))) {          /* (ARP on: the arp's notes are what sounds and goes out: arp_note_on) */
#endif
        midi_out_event(0x09u | (0x90u | mc) << 8 | x << 16 | vel << 24);
        kb_mo |= 1u << k;
    }
}
/* .. LAYER part 2's copy of it */
static void key_tone2(uint32_t k, uint32_t i, uint32_t vel)
{
    if (!midi_local_held(&trk[1], kb_chord2[k][i]))
        lay_input(&trk[1], kb_chord2[k][i], vel);
}

/* key k plays kb_note[k] on track t: its chord (chord.c; the note alone with CHRD OFF). A note another key
 * holds already sounds: it is not started again (nor sent to MIDI OUT); the key keeps its notes. STRUM (item 144, chord.c strum_blocks): only the lowest
 * tone now, strum_block the others STRUM apart (kb_sw) */
static void key_on(uint32_t k, track_t *t)
{
    uint32_t n = chord_build(t, kb_note[k], kb_chord[k]), i, m;
    uint32_t lay = duell_mode == DM_LAYER && t == trk, rt = lay ? lay_route(kb_raw(k)) : 3u;   /* (SPLIT: who plays it; the played key, before any TRN) */
    uint32_t vel = KEY_VELU(k), sb;
#if DUELL_VELO_CAPTURE
    velocap_note(k, KEY_VELU(k));                   /* developer capture build only: the velocity this note-on plays, with the key's range */
#endif
    kb_chn[k] = kb_chn2[k] = 0;
    kb_sw[k] = kb_sw2[k] = 0;
    kb_svel[k] = (uint8_t)vel;
    kb_mo &= ~(1u << k);
    if (!(rt & 1u))
        n = 0;
    sb = n > 1u ? strum_blocks(t) : 0u;
    m = sb ? 1u : n;                                /* (STRUM: the lowest tone now) */
    in_key = (uint8_t)(k + 1u);                     /* (the arp keeps the key: arp_add) */
    in_root = kb_note[k];
    for (i = 0; i < m; i++)
        key_tone(k, t, i, vel);
    kb_ctot[k] = (uint8_t)n;
    kb_sw[k] = (uint8_t)(m < n ? sb : 0u);
    if (lay && (rt & 2u)) {                         /* LAYER: part 2 gets the held key too, its own chord and arp */
        uint32_t r2 = kb_map(&trk[1], k), n2, sb2;  /* (part 2's own TRN, on the key: not part 1's note) */
        in_root = (uint8_t)r2;
        n2 = chord_build(&trk[1], r2, kb_chord2[k]);
        sb2 = n2 > 1u ? strum_blocks(&trk[1]) : 0u;
        m = sb2 ? 1u : n2;
        for (i = 0; i < m; i++)
            key_tone2(k, i, vel);
        kb_chn2[k] = (uint8_t)m;
        kb_ctot2[k] = (uint8_t)n2;
        kb_sw2[k] = (uint8_t)(m < n2 ? sb2 : 0u);
    }
    in_key = 0;
    kb_chn[k] = (uint8_t)(sb && n > 1u ? 1u : n);
    last_note = kb_note[k];                         /* (step entry: the key's note) */
}

/* key k is up: the notes it started end, but those another key still holds */
static void key_off(uint32_t k, track_t *t)
{
    uint32_t n = kb_chn[k], i, mc = trk_midi_ch(trk_index(t));
    kb_chn[k] = 0;
    kb_sw[k] = kb_sw2[k] = 0;                       /* (STRUM: the tones not started yet never start) */
    for (i = 0; i < n; i++) {
        uint32_t x = kb_chord[k][i];
        if (midi_local_held(t, x))
            continue;
        input_off(t, x);
        if ((kb_mo >> k) & 1u)
            midi_out_event(0x08u | (0x80u | mc) << 8 | x << 16);
    }
    kb_mo &= ~(1u << k);
    if (kb_chn2[k]) {                               /* (LAYER: part 2's copy) */
        uint32_t n2 = kb_chn2[k];
        kb_chn2[k] = 0;
        for (i = 0; i < n2; i++)
            if (!midi_local_held(&trk[1], kb_chord2[k][i]))
                input_off(&trk[1], kb_chord2[k][i]);
    }
}

#ifdef FM1_INPUT_LAT
/* the press latency stats (hal/fm1_input.h fm1_in_stat, console `inp`): from the first scan that saw
 * key k closed to its note-on here, and to the note's first sample leaving the DMA (kb_out_tick:
 * audio.c, the TIMER4 tick at which the block being rendered starts to play) */
static uint32_t kb_out_tick;
static __attribute__((noinline)) void kb_lat(uint32_t k)
{
    uint32_t t0 = fm1_in.note_t0[k], d = fm1_ticks() - t0, o = kb_out_tick - t0;
    fm1_in_stat.kb_n++;
    fm1_in_stat.kb_sum += d;
    fm1_in_stat.dac_sum += o;
    if (d > fm1_in_stat.kb_max)
        fm1_in_stat.kb_max = d;
    if (o > fm1_in_stat.dac_max)
        fm1_in_stat.dac_max = o;
}
#endif

/* the quick layers' keys (ui_layer.c): kb_mask the button bits whose hold makes keys a layer's (main sets it), kb_lock a layer
 * locked open (#83, no button held), kb_layer the keys held that are a layer's (a bit per key; ISR) */
static volatile uint32_t kb_mask;
static volatile uint8_t kb_lock;
static volatile uint32_t kb_layer;

/* PERFORM (perform.c): white key k down / up, the effect of its place (F3 = 1 .. G5 = 16). noinline: out of keyboard_block's
 * loop (the audio ISR) */
static __attribute__((noinline)) void perf_key_press(uint32_t k, int down)
{
    uint32_t p = key_place(k);
    if (p < PF_KEYS)
        perf_press(p, down);
}

/* The global SCALE / ROOT / OFF-SCALE changed (ui.c scale_sync bumps scale_gen): the chord shape of every key the arp holds is built
 * again on the key's note as it maps now (held_dt: each held note above it). held[] stays as it came (the key's release matches it,
 * MIDI OUT matches kb_chord), arp_idx / arp_pos / arp_ph are not touched (the phrase goes on); nothing is retriggered */
static __attribute__((noinline)) void arp_rechord(void)
{
    uint32_t ti, i, m, j;
    for (ti = 0; ti < NTRK; ti++) {
        track_t *t = &trk[ti];
        const uint8_t *hk = held_key[ti];
        int8_t *dt = held_dt[ti];
        for (i = 0; i < t->nheld; i++) {
            uint32_t s = hk[i], n, root;
            uint8_t out[CHORD_MAX];
            int32_t r;
            uint16_t mask;
            if (!s)
                continue;
            for (m = 0; m < i && hk[m] != s; m++)
                ;
            if (m < i)
                continue;                           /* (this key's notes are done with its first one) */
            root = kb_pitch(t, (s & 31u) - 1u, (int32_t)(s >> 5) - 3);
            n = chord_make(t, root, out, &r, &mask);
            for (m = i, j = 0; m < t->nheld; m++)
                if (hk[m] == s) {
                    dt[m] = (int8_t)((int32_t)out[j < n ? j : n - 1u] - (int32_t)root);
                    j++;
                }
        }
    }
}

#if DUELL_VELO
/* the keys' aftertouch, the cheap form: the strongest held key's pressure (sense.c) as channel pressure (0xD0) to the part the keys play,
 * through mod_midi's path (t->at, the matrix source AFTERTOUCH). Nothing per voice. Sent when it moves by SC_AT_DEAD, and 0 when it falls to 0, the
 * setting goes OFF or the keys play another part (the old one's pressure goes). With the setting OFF it never calls mod_midi. */
static __attribute__((noinline)) void velo_at_block(void)
{
    static uint8_t sent, tm;                      /* the last value sent; the parts (bit per part) it went to */
#if TIDE_PLAYSEL
    uint32_t v = sense_at_value(), lay = duell_mode == DM_LAYER || (song.both && split_note());   /* (STEREO / DUEL split: the keys play both parts too) */
#else
    uint32_t v = sense_at_value(), lay = duell_mode == DM_LAYER;
#endif
    uint32_t bits = lay ? 3u : 1u << song.sel, d = v > sent ? v - sent : sent - v, i;
    if (bits != tm) {
        for (i = 0; i < NTRK; i++)
            if (sent && (tm >> i) & 1u)
                mod_midi(&trk[i], 0xD0u, 0, 0);
        tm = (uint8_t)bits;
        sent = 0;
        d = v;
    }
    if (d >= SC_AT_DEAD || (!v && sent)) {
        sent = (uint8_t)v;
        for (i = 0; i < NTRK; i++)
            if ((bits >> i) & 1u)
                mod_midi(&trk[i], 0xD0u, v, 0);
    }
}
#endif

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes, ch, k;
    uint32_t lay, mute, pk;
    static uint8_t gen_seen;
    if (gen_seen != scale_gen) {
        gen_seen = scale_gen;
        arp_rechord();
    }
#if DUELL_VELO
    cur = sense_gate(cur, kb_prev);              /* a key whose velocity is not in yet waits for it (sense.c); at most SC_GATE_MAX_US */
    velo_at_block();
#endif
    ch = cur ^ kb_prev;                          /* keys also sound while entering steps */
    if (!ch)
        return;
    if (kb_boot_hold) {                           /* #137: under the splash; their release is silent too */
        for (k = 0; k < 27u; k++)
            if ((ch & cur) >> k & 1u)
                kb_note[k] = KB_SILENT;
        kb_prev = cur;
        return;
    }
    lay = (fm1_in.buttons & kb_mask) || kb_lock;  /* (#83: kb_lock, a layer locked open with no button held) */
    pk = perf_keys ? 1u : (fm1_in.buttons & perf_fxb) ? 2u : 0u;   /* PERFORM's white keys (2: FX held, PERFORM opens) */
    mute = song.grid == 2u ? ~0u : 0u;           /* the keys that never sound here */
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                    /* the selected track; the key-up goes to the same one */
            kb_trk[k] = (uint8_t)kb_part(k);      /* (LAYER: part 1; key_on gives part 2 its own copy. TIDE_PLAYSEL: STEREO / DUEL split with BOTH) */
            if (kb_asleep) {                      /* (SCREEN OFF: the wake press is swallowed) */
                kb_note[k] = KB_SILENT;
                continue;
            }
            if (lay) {                            /* a layer's button held (or locked): the key is the layer's */
                kb_note[k] = KB_SILENT;           /* (ui_layer.c) */
                kb_layer |= 1u << k;
                continue;
            }
            if (pk && !key_black(k)) {            /* PERFORM: a white key holds its effect, no note, no MIDI, no record */
                kb_note[k] = KB_SILENT;
                kb_perf |= 1u << k;
                if (pk == 2u)
                    perf_fx_hit = 1;
                perf_key_press(k, 1);
                continue;
            }
            if ((mute >> k) & 1u)                 /* NAME (ui_name.c): every key types, none sounds */
                kb_note[k] = KB_SILENT;
            else
                kb_note[k] = (uint8_t)kb_map(&trk[kb_trk[k]], k);
            if (kb_note[k] == KB_SILENT)
                continue;
            key_on(k, &trk[kb_trk[k]]);
#ifdef FM1_INPUT_LAT
            kb_lat(k);
#endif
        } else {
            if ((kb_layer >> k) & 1u) {
                kb_layer &= ~(1u << k);
                continue;
            }
            if ((kb_perf >> k) & 1u) {            /* (a PERFORM key: its effect ends, whatever is on now) */
                kb_perf &= ~(1u << k);
                perf_key_press(k, 0);
                continue;
            }
            if (kb_note[k] == KB_SILENT)
                continue;
            key_off(k, &trk[kb_trk[k] % NTRK]);
        }
    }
    kb_prev = cur;
}

/* -------------------------------------------------------- sequencer --- */
static void seq_start(void)
{
    uint32_t i;
    midi_grid_reset();
    TIDE_SEQ_START();                               /* TIDE hook (H3) */
    motion_begin();
    for (i = 0; i < NTRK; i++) {                   /* every track from its step 0, together */
        track_t *t = &trk[i];
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFF;                   /* step 0 fires on the first block */
        t->rskip_n = 0;
        t->rh_n = 0;
#if DUELL_TIDE
        t->rec_wr[0] = t->rec_wr[1] = t->arp_skip = 0;
        if (i < NSHOWN)
            memset(rec_key[i], 0, sizeof rec_key[i]);   /* (a key still held: its key-up takes the normal way and ends the voice) */
#endif
        t->rat_left = 0;
        t->nd_late = t->nd_early = ND_NONE;
        t->nd_fired = 0;
        if (t->p[P_LTRIG] && t->p[P_LSYNC])        /* LFO (1.2: "LFO 2") FREE + SYNC: in time with the bars from PLAY */
            t->lfo_ph = (uint32_t)t->p[P_LPHASE] << 25;
        if (t->p[P_L2TRIG] && t->p[P_L2SYNC])      /* DUELL: LFO 2 the same */
            t->lfo2.ph = (uint32_t)t->p[P_L2PHASE] << 25;
    }
    song.tick = 0;
    beat_pos = 0; beat_n = 0;                      /* the ARP LED's beat from the top too */
    clk_pos = CLK_START;                           /* the metronome's first beat with step 0 */
    song.playing = 1;
    duck_start();                                  /* fx.c: the DUCK's PUMP from the top as well */
    dlyev_start();                                 /* fx.c: the delay's event clock (RAND / OCT+) from the top as well */
    perf_start();                                  /* perform.c: its 1/16 grid and PUMP's beat too */
}

static void seq_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        arp_note_off(t, t->seq_notes[i]);
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;                             /* live MONO / LEG keys must not glide after it */
}

static void seq_stop(void)
{
    uint32_t i;
    song.playing = 0;
    cin_left = 0;                                  /* (a count-in: cancelled, nothing recorded) */
    cin_flush = 0;
    memset(cin_n, 0, sizeof cin_n);
    if (clock_src())                               /* external clock: the arp stops with the transport, */
        for (i = 0; i < NPART; i++)                /* its sounding note too (HOLD keeps the latched chord) */
            if (ARP_UP(&trk[i]))
                arp_off(&trk[i]);
    for (i = 0; i < NTRK; i++) {
        seq_release(&trk[i]);
        trk[i].rh_n = 0;                           /* a recorded note held over the stop: as far as it got */
        trk[i].rat_left = 0;
        trk[i].nd_late = trk[i].nd_early = ND_NONE;   /* (a nudged step not played yet: not after the stop) */
        trk[i].nd_fired = 0;
    }
    TIDE_GHOST_STOP();                             /* b7 (MULTIS): the AUTO TIDE ghost chords end with the transport */
    motion_end();
}

/* play one step: TIE extends, REST releases, NOTE (re)triggers its notes; a SLIDE on the previous step makes this one legato with a
 * glide (acid style). skip: bit k = note k already sounds from live
 * recording (not triggered, not released here); SEQ_REP: a RATCH repeat (seq_ratchet).
 * RATCH x2..x4 (core.h step_ratchet): a NOTE step plays all of it again in equal parts of its swung length, each
 * part retriggered with GATE of the part; the chance is rolled once for the whole step (a failed roll: no part),
 * a slide into it glides into the first part only, and it never slides or ties out (its last part ends at its
 * gate). x1, what every older pattern holds, is the step exactly as before. */
#define SEQ_REP (1u << 16)
#define SEQ_ROLLED (1u << 17)   /* seq_tick rolled the chance already (before the step's parameter locks): it passed */
#define SEQ_MISS (1u << 18)     /* .. it failed: nothing plays */
static __attribute__((noinline)) void seq_step(track_t *t, const step_t *s, uint32_t idx, uint32_t period, uint32_t skip)
{
    uint32_t i, j, gate = period * (uint32_t)t->p[P_SGATE] / 128u;
    uint32_t vel = (s->flags & SF_ACCENT) ? 127u : (s->vel ? s->vel : 96u);
    uint32_t slide_in = t->seq_hold && t->seq_n && !(skip & SEQ_REP);
    uint32_t len = TIDE_SLEN(t) ? TIDE_SLEN(t) : 1u;   /* TIDE hook (H3b) */
    uint32_t next_tie = TIDE_PLAY_STEP(t, (idx + 1u) % len)->time == ST_TIE;   /* TIDE hook (H2) */
    uint32_t hits = step_ratchet(s);
    uint8_t nn[4], vv[4];                           /* the notes it plays */
    uint32_t m = 0, sk = 0;
    if (!(skip & SEQ_REP)) {
        t->rat_left = 0;
        if ((skip & SEQ_MISS) ||
            (!(skip & SEQ_ROLLED) && step_chance(s) < 100u && rng() % 100u >= step_chance(s))) {
            seq_release(t);
            return;
        }
    }
    if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = gate + period / 2u;
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie;   /* chains hold at any GATE / swing */
        }
        return;
    }
    if (s->time == ST_REST || !s->n) {
        seq_release(t);
        return;
    }
    if (hits > 1u) {                                /* RATCH: the gate of a part, no slide or tie out */
        gate = step_samples(t, period, idx) / hits * (uint32_t)t->p[P_SGATE] / 128u;
        if (!(skip & SEQ_REP))
            t->rat_left = (uint8_t)(hits - 1u);
    }
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t x = s->note[i];
        x = (uint32_t)clamp(note_snap(t, (int32_t)x), 0, 127);   /* the global scale at playback (the step keeps its raw note) */
        nn[m] = (uint8_t)x;
        vv[m] = (uint8_t)vel;
        sk |= ((skip >> i) & 1u) << m;
        m++;
    }
    t->slide_glide = (uint8_t)slide_in;
    if (!slide_in)
        seq_release(t);
    for (i = 0; i < m; i++)
        if (!((sk >> i) & 1u))
            arp_note_on(t, nn[i], vv[i]);
    if (slide_in)                                   /* release what is not held over */
        for (i = 0; i < t->seq_n; i++) {
            for (j = 0; j < m && nn[j] != t->seq_notes[i]; j++)
                ;
            if (j == m)
                arp_note_off(t, t->seq_notes[i]);
        }
    t->seq_n = 0;
    for (i = 0; i < m; i++)
        if (!((sk >> i) & 1u))
            t->seq_notes[t->seq_n++] = nn[i];
    t->seq_off = gate;
    t->seq_hold = hits == 1u && ((s->flags & SF_SLIDE) != 0 || next_tie);   /* next step a TIE: keep the notes to it */
}

/* MENU > CONTROL > STEP PREVIEW (1.2, Discussion #169): the UI asks for a step of a track to sound once (aud_req: step
 * << 8 | track + 1; ui_input.c cursor_move); with the transport stopped it plays here as the sequencer would play it,
 * through the track's sound: its notes or hits, accent (not its chance, ratchet repeats or locks), for a 1/16
 * at the track's GATE (seq_tick lets it go, stopped too). A SLIDE or TIE after it does not hold it. Nothing is recorded:
 * the step is only read */
static volatile uint16_t aud_req;
static __attribute__((noinline)) void seq_audition(void)
{
    uint32_t r = aud_req, i = (r & 0xFFu) - 1u, idx = r >> 8;
    track_t *t;
    uint16_t at;
    aud_req = 0;
    if (song.playing || cin_left || i >= NTRK || idx >= NSTEP)
        return;
    t = &trk[i];
    at = t->seq_idx;
    t->seq_idx = (uint16_t)idx;
    seq_step(t, &t->step[idx], idx, div_samples(2), SEQ_ROLLED);
    t->seq_idx = at;
    t->seq_hold = 0;
    t->rat_left = 0;
}

/* RATCH: the next part of the playing step once the sequencer is that far into it (seq_step played the first).
 * The step is read again: an edit to fewer parts (or another source in a song) ends the repeats */
static __attribute__((noinline)) void seq_ratchet(track_t *t, uint32_t period)
{
    const step_t *s = TIDE_PLAY_STEP(t, t->seq_idx);   /* TIDE hook (H2) */
    uint32_t hits = step_ratchet(s);
    if (t->rat_left >= hits) {
        t->rat_left = 0;
        return;
    }
    if (t->seq_pos < (hits - t->rat_left) * (step_samples(t, period, t->seq_idx) / hits))
        return;
    t->rat_left--;
    seq_step(t, s, t->seq_idx, period, SEQ_REP);
}

/* the nudge of step s in 1/NUDGE_DIV of the step (NUDGE, core.h) when it plays off its start: the track's QUANTIZE OFF,
 * a NOTE step that plays something, x1 (a ratcheted step's parts divide the step: on its start); else 0 */
static int32_t seq_nudge(const track_t *t, const step_t *s)
{
    if (t->p[P_SQNT] || s->time != ST_NOTE || !s->n || step_ratchet(s) != 1u)
        return 0;
    return step_nudge(s);
}

/* MODULES' SEQ OFF (core.h MODS_SEQ), a step's time: nothing plays; what the steps still sound ends (a TIE or SLIDE held over: let go), the
 * automation's and the locks' values go back to the sound's own (they would otherwise stay where the last step left them) */
static __attribute__((noinline)) void seq_mods_off(track_t *t)
{
    uint32_t k = trk_index(t), w, any = 0;
    t->rskip_n = 0;
    t->rat_left = 0;
    if (t->seq_n)
        seq_release(t);
    for (w = 0; k < NLIVE && w < (P_COUNT + 31u) / 32u; w++)
        any |= motion_active[k][w];
    if (any)
        motion_restore(t);
}

/* step idx plays: its chance (the one roll, as seq_step made it: a step that does not play applies no lock), the
 * automation and the locks before the notes (so a note-on reads them), the notes live recording
 * already sounds left out (rskip), then its notes. On its start, or (NUDGE) at its start + its nudge */
static __attribute__((noinline)) void seq_play(track_t *t, uint32_t idx, uint32_t period)
{
    const step_t *s = TIDE_PLAY_STEP(t, idx);   /* TIDE hook (H2) */
    const motion_store_t *m = &motion;
    uint32_t skip = SEQ_ROLLED, i, k;
    if (mods_off & MODS_SEQ) {                      /* DUELL 0.2.14 GLO > MODULES SEQ OFF: no stored step plays, neither its notes nor its */
        seq_mods_off(t);                            /* locks nor the automation (the transport, the arp and the LFO sync run on) */
        return;
    }
    if (TIDE_CHANCE(t, idx, s, step_chance(s) < 100u && rng() % 100u >= step_chance(s)))   /* TIDE hook (H3c) */
        skip = SEQ_MISS;
#if DUELL_TIDE
    if (t->p[P_AMODE] != AM_SEQ)                    /* (b7, review B2: an ARP SEQ part's locks play on the arp's steps: arp_fire) */
#endif
    motion_step(t, idx, m, skip == SEQ_ROLLED);
    if (t->p[P_AMODE] == AM_SEQ) {                  /* DUELL 0.2.12: the steps are the SEQUENCER arp's (offsets on the held keys, */
        t->rskip_n = 0;                             /* arp_seq): PLAY does not also play them from C4 (the transport, the locks */
        return;                                     /* and the automation run on; STEP PREVIEW still sounds a step) */
    }
    if (t->rskip_n && t->rskip_idx == idx) {
        for (k = 0; k < t->rskip_n; k++) {
            for (i = 0; i < s->n; i++)
                if (s->note[i] == t->rskip[k])
                    skip |= 1u << i;
        }
        t->rskip_n = 0;
    }
    seq_step(t, s, idx, period, skip);
}

/* NUDGE (1.2, #162; QUANTIZE OFF): a step plays at its start + nudge x DIV / 16. When a step starts its own late play
 * (nd_late) and the next step's early one (nd_early: that much before the end of this one) are latched; an early
 * play marks the step (nd_fired), whose start then plays nothing. A late play falls before the next start (at most
 * 7/16 of the step, the shortest swung step is 0.6 of it); an early one never before the late one of the step before.
 * The pattern loops, so the first step's early play may fall at the end of the last one. QUANTIZE ON (the default), or every nudge 0 (every older pattern): each step plays on its start, as before.
 * seq_nd_due: before step seq_idx's end is checked, the latched plays due by now (seq_pos): this step's late one, then
 * the next step's early one (not inlined: off the audio ISR's loop, and only called with one latched) */
static __attribute__((noinline)) void seq_nd_due(track_t *t, uint32_t period, uint32_t len, uint32_t cur_len, uint32_t n)
{
    if (t->nd_late != ND_NONE && (t->seq_pos >= t->nd_late - 1u || t->seq_pos >= cur_len)) {
        t->nd_late = ND_NONE;                       /* (also when a shorter DIV / LEN left it past the end) */
        seq_play(t, t->seq_idx, period);
    }
    if (t->nd_early != ND_NONE && t->seq_pos >= t->nd_early - 1u && t->seq_pos != 0x7FFFFFFFu + n) {
        uint32_t nx = (t->seq_idx + 1u) % len;
        t->nd_early = ND_NONE;
        t->nd_fired = (uint8_t)(nx + 1u);
        seq_play(t, nx, period);
    }
}

/* NUDGE: step seq_idx has started: it plays now (or played early: nd_fired), or its late play is latched; and the
 * next step's early play (not inlined, as seq_nd_due) */
static __attribute__((noinline)) void seq_nd_start(track_t *t, uint32_t period, uint32_t len)
{
    uint32_t nx = (t->seq_idx + 1u) % len;
    int32_t d;
    t->nd_early = ND_NONE;
    if (t->nd_fired == t->seq_idx + 1u)
        ;                                           /* (it played early) */
    else if ((d = seq_nudge(t, TIDE_PLAY_STEP(t, t->seq_idx))) > 0)   /* TIDE hook (H2) */
        t->nd_late = (uint32_t)d * period / NUDGE_DIV + 1u;
    else
        seq_play(t, t->seq_idx, period);
    t->nd_fired = 0;
    if ((d = seq_nudge(t, TIDE_PLAY_STEP(t, nx))) < 0) {   /* TIDE hook (H2) */
        uint32_t at = step_samples(t, period, t->seq_idx), back = (uint32_t)-d * period / NUDGE_DIV;
        at = (at > back ? at - back : 0u) + 1u;
        t->nd_early = at < t->nd_late ? t->nd_late : at;   /* (never before this step's own late play) */
    }
}

static void seq_tick(track_t *t, uint32_t n)
{
    uint32_t period, len;
    if (clock_src())
        n = midi_grid_advance(&midi_seq_grid[trk_index(t)], n, (uint32_t)t->p[P_SDIV]);
    if (t->seq_n && !t->seq_hold) {
        if (t->seq_off <= n)
            seq_release(t);
        else
            t->seq_off -= n;
    }
    if (!song.playing)
        return;
    period = div_samples((uint32_t)t->p[P_SDIV]);
    len = TIDE_SLEN(t);                            /* TIDE hook (H3b) */
    len = len ? len : 1u;
    t->seq_pos += n;
    for (;;) {
        uint32_t cur_len = step_samples(t, period, t->seq_idx);
        if (t->nd_late | t->nd_early)
            seq_nd_due(t, period, len, cur_len, n);
        if (t->seq_pos < cur_len && t->seq_pos != 0x7FFFFFFFu + n)
            break;
        TIDE_SEQ_LOOP(t, len);                    /* TIDE hook (H3) */
        t->seq_pos = t->seq_pos >= 0x7FFFFFFFu ? 0u : t->seq_pos - cur_len;   /* (the pattern loops: % len below) */
        t->seq_idx = (uint16_t)TIDE_SEQ_NEXT(t, len);   /* TIDE hook (H3b) */
        TIDE_SEQ_RELEN(t, len);
#if DUELL_TIDE
        if (arp_mode(t) != AM_SEQ_ID) {             /* (an ARP SEQ part's holds and marks run on its arp steps: arp_rec_fire) */
            rec_hold(t, t->seq_idx, len);
        }
#else
        rec_hold(t, t->seq_idx, len);
#endif
        if (t->p[P_SQNT] && !t->nd_fired)
            seq_play(t, t->seq_idx, period);        /* QUANTIZE ON: every step on its start, as before 1.2 */
        else
            seq_nd_start(t, period, len);
    }
    if (t->rat_left) {
        if (mods_off & MODS_SEQ)
            t->rat_left = 0;                        /* (0.2.14: SEQ OFF in the middle of a ratchet: no more repeats) */
        else
            seq_ratchet(t, period);
    }
}

/* MIDI in: the track a channel plays (0..15): G_ROUTE CH1-4 (0) channels 1..4 their parts (5..16 never get
 * here: midi_event drops them), SEL (1) every channel the selected track; CH5-8 .. CH13-16 (2..4) as CH1-4 on
 * the parts' channels (midi_pch) */
static uint16_t midi_route = 32;           /* ROUT (bit 0, SEL) and the parts' channels events_block last saw (midi_route_key; 32: PARTS, 1, 2) */
static uint32_t midi_route_key(void) { return (song.g[G_ROUTE] == 1) | (uint32_t)midi_pch[0] << 1 | (uint32_t)midi_pch[1] << 5; }
static int midi_in_block(uint32_t ch)      /* ch is one of the parts' channels (not with SEL) */
{
    return song.g[G_ROUTE] != 1 && midi_part_of(ch) >= 0;
}
/* (DUELL step 5, LAYER: every channel heard enters at part 1; midi_play / midi_release give part 2 its own copy; midi_event drops part 2's own channel) */
static track_t *midi_track(uint32_t ch)
{
    if (duell_mode == DM_LAYER)
        return trk;
    return midi_in_block(ch) ? &trk[midi_part_of(ch)] : TSEL;
}

#include "midi_control.c"
#include "midi_clock.c"

/* DUELL step 5 (SPEC 5b.1): the MODE asked for (core.h duell_mode_req: the UI, the settings, a learned MODE CC: MODE has no shipped number) becomes the mode the
 * notes are routed by, at a block start (events_block) or at once (a learned MODE CC, before the next MIDI event). Every note let go
 * on every part, so none can hang across the change of routing (a LAYER note-on whose note-off would reach part 1 only, a
 * part 2 note whose channel LAYER ignores): the keys held stay silent until pressed again, MIDI's held and pedalled notes,
 * chords and bends are forgotten, the arps' and the steps' notes end, a latched chord is dropped. The mix follows with
 * its dip (fx.c dm_step), which stops what still rings at its silent point */
/* the mode the audio plays: the PLAY MODE row's (duell_mode_req), but TIDE_PLAYSEL with one part selected plays LAYER as STEREO (the part alone on every key; owner
 * 2026-11-10: ALGORITHM's PART 1 / PART 2 never play both parts) */
#if TIDE_PLAYSEL
#define MODE_WANT() (duell_mode_req == DM_LAYER && !song.both ? (uint32_t)DM_STEREO : (uint32_t)duell_mode_req)
#else
#define MODE_WANT() ((uint32_t)duell_mode_req)
#endif
static __attribute__((noinline)) void mode_apply(void)
{
    uint32_t i, k, m = MODE_WANT();
    m = m < DM_N ? m : DM_SPLIT;
    if (m == duell_mode)
        return;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        midi_forget_track(i);
        trk_all_off_1(t);
        t->nheld = t->arp_phys = t->rh_n = 0;
        arp_forget(t);
        t->seq_n = t->seq_hold = t->slide_glide = 0;
    }
    for (k = 0; k < 27u; k++)
        if (((kb_prev >> k) & 1u) && !((kb_layer >> k) & 1u)) {
            kb_note[k] = KB_SILENT;               /* (its release: nothing) */
            kb_chn[k] = 0;
        }
    memset(kb_chn2, 0, sizeof kb_chn2);
    memset(kb_sw, 0, sizeof kb_sw);               /* (item 144: no STRUM tone waits across the change) */
    memset(kb_sw2, 0, sizeof kb_sw2);
    trk[0].lay_xg = trk[1].lay_xg = 0;             /* (XFADE is LAYER's) */
    duell_mode = (uint8_t)m;
}

/* the count-in: its first beat now (accented) */
static __attribute__((noinline)) void cin_begin(void)
{
    cin_total = cin_left = (uint8_t)(4u * cin_bars);
    cin_pos = 0;
    memset(cin_n, 0, sizeof cin_n);
    click_req = 2;
}
/* n samples on: a beat ends; the last one starts the sequencer (in this block, before seq_tick: step 0 with it) */
static __attribute__((noinline)) void cin_block(uint32_t n)
{
    uint32_t b = click_beat_len();
    cin_pos += n;
    if (cin_pos < b)
        return;
    cin_pos -= b;
    cin_pos = cin_pos < b ? cin_pos : 0u;
    if (--cin_left) {
        click_req = (uint8_t)((cin_total - cin_left) % 4u ? 1u : 2u);
        return;
    }
    seq_start();
    cin_flush = 1;
}
/* the count-in's kept notes onto step 1, which has just started (events_block, after seq_tick): as if played now */
static __attribute__((noinline)) void cin_record(void)
{
    uint32_t i, k;
    cin_flush = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        for (k = 0; k < cin_n[i]; k++)
            if (rec_on(t)) {
                rec_note(t, cin_note[i][k], cin_vel[i][k]);
                if ((cin_up[i] >> k) & 1u)
                    rec_release(t, cin_note[i][k]);
            }
        cin_n[i] = 0;
    }
}
/* the samples from now to the next boundary of the arp's RATE on the transport's grid (clk_pos / clk_step count its 1/16 steps; 0: PLAY is
 * stopped or waiting for its first clock, or the transport is on one). A key that lands while PLAY runs starts the arp there (audit S07);
 * arp_pos holds 0xFFFFFFF less this wait until arp_step has counted it down */
static uint32_t arp_wait(const track_t *t)
{
    uint32_t p = div_samples((uint32_t)t->p[P_ARATE]), q = div_samples(2);
    if (!song.playing || clk_pos == CLK_START || !p)
        return 0;
    p -= (clk_step * q + clk_pos) % p;
    return p == div_samples((uint32_t)t->p[P_ARATE]) ? 0u : p;
}
/* the metronome's beat: adv samples on while the transport runs (click.c sounds it in this block) */
static void click_tick(uint32_t adv)
{
    if (clock_src())
        adv = midi_grid_advance(&midi_click_grid, adv, 2u);
    uint32_t p = div_samples(2), beat = 0;
    if (clk_pos == CLK_START) {
        clk_pos = 0;
        clk_step = 0;
        beat = 2;
    } else {
        clk_pos += adv;
        while (clk_pos >= p) {                      /* (as seq_tick: the remainder kept) */
            clk_pos -= p;
            clk_step = (clk_step + 1u) & 15u;
            if (!(clk_step & 3u))
                beat = clk_step ? 1u : 2u;
        }
    }
    if (beat && click_wanted())
        click_req = (uint8_t)beat;
}
/* the beat of the bar the click is on (0..3: 0 the accented first), -1 stopped: the count-in's, else the playing
 * transport's (clk_step, internal or external clock). It moves in the block a beat clicks in (the header's metronome,
 * ui_draw.c, swings on it; CLICK OFF it moves all the same) */
static int32_t click_beat_now(void)
{
    if (cin_left)
        return (int32_t)((uint32_t)(cin_total - cin_left) % 4u);
    if (!song.playing)
        return -1;
    return clk_pos == CLK_START ? 0 : (int32_t)(clk_step >> 2);
}

/* PLAY STOP double-pressed or held (ui_input.c): STOP ALL. The transport stopped and reset, every part's arp and pattern playback off (the
 * latched / held chords forgotten), every held and releasing voice faded out over the next block (voice_kill: ~3 ms, not the envelope's
 * release), the effects' lines flushed (delay, chorus, reverb: a ~22 ms fade, the lines cleared, the sends faded back in, fx.c snd_panic), the keys' and MIDI's held-note state cleared, and MIDI out gets All Sound Off (CC120) and All Notes
 * Off (CC123) on both parts' channels. (No MIDI clock is sent by DUELL: there is no clock out to stop.) */
static __attribute__((noinline)) void stop_all(void)
{
    uint32_t i, k, ch;
    seq_stop();
    snd_panic = 1;                                    /* the effect lines flushed with a quick fade (fx.c snd_step) */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        midi_forget_track(i);
        if (t->atg_lat)
            atg_release(t);
        trk_all_off(t);
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active)
                voice_kill(&t->v[k]);
        t->nheld = 0;
        t->arp_phys = 0;
        t->arp_idx = 0;
        arp_forget(t);
        t->seq_n = t->seq_hold = t->slide_glide = t->rat_left = 0;
    }
    memset(kb_chn, 0, sizeof kb_chn);
    kb_mo = 0;
#if DUELL_TIDE
    memset(rec_key, 0, sizeof rec_key);            /* (kb_chn is zero: the key ups of keys held never reach input_off to clear their marks) */
#endif
    memset(kb_chn2, 0, sizeof kb_chn2);
    memset(kb_sw, 0, sizeof kb_sw);               /* (item 144: the STRUM tones waiting) */
    memset(kb_sw2, 0, sizeof kb_sw2);
    memset(mchord, 0, sizeof mchord);
    for (i = 0; i < NSHOWN; i++) {
        ch = trk_midi_ch(i);
        midi_out_event(0x0Bu | (0xB0u | ch) << 8 | 120u << 16);
        midi_out_event(0x0Bu | (0xB0u | ch) << 8 | 123u << 16);
    }
}

/* DUELL 0.2.11 (SPEC 6.3): the MIDI-in ring's drain, MIDI_DRAIN events a block (a flood of note events used to start and steal
 * every voice in one block: +25 % on that half, PHYS 121 %). Real-time bytes are never held back, an All Notes Off / All Sound
 * Off never waits behind the limit (events_block) */
#ifndef MIDI_DRAIN
#define MIDI_DRAIN 16u                                 /* (a test may set another) */
#endif
#define MIDI_TAKEN 0u                                  /* a real-time byte taken ahead of its turn (no USB-MIDI packet is 0) */
static int midi_all_off(uint32_t pkt)                 /* CC 123 All Notes Off / CC 120 All Sound Off */
{
    uint32_t d1 = (pkt >> 16) & 0x7Fu;
    return ((pkt >> 8) & 0xF0u) == 0xB0u && (d1 == 123u || d1 == 120u);
}
static void midi_in_take(uint32_t at)                 /* one event of the ring (at: its slot) */
{
    uint32_t pkt = midi_in_q[at], status = (pkt >> 8) & 0xFFu;
    uint32_t source = midi_in_source[at] ? midi_in_source[at] : 1u, ms = midi_in_ms[at];
    if (pkt == MIDI_TAKEN)
        return;
    if (status >= 0xF8u) {
        if (song.g[G_CLOCK] == CLK_AUTO)
            clock_auto(source, status, ms);         /* (DUELL 0.2.14: AUTO, either port: midi_clock.c) */
        else if ((uint32_t)song.g[G_CLOCK] == source) {
            if (status == 0xF8u)
                midi_clock_pulse(ms);
            else
                midi_clock_transport(status, ms);
        }
    } else
        midi_event(status & 0xF0u, status & 0x0Fu, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
}

/* STRUM (item 144, chord.c): once a block, before the keys and MIDI of the block (so a tone due now starts before a release of this block ends it: never a
 * stuck note either way). Each chord with tones waiting counts down its blocks; at 0 its next tone starts (key_tone / key_tone2 / mchord_tone: through input_on,
 * so the voices' start limit, voice.c vs_*, applies as to any note) and the count starts again from the part's STRUM as it is now (turned to OFF: the rest at once).
 * The arp switched on meanwhile: the rest go to it as held notes, as a key's would. Not inlined: out of the audio ISR's loops */
static __attribute__((noinline)) void strum_block(void)
{
    uint32_t k, f;
    for (k = 0; k < 27u; k++) {
        if (kb_sw[k] && !--kb_sw[k]) {
            track_t *t = &trk[kb_trk[k] % NTRK];
            in_key = (uint8_t)(k + 1u);
            in_root = kb_note[k];
            do {
                key_tone(k, t, kb_chn[k], kb_svel[k]);
                kb_chn[k]++;
                kb_sw[k] = (uint8_t)(kb_chn[k] < kb_ctot[k] ? strum_blocks(t) : 0u);
            } while (!kb_sw[k] && kb_chn[k] < kb_ctot[k]);
            in_key = 0;
        }
        if (kb_sw2[k] && !--kb_sw2[k]) {
            in_key = (uint8_t)(k + 1u);
            in_root = (uint8_t)kb_map(&trk[1], k);
            do {
                key_tone2(k, kb_chn2[k], kb_svel[k]);
                kb_chn2[k]++;
                kb_sw2[k] = (uint8_t)(kb_chn2[k] < kb_ctot2[k] ? strum_blocks(&trk[1]) : 0u);
            } while (!kb_sw2[k] && kb_chn2[k] < kb_ctot2[k]);
            in_key = 0;
        }
    }
    for (f = 0; f < MCHORD_N; f++) {
        mchord_t *m = &mchord[f];
        if (m->id && m->wait && !--m->wait) {
            do {
                mchord_tone(m, m->n);
                m->wait = (uint8_t)(m->n < m->tot ? strum_blocks(&trk[(m->id - 1u) % NTRK]) : 0u);
            } while (!m->wait && m->n < m->tot);
        }
    }
}

/* everything that happens between two rendered blocks */
static void events_block(uint32_t n)
{
    uint32_t i, pr, pk, seq_n = n, arp_n = n, arp_gate = n;   /* arp_n: the samples the ARP's steps advance by, arp_gate: its gates */
    uint32_t clock_mode;
    if (midi_clock.mode != (uint32_t)song.g[G_CLOCK])
        clock_choice((uint32_t)song.g[G_CLOCK]);     /* (the CLK choice changed: midi_clock.c, a stop only if the active source changes) */
    clock_mode = clock_src();                        /* the ACTIVE source: 0 INT (CLK INT, or AUTO with no clock), 1 USB, 2 TRS */
    if (MODE_WANT() != duell_mode)
        mode_apply();                                 /* DUELL step 5: the MODE changed (every note let go) */
    if (cin_left && !clock_mode)
        cin_block(n);
    if (transport_req == 1u) {
        if (clock_mode) {                           /* an external clock arriving: PLAY waits for the master's bar (midi_clock_pulse), else it starts at once */
            if (midi_clock.seen && !song.playing && (int32_t)(fm1_ms - midi_clock.last_ms) < 250)
                midi_bar |= 0x80u;
            else
                midi_clock_transport(0xFAu, fm1_ms);
        } else if (cin_left)
            ;                                       /* counting in already (the UI's PLAY stops it: seq_counting) */
        else if (cin_bars && song.rec && !song.playing)
            cin_begin();                            /* recording from stop: the count-in first */
        else
            seq_start();
        transport_req = 0;
    } else if (transport_req == 2u) {
        midi_bar &= 0x7Fu;
        if (clock_mode)
            midi_clock.stopped = 1;                 /* (an external clock: the arp waits for the next Start, as for 0xFC) */
        seq_stop();
        transport_req = 0;
    } else if (transport_req == 4u) {                 /* PLAY STOP twice or held: STOP ALL */
        midi_bar &= 0x7Fu;
        stop_all();
        transport_req = 0;
    } else if (transport_req == 3u) {                 /* GLO + PLAY: from the top without stopping (INT clock) */
        if (!clock_mode) {
            seq_stop();
            seq_start();
        }
        transport_req = 0;
    }
    if (midi_route != (uint16_t)midi_route_key()) {   /* ROUT or a part's channel changed: the parts' channels let go of the others */
        midi_route = (uint16_t)midi_route_key();
        if (!(midi_route & 1u))
            midi_route_block();
    }
#if DUELL_TIDE
    if (song.sel != midi_sel_was)
        midi_sel_change();                            /* review FR8 (A6) */
#endif
    pr = panic_req;
    pk = panic_keep;
    panic_req = 0;
    panic_keep = 0;
    if (midi_in_overflow) {                            /* a lost note-off must never leave a held note */
        mi_r = mi_w;
        memset(midi_sel_on, 0, sizeof midi_sel_on);
        memset(midi_ch, 0, sizeof midi_ch);
        memset(midi_morph_fired, 0, sizeof midi_morph_fired);   /* (DUELL 4b: the MORPH's CC triggers armed) */
        memset(midi_owners, 0, sizeof midi_owners);
        memset(mchord, 0, sizeof mchord);
        midi_hint = 0;
        for (i = 0; i < NTRK; i++) {
            trk[i].rh_n = trk[i].rskip_n = 0;
#if DUELL_TIDE
            trk[i].rec_wr[0] = trk[i].rec_wr[1] = trk[i].arp_skip = 0;
            if (i < NSHOWN)
                memset(rec_key[i], 0, sizeof rec_key[i]);
#endif
            trk[i].seq_n = trk[i].seq_hold = trk[i].slide_glide = trk[i].rat_left = 0;
        }
        pr |= (1u << NTRK) - 1u;
        pk = 0;
        RING_PUBLISH();
        midi_in_overflow = 0;
    }
    /* (LAYER: part 2 holds its own copy of the played notes and releases it by its own key / MIDI tracking: a panic of part 1,
     * e.g. an engine or preset change on part 1, must NOT end part 2's notes: the owner wants part 2 to keep playing) */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if ((pr >> i) & 1u) {
#if DUELL_TIDE
            int keep = ((pk >> i) & 1u) && (mods_keep & MODK_ARP) && arp_mode(t);
            if (!keep)                                /* review FR8 (A3): the kept arp keeps who holds which MIDI note, so their note-offs still reach arp_remove */
#endif
                midi_forget_track(i);
            strum_forget(i);                          /* (item 144: a held key's tones still waiting are dropped with the rest) */
            trk_all_off(t);
            t->m_cycle = t->m_env = t->m_env2 = 0;                            /* (item 135: the latest note's CYCLE for the per-block destinations: a new sound's first block does not read the old one's) */
#if DUELL_TIDE
            if (keep) {
#else
            if (((pk >> i) & 1u) && (mods_keep & MODK_ARP) && arp_mode(t)) {
#endif
                /* DUELL 1.1 ARP KEEPS RUNNING, a patch load: the voices end (a new sound starts clean) and the arp's own notes get their MIDI note-offs (mo_flush), but the held and latched
                 * notes (nheld, held[], arp_phys), the step counter and the phrase (arp_ph) stay: the next arp step plays the new sound */
                mo_flush(t);
                t->arp_note = t->arp_ch[0] = t->arp_ch[1] = t->arp_ch[2] = t->arp_nz = 0;
                t->atg_note = t->atg_vel = t->atg_lat = 0;
            } else {
                t->nheld = 0;
                t->arp_phys = 0;
                arp_forget(t);
            }
        }
        engine_block(t);                              /* engine switch: fade, then switch (voice.c) */
        /* HOLD ALL left (arp off): the latched chord's voices end, normal release */
        int32_t am = arp_mode(t), ah = arp_hold(t);   /* (0.2.14: as they act now: MODULES' ARP OFF / SEQ OFF switch them as the arp's own MODE does) */
        if (t->aholdp == 2 && ah != 2 && !t->armp) {
            uint32_t j;
            for (j = 0; j < t->nheld; j++)
                trk_note_off(t, t->held[j]);
            t->nheld = 0;
        }
        /* HOLD ALL, the arp switched on or off: the latched chord changes hands (the arp plays it / the voices hold it) */
        if (ah == 2 && t->aholdp == 2 && t->armp != am) {
            uint32_t j;
            if (am)
                for (j = 0; j < t->nheld; j++)
                    trk_note_off(t, t->held[j]);
            else {
                if (t->atg_lat)
                    atg_release(t);
                if (ARP_UP(t))
                    arp_silence(t);
                for (j = 0; j < t->nheld; j++)
                    trk_note_on(t, t->held[j], 100);
            }
        }
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !am && ah != 2) || (t->aholdp && !ah && !t->arp_phys)) {
            if (t->atg_lat)
                atg_release(t);                     /* ARP TARGET: the latched root voices end */
            t->nheld = 0;
            if (!am)
                t->arp_phys = 0;
            if (ARP_UP(t))
                arp_silence(t);
        }
        t->armp = (int16_t)am;
        t->aholdp = (int16_t)ah;
    }
    vs_block();                                       /* (item 144: the voices the start limit held back last block start first: voice.c) */
    strum_block();                                    /* (.. then the STRUM tones due now: chord.c) */
    keyboard_block();
    {                                                 /* USB-MIDI (and TRS) in: DUELL 0.2.11 (SPEC 6.3) at most MIDI_DRAIN events a block, */
        uint32_t w = mi_w, end = w - mi_r > MIDI_DRAIN ? mi_r + MIDI_DRAIN : w, j;   /* the rest the next block, in order */
        for (j = end; j != w; j++)                    /* an All Notes Off / All Sound Off further on: everything up to it now (never late, never */
            if (midi_all_off(midi_in_q[j % MQ]))      /* ahead of the notes before it) */
                end = j + 1u;
        while (mi_r != end)
            midi_in_take(mi_r++ % MQ);
        for (j = mi_r; j != w; j++)                   /* what waits: its real-time bytes (clock, start, stop) now, in their order; */
            if (((midi_in_q[j % MQ] >> 8) & 0xFFu) >= 0xF8u) {   /* marked done, the next block skips them */
                midi_in_take(j % MQ);
                midi_in_q[j % MQ] = MIDI_TAKEN;
            }
    }
    if (song.g[G_CLOCK] == CLK_AUTO && midi_clock.lock && (int32_t)(fm1_ms - midi_clock.last_ms) >= (int32_t)AUTO_DROP_MS)
        clock_unlock();                             /* AUTO: the locked port went silent: the internal clock goes on, nothing stops */
    clock_mode = clock_src();                       /* (AUTO: a port locked or let go in this block) */
    if (clock_mode) {
        /* The ARP's held notes follow the keys at once (above); only its STEPPING waits for the clock: the transport's pulses while it
         * plays, or the pulses of a clock that runs without a Start (not after a Stop). No pulses: the arp waits (the UI says NO CLOCK).
         * AUTO never stops on a missing clock: the steps run on (midi_clock_advance) and the internal clock takes over (above) */
        uint32_t fixed = song.g[G_CLOCK] != CLK_AUTO;
        seq_n = arp_n = 0;
        if (song.playing) {
            uint32_t last = midi_clock.have_pulse ? midi_clock.last_ms : midi_clock.start_ms;
            if (fixed && fm1_ms - last > 500u) {
                seq_stop();                         /* cable loss must not leave a running held note */
                midi_clock.tempo_valid = 0;
            } else
                seq_n = arp_n = arp_gate = midi_clock_advance(fm1_ms);
        } else if (!midi_clock.stopped && midi_clock.have_pulse) {
            if (fixed && (int32_t)(fm1_ms - midi_clock.last_ms) > 500) {
                midi_clock.have_pulse = 0;          /* the clock went away: a returning one starts clean */
                midi_clock.pos = midi_clock.rendered = midi_clock.rem = 0;
            } else
                arp_n = arp_gate = midi_clock_advance(fm1_ms);
        }
    }
    if (aud_req)
        seq_audition();                               /* (STEP PREVIEW: a step sounds, stopped) */
    uint32_t beat_adv = clock_mode ? midi_grid_advance(&midi_beat_grid, arp_n, 0u) : arp_n;
    TIDE_BEAT_BLOCK(beat_adv);                         /* TIDE hook (H3) */
    uint32_t armed = clock_mode && song.playing && !midi_clock.have_pulse;   /* Start waits: step 0 (and the click) go with the first clock (midi_clock_pulse) */
    for (i = 0; i < NSHOWN && !armed; i++)            /* (tracks 3, 4: never step) */
        seq_tick(&trk[i], seq_n);
    if (cin_flush)
        cin_record();
    if (song.playing && !armed)
        click_tick(seq_n);
    for (i = 0; i < NSHOWN; i++) {
        uint32_t an = clock_mode ? midi_grid_advance(&midi_arp_grid[i], arp_n, (uint32_t)trk[i].p[P_ARATE]) : arp_n;
        uint32_t gn = clock_mode && (song.playing || (!midi_clock.stopped && midi_clock.have_pulse)) ? an : arp_gate;
        arp_step(&trk[i], an, gn);
    }
    duck_pump(beat_adv);                              /* DUCK SRC PUMP: the arp's samples (fx.c) */
    beat_pos += beat_adv;            /* the beat the ARP LED flashes on (ui_leds) */
    if (beat_pos >= beat_samples()) {
        beat_pos -= beat_samples();
        beat_pos = beat_pos < beat_samples() ? beat_pos : 0u;
        beat_n = (beat_n + 1u) & 3u;
        perf_beat++;                                  /* (PERFORM's PUMP: the beats, perform.c) */
        TIDE_BEAT();                                  /* TIDE hook (H3) */
    }
    if (song.playing)
        song.tick++;
}
#if DUELL_TIDE
#include "tide_seq.c"          /* TIDE hook (H1): TIDE's sequencer side */
#endif
