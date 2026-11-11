/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab measure / sweep: command line glue around sl_measure_core. */
#include "sl_lab.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static void summary(const char *prefix, const sl_result *R) {
    printf("%s: %.1f s | LUFS %.1f TP %.1f dBFS | <150 Hz %.0f%% centroid %.0f Hz | side/mid %.1f dB | bed %.1f silence %.1f%% | rub %.1f%% | echo HF %.1f dB/rep\n",
           prefix, R->seconds, R->lufs, R->true_peak_db, R->bands[4], R->centroid_hz, R->side_mid_db, R->bed_both_mean, R->silence_notes_pct, R->rub_pct[1], R->hf_drop_db);
}

int sl_measure_prefix(const char *prefix, int from_bar, int to_bar, const char *control) {
    sl_result R; sl_jw w; char fn[640]; int rc;
    jw_init(&w);
    rc = sl_measure_core(prefix, from_bar, to_bar, control, 0.0, &R, &w);
    if (rc) { free(w.buf); return rc; }
    snprintf(fn, sizeof fn, "%s.metrics.json", prefix);
    if (jw_save(&w, fn)) { fprintf(stderr, "soundlab measure: cannot write %s\n", fn); return -1; }
    return 0;
}

int sl_cmd_measure(int argc, char **argv) {
    const char *prefix = NULL, *control = NULL; int from_bar = 0, to_bar = -1, i, rc; double delay = 0; sl_result R; sl_jw w; char fn[640];
    for (i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "--from") && i + 1 < argc) from_bar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--to") && i + 1 < argc) to_bar = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--control") && i + 1 < argc) control = argv[++i];
        else if (!strcmp(argv[i], "--delay-ms") && i + 1 < argc) delay = atof(argv[++i]);
        else if (argv[i][0] != '-' && !prefix) prefix = argv[i];
        else { fprintf(stderr, "soundlab measure: unknown argument %s\nusage: soundlab measure PREFIX [--from BAR --to BAR] [--control CTLPREFIX] [--delay-ms T]\n", argv[i]); return 2; }
    }
    if (!prefix) { fprintf(stderr, "usage: soundlab measure PREFIX [--from BAR --to BAR] [--control CTLPREFIX] [--delay-ms T]\n"); return 2; }
    jw_init(&w);
    rc = sl_measure_core(prefix, from_bar, to_bar, control, delay, &R, &w);
    if (rc) { free(w.buf); return 1; }
    snprintf(fn, sizeof fn, "%s.metrics.json", prefix);
    if (jw_save(&w, fn)) { fprintf(stderr, "soundlab measure: cannot write %s\n", fn); return 1; }
    summary(prefix, &R);
    return 0;
}

/* ---- sweep table ---- */
static const sl_jv *by_path(const sl_jv *root, const char *path) {
    char part[96]; const sl_jv *c = root; const char *p = path;
    while (c && *p) {
        size_t n = 0;
        while (p[n] && p[n] != '.') n++;
        if (n >= sizeof part) return NULL;
        memcpy(part, p, n); part[n] = 0;
        if (c->type == SLJ_ARR) c = sl_json_at(c, atoi(part)); else c = sl_json_get(c, part);
        p += n; if (*p == '.') p++;
    }
    return c;
}

