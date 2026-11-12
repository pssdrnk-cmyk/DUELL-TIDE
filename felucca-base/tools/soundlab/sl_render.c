/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab render (audit tool, not firmware): one factory multi or sound rendered through the host simulation to PREFIX.wav + PREFIX.json.
 * This file unifies the one-off renderers of the b8 / b9 sound lanes: work/reel-2026-11-10/sound-check/tools/reelrender.c (the voice log, TIDE with
 * the style's rows, the unit's NOTES budget), work/b9/dub/reelrender.c (+ RR_DRY / RR_NOFX / RR_SOLO / RR_SUB / RR_KEYS / RR_SHIFT / RR_TAIL),
 * tests/b9_rules_probe.c (End A / middle / End B, TIDE WANDER with PITCH on), work/b9/knobs/click (a delay TIME step at a block), work/b9/knobstr/ksrender.c
 * (a genre knob turned by detents through the UI, a held chord, the tail). It is built like the host tests (tests/tide_roles_test.c as a library: its main is
 * renamed away, the firmware is one translation unit). See tools/soundlab/README.md for the options; `soundlab render` with no argument prints them.
 * The WAV is the master output (after MASTER and the limiter, HOST_MIX_FOLLOWS_MASTER), 16-bit stereo at the firmware's FS; the JSON is described in sl.h's
 * neighbour, README.md ("PREFIX.json"). Deterministic: the same arguments give the same bytes (no clock, no rand outside the firmware's own seeded ones). */
/* (build.sh: tests/tide_roles_test.c with its main renamed, as build/soundlab/trt_lib.c; a #define main does not survive tests/ui_test.c) */
#include "trt_lib.c"
#include <ctype.h>
#include "sl.h"

#ifndef SL_COMMIT
#define SL_COMMIT "unknown"
#endif

/* ---------------------------------------------------------------- tables */
static const char *const SL_STYLE[6] = {"DUB", "DEEP", "MINIMAL", "AMBIENT", "PEAK", "DRONE"};
static const uint8_t SL_STYLE_OF[24] = {0, 0, 1, 1, 1, 4, 4, 1, 2, 2, 2, 2, 4, 3, 1, 3, 5, 5, 2, 0, 1, 4, 3, 2};   /* the suggested STYLE of M01..M18 (work/b9/sound/render18.sh); b10: M19 MINIMAL, M20 DUB, M21 DEEP, M22-M24 PEAK, AMBIENT, MINIMAL */
_Static_assert(MU_SHOWN <= sizeof SL_STYLE_OF, "soundlab: a suggested STYLE for every shown multi");
static const char *const SL_SET[8] = {"AUTO", "STD", "DUB", "HSE", "TECH", "ACID", "ELEC", "AMB"};   /* tide_knobs = index + 1 (1 AUTO, 2 STD, 3 DUB ..) */
static const char *const SL_KNOBNAME[20] = {"FLT", "RES", "DLY", "REV", "EDF", "ATK", "DEC", "REL", "LRT", "LDF", "DKD", "AGT", "SGT", "APB", "DST", "CHO", "DKS", "FDB", "COL", "TIM"};

enum { EV_KNOB, EV_STEP, EV_SET };
typedef struct {
    int kind, bar, done;
    int set, k, det;                                   /* EV_KNOB: SL_SET index, K 2..4, detents */
    int delta, absolute;                               /* EV_STEP: DTIME += delta (absolute: = delta) */
    int pi, gid;                                       /* EV_SET: part (0: global, 1, 2, 3 = both) and the id */
    int val;
    char name[24];
} sl_ev_t;
typedef struct {
    double t0, tr, t1;
    int part, note, vel;
    char name[24];
} sl_voice_t;

/* ---------------------------------------------------------------- small helpers */
static void sl_w32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void sl_w16(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }
static int sl_ieq(const char *a, const char *b)         /* equal, ignoring case, spaces and underscores */
{
    for (;;) {
        while (*a == ' ' || *a == '_')
            a++;
        while (*b == ' ' || *b == '_')
            b++;
        if (!*a || !*b)
            return *a == *b;
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++, b++;
    }
}
static int sl_die(const char *what, const char *arg)
{
    fprintf(stderr, "soundlab render: %s%s%s\n", what, arg ? ": " : "", arg ? arg : "");
    return 2;
}
static int sl_note(const char *s)                       /* C4 = 60: "C3", "Eb3", "F#4", "c#2", "60" */
{
    static const int PC[7] = {9, 11, 0, 2, 4, 5, 7};
    int n, o;
    char *e;
    if (!s[0])
        return -1;
    if (isdigit((unsigned char)s[0]) || s[0] == '-') {
        n = (int)strtol(s, &e, 10);
        return *e || n < 0 || n > 127 ? -1 : n;
    }
    if (tolower((unsigned char)s[0]) < 'a' || tolower((unsigned char)s[0]) > 'g')
        return -1;
    n = PC[tolower((unsigned char)s[0]) - 'a'];
    s++;
    while (*s == '#' || *s == 'b') {
        n += *s == '#' ? 1 : -1;
        s++;
    }
    o = (int)strtol(s, &e, 10);
    if (e == s || *e)
        return -1;
    n += 12 * (o + 1);
    return n < 0 || n > 127 ? -1 : n;
}
static int sl_style(const char *s)
{
    int i;
    for (i = 0; i < 6; i++)
        if (sl_ieq(s, SL_STYLE[i]))
            return i;
    return -1;
}
static int sl_setix(const char *s)
{
    int i;
    for (i = 0; i < 8; i++)
        if (sl_ieq(s, SL_SET[i]))
            return i;
    return -1;
}
static void sl_jstr(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\')
            fputc('\\', f), fputc(*s, f);
        else if ((unsigned char)*s < 32 || (unsigned char)*s > 126)
            fputc('?', f);
        else
            fputc(*s, f);
    }
    fputc('"', f);
}
static void sl_jnum(FILE *f, double v)
{
    if (v != v)
        fputs("null", f);
    else
        fprintf(f, "%.6f", v);
}

