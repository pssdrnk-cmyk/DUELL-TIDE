/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab, lane B (measure / rules): shared declarations. Plain C99, no firmware headers. */
#ifndef SL_LAB_H
#define SL_LAB_H
#include <stddef.h>
#include <stdio.h>

/* ---- entry points the soundlab main calls ---- */
int sl_cmd_measure(int argc, char **argv);   /* argv: PREFIX [--from BAR] [--to BAR] [--control CTLPREFIX] [--delay-ms T] */
int sl_cmd_rules(int argc, char **argv);     /* argv: PREFIX [--from BAR] [--to BAR] [--lufs-window LO,HI] */
int sl_measure_prefix(const char *prefix, int from_bar, int to_bar, const char *control);   /* writes PREFIX.metrics.json */
int sl_sweep_table(const char *name, const char *const *prefixes, const double *points, int n);

/* ---- dsp (sl_dsp.c) ---- */
void sl_fft_flush(void);                                             /* drop the cached FFT plans */
void sl_fft(double *re, double *im, size_t n, int inverse);          /* any n; inverse is scaled by 1/n */
void sl_rfft(const double *x, size_t n, double *re, double *im);     /* n/2+1 bins, like numpy rfft */
void sl_rfft_power(const double *x, size_t n, double *pw);           /* |X|^2, n/2+1 bins */
void sl_lowpass_brick(const double *x, size_t n, double fc, int sr, double *out);   /* zero all bins above fc (numpy rfft / irfft) */
double sl_hann(size_t i, size_t n);                                  /* np.hanning */
double sl_percentile(const double *x, size_t n, double q);           /* np.percentile, linear */
double sl_median(const double *x, size_t n);
double sl_db20(double v);                                            /* 20 log10(max(v, 1e-9)) */
double sl_rms(const double *x, size_t n);
double sl_band_pct(const double *x, size_t n, int sr, double lo, double hi);   /* old band_pct: share of |rfft|^2 in [lo, hi) of all f >= 20 */
double sl_hf_db(const double *x, size_t n, int sr);                  /* old hf_db: Hann, energy >= 2 kHz re total, dB */
double sl_lufs(const double *l, const double *r, size_t n, int sr);  /* BS.1770, 400 ms blocks, -70 / -10 gates */
double sl_true_peak(const double *l, const double *r, size_t n);     /* 4x oversampled, linear */

/* ---- wav (sl_dsp.c) ---- */
typedef struct { int sr; size_t n; double *l, *r; } sl_wav;
int sl_wav_read(const char *fn, sl_wav *w);                          /* 16-bit PCM, mono or stereo; samples scaled to +-1 */
void sl_wav_free(sl_wav *w);

/* ---- json (sl_json.c) ---- */
enum { SLJ_NULL, SLJ_BOOL, SLJ_NUM, SLJ_STR, SLJ_ARR, SLJ_OBJ };
typedef struct sl_jv { int type; double num; char *str; struct sl_jv **kid; char **key; int n; } sl_jv;
sl_jv *sl_json_parse(const char *text);
sl_jv *sl_json_load(const char *fn);
const sl_jv *sl_json_get(const sl_jv *o, const char *key);
const sl_jv *sl_json_at(const sl_jv *a, int i);
double sl_json_num(const sl_jv *v, double dflt);
const char *sl_json_str(const sl_jv *v, const char *dflt);
void sl_json_free(sl_jv *v);

typedef struct { char *buf; size_t len, cap; int depth; unsigned char st[40]; } sl_jw;   /* st[d]: bit0 = nothing written yet, bit1 = array */
void jw_init(sl_jw *w);
void jw_open(sl_jw *w, const char *key, int is_array);    /* key NULL inside an array and for the top level */
void jw_close(sl_jw *w);
void jw_num(sl_jw *w, const char *key, double v);          /* NaN / inf -> null */
void jw_int(sl_jw *w, const char *key, long v);
void jw_str(sl_jw *w, const char *key, const char *s);
void jw_bool(sl_jw *w, const char *key, int b);
void jw_null(sl_jw *w, const char *key);
int jw_save(sl_jw *w, const char *fn);                     /* closes every open level, writes, frees */

/* ---- the measured set (sl_metrics.c fills it, sl_rules.c reads it); NaN = not measurable ---- */
#define SL_NP 3                                            /* index 1, 2 = parts 1, 2 (0 unused) */
typedef struct {
    int sr, bars, from_bar, to_bar; double bpm, seconds;
    int has_part[SL_NP];
    double lufs, true_peak_db, sample_peak_db, rms_db, crest_db, dyn_db, silence_audio_pct;
    double clip_count, dc_db;
    double bands[10];          /* lt60 60_150 150_400 lt120 lt150 lt250 lt300 gt2k gt4k gt6k  (% of energy above 20 Hz) */
    double centroid_hz, centroid_bar_min, centroid_bar_max, centroid_bar_range_oct, level_bar_range_db, side_bar_range_db;
    double whistle_spike_db, whistle_frames_pct, whistle_longest_s;
    double side_mid_db, lr_corr;
    double bed_mean[SL_NP], bed_both_mean, bed_both_max, silence_notes_pct;
    double onsets_bar[SL_NP + 1];
    double low_onsets[4], sub_db, sub_mod;
    double odd_even_db[SL_NP]; int odd_even_clean[SL_NP];
    double env_move_pct[SL_NP], lowest_note[SL_NP];
    double attack_ms;
    double rub_pct[2], rub_oct_pct[2], both_pct[2];        /* [0] = REL, [1] = ALL */
    double hf_drop_db, echo_repeats_m40, tail_t40_s, tail_share_pct;
    double note_med[SL_NP], note_p90[SL_NP], note_max[SL_NP];
} sl_result;
int sl_measure_core(const char *prefix, int from_bar, int to_bar, const char *control, double delay_ms, sl_result *res, sl_jw *out);

#endif