static const struct { const char *path, *label; } SWEEP_ROWS[] = {
    { "level.lufs", "LUFS" }, { "level.true_peak_dbfs", "true peak dBFS" }, { "level.rms_dbfs", "RMS dBFS" },
    { "spectrum.bands_pct.lt60", "<60 Hz %" }, { "spectrum.bands_pct.60_150", "60-150 Hz %" }, { "spectrum.bands_pct.150_400", "150-400 Hz %" },
    { "spectrum.bands_pct.gt2k", ">2 kHz %" }, { "spectrum.bands_pct.gt4k", ">4 kHz %" }, { "spectrum.centroid_hz", "centroid Hz" },
    { "spectrum.centroid_bar_range_oct", "centroid range over bars (oct)" }, { "spectrum.whistle.spike_db", "whistle spike dB" },
    { "time.bed.both_mean", "notes sounding (mean)" }, { "time.silence_notes_pct", "no note sounding %" }, { "time.onsets_per_bar.all", "onsets a bar" },
    { "time.low_onsets_per_beat.0", "low onset beat 1" }, { "time.low_onsets_per_beat.1", "low onset beat 2" },
    { "space.side_mid_db", "side/mid dB" }, { "space.lr_correlation", "L/R correlation" }, { "space.after_noteoff.energy_pct", "energy after note-offs %" },
    { "space.echo.hf_drop_db_per_repeat", "echo HF drop dB/repeat" }, { "space.echo.repeats_above_minus40db", "echo repeats > -40 dB" },
    { "space.echo.time_to_minus40db_s", "echo time to -40 dB s" }, { "pitch.rubs_ALL.minor2_or_9_pct", "rub % (ALL)" },
    { "quality.second_difference_click_db", "2nd-difference click dB" }, { "quality.end_cut_dbfs", "end cut dBFS" },
};

int sl_sweep_table(const char *name, const char *const *prefixes, const double *points, int n) {
    sl_jv **js = (sl_jv **)calloc((size_t)n, sizeof(sl_jv *)); int i, r, zero = 0, nrows = (int)(sizeof SWEEP_ROWS / sizeof SWEEP_ROWS[0]); char fn[640]; sl_jw w;
    for (i = 0; i < n; i++) {
        snprintf(fn, sizeof fn, "%s.metrics.json", prefixes[i]);
        js[i] = sl_json_load(fn);
        if (!js[i]) { fprintf(stderr, "soundlab sweep: cannot read %s\n", fn); }
        if (points[i] == 0.0) zero = i;
    }
    printf("sweep %s (delta vs the point %+g)\n\n", name ? name : "", points[zero]);
    printf("| metric |");
    for (i = 0; i < n; i++) printf(" %+g |", points[i]);
    printf("\n|---|");
    for (i = 0; i < n; i++) printf("---|");
    printf("\n");
    for (r = 0; r < nrows; r++) {
        printf("| %s |", SWEEP_ROWS[r].label);
        for (i = 0; i < n; i++) {
            const sl_jv *v = js[i] ? by_path(js[i], SWEEP_ROWS[r].path) : NULL, *v0 = js[zero] ? by_path(js[zero], SWEEP_ROWS[r].path) : NULL;
            if (!v || v->type != SLJ_NUM) printf(" - |");
            else if (i == zero || !v0 || v0->type != SLJ_NUM) printf(" %.4g |", v->num);
            else printf(" %.4g (%+.3g) |", v->num, v->num - v0->num);
        }
        printf("\n");
    }
    if (name && name[0]) {
        jw_init(&w); jw_open(&w, NULL, 0);
        jw_str(&w, "tool", "soundlab sweep"); jw_str(&w, "name", name);
        jw_open(&w, "points", 1); for (i = 0; i < n; i++) jw_num(&w, NULL, points[i]); jw_close(&w);
        jw_open(&w, "metrics", 0);
        for (r = 0; r < nrows; r++) {
            jw_open(&w, SWEEP_ROWS[r].path, 1);
            for (i = 0; i < n; i++) { const sl_jv *v = js[i] ? by_path(js[i], SWEEP_ROWS[r].path) : NULL; if (v && v->type == SLJ_NUM) jw_num(&w, NULL, v->num); else jw_null(&w, NULL); }
            jw_close(&w);
        }
        jw_close(&w);
        snprintf(fn, sizeof fn, "%s.sweep.json", name);
        jw_save(&w, fn);
    }
    for (i = 0; i < n; i++) sl_json_free(js[i]);
    free(js);
    return 0;
}