/* the parameter ids a name can mean: --g (globals, song.g[]) and --p (a part's p[]); aliases first, then the descriptor labels (case, spaces ignored) */
static int sl_gid(const char *name)
{
    static const struct { const char *n; int id; } A[] = {{"BPM", G_BPM}, {"DTIME", G_DTIME}, {"DFDBK", G_DFDBK}, {"DCOLOR", G_DCOLOR}, {"DMIX", G_DMIX}, {"DMRATE", G_DMRATE},
                                                          {"DMDEP", G_DMDEP}, {"DMODE", G_DMODE}, {"RTYPE", G_RTYPE}, {"SWING", G_SWING}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof *A; i++)
        if (sl_ieq(name, A[i].n))
            return A[i].id;
    if (name[0] == '#' && isdigit((unsigned char)name[1]) && atoi(name + 1) < (int)G_COUNT)
        return atoi(name + 1);
    for (i = 0; i < (uint32_t)G_COUNT; i++)
        if (GP[i].label && sl_ieq(name, GP[i].label))
            return (int)i;
    return -1;
}
static int sl_pid(const char *name)
{
    static const struct { const char *n; int id; } A[] = {{"DLY", P_DLY}, {"REV", P_REV}, {"CHOR", P_CHOR}, {"LEVEL", P_LEVEL}, {"SUBLVL", XW_BASE + X_SUBLVL}, {"SUBOCT", XW_BASE + X_SUBOCT}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof *A; i++)
        if (sl_ieq(name, A[i].n))
            return A[i].id;
    if (name[0] == '#' && isdigit((unsigned char)name[1]) && atoi(name + 1) < (int)EXT_P_LEN)
        return atoi(name + 1);
    for (i = 0; i < (uint32_t)P_COUNT; i++)
        if (TP[i].label && sl_ieq(name, TP[i].label))
            return (int)i;
    return -1;
}
static void sl_setval(const sl_ev_t *e)                 /* a --g / --p value, clamped to the descriptor's range */
{
    uint32_t p;
    if (e->pi == 0) {
        const param_desc_t *d = &GP[e->gid];
        int v = e->val;
        song.g[e->gid] = (int16_t)(v < d->min ? d->min : v > d->max ? d->max : v);
        return;
    }
    for (p = 0; p < 2u; p++) {
        if (!((e->pi >> p) & 1))
            continue;
        const param_desc_t *d = track_desc(&trk[p], (uint32_t)e->gid);
        int v = e->val;
        if (d && d->max > d->min)
            v = v < d->min ? d->min : v > d->max ? d->max : v;
        trk[p].p[e->gid] = (int16_t)v;
    }
}

/* ---------------------------------------------------------------- usage */
static void sl_usage(void)
{
    puts("usage: soundlab render --multi N|all | --sound N|NAME  --out PREFIX  [options]\n"
         "  --multi N        factory multi 1..MU_SHOWN (M01..; `all`: every one as PREFIX_Mnn)      --sound N|NAME  one factory sound (1-based index or its name) on part 1\n"
         "  --keys none|chord:C3,Eb3,G3|note:C3   keys held through the run (C4 = 60; default none; for --sound note:C3)\n"
         "  --bars 16 --bpm B  bars to record; tempo (default: the multi's own)\n"
         "  --tide off|evolve|travel|auto   off: PLAY alone; evolve / travel (= wander / journey): TIDE with the style's rows, MODE forced; auto: the style's own MODE\n"
         "  --style NAME     DUB DEEP MINIMAL AMBIENT PEAK DRONE (default: the multi's suggested STYLE)     --pitch on|off  TIDE's pitch moves (default off)\n"
         "  --morph a|mid|b|0..127   both MORPH faders (default: as loaded, End A)\n"
         "  --knob SET:K:DET[@BAR]   a genre KNOB turned DET detents (K 2..4; SET AUTO STD DUB HSE TECH ACID ELEC AMB) before PLAY, or at the start of bar BAR; repeatable\n"
         "  --fx-time-step BAR:+1    the delay TIME index moved by +-n (BAR:=v sets it) at the start of bar BAR; --control also renders PREFIX_ctl without the step\n"
         "  --dry | --fx dry|delay|reverb|full   FX sends: all 0 | none of delay+reverb | delay only | reverb only | as stored      --nofx drc  letters: d delay, r reverb, c chorus sends 0\n"
         "  --solo 1|2       the other part's LEVEL 0     --g NAME=V  a global (BPM DTIME DFDBK DCOLOR DMDEP DMODE .. or its label)     --p [1.|2.]NAME=V  a part value (DLY REV CHOR LEVEL SUBLVL SUBOCT .. or its label)\n"
         "  --keys-raw       the keys by what they sound with TRN 0 (default: the notes are what SOUNDS, TRN is compensated)     --keys-turn MS  keys pressed one after another, MS apart, in the order given\n"
         "  --ringout SEC    after the last bar: keys up, transport stopped, SEC more seconds (the tails)      --skip N  bars played and not recorded (default 4 with TIDE, else 0)\n"
         "  --transport on|off   the sequencer (default on for a multi, off for a sound: the held keys alone, bars by time)      --uncap  no NOTES budget\n"
         "  --dump-params    also PREFIX.params.tsv: every part and global value at the start of each bar\n"
         "  PREFIX.wav (stereo 16-bit) and PREFIX.json are written.");
}

/* ---------------------------------------------------------------- the run */
typedef struct {
    int multi, sound, all;                              /* multi 1..18; sound 0-based index; -1 none */
    char keys[96];                                      /* "none", or the text after "--keys " */
    int keys_given;
    int bars, bpm;
    int tide;                                           /* 0 off, 1 evolve (GM_WANDER), 2 travel (GM_JOURNEY), 3 auto (the style's MODE) */
    int style, pitch, morph;                            /* style -1: suggested; morph -1: as loaded */
    int fx;                                             /* 0 full, 1 dry, 2 delay only (reverb 0), 3 reverb only (delay 0) */
    char nofx[8];
    int solo, uncap, transport, dump_params, control;   /* transport -1: default */
    int keys_raw, keys_turn_ms;                         /* --keys-raw: the keys as pressed (part 1's TRN not compensated); --keys-turn MS: pressed one after another, in the order given */
    double ringout;
    int skip;
    const char *out;
    sl_ev_t ev[64];
    int nev;
} sl_opt_t;

static uint32_t sl_keymask(const int *notes, int n, int *oct_out, int *dropped)   /* the keys whose notes are `notes` on part 1, in an octave setting that reaches them all */
{
    static const int8_t TRY[9] = {0, -1, 1, -2, 2, -3, 3, -4, 4};
    int i, t, k, best = -1, bestn = -1;
    uint32_t bestm = 0;
    int8_t keep = song.octave;
    for (t = -1; t < 9; t++) {
        int oct = t < 0 ? keep : TRY[t], got = 0;
        uint32_t m = 0;
        for (i = 0; i < n; i++) {
            int hit = 0;
            for (k = 0; k < 27; k++)
                if ((int)kb_pitch(&trk[0], (uint32_t)k, oct) == notes[i])
                    m |= 1u << k, hit = 1;
            got += hit;
        }
        if (got > bestn) {
            bestn = got, best = oct, bestm = m;
            if (got == n)
                break;
        }
    }
    *oct_out = best;
    *dropped = n - bestn;
    return bestm;
}
static void sl_pname(uint32_t p, char *dst24)           /* the part's sound as the footer names it, without its "#80 " number */
{
    char n[24];
    const char *s = n;
    tide_part_name(p, n);
    if (s[0] == '#') {
        while (*s && *s != ' ')
            s++;
        while (*s == ' ')
            s++;
    }
    snprintf(dst24, 24, "%s", s);
}
static void sl_clip_write(FILE *fw, const int32_t *out)
{
    uint32_t i;
    for (i = 0; i < CTL; i++) {
        int32_t L = out[2u * i], R = out[2u * i + 1u];
        L = L > 32767 ? 32767 : L < -32768 ? -32768 : L;
        R = R > 32767 ? 32767 : R < -32768 ? -32768 : R;
        sl_w16(fw, (uint32_t)(uint16_t)(int16_t)L);
        sl_w16(fw, (uint32_t)(uint16_t)(int16_t)R);
    }
}
static void sl_knob_turn(const sl_ev_t *e)
{
    int i, n = e->det > 0 ? e->det : -e->det;
    tide_knobs = (uint8_t)(e->set + 1);
    for (i = 0; i < n; i++)
        turn(EN_K2 + (uint32_t)(e->k - 2), e->det > 0 ? 1 : -1);
    frames(64);
}

