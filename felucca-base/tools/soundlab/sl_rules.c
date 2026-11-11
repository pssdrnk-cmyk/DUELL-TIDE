/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab rules: the measurable part of work/b9/MUSICAL_RULES_B9.md (rules 1 .. 10) on one render, PASS / FAIL / UNSURE with the numbers.
 * A rule that needs more than one render, a source or an ear is UNSURE and says what is missing; the coordinator decides those. */
#include "sl_lab.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

enum { PASS, FAIL, UNSURE };
static const char *VN[3] = { "PASS", "FAIL", "UNSURE" };

typedef struct { int v; char text[600]; } rule;

static void say(rule *r, int v, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
#include <stdarg.h>
static void say(rule *r, int v, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); r->v = v; vsnprintf(r->text, sizeof r->text, fmt, ap); va_end(ap);
}
static int worst(int a, int b) { return (a == FAIL || b == FAIL) ? FAIL : (a == UNSURE || b == UNSURE) ? UNSURE : PASS; }

int sl_cmd_rules(int argc, char **argv) {
    const char *prefix = NULL; int from_bar = 0, to_bar = -1, i, nf = 0, nu = 0, np = 0; double lo = -24.1, hi = -18.1; sl_result R; sl_jw junk, out; rule r[11]; char fn[640];
    sl_jv *js; const char *tide = "off", *style = "", *morph = "";
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--from") && i + 1 < argc) from_bar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--to") && i + 1 < argc) to_bar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--lufs-window") && i + 1 < argc) { if (sscanf(argv[++i], "%lf,%lf", &lo, &hi) != 2) { fprintf(stderr, "soundlab rules: --lufs-window LO,HI\n"); return 2; } }
        else if (argv[i][0] != '-' && !prefix) prefix = argv[i];
        else { fprintf(stderr, "usage: soundlab rules PREFIX [--from BAR --to BAR] [--lufs-window LO,HI]\n"); return 2; }
    }
    if (!prefix) { fprintf(stderr, "usage: soundlab rules PREFIX [--from BAR --to BAR] [--lufs-window LO,HI]\n"); return 2; }
    jw_init(&junk);
    if (sl_measure_core(prefix, from_bar, to_bar, NULL, 0.0, &R, &junk)) { free(junk.buf); return 1; }
    free(junk.buf);
    snprintf(fn, sizeof fn, "%s.json", prefix);
    js = sl_json_load(fn);
    if (js) {
        const sl_jv *st = sl_json_get(js, "settings");
        tide = sl_json_str(sl_json_get(st, "tide"), "off"); style = sl_json_str(sl_json_get(st, "style"), ""); morph = sl_json_str(sl_json_get(st, "morph"), "");
    }
    memset(r, 0, sizeof r);

    /* 1 in key and consonant: the clash between the parts (voicings and the key are not measured) */
    {
        double a = R.rub_pct[1], b = R.rub_pct[0];
        if (!R.has_part[1] || !R.has_part[2]) say(&r[1], PASS, "one part only, no clash between parts possible (voicing / key not measured)");
        else say(&r[1], a <= 1.0 ? PASS : a <= 5.0 ? UNSURE : FAIL, "minor-2nd / 9th rub between the parts %.1f %% of the time both sound (ALL; REL %.1f %%, any-octave 2nd/7th %.1f %%); limit 1 %% unless the style is sourced for it; voicing / key not measured",
                 a, b, R.rub_oct_pct[1]);
    }
    /* 2 one job each part, not the same register */
    {
        if (!R.has_part[1] || !R.has_part[2]) say(&r[2], UNSURE, "needs two parts");
        else {
            double dm = fabs(R.note_med[1] - R.note_med[2]), o1 = R.onsets_bar[1], o2 = R.onsets_bar[2], ratio = (o1 > o2 ? o1 : o2) / ((o1 < o2 ? o1 : o2) + 1e-9);
            int v = (dm >= 7.0 || ratio >= 3.0) ? PASS : (dm < 5.0 && ratio < 1.5) ? FAIL : UNSURE;
            say(&r[2], v, "median note p1 %.0f / p2 %.0f (%.0f semitones apart), onsets a bar %.2f / %.2f (x%.1f); PASS when 7+ semitones apart or the onset density differs 3x (bed against stab); the jobs themselves are not measured",
                R.note_med[1], R.note_med[2], dm, o1, o2, ratio);
        }
    }
    /* 3 a held layer */
    {
        int ok = R.bed_both_mean >= 2.5 && R.silence_notes_pct <= 5.0;
        say(&r[3], ok ? PASS : FAIL, "notes sounding at once %.2f on average (need 2.5), no note sounding %.1f %% of the time (max 5 %%), most at once %.0f%s",
            R.bed_both_mean, R.silence_notes_pct, R.bed_both_max, "; sparse genres need a source (UNSURE for the coordinator)");
    }
    /* 4 space: repeats darken, width -11 dB or wider, no mono part */
    {
        int v = PASS, vw = R.side_mid_db >= -11.0 ? PASS : FAIL, vh;
        if (!isfinite(R.hf_drop_db)) vh = UNSURE; else vh = R.hf_drop_db < -0.5 ? PASS : R.hf_drop_db > 0.5 ? FAIL : UNSURE;
        v = worst(vw, vh);
        say(&r[4], v, "side/mid %.1f dB (-11 or wider), L/R correlation %.2f; HF change per echo repeat %s%.1f dB (must fall); echo / reverb ranges and a mono part are not measurable on the mix%s",
            R.side_mid_db, R.lr_corr, isfinite(R.hf_drop_db) ? "" : "n/a ", isfinite(R.hf_drop_db) ? R.hf_drop_db : 0.0, isfinite(R.hf_drop_db) ? "" : " (render a hit followed by 4 s of silence to measure repeats)");
    }
    /* 5 movement */
    {
        double e1 = fabs(isfinite(R.env_move_pct[1]) ? R.env_move_pct[1] : 0), e2 = fabs(isfinite(R.env_move_pct[2]) ? R.env_move_pct[2] : 0), em = e1 > e2 ? e1 : e2;
        double co = isfinite(R.centroid_bar_range_oct) ? R.centroid_bar_range_oct : 0, lv = isfinite(R.level_bar_range_db) ? R.level_bar_range_db : 0;
        int v = (co >= 0.1 || lv >= 1.0 || em >= 10.0) ? PASS : (co < 0.03 && lv < 0.3 && em < 3.0) ? FAIL : UNSURE;
        say(&r[5], v, "centroid moves %.2f octaves over the bars, bar level range %.1f dB, tone movement inside a note %.0f %% (p1 %.0f, p2 %.0f); PASS needs 0.1 oct, 1 dB or 10 %%",
            co, lv, em, isfinite(R.env_move_pct[1]) ? R.env_move_pct[1] : 0.0, isfinite(R.env_move_pct[2]) ? R.env_move_pct[2] : 0.0);
    }
    /* 6 timbre: no square lead, no whistle */
    {
        int v = PASS, p; char sq[160] = "";
        v = R.whistle_spike_db <= 12.0 ? PASS : UNSURE;   /* never FAIL on this alone: it flags every harmonic sound (DUB-REVIEW, REVOICE-B, B9-LOWEND reports) */
        for (p = 1; p < SL_NP; p++)
            if (R.has_part[p] && isfinite(R.odd_even_db[p]) && R.odd_even_db[p] > 10.0 && R.lowest_note[p] >= 52) {
                char t[60]; snprintf(t, sizeof t, " square-like p%d (odd/even %.1f dB);", p, R.odd_even_db[p]); strncat(sq, t, sizeof sq - strlen(sq) - 1); v = worst(v, FAIL);
            }
        say(&r[6], v, "narrow peak above 2 kHz %.1f dB over its neighbours (max 12), flagged frames %.0f %%, longest run %.2f s;%s above 12 dB the test cannot tell a held partial from a resonance: UNSURE, a place to listen",
            R.whistle_spike_db, R.whistle_frames_pct, R.whistle_longest_s, sq[0] ? sq : " no square-like lead (odd/even above 10 dB);");
    }
    /* 7 level and headroom */
    {
        int ok = R.lufs >= lo && R.lufs <= hi && R.true_peak_db <= -1.0 && R.clip_count == 0;
        say(&r[7], ok ? PASS : FAIL, "LUFS %.1f (window %.1f .. %.1f), true peak %.1f dBFS (max -1), clipped samples %.0f; this render is morph '%s', End A / middle / End B need a render each",
            R.lufs, lo, hi, R.true_peak_db, R.clip_count, morph[0] ? morph : "?");
    }
    /* 8 low end welcome: only no thump on every beat */
    {
        int k, n75 = 0; double mn = 1e9;
        for (k = 0; k < 4; k++) { if (isfinite(R.low_onsets[k]) && R.low_onsets[k] >= 0.75) n75++; if (isfinite(R.low_onsets[k]) && R.low_onsets[k] < mn) mn = R.low_onsets[k]; }
        say(&r[8], n75 == 4 ? FAIL : n75 == 3 ? UNSURE : PASS, "low band (<120 Hz) starts in %.0f / %.0f / %.0f / %.0f %% of the bars on beats 1-4 (a kick-like thump on every beat would be 75 %%+ on all four); under 150 Hz %.0f %% of the energy, level modulation %.2f",
            100 * R.low_onsets[0], 100 * R.low_onsets[1], 100 * R.low_onsets[2], 100 * R.low_onsets[3], R.bands[4], R.sub_mod);
    }
    /* 9 TIDE-proof: rules 1, 4 and 7 on a render with TIDE moving */
    {
        int tideon = strcmp(tide, "off") && strcmp(tide, "none") && tide[0];
        if (!tideon) say(&r[9], UNSURE, "this render has TIDE %s; render 16 bars with --tide evolve (or travel) under the suggested --style and run rules again", tide[0] ? tide : "off");
        else if (R.bars < 16) say(&r[9], UNSURE, "TIDE %s style '%s' but only %d bars; 16 bars needed", tide, style, R.bars);
        else { int v = worst(worst(r[1].v, r[4].v), r[7].v); say(&r[9], v, "TIDE %s, style '%s', %d bars: rules 1 / 4 / 7 here are %s / %s / %s", tide, style, R.bars, VN[r[1].v], VN[r[4].v], VN[r[7].v]); }
    }
    /* 10 identity */
    say(&r[10], UNSURE, "identity against the category siblings needs the other multis: compare their metrics files");

    jw_init(&out); jw_open(&out, NULL, 0);
    jw_str(&out, "tool", "soundlab rules"); jw_str(&out, "prefix", prefix);
    jw_open(&out, "rules", 1);
    printf("soundlab rules %s  (%.1f s, %d bars, tide %s%s%s)\n", prefix, R.seconds, R.bars, tide, style[0] ? ", style " : "", style);
    for (i = 1; i <= 10; i++) {
        printf("rule %-2d %-6s %s\n", i, VN[r[i].v], r[i].text);
        jw_open(&out, NULL, 0); jw_int(&out, "rule", i); jw_str(&out, "verdict", VN[r[i].v]); jw_str(&out, "numbers", r[i].text); jw_close(&out);
        if (r[i].v == FAIL) nf++; else if (r[i].v == UNSURE) nu++; else np++;
    }
    jw_close(&out);
    jw_int(&out, "pass", np); jw_int(&out, "fail", nf); jw_int(&out, "unsure", nu);
    snprintf(fn, sizeof fn, "%s.rules.json", prefix);
    jw_save(&out, fn);
    printf("%d PASS, %d FAIL, %d UNSURE\n", np, nf, nu);
    sl_json_free(js);
    return nf ? 1 : 0;
}