static int sl_run(const sl_opt_t *o, int argc, char **argv)
{
    static int32_t out[2 * CTL];
    char fn[600], keytext[96], sname[48];
    uint32_t i, p, b, macc = 0, bar = 0, curbar = 0, started = 0, nsamp = 0, perbar, guard, skip, nb = (uint32_t)o->bars, bar_rec = 0;
    int transport = o->transport >= 0 ? o->transport : (o->multi > 0 ? 1 : 0), style = o->style, ev_i, fire_end;
    int keys_n = 0, keys[8], keys_oct = 0, keys_drop = 0, k0_set = 0, stag_n = 0;
    uint32_t stag_bit[8] = {0};
    double bar_t[4096], ring_start = -1.0;
    uint8_t pst[2][NVOICE] = {{0}}, pact[2][NVOICE] = {{0}};
    uint32_t page[2][NVOICE] = {{0}};
    int open_v[2][NVOICE], vmax[2] = {0, 0}, fader0 = 0, fader1 = 0;
    uint32_t capsum[2] = {0, 0}, capn = 0, nvoice = 0, vcap = 0;
    sl_voice_t *voice = 0;
    FILE *fw, *fj, *fp = 0;
    int knob_seen = 0, kn_before[2][KN_NID], kn_after[2][KN_NID], dly_before = 0, dly_after = 0;
    sl_ev_t log_ev[64];
    double log_t[64];
    int nlog = 0;
    const int need_ui = o->nev > 0;

    memset(open_v, -1, sizeof open_v);
    /* ---- the keys */
    snprintf(keytext, sizeof keytext, "%s", o->keys_given ? o->keys : (o->multi > 0 ? "none" : "note:C3"));
    if (strncmp(keytext, "none", 4)) {
        const char *s = strchr(keytext, ':');
        char buf[96], *tok;
        if (!s || (strncmp(keytext, "chord:", 6) && strncmp(keytext, "note:", 5)))
            return sl_die("--keys: none, chord:C3,Eb3,G3 or note:C3", keytext);
        snprintf(buf, sizeof buf, "%s", s + 1);
        for (tok = strtok(buf, ","); tok; tok = strtok(0, ",")) {
            int n = sl_note(tok);
            if (n < 0 || keys_n >= 8)
                return sl_die("--keys: bad note", tok);
            keys[keys_n++] = n;
        }
        if (!keys_n)
            return sl_die("--keys: no note", keytext);
    }

    /* ---- load */
    if (o->multi > 0) {
        load((uint32_t)(o->multi - 1));
        snprintf(sname, sizeof sname, "%s", "");
    } else {
        uint32_t e, k = 0;
        const ds_sound_t *s;
        for (e = 0; e < NENGINES && (uint32_t)o->sound >= (uint32_t)(DS_ENG_AT[e + 1u]); e++)
            ;
        if (e >= NENGINES)
            return sl_die("--sound: no such sound", 0);
        k = (uint32_t)o->sound - (uint32_t)DS_ENG_AT[e];
        s = ds_get(e, k);
        load(0);
        host_tracks_init();
        song.g[G_RTYPE] = GP[G_RTYPE].def;
        duell_mode_force(DM_STEREO);
        host_ds_load(&trk[0], e, k);
        trk[1].p[P_LEVEL] = 0;
        snprintf(sname, sizeof sname, "%s", s ? s->name : "?");
    }
    song.master_q12 = 2048;
    if (o->bpm > 0)
        song.g[G_BPM] = (int16_t)o->bpm;
    /* ---- FX sends, solo, generic values */
    {
        int fx = o->fx;
        for (p = 0; p < 2u; p++) {
            if (fx == 1)
                trk[p].p[P_DLY] = trk[p].p[P_REV] = 0;
            if (fx == 2)
                trk[p].p[P_REV] = 0;
            if (fx == 3)
                trk[p].p[P_DLY] = 0;
            if (strchr(o->nofx, 'd'))
                trk[p].p[P_DLY] = 0;
            if (strchr(o->nofx, 'r'))
                trk[p].p[P_REV] = 0;
            if (strchr(o->nofx, 'c'))
                trk[p].p[P_CHOR] = 0;
        }
        if (o->solo == 1 || o->solo == 2)
            trk[2 - o->solo].p[P_LEVEL] = 0;
        for (ev_i = 0; ev_i < o->nev; ev_i++)
            if (o->ev[ev_i].kind == EV_SET && o->ev[ev_i].bar == 0) {
                sl_setval(&o->ev[ev_i]);
                log_ev[nlog] = o->ev[ev_i], log_t[nlog++] = 0.0;
            }
    }
    for (i = 0; i < 8u; i++) {
        mix_block(out, CTL);
        while (wave_service())
            ;
    }
    /* ---- knobs (the UI is set up as work/b9/knobstr/ksrender.c did: the MORPH page, BOTH parts, the sound's values taken) */
    if (need_ui) {
        ui.menu = 0;
        go_title("MORPH");
        frames(32);
        song.both = 1;
        song.sel = 0;
        h1_take(0);
        h1_take(1);
        frames(32);
    }
    for (ev_i = 0; ev_i < o->nev; ev_i++) {
        const sl_ev_t *e = &o->ev[ev_i];
        if (e->bar != 0 || e->kind == EV_SET)
            continue;
        if (e->kind == EV_KNOB) {
            if (!knob_seen) {
                for (p = 0; p < 2u; p++)
                    for (i = 0; i < KN_NID; i++)
                        kn_before[p][i] = (int)(i < KN_NP ? trk[p].p[KN_ID[i]] : song.g[KN_ID[i]]);
                dly_before = (int)delay_samples();
                knob_seen = 1;
            }
            sl_knob_turn(e);
        } else {
            const param_desc_t *d = &GP[G_DTIME];
            int v = e->absolute ? e->delta : song.g[G_DTIME] + e->delta;
            song.g[G_DTIME] = (int16_t)(v < d->min ? d->min : v > d->max ? d->max : v);
        }
        log_ev[nlog] = *e, log_t[nlog++] = 0.0;
    }
    if (knob_seen) {
        for (p = 0; p < 2u; p++)
            for (i = 0; i < KN_NID; i++)
                kn_after[p][i] = (int)(i < KN_NP ? trk[p].p[KN_ID[i]] : song.g[KN_ID[i]]);
        dly_after = (int)delay_samples();
        for (i = 0; i < 8u; i++) {
            mix_block(out, CTL);
            while (wave_service())
                ;
        }
    }
    /* ---- MORPH faders */
    if (o->morph >= 0)
        morph_ready(0)->fader = morph_ready(1)->fader = (uint8_t)o->morph;
    fader0 = mph[0].fader, fader1 = mph[1].fader;
    /* ---- keys down */
    if (keys_n) {
        int16_t trn0 = trk[0].p[P_TRANS];
        if (o->keys_raw)
            trk[0].p[P_TRANS] = 0;                      /* the notes name the keys as they sound with TRN 0: a TRN set by --p or the sound then moves them */
        fm1_in.notes = sl_keymask(keys, keys_n, &keys_oct, &keys_drop);
        for (stag_n = 0; stag_n < keys_n && stag_n < 8; stag_n++) {   /* each note's own key, for --keys-turn */
            int kk;
            stag_bit[stag_n] = 0u;
            for (kk = 0; kk < 27; kk++)
                if ((int)kb_pitch(&trk[0], (uint32_t)kk, keys_oct) == keys[stag_n])
                    stag_bit[stag_n] |= 1u << kk;
        }
        trk[0].p[P_TRANS] = trn0;
        if (o->keys_turn_ms > 0 && keys_n > 1)
            fm1_in.notes = stag_bit[0];                 /* --keys-turn: only the first key down at the start */
        song.octave = (int8_t)keys_oct;
        if (keys_drop) {
            fprintf(stderr, "soundlab render: %d key(s) of %s cannot be played on part 1's keys (SCALE / TRANS / OFF-SCALE move them): use --p TRN=.. or other notes\n", keys_drop, keytext);
            return 2;
        }
    } else
        fm1_in.notes = 0u;
    /* ---- TIDE, PLAY */
    if (style < 0)
        style = o->multi > 0 ? SL_STYLE_OF[o->multi - 1] : STY_DUB;
    skip = (uint32_t)(o->skip >= 0 ? o->skip : (o->tide && transport ? 4 : 0));
    if (o->tide) {
        tide_style_apply((uint32_t)style);
        if (o->tide == 1)
            gl.mode = GM_WANDER;
        else if (o->tide == 2)
            gl.mode = GM_JOURNEY;
        gl.target = GT_BOTH;
        morph_musical = 1;
        if (o->pitch)
            morph.cfg |= MC_PIT;
        else
            morph.cfg = (uint8_t)(morph.cfg & ~MC_PIT);
        glide_launch(1u, 3u, 0u);
    } else if (transport)
        transport_req = 1;
    (void)k0_set;

    /* ---- outputs */
    snprintf(fn, sizeof fn, "%s.wav", o->out);
    fw = fopen(fn, "wb");
    if (!fw)
        return sl_die("cannot write", fn);
    fwrite("RIFF\0\0\0\0WAVEfmt ", 1, 16, fw);
    sl_w32(fw, 16); sl_w16(fw, 1); sl_w16(fw, 2); sl_w32(fw, FS); sl_w32(fw, FS * 4u); sl_w16(fw, 4); sl_w16(fw, 16);
    fwrite("data\0\0\0\0", 1, 8, fw);
    if (o->dump_params) {
        snprintf(fn, sizeof fn, "%s.params.tsv", o->out);
        fp = fopen(fn, "w");
    }
    perbar = (uint32_t)(4.0 * FS * 60.0 / (song.g[G_BPM] ? song.g[G_BPM] : 120) / CTL);   /* control blocks a bar */
    guard = (nb + skip + 8u) * perbar * 2u + 8000u;
    bar_t[0] = 0.0;
    fire_end = 0;
    if (nb > 4000u)
        return sl_die("--bars: at most 4000", 0);

#define SL_DUMP_PARAMS(barno)                                                                                                                         \
    do {                                                                                                                                              \
        if (fp) {                                                                                                                                     \
            uint32_t q, r;                                                                                                                            \
            for (q = 0; q < 2u; q++)                                                                                                                  \
                for (r = 0; r < (uint32_t)EXT_P_LEN; r++) {                                                                                           \
                    const param_desc_t *d = track_desc(&trk[q], r);                                                                                   \
                    fprintf(fp, "%u\tP%u\t%u\t%s\t%d\n", (unsigned)(barno), (unsigned)(q + 1u), (unsigned)r, d && d->label ? d->label : "", (int)trk[q].p[r]);  \
                }                                                                                                                                     \
            for (r = 0; r < (uint32_t)G_COUNT; r++)                                                                                                   \
                fprintf(fp, "%u\tG\t%u\t%s\t%d\n", (unsigned)(barno), (unsigned)r, GP[r].label ? GP[r].label : "", (int)song.g[r]);                  \
        }                                                                                                                                             \
    } while (0)
    if (fp)
        fputs("bar\tscope\tid\tname\tvalue\n", fp);

    /* ---- the main loop. A "step" below is one control block of CTL samples */
    for (b = 0; b < guard; b++) {
        int rec_bar = -1;
        mix_block(out, CTL);
        while (wave_service())
            ;
        macc += 1000u * CTL;
        if (macc >= (uint32_t)FS * 16u) {
            macc -= (uint32_t)FS * 16u;
            frame();
            if (!o->uncap) {                          /* the unit's NOTES budget, two-core rule (budget.c budget_poll with bd_two_now) */
                bd_t bb;
                uint32_t n2[2];
                budget_poll();
                bd_build(&bb);
                budget_solve2(&bb, n2, 1);
                bnotes[0] = (uint8_t)n2[0], bnotes[1] = (uint8_t)n2[1];
                capsum[0] += n2[0], capsum[1] += n2[1], capn++;
            }
        }
        if (transport) {
            if (!song.playing)
                continue;
            if (!started) {
                started = 1;
                curbar = sqm_bar;
            } else if ((uint32_t)sqm_bar != curbar) {
                bar++;
                curbar = sqm_bar;
                if (bar >= skip) {
                    bar_rec = (int)(bar - skip);
                    if (bar_rec > 0 && (uint32_t)bar_rec <= nb)
                        bar_t[bar_rec] = (double)nsamp / FS;
                    if (bar_rec > 0)
                        SL_DUMP_PARAMS(bar_rec);
                }
            }
            if (bar < skip)
                continue;
            if (bar >= nb + skip)
                break;
        } else {
            uint32_t cur = nsamp / (perbar * CTL);
            if (cur >= nb)
                break;
            if (nsamp % (perbar * CTL) == 0u) {
                bar_rec = (int)cur;
                if (cur > 0)
                    bar_t[cur] = (double)nsamp / FS;
            }
        }
        if (o->keys_turn_ms > 0 && keys_n > 1 && nsamp < (uint32_t)(o->keys_turn_ms * (keys_n + 1) * (FS / 1000u))) {   /* --keys-turn: key i goes down at i * MS */
            uint32_t due = 1u + (uint32_t)(((double)nsamp * 1000.0 / (double)FS) / (double)o->keys_turn_ms), m = 0, i2;
            for (i2 = 0; i2 < due && i2 < (uint32_t)keys_n; i2++)
                m |= stag_bit[i2];
            fm1_in.notes = m;
        }
        /* ---- events at the start of a bar (bar 0 ones ran before PLAY) */
        if (bar_rec > 0) {
            for (ev_i = 0; ev_i < o->nev; ev_i++) {
                sl_ev_t *e = (sl_ev_t *)&o->ev[ev_i];
                if (e->bar != bar_rec || e->done)
                    continue;
                e->done = 1;
                if (e->kind == EV_KNOB) {
                    if (!knob_seen) {
                        for (p = 0; p < 2u; p++)
                            for (i = 0; i < KN_NID; i++)
                                kn_before[p][i] = (int)(i < KN_NP ? trk[p].p[KN_ID[i]] : song.g[KN_ID[i]]);
                        dly_before = (int)delay_samples();
                        knob_seen = 1;
                    }
                    sl_knob_turn(e);
                    for (p = 0; p < 2u; p++)
                        for (i = 0; i < KN_NID; i++)
                            kn_after[p][i] = (int)(i < KN_NP ? trk[p].p[KN_ID[i]] : song.g[KN_ID[i]]);
                    dly_after = (int)delay_samples();
                } else if (e->kind == EV_STEP) {
                    const param_desc_t *d = &GP[G_DTIME];
                    int v = e->absolute ? e->delta : song.g[G_DTIME] + e->delta;
                    song.g[G_DTIME] = (int16_t)(v < d->min ? d->min : v > d->max ? d->max : v);
                } else
                    sl_setval(e);
                if (nlog < 64)
                    log_ev[nlog] = *e, log_t[nlog++] = (double)nsamp / FS;
                if (e->kind == EV_STEP)
                    log_ev[nlog - 1].val = song.g[G_DTIME];
            }
        }
        if (nsamp == 0u)
            SL_DUMP_PARAMS(0);
        /* ---- the voice log (before the block is written: the time of an event is the start of this block) */
        for (p = 0; p < 2u; p++) {
            int c = 0;
            for (i = 0; i < NVOICE; i++) {
                const voice_t *v = &trk[p].v[i];
                int act = v->active != 0, st = act ? v->stage : 0;
                double now = (double)nsamp / FS;
                if (act && (!pact[p][i] || v->age != page[p][i])) {
                    if (open_v[p][i] >= 0 && voice[open_v[p][i]].t1 < 0)
                        voice[open_v[p][i]].t1 = now;
                    if (nvoice == vcap) {
                        vcap = vcap ? vcap * 2u : 1024u;
                        voice = (sl_voice_t *)realloc(voice, vcap * sizeof *voice);
                        if (!voice)
                            return sl_die("out of memory", 0);
                    }
                    voice[nvoice].t0 = now, voice[nvoice].tr = -1.0, voice[nvoice].t1 = -1.0;
                    voice[nvoice].part = (int)p + 1, voice[nvoice].note = (int)v->note, voice[nvoice].vel = (int)v->vel;
                    sl_pname(p, voice[nvoice].name);
                    voice[nvoice].name[sizeof voice[nvoice].name - 1] = 0;
                    open_v[p][i] = (int)nvoice++;
                } else if (act && st >= 3 && pst[p][i] < 3 && open_v[p][i] >= 0 && voice[open_v[p][i]].tr < 0)
                    voice[open_v[p][i]].tr = now;
                if (!act && pact[p][i] && open_v[p][i] >= 0 && voice[open_v[p][i]].t1 < 0) {
                    voice[open_v[p][i]].t1 = now;
                    open_v[p][i] = -1;
                }
                pact[p][i] = (uint8_t)act, pst[p][i] = (uint8_t)st, page[p][i] = v->age;
                c += act;
            }
            if (c > vmax[p])
                vmax[p] = c;
        }
        sl_clip_write(fw, out);
        nsamp += CTL;
    }
    (void)fire_end;
    bar_t[nb] = (double)nsamp / FS;
    /* ---- the tail: keys up, transport stopped, rendered on (the echoes' and the reverb's tail) */
    if (o->ringout > 0.0) {
        uint32_t tb, tn = (uint32_t)(o->ringout * FS / CTL);
        fm1_in.notes = 0u;
        if (transport)
            transport_req = 2;
        ring_start = (double)nsamp / FS;
        for (tb = 0; tb < tn; tb++) {
            mix_block(out, CTL);
            while (wave_service())
                ;
            macc += 1000u * CTL;
            if (macc >= (uint32_t)FS * 16u) {
                macc -= (uint32_t)FS * 16u;
                frame();
            }
            for (p = 0; p < 2u; p++)
                for (i = 0; i < NVOICE; i++) {
                    const voice_t *v = &trk[p].v[i];
                    int act = v->active != 0, st = act ? v->stage : 0;
                    double now = (double)nsamp / FS;
                    if (act && (!pact[p][i] || v->age != page[p][i]) && nvoice < vcap) {
                        voice[nvoice].t0 = now, voice[nvoice].tr = -1.0, voice[nvoice].t1 = -1.0;
                        voice[nvoice].part = (int)p + 1, voice[nvoice].note = (int)v->note, voice[nvoice].vel = (int)v->vel;
                        sl_pname(p, voice[nvoice].name);
                        open_v[p][i] = (int)nvoice++;
                    } else if (act && st >= 3 && pst[p][i] < 3 && open_v[p][i] >= 0 && voice[open_v[p][i]].tr < 0)
                        voice[open_v[p][i]].tr = now;
                    if (!act && pact[p][i] && open_v[p][i] >= 0 && voice[open_v[p][i]].t1 < 0) {
                        voice[open_v[p][i]].t1 = now;
                        open_v[p][i] = -1;
                    }
                    pact[p][i] = (uint8_t)act, pst[p][i] = (uint8_t)st, page[p][i] = v->age;
                }
            sl_clip_write(fw, out);
            nsamp += CTL;
        }
    }
    for (i = 0; i < nvoice; i++)
        if (voice[i].t1 < 0.0)
            voice[i].t1 = (double)nsamp / FS;
    fseek(fw, 4, SEEK_SET); sl_w32(fw, 36u + nsamp * 4u);
    fseek(fw, 40, SEEK_SET); sl_w32(fw, nsamp * 4u);
    fclose(fw);
    if (fp)
        fclose(fp);

    /* ---- the JSON */
    snprintf(fn, sizeof fn, "%s.json", o->out);
    fj = fopen(fn, "w");
    if (!fj)
        return sl_die("cannot write", fn);
    {
        char n1[24], n2[24], cmd[1200] = "", ctl_name[620];
        int a;
        for (a = 1; a < argc; a++) {                      /* (without --out: the same arguments give the same bytes whatever the file is called) */
            if (!strcmp(argv[a], "--out")) {
                a++;
                continue;
            }
            if (strlen(cmd) + strlen(argv[a]) + 4 < sizeof cmd)
                strcat(cmd, cmd[0] ? " " : ""), strcat(cmd, argv[a]);
        }
        snprintf(ctl_name, sizeof ctl_name, "%s_ctl", o->out);
        tide_part_name(0u, n1);
        tide_part_name(1u, n2);
        fprintf(fj, "{\"tool\":\"soundlab\",\"commit\":\"%s\",\"sample_rate\":%u,\"bars\":%u,\"bpm\":%d,\n", SL_COMMIT, (unsigned)FS, nb, (int)song.g[G_BPM]);
        fprintf(fj, " \"seconds\":");
        sl_jnum(fj, (double)nsamp / FS);
        fprintf(fj, ",\"skip_bars\":%u,\"ringout\":", skip);
        sl_jnum(fj, o->ringout > 0.0 ? o->ringout : 0.0);
        fprintf(fj, ",\"ringout_start\":");
        if (ring_start >= 0.0)
            sl_jnum(fj, ring_start);
        else
            fputs("null", fj);
        fputs(",\"control\":", fj);
        if (o->control)
            sl_jstr(fj, ctl_name);
        else
            fputs("null", fj);
        fputs(",\n", fj);
        fputs(" \"source\":{\"multi\":", fj);
        if (o->multi > 0)
            fprintf(fj, "%d,\"name\":", o->multi);
        else
            fputs("null,\"name\":", fj);
        {
            char nm[32];
            project_cur_name(nm);
            sl_jstr(fj, o->multi > 0 ? nm : sname);
        }
        fputs(",\"sound\":", fj);
        if (o->multi > 0)
            fputs("null", fj);
        else
            fprintf(fj, "%d", o->sound + 1);
        fprintf(fj, ",\"part_names\":[");
        sl_jstr(fj, n1), fputc(',', fj), sl_jstr(fj, n2);
        fprintf(fj, "],\"engines\":[");
        sl_jstr(fj, ENGINES[trk[0].engine]->name), fputc(',', fj), sl_jstr(fj, ENGINES[trk[1].engine]->name);
        fputs("]},\n", fj);
        fputs(" \"settings\":{\"keys\":", fj);
        sl_jstr(fj, strncmp(keytext, "none", 4) ? keytext : "none");
        fprintf(fj, ",\"keys_octave\":%d,\"tide\":", keys_n ? keys_oct : (int)song.octave);
        sl_jstr(fj, o->tide == 0 ? "off" : o->tide == 1 ? "evolve" : o->tide == 2 ? "travel" : "auto");
        fputs(",\"style\":", fj);
        if (o->tide)
            sl_jstr(fj, SL_STYLE[style]);
        else
            fputs("null", fj);
        fprintf(fj, ",\"pitch\":%s,\"morph\":", o->tide && o->pitch ? "true" : "false");
        if (o->morph < 0 && fader0 == 0 && fader1 == 0)
            sl_jstr(fj, "a");
        else if (fader0 == fader1)
            fprintf(fj, "%d", fader0);
        else
            fprintf(fj, "[%d,%d]", fader0, fader1);
        fprintf(fj, ",\"transport\":%s,\"fx\":", transport ? "true" : "false");
        sl_jstr(fj, o->fx == 0 ? "full" : o->fx == 1 ? "dry" : o->fx == 2 ? "delay" : "reverb");
        fputs(",\"nofx\":", fj);
        sl_jstr(fj, o->nofx);
        fprintf(fj, ",\"solo\":%d,\"uncap\":%s,\"knobs\":[", o->solo, o->uncap ? "true" : "false");
        {
            int first = 1;
            for (ev_i = 0; ev_i < o->nev; ev_i++)
                if (o->ev[ev_i].kind == EV_KNOB)
                    fprintf(fj, "%s\"%s:%d:%+d@%d\"", first ? "" : ",", SL_SET[o->ev[ev_i].set], o->ev[ev_i].k, o->ev[ev_i].det, o->ev[ev_i].bar), first = 0;
        }
        fputs("],\"overrides\":[", fj);
        {
            int first = 1;
            for (ev_i = 0; ev_i < o->nev; ev_i++)
                if (o->ev[ev_i].kind == EV_SET)
                    fprintf(fj, "%s\"%s%s=%d@%d\"", first ? "" : ",", o->ev[ev_i].pi == 0 ? "g." : o->ev[ev_i].pi == 1 ? "1." : o->ev[ev_i].pi == 2 ? "2." : "p.", o->ev[ev_i].name,
                            o->ev[ev_i].val, o->ev[ev_i].bar), first = 0;
        }
        fputs("],\"fx_time_step\":[", fj);
        {
            int first = 1;
            for (ev_i = 0; ev_i < o->nev; ev_i++)
                if (o->ev[ev_i].kind == EV_STEP)
                    fprintf(fj, "%s\"%d:%s%d\"", first ? "" : ",", o->ev[ev_i].bar, o->ev[ev_i].absolute ? "=" : "", o->ev[ev_i].delta), first = 0;
        }
        fputs("]},\n", fj);
        fputs(" \"command\":", fj);
        sl_jstr(fj, cmd);
        fputs(",\n \"bar_starts\":[", fj);
        for (b = 0; b <= nb; b++)
            fprintf(fj, "%s%.6f", b ? "," : "", bar_t[b]);
        fputs("],\n \"events\":[", fj);
        for (a = 0; a < nlog; a++) {
            const sl_ev_t *e = &log_ev[a];
            fprintf(fj, "%s\n  {\"t\":%.6f,", a ? "," : "", log_t[a]);
            if (e->kind == EV_KNOB)
                fprintf(fj, "\"kind\":\"knob\",\"set\":\"%s\",\"k\":%d,\"detents\":%d,\"bar\":%d}", SL_SET[e->set], e->k, e->det, e->bar);
            else if (e->kind == EV_STEP)
                fprintf(fj, "\"kind\":\"fx_time\",\"sample\":%ld,\"delta\":%d,\"absolute\":%s,\"to\":%d,\"bar\":%d}", (long)(log_t[a] * FS + 0.5), e->delta, e->absolute ? "true" : "false", e->val, e->bar);
            else
                fprintf(fj, "\"kind\":\"set\",\"name\":\"%s\",\"value\":%d,\"part\":%d,\"bar\":%d}", e->name, e->val, e->pi, e->bar);
        }
        fputs(nlog ? "\n ],\n" : "],\n", fj);
        fputs(" \"voices\":[", fj);
        for (i = 0; i < nvoice; i++) {
            fprintf(fj, "%s\n  {\"t0\":%.6f,\"t1\":%.6f,\"t_end\":%.6f,\"part\":%d,\"note\":%d,\"vel\":%d,\"sound\":", i ? "," : "", voice[i].t0, voice[i].tr >= 0.0 ? voice[i].tr : voice[i].t1, voice[i].t1, voice[i].part, voice[i].note, voice[i].vel);
            sl_jstr(fj, voice[i].name);
            fputc('}', fj);
        }
        fputs(nvoice ? "\n ],\n" : "],\n", fj);
        fprintf(fj, " \"blocks\":{\"cpu_note_caps\":[%u,%u],\"cpu_note_caps_avg\":[%.2f,%.2f],\"voices_max\":[%d,%d],\"ctl_samples\":%u},\n", (unsigned)bnotes[0], (unsigned)bnotes[1],
                capn ? (double)capsum[0] / capn : 0.0, capn ? (double)capsum[1] / capn : 0.0, vmax[0], vmax[1], (unsigned)CTL);
        fputs(" \"params\":{", fj);
        for (p = 0; p < 2u; p++) {
            fprintf(fj, "%s\"part%u\":[", p ? "," : "", p + 1u);
            for (i = 0; i < (uint32_t)EXT_P_LEN; i++)
                fprintf(fj, "%s%d", i ? "," : "", (int)trk[p].p[i]);
            fputc(']', fj);
        }
        fputs(",\"glob\":[", fj);
        for (i = 0; i < (uint32_t)G_COUNT; i++)
            fprintf(fj, "%s%d", i ? "," : "", (int)song.g[i]);
        fputs("]}", fj);
        if (knob_seen) {                                  /* what the knob moved (ksrender.c's before / after lines; values in KN_ID's order) */
            fputs(",\n \"knob_params\":{\"names\":[", fj);
            for (i = 0; i < KN_NID; i++)
                fprintf(fj, "%s\"%s\"", i ? "," : "", SL_KNOBNAME[i]);
            fputs("],\"min\":[", fj);
            for (i = 0; i < KN_NID; i++)
                fprintf(fj, "%s%d", i ? "," : "", (int)(i < KN_NP ? track_desc(&trk[0], KN_ID[i]) : glob_desc(KN_ID[i]))->min);
            fputs("],\"max\":[", fj);
            for (i = 0; i < KN_NID; i++)
                fprintf(fj, "%s%d", i ? "," : "", (int)(i < KN_NP ? track_desc(&trk[0], KN_ID[i]) : glob_desc(KN_ID[i]))->max);
            fputs("]", fj);
            for (a = 0; a < 2; a++) {
                fprintf(fj, ",\"before%d\":[", a + 1);
                for (i = 0; i < KN_NID; i++)
                    fprintf(fj, "%s%d", i ? "," : "", kn_before[a][i]);
                fprintf(fj, "],\"after%d\":[", a + 1);
                for (i = 0; i < KN_NID; i++)
                    fprintf(fj, "%s%d", i ? "," : "", kn_after[a][i]);
                fputc(']', fj);
            }
            fprintf(fj, ",\"delay_samples\":[%d,%d]}", dly_before, dly_after);
        }
        fputs("}\n", fj);
    }
    fclose(fj);
    free(voice);
    return 0;
}

static int sl_sys(const char *cmd)                      /* a command line through the shell (cmd.exe wants the whole line in one more pair of quotes) */
{
#ifdef _WIN32
    char buf[4200];
    snprintf(buf, sizeof buf, "\"%s\"", cmd);
    return system(buf);
#else
    return system(cmd);
#endif
}
/* ---------------------------------------------------------------- arguments */
static int sl_parse_event(sl_opt_t *o, int kind, const char *s)
{
    sl_ev_t *e;
    char buf[160], *c1, *c2, *at;
    if (o->nev >= 64)
        return sl_die("too many events", s);
    e = &o->ev[o->nev];
    memset(e, 0, sizeof *e);
    e->kind = kind;
    snprintf(buf, sizeof buf, "%s", s);
    if (kind == EV_KNOB) {                              /* SET:K:DET[@BAR] */
        at = strchr(buf, '@');
        if (at)
            *at = 0, e->bar = atoi(at + 1);
        c1 = strchr(buf, ':');
        c2 = c1 ? strchr(c1 + 1, ':') : 0;
        if (!c1 || !c2)
            return sl_die("--knob SET:K:DETENTS[@BAR]", s);
        *c1 = *c2 = 0;
        e->set = sl_setix(buf);
        e->k = atoi(c1 + 1);
        e->det = atoi(c2 + 1);
        if (e->set < 0 || e->k < 2 || e->k > 4 || e->bar < 0)
            return sl_die("--knob: SET AUTO STD DUB HSE TECH ACID ELEC AMB, K 2..4", s);
    } else if (kind == EV_STEP) {                       /* BAR:+1 or BAR:=12 */
        c1 = strchr(buf, ':');
        if (!c1)
            return sl_die("--fx-time-step BAR:+1", s);
        *c1 = 0;
        e->bar = atoi(buf);
        e->absolute = c1[1] == '=';
        e->delta = atoi(c1 + 1 + e->absolute);
        if (e->bar < 0)
            return sl_die("--fx-time-step: bar", s);
    }
    o->nev++;
    return 0;
}
static int sl_parse_set(sl_opt_t *o, int global, const char *s)   /* --g NAME=V[@BAR], --p [1.|2.]NAME=V[@BAR] */
{
    sl_ev_t *e;
    char buf[160], *eq, *at, *nm = buf;
    if (o->nev >= 64)
        return sl_die("too many events", s);
    e = &o->ev[o->nev];
    memset(e, 0, sizeof *e);
    e->kind = EV_SET;
    snprintf(buf, sizeof buf, "%s", s);
    eq = strchr(buf, '=');
    if (!eq)
        return sl_die("--g / --p NAME=VALUE", s);
    *eq = 0;
    at = strchr(eq + 1, '@');
    if (at)
        *at = 0, e->bar = atoi(at + 1);
    e->val = atoi(eq + 1);
    if (global) {
        e->pi = 0;
        e->gid = sl_gid(nm);
    } else {
        e->pi = 3;
        if ((nm[0] == '1' || nm[0] == '2') && nm[1] == '.')
            e->pi = nm[0] - '0', nm += 2;
        e->gid = sl_pid(nm);
    }
    if (e->gid < 0)
        return sl_die(global ? "--g: unknown global" : "--p: unknown part value", nm);
    snprintf(e->name, sizeof e->name, "%s", nm);
    o->nev++;
    return 0;
}

int sl_render_main(int argc, char **argv)
{
    sl_opt_t o;
    int a, r = 0;
    memset(&o, 0, sizeof o);
    o.multi = 0, o.sound = -1, o.bars = 16, o.style = -1, o.morph = -1, o.transport = -1, o.skip = -1, o.solo = 0;
    if (argc < 2) {
        sl_usage();
        return 2;
    }
    for (a = 1; a < argc; a++) {
        const char *k = argv[a], *v = a + 1 < argc ? argv[a + 1] : 0;
        if (!strcmp(k, "--help") || !strcmp(k, "-h")) {
            sl_usage();
            return 0;
        }
#define NEEDV do { if (!v) return sl_die("missing value for", k); a++; } while (0)
        if (!strcmp(k, "--multi")) {
            NEEDV;
            if (!strcmp(v, "all"))
                o.all = 1;
            else if ((o.multi = atoi(v)) < 1 || o.multi > (int)MU_SHOWN)
                return sl_die("--multi: 1..MU_SHOWN (the shown multis) or all", v);
        } else if (!strcmp(k, "--sound")) {
            NEEDV;
            if (isdigit((unsigned char)v[0])) {
                o.sound = atoi(v) - 1;
                if (o.sound < 0 || o.sound >= (int)DS_N)
                    return sl_die("--sound: number out of range", v);
            } else {
                int i;
                for (i = 0; i < (int)DS_N; i++)
                    if (sl_ieq(v, DS_SOUNDS[i].name)) {
                        o.sound = i;
                        break;
                    }
                if (o.sound < 0)
                    return sl_die("--sound: no factory sound of that name", v);
            }
        } else if (!strcmp(k, "--keys")) {
            NEEDV;
            snprintf(o.keys, sizeof o.keys, "%s", v);
            o.keys_given = 1;
        } else if (!strcmp(k, "--bars")) {
            NEEDV;
            o.bars = atoi(v);
            if (o.bars < 1)
                return sl_die("--bars", v);
        } else if (!strcmp(k, "--bpm")) {
            NEEDV;
            o.bpm = atoi(v);
        } else if (!strcmp(k, "--tide")) {
            NEEDV;
            if (!strcmp(v, "off"))
                o.tide = 0;
            else if (!strcmp(v, "evolve") || !strcmp(v, "wander"))
                o.tide = 1;
            else if (!strcmp(v, "travel") || !strcmp(v, "journey"))
                o.tide = 2;
            else if (!strcmp(v, "auto"))
                o.tide = 3;
            else
                return sl_die("--tide off|evolve|travel|auto", v);
        } else if (!strcmp(k, "--style")) {
            NEEDV;
            if ((o.style = sl_style(v)) < 0)
                return sl_die("--style DUB DEEP MINIMAL AMBIENT PEAK DRONE", v);
        } else if (!strcmp(k, "--pitch")) {
            NEEDV;
            o.pitch = !strcmp(v, "on");
        } else if (!strcmp(k, "--morph")) {
            NEEDV;
            o.morph = !strcmp(v, "a") ? 0 : !strcmp(v, "mid") ? 63 : !strcmp(v, "b") ? 127 : atoi(v);
            if (o.morph < 0 || o.morph > 127 || (o.morph == 0 && strcmp(v, "a") && strcmp(v, "0")))
                return sl_die("--morph a|mid|b|0..127", v);
        } else if (!strcmp(k, "--knob")) {
            NEEDV;
            if (sl_parse_event(&o, EV_KNOB, v))
                return 2;
        } else if (!strcmp(k, "--fx-time-step")) {
            NEEDV;
            if (sl_parse_event(&o, EV_STEP, v))
                return 2;
        } else if (!strcmp(k, "--control")) {
            o.control = 1;
        } else if (!strcmp(k, "--g")) {
            NEEDV;
            if (sl_parse_set(&o, 1, v))
                return 2;
        } else if (!strcmp(k, "--p")) {
            NEEDV;
            if (sl_parse_set(&o, 0, v))
                return 2;
        } else if (!strcmp(k, "--dry")) {
            o.fx = 1;
        } else if (!strcmp(k, "--fx")) {
            NEEDV;
            o.fx = !strcmp(v, "full") ? 0 : !strcmp(v, "dry") ? 1 : !strcmp(v, "delay") ? 2 : !strcmp(v, "reverb") ? 3 : -1;
            if (o.fx < 0)
                return sl_die("--fx dry|delay|reverb|full", v);
        } else if (!strcmp(k, "--nofx")) {
            NEEDV;
            snprintf(o.nofx, sizeof o.nofx, "%s", v);
        } else if (!strcmp(k, "--solo")) {
            NEEDV;
            o.solo = atoi(v);
        } else if (!strcmp(k, "--uncap")) {
            o.uncap = 1;
        } else if (!strcmp(k, "--keys-raw")) {
            o.keys_raw = 1;
        } else if (!strcmp(k, "--keys-turn")) {
            NEEDV;
            o.keys_turn_ms = atoi(v);
        } else if (!strcmp(k, "--ringout")) {
            NEEDV;
            o.ringout = atof(v);
        } else if (!strcmp(k, "--skip")) {
            NEEDV;
            o.skip = atoi(v);
        } else if (!strcmp(k, "--transport")) {
            NEEDV;
            o.transport = !strcmp(v, "on");
        } else if (!strcmp(k, "--dump-params")) {
            o.dump_params = 1;
        } else if (!strcmp(k, "--out")) {
            NEEDV;
            o.out = v;
        } else
            return sl_die("unknown option", k);
    }
    if (!o.out)
        return sl_die("--out PREFIX is required", 0);
    if ((o.multi > 0) + (o.all > 0) + (o.sound >= 0) != 1)
        return sl_die("give exactly one of --multi N, --multi all, --sound N", 0);
    if (o.all) {                                         /* every multi in a process of its own (the firmware's state is not reset by a second load) */
        char cmd[4096];
        int m;
        for (m = 1; m <= (int)MU_SHOWN && !r; m++) {
            int j, n = 0;
            n += snprintf(cmd, sizeof cmd, "\"%s\" render", sl_self);
            for (j = 1; j < argc; j++) {
                if (!strcmp(argv[j], "--multi") || !strcmp(argv[j], "--out")) {
                    j++;
                    continue;
                }
                n += snprintf(cmd + n, sizeof cmd - (size_t)n, " \"%s\"", argv[j]);
            }
            snprintf(cmd + n, sizeof cmd - (size_t)n, " --multi %d --out \"%s_M%02d\"", m, o.out, m);
            r = sl_sys(cmd) != 0;
        }
        return r;
    }
    r = sl_run(&o, argc, argv);
    if (!r && o.control && o.nev) {                      /* the control render: the same run without the delay TIME step(s) */
        char cmd[4096];
        int j, n = 0;
        n += snprintf(cmd, sizeof cmd, "\"%s\" render", sl_self);
        for (j = 1; j < argc; j++) {
            if (!strcmp(argv[j], "--control"))
                continue;
            if (!strcmp(argv[j], "--fx-time-step") || !strcmp(argv[j], "--out")) {
                j++;
                continue;
            }
            n += snprintf(cmd + n, sizeof cmd - (size_t)n, " \"%s\"", argv[j]);
        }
        snprintf(cmd + n, sizeof cmd - (size_t)n, " --out \"%s_ctl\"", o.out);
        r = sl_sys(cmd) != 0;
    }
    return r;
}

/* ---------------------------------------------------------------- params: the descriptors, for tools/soundlab/deadparams.py
 * One block per engine that has factory sounds: "ENG <index> <name> <number of its first factory sound, 1-based> <its name>", then a line per parameter id of part 1 after that
 * sound is loaded: "P <engine> <id> <label> <min> <max> <default> <value the sound has>" (a space in a label is written "_"). That sound is the base the dead-parameter renders start from. */
int sl_params_main(int argc, char **argv)
{
    uint32_t e, id;
    (void)argc;
    (void)argv;
    for (e = 0; e < NENGINES; e++) {
        const ds_sound_t *s;
        if ((uint32_t)DS_ENG_AT[e + 1u] <= (uint32_t)DS_ENG_AT[e])
            continue;
        s = ds_get(e, 0);
        load(0);
        host_tracks_init();
        song.g[G_RTYPE] = GP[G_RTYPE].def;
        duell_mode_force(DM_STEREO);
        host_ds_load(&trk[0], e, 0);
        printf("ENG\t%u\t%s\t%u\t%s\n", (unsigned)e, ENGINES[trk[0].engine]->name, (unsigned)DS_ENG_AT[e] + 1u, s ? s->name : "?");
        for (id = 0; id < (uint32_t)EXT_P_LEN; id++) {
            const param_desc_t *d = track_desc(&trk[0], id);
            char lab[64], *c;
            if (!d || !d->label || !d->label[0] || d->max <= d->min)
                continue;
            snprintf(lab, sizeof lab, "%s", d->label);
            for (c = lab; *c; c++)
                if (*c == ' ' || *c == '\t')
                    *c = '_';
            printf("P\t%u\t%u\t%s\t%d\t%d\t%d\t%d\n", (unsigned)e, (unsigned)id, lab, d->min, d->max, d->def, (int)trk[0].p[id]);
        }
    }
    return 0;
}
