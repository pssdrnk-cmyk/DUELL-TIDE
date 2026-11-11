/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab measure: PREFIX.wav + PREFIX.json -> PREFIX.metrics.json.
 * Every definition is a port of an older numpy script so earlier reports stay comparable; the source is named at each block:
 *   DM = work/b9/dub/dubmetrics.py   SP = work/b9/dub/space.py   RU = work/b9/dub/rules.py   M18 = work/b9/sound/measure18.py
 *   RB = work/b9/revoice/B/{metrics,rules}.py   AN = work/reel-2026-11-10/sound-check/tools/analyse.py
 *   QU = work/audit/sound-vs-base-2026-11-09/tools/quality.py   CK = work/b9/knobs/click/analyse_click.py
 * The old scripts measured stems (a part alone, dry, the delay return); soundlab has the mix and the voice log. Where a figure is
 * taken from the mix the "notes" array of the JSON says so. */
#include "sl_int.h"
#include <stdlib.h>
#include <string.h>

#define NA NAN
#define FILE_VERSION 1

static double med(const double *a, int n) { return n ? sl_median(a, (size_t)n) : NA; }
static double dbv(double x) { return sl_db20(x); }
static double meanv(const double *a, long n) { long i; double s = 0; if (!n) return NA; for (i = 0; i < n; i++) s += a[i]; return s / (double)n; }
static double fbin(long k, long n, int sr) { return (double)k / ((double)n * (1.0 / (double)sr)); }   /* np.fft.rfftfreq */

/* rms of consecutive frames of fr samples (DM env, SP frames, AN rms_frames without the 1e-20) */
static double *env_frames(const double *x, long n, long fr, long *nf) {
    long k = n / fr, i; double *e = (double *)malloc(sizeof(double) * (size_t)(k + 1));
    for (i = 0; i < k; i++) e[i] = sl_rms(x + i * fr, (size_t)fr);
    *nf = k; return e;
}

/* SP sm_db(): side / mid energy in dB and the L/R correlation (np.corrcoef; 1.0 for a silent signal) */
static void sm_db(const double *l, const double *r, long n, double *smo, double *corr) {
    long i; double es = 0, em = 0;
    for (i = 0; i < n; i++) { double mm = (l[i] + r[i]) / 2, ss = (l[i] - r[i]) / 2; em += mm * mm; es += ss * ss; }
    *smo = 10.0 * log10((es + 1e-30) / (em + 1e-30));
    if (corr) {
        double ml = 0, mr = 0, sl = 0, srr = 0, sx = 0, mu = 0, var = 0;
        for (i = 0; i < n; i++) { ml += l[i]; mr += r[i]; }
        ml /= (double)n; mr /= (double)n; mu = (ml + mr) / 2;
        for (i = 0; i < n; i++) { sl += (l[i] - ml) * (r[i] - mr); srr += (l[i] - ml) * (l[i] - ml); sx += (r[i] - mr) * (r[i] - mr); var += (l[i] - mu) * (l[i] - mu) + (r[i] - mu) * (r[i] - mu); }
        var = sqrt(var / (2.0 * (double)n));      /* x.std() of the 2-column array */
        *corr = (var > 1e-9 && srr > 0 && sx > 0) ? sl / sqrt(srr * sx) : 1.0;
    }
}

/* ---- loading ---- */
static int path_with(char *b, size_t cap, const char *prefix, const char *ext) { return snprintf(b, cap, "%s%s", prefix, ext) < (int)cap ? 0 : -1; }

typedef struct { double t; long smp; int has_smp; } fxev;

typedef struct {
    sl_wav w; sl_jv *js;
    long *bsfull; int bars; double bpm;
    long a, b;                 /* range in the file */
    sig s;
    fxev fx[16]; int nfx;
    char control[600];
    char name[160]; long multi;
} job;

static long rnd(double v) { return (long)floor(v + 0.5); }

static int job_load(job *j, const char *prefix, int from_bar, int to_bar) {
    char fn[600]; int i; const sl_jv *v, *vs, *ev;
    memset(j, 0, sizeof *j);
    path_with(fn, sizeof fn, prefix, ".wav");
    if (sl_wav_read(fn, &j->w)) { fprintf(stderr, "soundlab measure: cannot read %s\n", fn); return -1; }
    path_with(fn, sizeof fn, prefix, ".json");
    j->js = sl_json_load(fn);
    if (!j->js) { fprintf(stderr, "soundlab measure: cannot read %s\n", fn); return -1; }
    j->bpm = sl_json_num(sl_json_get(j->js, "bpm"), 120.0);
    {
        double barlen = 240.0 / j->bpm * j->w.sr;
        const sl_jv *bsj = sl_json_get(j->js, "bar_starts");
        j->bars = (int)sl_json_num(sl_json_get(j->js, "bars"), floor((double)j->w.n / barlen));
        if (j->bars < 1) j->bars = 1;
        j->bsfull = (long *)malloc(sizeof(long) * (size_t)(j->bars + 2));
        for (i = 0; i <= j->bars; i++) {
            const sl_jv *e = sl_json_at(bsj, i);
            j->bsfull[i] = e ? rnd(sl_json_num(e, 0) * j->w.sr) : rnd(i * barlen);
        }
    }
    if (from_bar < 0) from_bar = 0;
    if (from_bar >= j->bars) from_bar = j->bars - 1;
    j->a = j->bsfull[from_bar];
    j->b = (to_bar < 0 || to_bar > j->bars) ? (long)j->w.n : j->bsfull[to_bar];
    if (j->b > (long)j->w.n) j->b = (long)j->w.n;
    if (j->b - j->a < 64) { fprintf(stderr, "soundlab measure: range too short\n"); return -1; }
    j->s.sr = j->w.sr; j->s.n = j->b - j->a; j->s.bpm = j->bpm;
    j->s.l = j->w.l + j->a; j->s.r = j->w.r + j->a;
    j->s.m = (double *)malloc(sizeof(double) * (size_t)j->s.n);
    for (i = 0; i < j->s.n; i++) j->s.m[i] = (j->s.l[i] + j->s.r[i]) / 2.0;
    {   /* bars inside the range */
        int k, e = (to_bar < 0 || to_bar > j->bars) ? j->bars : to_bar;
        j->s.bs = (long *)malloc(sizeof(long) * (size_t)(j->bars + 1));
        for (k = from_bar; k < e; k++) if (j->bsfull[k] - j->a < j->s.n) j->s.bs[j->s.nbar++] = j->bsfull[k] - j->a;
    }
    vs = sl_json_get(j->js, "voices");
    j->s.v = (voice *)malloc(sizeof(voice) * (size_t)((vs ? vs->n : 0) + 1));
    for (i = 0; vs && i < vs->n; i++) {
        const sl_jv *o = vs->kid[i]; voice vv; const sl_jv *te = sl_json_get(o, "t_end");
        vv.s0 = rnd(sl_json_num(sl_json_get(o, "t0"), 0) * j->w.sr);
        vv.s1 = rnd(sl_json_num(sl_json_get(o, "t1"), sl_json_num(sl_json_get(o, "t0"), 0)) * j->w.sr);
        vv.se = te ? rnd(sl_json_num(te, 0) * j->w.sr) : vv.s1;
        if (te) j->s.has_end = 1;
        else if (sl_json_get(o, "tr")) { vv.se = vv.s1; vv.s1 = rnd(sl_json_num(sl_json_get(o, "tr"), 0) * j->w.sr); j->s.has_end = 1; }   /* an early renderer's key-off "tr" with t1 = voice end */
        vv.part = (int)sl_json_num(sl_json_get(o, "part"), 1); vv.note = (int)sl_json_num(sl_json_get(o, "note"), 60); vv.vel = (int)sl_json_num(sl_json_get(o, "vel"), 100);
        if (vv.s0 >= j->b || (vv.se > vv.s1 ? vv.se : vv.s1) <= j->a) continue;   /* keep voices that sound inside the range; onsets before it are skipped by the onset statistics */
        vv.s0 -= j->a; vv.s1 -= j->a; vv.se -= j->a;
        if (vv.s1 < vv.s0) vv.s1 = vv.s0;
        if (vv.se < vv.s1) vv.se = vv.s1;
        j->s.v[j->s.nv++] = vv;
    }
    ev = sl_json_get(j->js, "events");
    for (i = 0; ev && i < ev->n && j->nfx < 16; i++) {
        const sl_jv *o = ev->kid[i]; const char *kind = sl_json_str(sl_json_get(o, "kind"), "");
        if (strstr(kind, "time") || !strcmp(kind, "fxstep") || !strcmp(kind, "fx_time")) {
            const sl_jv *sm = sl_json_get(o, "sample");
            j->fx[j->nfx].t = sl_json_num(sl_json_get(o, "t"), 0);
            j->fx[j->nfx].has_smp = sm != NULL; j->fx[j->nfx].smp = sm ? (long)sl_json_num(sm, 0) : rnd(j->fx[j->nfx].t * j->w.sr);
            j->nfx++;
        }
    }
    v = sl_json_get(j->js, "control");
    if (v && sl_json_str(v, NULL)) snprintf(j->control, sizeof j->control, "%s", sl_json_str(v, ""));
    v = sl_json_get(sl_json_get(j->js, "source"), "name"); snprintf(j->name, sizeof j->name, "%s", sl_json_str(v, ""));
    j->multi = (long)sl_json_num(sl_json_get(sl_json_get(j->js, "source"), "multi"), -1);
    return 0;
}

static void job_free(job *j) {
    sl_wav_free(&j->w); sl_json_free(j->js); free(j->bsfull); free(j->s.m); free(j->s.bs); free(j->s.v);
}

/* ---- the whistle test: two definitions, kept apart (RU spike(), RB hf()) ---- */
static double whistle_spike(const double *x, long n, int sr) {
    const long N = 32768; double best = 0; long s, k, i; int h = (int)(N / 2 + 1);
    double *w = (double *)malloc(sizeof(double) * (size_t)N), *re = (double *)malloc(sizeof(double) * (size_t)h), *im = (double *)malloc(sizeof(double) * (size_t)h), *X = (double *)malloc(sizeof(double) * (size_t)h);
    for (s = 0; s < n - N; s += N) {
        int cnt = 0;
        for (i = 0; i < N; i++) w[i] = x[s + i] * sl_hann((size_t)i, (size_t)N);
        sl_rfft(w, (size_t)N, re, im);
        for (i = 0; i < h; i++) X[i] = 20.0 * log10(sqrt(re[i] * re[i] + im[i] * im[i]) + 1e-9);
        for (k = 0; k < h; k++) {
            double f = fbin(k, N, sr), nb[80]; int m = 0; long q;
            if (!(f > 2000 && f < 12000)) continue;
            if ((cnt++ % 7) != 0) continue;
            if (k - 40 < 0 || k + 41 > h) continue;
            for (q = k - 40; q < k - 3; q++) nb[m++] = X[q];
            for (q = k + 4; q < k + 41; q++) nb[m++] = X[q];
            { double d = X[k] - sl_median(nb, (size_t)m); if (d > best) best = d; }
        }
    }
    free(w); free(re); free(im); free(X); return best;
}
static void whistle_frames(const double *x, long n, int sr, double *pct, double *longest) {
    const long N = 4096; long i, nfr = 0, flagged = 0, run = 0, best = 0, k; int h = (int)(N / 2 + 1);
    double *w = (double *)malloc(sizeof(double) * (size_t)N), *re = (double *)malloc(sizeof(double) * (size_t)h), *im = (double *)malloc(sizeof(double) * (size_t)h), *S = (double *)malloc(sizeof(double) * (size_t)h);
    int *idx = (int *)malloc(sizeof(int) * (size_t)h), ni = 0;
    for (k = 0; k < h; k++) { double f = fbin(k, N, sr); if (f > 2000 && f < 12000) idx[ni++] = (int)k; }
    for (i = 0; i < n - N; i += 1024) {
        double sall = -1e9, smax = -1e9, nb[80]; int kk = 0, m = 0, lo, hi, q, fl = 0;
        for (k = 0; k < N; k++) w[k] = x[i + k] * sl_hann((size_t)k, (size_t)N);
        sl_rfft(w, (size_t)N, re, im);
        for (k = 0; k < h; k++) { S[k] = 20.0 * log10(sqrt(re[k] * re[k] + im[k] * im[k]) + 1e-9); if (S[k] > sall) sall = S[k]; }
        for (q = 0; q < ni; q++) if (S[idx[q]] > smax) { smax = S[idx[q]]; kk = q; }
        lo = kk - 28 > 0 ? kk - 28 : 0; hi = kk + 29 < ni ? kk + 29 : ni;
        for (q = lo; q < (kk - 3 > lo ? kk - 3 : lo); q++) nb[m++] = S[idx[q]];
        for (q = kk + 4; q < hi; q++) nb[m++] = S[idx[q]];
        if (m && smax > sall - 25 && smax - sl_median(nb, (size_t)m) > 12 && sall > -30) fl = 1;
        nfr++; flagged += fl;
        if (fl) { run++; if (run > best) best = run; } else run = 0;
    }
    *pct = nfr ? 100.0 * (double)flagged / (double)nfr : NA;
    *longest = (double)best * 1024.0 / sr;
    free(w); free(re); free(im); free(S); free(idx);
}

/* ---- sweep over the hits of one part (DM hits / stab_* / SP gaps / tail_share) ---- */
typedef struct { long t; } hit;
static int hits_of(const sig *s, int part, long tol, hit *h) {
    voice *a; int k = sl_part_voices(s, part, &a), i, nh = 0;
    for (i = 0; i < k; i++) {
        if (nh && a[i].s0 - h[nh - 1].t < tol) continue;
        h[nh++].t = a[i].s0;
    }
    free(a); return nh;
}

int sl_measure_core(const char *prefix, int from_bar, int to_bar, const char *control, double delay_ms, sl_result *R, sl_jw *W) {
    job J; sig *s; int sr, i, p; long n, k; sl_jw *w = W;
    double *pw, tot20 = 0, totall = 0, cent_num = 0, sallpw;
    double bandlo[10] = { 20, 60, 150, 20, 20, 20, 20, 2000, 4000, 6000 }, bandhi[10] = { 60, 150, 400, 120, 150, 250, 300, 1e9, 1e9, 22000 };
    const char *bname[10] = { "lt60", "60_150", "150_400", "lt120", "lt150", "lt250", "lt300", "gt2k", "gt4k", "gt6k" };
    double reel_e[6] = { 0, 120, 500, 2000, 6000, 30000 }, reel_sh[5];
    partm PM[SL_NP]; bedm B; rubm RUB[2];
    memset(R, 0, sizeof *R);
    if (job_load(&J, prefix, from_bar, to_bar)) return -1;
    s = &J.s; sr = s->sr; n = s->n;
    R->sr = sr; R->bars = J.bars; R->bpm = J.bpm; R->seconds = (double)n / sr; R->from_bar = from_bar < 0 ? 0 : from_bar; R->to_bar = to_bar;
    for (p = 1; p < SL_NP; p++) for (i = 0; i < s->nv; i++) if (s->v[i].part == p) R->has_part[p] = 1;

    jw_open(w, NULL, 0);
    jw_str(w, "tool", "soundlab"); jw_int(w, "version", FILE_VERSION);
    jw_str(w, "prefix", prefix);
    jw_str(w, "name", J.name); jw_int(w, "multi", J.multi);
    jw_int(w, "sample_rate", sr); jw_num(w, "bpm", J.bpm);
    jw_open(w, "range", 0); jw_int(w, "from_bar", R->from_bar); jw_int(w, "to_bar", to_bar < 0 ? J.bars : to_bar);
    jw_num(w, "start_s", (double)J.a / sr); jw_num(w, "seconds", R->seconds); jw_int(w, "bars", s->nbar); { int cnt = 0; for (i = 0; i < s->nv; i++) if (s->v[i].s0 >= 0) cnt++; jw_int(w, "voices", cnt); } jw_close(w);

    /* ================= level ================= */
    {
        double pk = 0, e2 = 0, tp, sdc[2] = { 0, 0 }; long clip = 0, fr = (long)(0.02 * sr + 0.5), nf; double *e, p95, p10; int sil = 0;
        for (k = 0; k < n; k++) {
            double a = fabs(s->l[k]), b = fabs(s->r[k]);
            if (a > pk) pk = a;
            if (b > pk) pk = b;
            e2 += s->l[k] * s->l[k] + s->r[k] * s->r[k];
            if (a * 32768.0 >= 32766.5) clip++;
            if (b * 32768.0 >= 32766.5) clip++;
            sdc[0] += s->l[k]; sdc[1] += s->r[k];
        }
        R->lufs = sl_lufs(s->l, s->r, (size_t)n, sr);
        tp = sl_true_peak(s->l, s->r, (size_t)n);
        R->true_peak_db = dbv(tp); R->sample_peak_db = 20.0 * log10(pk + 1e-12); R->rms_db = 10.0 * log10(e2 / (2.0 * n) + 1e-20);
        R->crest_db = R->sample_peak_db - R->rms_db; R->clip_count = (double)clip;
        R->dc_db = 20.0 * log10((fabs(sdc[0]) > fabs(sdc[1]) ? fabs(sdc[0]) : fabs(sdc[1])) / n + 1e-12);
        e = env_frames(s->m, n, fr, &nf);       /* AN rms_frames: sqrt(mean + 1e-20) */
        for (k = 0; k < nf; k++) e[k] = sqrt(e[k] * e[k] + 1e-20);
        p95 = sl_percentile(e, (size_t)nf, 95); p10 = sl_percentile(e, (size_t)nf, 10);
        R->dyn_db = 20.0 * log10(p95 / (p10 + 1e-9));
        for (k = 0; k < nf; k++) if (e[k] < p95 * pow(10.0, -30.0 / 20.0)) sil++;
        R->silence_audio_pct = nf ? 100.0 * sil / (double)nf : NA;
        free(e);
        jw_open(w, "level", 0);
        jw_num(w, "lufs", R->lufs); jw_num(w, "true_peak_dbfs", R->true_peak_db); jw_num(w, "sample_peak_dbfs", R->sample_peak_db); jw_num(w, "rms_dbfs", R->rms_db);
        jw_num(w, "crest_db", R->crest_db); jw_num(w, "dynamic_range_db", R->dyn_db); jw_num(w, "silence_audio_pct", R->silence_audio_pct);
        jw_int(w, "clipped_samples", clip); jw_num(w, "dc_dbfs", R->dc_db);
        jw_close(w);
    }

    /* ================= spectrum: the whole range, mono ================= */
    pw = (double *)malloc(sizeof(double) * (size_t)(n / 2 + 1));
    sl_rfft_power(s->m, (size_t)n, pw);
    {
        double sb[10];
        memset(sb, 0, sizeof sb); memset(reel_sh, 0, sizeof reel_sh);
        for (k = 0; k <= n / 2; k++) {
            double f = fbin(k, n, sr); int b, q;
            totall += pw[k]; cent_num += f * pw[k];
            if (f >= 20) tot20 += pw[k];
            for (b = 0; b < 10; b++) if (f >= bandlo[b] && f < bandhi[b]) sb[b] += pw[k];
            for (q = 0; q < 5; q++) if (f >= reel_e[q] && f < reel_e[q + 1]) reel_sh[q] += pw[k];
        }
        for (i = 0; i < 10; i++) R->bands[i] = 100.0 * sb[i] / (tot20 + 1e-30);
        for (i = 0; i < 5; i++) reel_sh[i] = 100.0 * reel_sh[i] / (totall + 1e-300);
        R->centroid_hz = cent_num / totall;
    }
    sallpw = totall; (void)sallpw;
    free(pw);
    sl_fft_flush();
    jw_open(w, "spectrum", 0);
    jw_open(w, "bands_pct", 0);
    for (i = 0; i < 10; i++) jw_num(w, bname[i], R->bands[i]);
    jw_close(w);
    jw_open(w, "reel_bands_pct", 0);
    { const char *rn[5] = { "lt120", "120_500", "500_2k", "2k_6k", "gt6k" }; for (i = 0; i < 5; i++) jw_num(w, rn[i], reel_sh[i]); }
    jw_close(w);
    jw_num(w, "centroid_hz", R->centroid_hz); jw_num(w, "centroid_oct_re_c4", log2(R->centroid_hz / 261.6255653));

    /* per-bar series (DM bars_trace, SP drift): centroid (Hann, f >= 20), level and side / mid of the bar */
    {
        double *cc = (double *)malloc(sizeof(double) * (size_t)(s->nbar + 1)), *lv = (double *)malloc(sizeof(double) * (size_t)(s->nbar + 1)), *sm = (double *)malloc(sizeof(double) * (size_t)(s->nbar + 1));
        int b; double cmin = 1e30, cmax = 0, lmin = 1e30, lmax = -1e30, smin = 1e30, smax = -1e30; int ok_c = 0, ok_s = 0;
        for (b = 0; b < s->nbar; b++) {
            long a0 = s->bs[b], e1 = b + 1 < s->nbar ? s->bs[b + 1] : n, len = e1 - a0;
            cc[b] = lv[b] = sm[b] = NA;
            if (len < 4096) continue;
            {
                double *ww = (double *)malloc(sizeof(double) * (size_t)len), *pp = (double *)malloc(sizeof(double) * (size_t)(len / 2 + 1)), num = 0, den = 0; long q;
                for (q = 0; q < len; q++) ww[q] = s->m[a0 + q] * sl_hann((size_t)q, (size_t)len);
                sl_rfft_power(ww, (size_t)len, pp);
                for (q = 0; q <= len / 2; q++) { double f = fbin(q, len, sr); if (f >= 20) { num += f * pp[q]; den += pp[q]; } }
                cc[b] = num / (den + 1e-30); lv[b] = dbv(sl_rms(s->m + a0, (size_t)len));
                if (cc[b] < cmin) cmin = cc[b];
                if (cc[b] > cmax) cmax = cc[b];
                if (lv[b] < lmin) lmin = lv[b];
                if (lv[b] > lmax) lmax = lv[b];
                ok_c++;
                free(ww); free(pp);
            }
            if (len > 8192) {
                double c; sm_db(s->l + a0, s->r + a0, len, &sm[b], &c);
                if (sm[b] < smin) smin = sm[b];
                if (sm[b] > smax) smax = sm[b];
                ok_s++;
            }
        }
        R->centroid_bar_min = ok_c ? cmin : NA; R->centroid_bar_max = ok_c ? cmax : NA;
        R->centroid_bar_range_oct = ok_c ? log2(cmax / cmin) : NA;
        R->level_bar_range_db = ok_c ? lmax - lmin : NA; R->side_bar_range_db = ok_s ? smax - smin : NA;
        jw_num(w, "centroid_bar_min_hz", R->centroid_bar_min); jw_num(w, "centroid_bar_max_hz", R->centroid_bar_max);
        jw_num(w, "centroid_bar_range_oct", R->centroid_bar_range_oct);
        jw_num(w, "level_bar_range_db", R->level_bar_range_db); jw_num(w, "side_mid_bar_range_db", R->side_bar_range_db);
        jw_open(w, "per_bar", 0);
        jw_open(w, "centroid_hz", 1); for (b = 0; b < s->nbar; b++) jw_num(w, NULL, cc[b]); jw_close(w);
        jw_open(w, "level_dbfs", 1); for (b = 0; b < s->nbar; b++) jw_num(w, NULL, lv[b]); jw_close(w);
        jw_open(w, "side_mid_db", 1); for (b = 0; b < s->nbar; b++) jw_num(w, NULL, sm[b]); jw_close(w);
        jw_close(w);
        free(cc); free(lv); free(sm);
    }
    /* whistle test: cannot tell held partials from resonance */
    R->whistle_spike_db = whistle_spike(s->m, n, sr);
    whistle_frames(s->m, n, sr, &R->whistle_frames_pct, &R->whistle_longest_s);
    jw_open(w, "whistle", 0);
    jw_num(w, "spike_db", R->whistle_spike_db); jw_num(w, "narrow_frames_pct", R->whistle_frames_pct); jw_num(w, "narrow_longest_s", R->whistle_longest_s);
    jw_str(w, "caveat", "a narrow peak above 2 kHz is not told apart as resonance or a held partial");
    jw_close(w);

    /* ================= voice-log parts ================= */
    for (p = 1; p < SL_NP; p++) sl_part_metrics(s, p, &PM[p]);
    sl_bed(s, &B);
    sl_rubs(s, 0, &RUB[0]); sl_rubs(s, 1, &RUB[1]);
    for (p = 1; p < SL_NP; p++) {
        R->bed_mean[p] = B.mean[p]; R->onsets_bar[p] = PM[p].onsets_bar; R->odd_even_db[p] = PM[p].oddeven_db; R->odd_even_clean[p] = PM[p].timbre_clean;
        R->env_move_pct[p] = PM[p].env_move_pct; R->lowest_note[p] = PM[p].nnotes ? PM[p].low : NA;
        R->note_med[p] = PM[p].note_med; R->note_p90[p] = PM[p].note_p90; R->note_max[p] = PM[p].nnotes ? PM[p].high : NA;
    }
    R->onsets_bar[0] = NA;
    R->bed_both_mean = B.both_mean; R->bed_both_max = B.both_max; R->silence_notes_pct = B.both_sil;
    for (i = 0; i < 2; i++) { R->rub_pct[i] = RUB[i].rub12_pct; R->rub_oct_pct[i] = RUB[i].rub_oct_pct; R->both_pct[i] = RUB[i].both_pct; }

    jw_close(w);   /* end spectrum */

    /* ================= time ================= */
    jw_open(w, "time", 0);
    jw_num(w, "silence_audio_pct", R->silence_audio_pct);
    jw_num(w, "silence_notes_pct", B.both_sil);
    jw_bool(w, "voice_end_known", s->has_end);
    jw_open(w, "bed", 0);
    jw_num(w, "p1_mean", B.mean[1]); jw_num(w, "p2_mean", B.mean[2]); jw_num(w, "both_mean", B.both_mean); jw_num(w, "both_max", B.both_max);
    jw_num(w, "p1_max", B.maxc[1]); jw_num(w, "p2_max", B.maxc[2]); jw_num(w, "p1_silence_pct", B.sil[1]); jw_num(w, "p2_silence_pct", B.sil[2]); jw_num(w, "both_silence_pct", B.both_sil);
    for (p = 1; p < SL_NP; p++) {
        char key[24]; int q;
        snprintf(key, sizeof key, "fullest_set_p%d", p);
        jw_open(w, key, 1); for (q = 0; q < B.nfull[p]; q++) jw_int(w, NULL, B.fullest[p][q]); jw_close(w);
    }
    jw_close(w);
    /* low-band onsets per beat (M18 low_onsets) and the sub level (RU) */
    {
        double *lo = (double *)malloc(sizeof(double) * (size_t)n); long wl = (long)(0.1 * sr + 0.5); int hitc[4] = { 0, 0, 0, 0 }, nb = 0, b, q;
        sl_lowpass_brick(s->m, (size_t)n, 120.0, sr, lo);
        for (b = 0; b < s->nbar; b++) {
            long a0 = s->bs[b], e1 = b + 1 < s->nbar ? s->bs[b + 1] : n;
            nb++;
            for (q = 0; q < 4; q++) {
                long t = a0 + (e1 - a0) * q / 4; double pre, post;
                if (t - wl < 0 || t + wl > n) continue;
                pre = dbv(sl_rms(lo + t - wl, (size_t)wl)); post = dbv(sl_rms(lo + t, (size_t)wl));
                if (post > -45 && post - pre >= 6) hitc[q]++;
            }
        }
        for (q = 0; q < 4; q++) R->low_onsets[q] = nb ? (double)hitc[q] / nb : NA;
        jw_open(w, "low_onsets_per_beat", 1); for (q = 0; q < 4; q++) jw_num(w, NULL, R->low_onsets[q]); jw_close(w);
        sl_lowpass_brick(s->m, (size_t)n, 100.0, sr, lo);
        {
            long fl = sr / 10, nf = 0, f0; double *e = (double *)malloc(sizeof(double) * (size_t)(n / fl + 2));
            for (f0 = 0; f0 < n - fl; f0 += fl) e[nf++] = sl_rms(lo + f0, (size_t)fl);
            R->sub_db = dbv(sl_rms(lo, (size_t)n));
            R->sub_mod = NA;
            if (nf) { double mu = meanv(e, nf), v = 0; long z; for (z = 0; z < nf; z++) v += (e[z] - mu) * (e[z] - mu); R->sub_mod = sqrt(v / (double)nf) / (mu + 1e-12); }
            free(e);
        }
        jw_open(w, "sub", 0); jw_num(w, "level_dbfs", R->sub_db); jw_num(w, "mod_depth_std_over_mean", R->sub_mod); jw_close(w);
        free(lo);
        sl_fft_flush();
    }
    {   /* onsets a bar */
        double all = 0;
        jw_open(w, "onsets_per_bar", 0);
        for (p = 1; p < SL_NP; p++) { char key[8]; snprintf(key, sizeof key, "p%d", p); jw_num(w, key, PM[p].onsets_bar); if (isfinite(PM[p].onsets_bar)) all += PM[p].onsets_bar; }
        R->onsets_bar[0] = all; jw_num(w, "all", all);
        jw_close(w);
    }
    jw_close(w);   /* time */

    /* ================= parts ================= */
    jw_open(w, "parts", 0);
    for (p = 1; p < SL_NP; p++) {
        char key[8]; partm *o = &PM[p];
        if (!o->nnotes) continue;
        snprintf(key, sizeof key, "p%d", p);
        jw_open(w, key, 0);
        jw_int(w, "notes", o->nnotes); jw_int(w, "onsets", o->ngroups);
        jw_num(w, "onsets_per_bar", o->onsets_bar); jw_num(w, "notes_per_onset", o->notes_onset);
        jw_num(w, "note_len_median_ms", o->note_len_med_ms);
        jw_num(w, "gate_median_16ths", o->gate_med_steps); jw_num(w, "sound_median_16ths", o->sound_med_steps);
        jw_num(w, "blip_pct", o->blip_pct); jw_num(w, "gap_fill_median", o->gapfill_med);
        jw_num(w, "share_on_offbeat_16ths_pct", o->share_16th_off); jw_num(w, "ioi_1_16th_pct", o->ioi1_share); jw_num(w, "swing_pct", o->swing_pct);
        jw_num(w, "vel_min", o->vel_min); jw_num(w, "vel_max", o->vel_max); jw_num(w, "vel_sd", o->vel_sd); jw_num(w, "vel_distinct", o->vel_distinct);
        jw_num(w, "jump_octave_pct", o->jump_oct_pct); jw_num(w, "jump_fifth_pct", o->jump_5th_pct); jw_num(w, "jump_mean_semitones", o->jump_mean);
        jw_num(w, "attack_ms", o->attack_ms);
        jw_num(w, "harmonics_within_30db", o->h30); jw_num(w, "odd_even_db", o->oddeven_db); jw_num(w, "odd_harmonic_share_pct", o->odd_share_pct);
        jw_num(w, "fundamental_share_db", o->h1_share_db); jw_num(w, "harmonic_centroid", o->hcentroid);
        jw_num(w, "tone_move_pct", o->move_pct); jw_num(w, "env_move_pct", o->env_move_pct); jw_num(w, "held_centroid_cv_pct", o->held_cv_pct);
        jw_bool(w, "timbre_from_clean_onsets", o->timbre_clean); jw_int(w, "timbre_onsets", o->timbre_n);
        jw_close(w);
    }
    jw_close(w);

    /* ================= pitch ================= */
    jw_open(w, "pitch", 0);
    for (p = 1; p < SL_NP; p++) {
        char key[8]; int q;
        if (!PM[p].nnotes) continue;
        snprintf(key, sizeof key, "p%d", p);
        jw_open(w, key, 0); jw_num(w, "median", PM[p].note_med); jw_num(w, "p90", PM[p].note_p90); jw_num(w, "highest", PM[p].high); jw_num(w, "lowest", PM[p].low);
        jw_open(w, "set", 1); for (q = 0; q < RUB[1].nset[p]; q++) jw_int(w, NULL, RUB[1].sets[p][q]); jw_close(w);
        jw_close(w);
    }
    {
        int m;
        for (m = 0; m < 2; m++) {
            char key[24]; snprintf(key, sizeof key, "rubs_%s", m ? "ALL" : "REL");
            jw_open(w, key, 0);
            jw_num(w, "minor2_or_9_pct", RUB[m].rub12_pct); jw_num(w, "minor2_or_maj7_any_octave_pct", RUB[m].rub_oct_pct);
            jw_num(w, "both_parts_sounding_pct", RUB[m].both_pct);
            jw_num(w, "within_p1_pct", RUB[m].within_pct[1]); jw_num(w, "within_p2_pct", RUB[m].within_pct[2]);
            jw_close(w);
        }
        /* RU minor2nd(): pitch-class pairs of the two sets over the whole range */
        {
            int a, b, pc1[12] = { 0 }, pc2[12] = { 0 }, any = 0;
            for (a = 0; a < RUB[1].nset[1]; a++) pc1[RUB[1].sets[1][a] % 12] = 1;
            for (a = 0; a < RUB[1].nset[2]; a++) pc2[RUB[1].sets[2][a] % 12] = 1;
            jw_open(w, "minor2_pitchclass_pairs", 1);
            for (a = 0; a < 12; a++) for (b = 0; b < 12; b++) if (pc1[a] && pc2[b] && (((a - b) % 12 + 12) % 12 == 1 || ((a - b) % 12 + 12) % 12 == 11)) {
                char t[16]; snprintf(t, sizeof t, "%d-%d", a, b); jw_str(w, NULL, t); any = 1;
            }
            jw_close(w); (void)any;
        }
    }
    jw_close(w);

    /* ================= space ================= */
    jw_open(w, "space", 0);
    {
        double c; sm_db(s->l, s->r, n, &R->side_mid_db, &c); R->lr_corr = c;
        jw_num(w, "side_mid_db", R->side_mid_db); jw_num(w, "lr_correlation", c);
        jw_num(w, "air_gt6k_pct", R->bands[9]);
        /* the hit part: part 1 (the "stab") or, without it, the first part that has notes */
        {
            int hp = PM[1].nnotes ? 1 : (PM[2].nnotes ? 2 : 0), nh = 0; hit *H = (hit *)malloc(sizeof(hit) * (size_t)(s->nv + 1));
            long tol = (long)(400.0 * sr / 44100.0 + 0.5);
            double *sdec = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1)), *gp = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1));
            double *cen = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1)), *ro = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1)), *l150 = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1));
            double *l300 = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1)), *hf = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1));
            int nd = 0, ng = 0, ns = 0;
            if (hp) nh = hits_of(s, hp, tol, H);
            for (i = 0; i < nh; i++) {   /* DM stab_decay(): time for the 5 ms RMS to fall to 10 % of its peak after the hit */
                long t0 = H[i].t, t1 = i + 1 < nh ? H[i + 1].t : n, lim1 = t0 + (long)(1.5 * sr), fr = (long)(0.005 * sr + 0.5), nf; double *e; long j, pk = 0;
                if (lim1 < t1) t1 = lim1;
                if (t1 - t0 < fr) continue;
                e = env_frames(s->m + t0, t1 - t0, fr, &nf);
                { double mx = 0; for (j = 0; j < nf; j++) if (e[j] > mx) { mx = e[j]; pk = j; } if (nf < 3 || mx < 1e-5) { free(e); continue; } }
                { double lim = e[pk] * 0.1; j = pk; while (j < nf && e[j] > lim) j++; sdec[nd++] = (double)(j - pk) * 5.0; }
                free(e);
            }
            for (i = 0; i < nh; i++) {   /* DM stab_spec(): the first 250 ms of each hit */
                long t0 = H[i].t, len = (long)(0.25 * sr); double *ww, *pp, tot = 0, num = 0, hfe = 0, a150 = 0, a300 = 0, cum = 0, r85 = 0; long q;
                if (t0 + len > n) len = n - t0;
                if (len < 4096 || sl_rms(s->m + t0, (size_t)len) < 1e-5) continue;
                ww = (double *)malloc(sizeof(double) * (size_t)len); pp = (double *)malloc(sizeof(double) * (size_t)(len / 2 + 1));
                for (q = 0; q < len; q++) ww[q] = s->m[t0 + q] * sl_hann((size_t)q, (size_t)len);
                sl_rfft_power(ww, (size_t)len, pp);
                for (q = 0; q <= len / 2; q++) { double f = fbin(q, len, sr); if (f >= 20) { tot += pp[q]; num += f * pp[q]; if (f < 150) a150 += pp[q]; if (f < 300) a300 += pp[q]; if (f >= 2000) hfe += pp[q]; } }
                tot += 1e-30;
                for (q = 0; q <= len / 2; q++) { double f = fbin(q, len, sr); if (f >= 20) { cum += pp[q] / tot; if (cum >= 0.85) { r85 = f; break; } } }
                cen[ns] = num / tot; ro[ns] = r85; l150[ns] = 100.0 * a150 / tot; l300[ns] = 100.0 * a300 / tot; hf[ns] = 10.0 * log10((hfe + 1e-30) / tot); ns++;
                free(ww); free(pp);
            }
            for (i = 0; i + 1 < nh; i++) {   /* SP gaps(): level of the last 150 ms before the next hit re the hit's first 200 ms */
                long t0 = H[i].t, t1 = H[i + 1].t; double st; long g0 = t1 - (long)(0.15 * sr) - 220, g1 = t1 - 220;
                if (t1 - t0 < (long)(0.5 * sr) || t0 + (long)(0.2 * sr) > n || g0 < 0) continue;
                st = sl_rms(s->m + t0, (size_t)(0.2 * sr));
                if (st > 1e-5) gp[ng++] = dbv(sl_rms(s->m + g0, (size_t)(g1 - g0))) - dbv(st);
            }
            {   /* DM tail_share(): energy outside the first 200 ms of each hit */
                double tot = 1e-30, ins = 0; long q;
                for (q = 0; q < n; q++) tot += s->m[q] * s->m[q];
                for (i = 0; i < nh; i++) { long q0 = H[i].t, q1 = q0 + (long)(0.2 * sr); if (q1 > n) q1 = n; for (q = q0; q < q1; q++) ins += s->m[q] * s->m[q]; }
                R->tail_share_pct = nh ? 100.0 * (1 - ins / tot) : NA;
            }
            jw_open(w, "hits", 0);
            jw_int(w, "part", hp); jw_int(w, "count", nh);
            jw_num(w, "decay_to_minus20db_ms_median", med(sdec, nd));
            jw_num(w, "centroid_hz_median", med(cen, ns)); jw_num(w, "rolloff85_hz_median", med(ro, ns));
            jw_num(w, "lt150_pct_median", med(l150, ns)); jw_num(w, "lt300_pct_median", med(l300, ns)); jw_num(w, "gt2k_db_re_total_median", med(hf, ns));
            if (ns) {
                double mn = cen[0], mx = cen[0]; for (i = 1; i < ns; i++) { if (cen[i] < mn) mn = cen[i]; if (cen[i] > mx) mx = cen[i]; }
                jw_num(w, "centroid_range_oct", log2(mx / mn));
            } else jw_null(w, "centroid_range_oct");
            jw_num(w, "gap_fill_db_re_hit_median", med(gp, ng));
            jw_num(w, "energy_after_first_200ms_pct", R->tail_share_pct);
            jw_close(w);

            /* ---- echo / tail, from the mix: the hit with the longest quiet gap after it ---- */
            {
                long best = -1, bestgap = 0; int bi = -1;
                int *dlt = NULL;
                double *allon = (double *)malloc(sizeof(double) * (size_t)(s->nv + 1)); int na = 0;
                (void)dlt;
                for (i = 0; i < s->nv; i++) if (s->v[i].s0 >= 0) allon[na++] = (double)s->v[i].s0;
                for (i = 0; i < nh; i++) {
                    long nxt = n, gap; int q;
                    for (q = 0; q < na; q++) if (allon[q] > (double)(H[i].t + tol) && (long)allon[q] < nxt) nxt = (long)allon[q];
                    gap = nxt - H[i].t;
                    if (gap >= bestgap) { bestgap = gap; best = H[i].t; bi = i; }
                }
                free(allon);
                jw_open(w, "echo", 0);
                R->hf_drop_db = R->echo_repeats_m40 = R->tail_t40_s = NA;
                if (bi >= 0 && bestgap >= sr) {
                    long t0 = best, gapn = bestgap, Ts, wl = (long)(0.1 * sr); double T = delay_ms > 0 ? delay_ms : 45000.0 / J.bpm; double ref, lv[8], hfv[8], l3[8]; int nr = 0, kk;
                    Ts = (long)(T * sr / 1000.0);
                    ref = sl_rms(s->m + t0, (size_t)wl);
                    for (kk = 1; kk <= 8; kk++) {
                        long q0 = t0 + kk * Ts - (long)(0.01 * sr);
                        if (q0 + wl > t0 + gapn || q0 + wl > n) break;
                        lv[nr] = dbv(sl_rms(s->m + q0, (size_t)wl)) - dbv(ref); hfv[nr] = sl_hf_db(s->m + q0, (size_t)wl, sr); l3[nr] = sl_band_pct(s->m + q0, (size_t)wl, sr, 20, 300); nr++;
                    }
                    jw_num(w, "hit_time_s", (double)(t0 + J.a) / sr); jw_num(w, "gap_s", (double)gapn / sr); jw_num(w, "delay_ms_used", T);
                    jw_str(w, "delay_ms_source", delay_ms > 0 ? "option" : "dotted eighth of the bpm");
                    jw_open(w, "repeat_level_db_re_hit", 1); for (kk = 0; kk < nr; kk++) jw_num(w, NULL, lv[kk]); jw_close(w);
                    jw_open(w, "repeat_gt2k_db_re_window", 1); for (kk = 0; kk < nr; kk++) jw_num(w, NULL, hfv[kk]); jw_close(w);
                    jw_open(w, "repeat_lt300_pct", 1); for (kk = 0; kk < nr; kk++) jw_num(w, NULL, l3[kk]); jw_close(w);
                    { int above = 0; for (kk = 0; kk < nr; kk++) if (lv[kk] > -40) above++; R->echo_repeats_m40 = nr ? above : NA; jw_num(w, "repeats_above_minus40db", R->echo_repeats_m40); }
                    if (nr > 2) {
                        R->hf_drop_db = (hfv[2] - hfv[0]) / 2.0;
                        jw_num(w, "hf_drop_db_per_repeat", R->hf_drop_db); jw_num(w, "level_loss_db_per_repeat", (lv[2] - lv[0]) / 2.0);
                    } else { jw_null(w, "hf_drop_db_per_repeat"); jw_null(w, "level_loss_db_per_repeat"); }
                    {   /* time to -40 dB: 100 ms RMS windows after the hit fall 40 dB below the first repeat's window (DM tail()) */
                        long fl = sr / 10, nf, jb = (long)rint(T / 100.0), j; double *e = env_frames(s->m + t0, gapn, fl, &nf), t40 = NA;
                        if (nf > 8 && jb < nf && e[jb] > 0) { double base = e[jb]; for (j = jb; j < nf; j++) if (e[j] < base * 0.01) { t40 = (double)j * 0.1; break; } }
                        R->tail_t40_s = t40; jw_num(w, "time_to_minus40db_s", t40); jw_num(w, "observed_s", (double)nf * 0.1);
                        free(e);
                    }
                    {   /* echo lag, 10 ms envelope autocorrelation over 150 .. 700 ms (DM echo_lag), for the record only */
                        long fl = (long)(0.01 * sr + 0.5), nf, lag, q, bl = 0; double *e = env_frames(s->m + t0, gapn, fl, &nf), mu, bv = -1e30;
                        if (nf > 75) {
                            mu = meanv(e, nf); for (q = 0; q < nf; q++) e[q] -= mu;
                            for (lag = 15; lag < 70; lag++) { double ac = 0; for (q = 0; q + lag < nf; q++) ac += e[q] * e[q + lag]; if (ac > bv) { bv = ac; bl = lag; } }
                            jw_num(w, "autocorrelation_lag_ms", (double)bl * 10.0);
                        }
                        free(e);
                    }
                    {   /* the tail at +0.5 .. 4 s */
                        static const double tt[5] = { 0.5, 1, 2, 3, 4 }; int q;
                        jw_open(w, "tail_level_db_re_hit_at_0.5_1_2_3_4s", 1);
                        for (q = 0; q < 5; q++) {
                            long q0 = t0 + (long)(tt[q] * sr);
                            if (q0 + wl <= t0 + gapn && q0 + wl <= n) jw_num(w, NULL, dbv(sl_rms(s->m + q0, (size_t)wl)) - dbv(ref)); else jw_null(w, NULL);
                        }
                        jw_close(w);
                    }
                } else jw_str(w, "note", "no hit followed by a gap of 1 s or more");
                jw_str(w, "caveat", "measured on the mix: other parts and the reverb are in the windows");
                jw_close(w);
            }
            free(H); free(sdec); free(gp); free(cen); free(ro); free(l150); free(l300); free(hf);
        }
        {   /* energy while no key is held (after every note-off) against while a key is held */
            int *d = (int *)calloc((size_t)n + 2, sizeof(int)); long q; int cur = 0; double eg = 0, eu = 0; long ng2 = 0, nu = 0;
            for (i = 0; i < s->nv; i++) { long a0 = s->v[i].s0, b0 = s->v[i].s1; if (a0 < 0) a0 = 0; if (b0 > n) b0 = n; if (b0 <= a0) continue; d[a0]++; d[b0]--; }
            for (q = 0; q < n; q++) { double e2 = s->m[q] * s->m[q]; cur += d[q]; if (cur > 0) { eg += e2; ng2++; } else { eu += e2; nu++; } }
            jw_open(w, "after_noteoff", 0);
            jw_num(w, "energy_pct", 100.0 * eu / (eg + eu + 1e-30));
            jw_num(w, "level_db_re_during_notes", (ng2 && nu) ? 10.0 * log10(((eu / nu) + 1e-30) / ((eg / ng2) + 1e-30)) : NA);
            jw_num(w, "time_pct", 100.0 * (double)nu / (double)n);
            jw_close(w);
            free(d);
        }
        {   /* SP: the quietest / loudest 50 ms frames */
            long fl = (long)(0.05 * sr + 0.5), nf; double *e = env_frames(s->m, n, fl, &nf);
            if (nf) {
                double p5 = sl_percentile(e, (size_t)nf, 5), p95 = sl_percentile(e, (size_t)nf, 95);
                jw_num(w, "quietest_5pct_frame_dbfs", dbv(p5)); jw_num(w, "loud_to_gap_range_db", dbv(p95) - dbv(p5));
            }
            free(e);
        }
    }
    jw_close(w);

    /* ================= quality (QU) ================= */
    jw_open(w, "quality", 0);
    {
        long q, blk = (long)(0.002 * sr + 0.5), nb, nzl = -1; double *d2, *eb, endlvl = -300, tail, hf = -300, cor, ll = 0, rr2 = 0, lr = 0;
        d2 = (double *)malloc(sizeof(double) * (size_t)n);
        for (q = 2; q < n; q++) d2[q - 2] = fabs(s->m[q] - 2 * s->m[q - 1] + s->m[q - 2]) * 32768.0;
        nb = (n - 2) / blk; eb = (double *)malloc(sizeof(double) * (size_t)(nb + 1));
        for (k = 0; k < nb; k++) { double mx = 0; for (q = 0; q < blk; q++) if (d2[k * blk + q] > mx) mx = d2[k * blk + q]; eb[k] = mx; }
        if (nb > 0) {
            double md = sl_median(eb, (size_t)nb), emax = 0, *loud = (double *)malloc(sizeof(double) * (size_t)nb); long nl = 0; int anypos = 0;
            for (k = 0; k < nb; k++) { if (eb[k] > emax) emax = eb[k]; if (eb[k] > 0) anypos = 1; }
            for (k = 0; k < nb; k++) if (eb[k] > md) loud[nl++] = eb[k];
            if (!anypos) { for (k = 0; k < nb; k++) loud[k] = eb[k]; nl = nb; }
            jw_num(w, "second_difference_click_db", 20.0 * log10((emax + 1e-9) / (sl_median(loud, (size_t)(nl ? nl : 1)) + 1e-9)));
            free(loud);
        }
        for (q = 0; q < n; q++) if (fabs(s->l[q]) > 0 || fabs(s->r[q]) > 0) nzl = q;
        if (nzl >= 0 && nzl < n - sr / 100) {
            long a0 = nzl - 240 > 0 ? nzl - 240 : 0; double acc = 0;
            for (q = a0; q <= nzl; q++) acc += s->l[q] * s->l[q] + s->r[q] * s->r[q];
            endlvl = 20.0 * log10(sqrt(acc / (2.0 * (double)(nzl - a0 + 1))) + 1e-12);
        }
        { long t0 = n - sr / 5 > 0 ? n - sr / 5 : 0; double acc = 0; for (q = t0; q < n; q++) acc += s->l[q] * s->l[q] + s->r[q] * s->r[q]; tail = 20.0 * log10(sqrt(acc / (2.0 * (double)(n - t0))) + 1e-12); }
        {   /* hf: energy above 16 kHz of 1 .. 6 s, 8192 Hann frames hop 4096 */
            long x0 = sr, x1 = 6L * sr, nfm = 0, f0; double *acc = (double *)calloc(4097, sizeof(double)), *ww = (double *)malloc(sizeof(double) * 8192), *pp = (double *)malloc(sizeof(double) * 4097);
            if (x1 > n) x1 = n;
            for (f0 = x0; f0 + 8192 < x1; f0 += 4096) {
                long z; for (z = 0; z < 8192; z++) ww[z] = s->m[f0 + z] * sl_hann((size_t)z, 8192);
                sl_rfft_power(ww, 8192, pp); for (z = 0; z < 4097; z++) acc[z] += pp[z]; nfm++;
            }
            if (nfm) { double a16 = 0, all = 0; long z; for (z = 0; z < 4097; z++) { all += acc[z]; if (fbin(z, 8192, sr) > 16000) a16 += acc[z]; } hf = 10.0 * log10(a16 / (all + 1e-30) + 1e-30); }
            free(acc); free(ww); free(pp);
        }
        for (q = 0; q < n; q++) { ll += s->l[q] * s->l[q]; rr2 += s->r[q] * s->r[q]; lr += s->l[q] * s->r[q]; }
        cor = lr / (sqrt(ll * rr2) + 1e-9 / (32768.0 * 32768.0));
        jw_num(w, "end_cut_dbfs", endlvl); jw_num(w, "tail_last_200ms_dbfs", tail); jw_num(w, "gt16k_db_re_total", hf); jw_num(w, "lr_correlation_uncentered", cor);
        free(d2); free(eb);
    }
    jw_close(w);

    /* ================= click test around a delay-TIME step (CK) ================= */
    if (J.nfx) {
        sl_wav C; int have = 0; char cfn[640]; const char *cp = control && control[0] ? control : (J.control[0] ? J.control : NULL); int ev, anyclick = 0;
        char defc[640];
        if (!cp) { snprintf(defc, sizeof defc, "%s_ctl", prefix); cp = defc; }
        memset(&C, 0, sizeof C);
        snprintf(cfn, sizeof cfn, "%s.wav", cp);
        if (sl_wav_read(cfn, &C) == 0 && C.sr == sr) have = 1;
        jw_open(w, "click", 0);
        jw_bool(w, "control_found", have); jw_str(w, "control", have ? cp : "");
        jw_open(w, "steps", 1);
        for (ev = 0; ev < J.nfx; ev++) {
            long c = J.fx[ev].smp - J.a, win = (long)(0.005 * sr + 0.5), lo = c - win, hi = c + win, q; double mx = 0, mxk = 0, at, atk = 0; int same = 1;
            if (c < 2 || c >= n) continue;
            if (lo < 1) lo = 1;
            if (hi > n - 1) hi = n - 1;
            for (q = lo; q <= hi; q++) {
                double a = fabs(s->l[q] - s->l[q - 1]), b = fabs(s->r[q] - s->r[q - 1]);
                if (a > mx) mx = a;
                if (b > mx) mx = b;
            }
            at = fmax(fabs(s->l[c] - s->l[c - 1]), fabs(s->r[c] - s->r[c - 1]));
            jw_open(w, NULL, 0);
            jw_num(w, "t_s", (double)J.fx[ev].smp / sr); jw_num(w, "window_db", mx > 0 ? 20 * log10(mx) : -999.0); jw_num(w, "at_change_db", at > 0 ? 20 * log10(at) : -999.0);
            if (have && (long)C.n > hi + J.a) {
                for (q = lo; q <= hi; q++) {
                    double a = fabs(C.l[q + J.a] - C.l[q + J.a - 1]), b = fabs(C.r[q + J.a] - C.r[q + J.a - 1]);
                    if (a > mxk) mxk = a;
                    if (b > mxk) mxk = b;
                }
                atk = fmax(fabs(C.l[c + J.a] - C.l[c + J.a - 1]), fabs(C.r[c + J.a] - C.r[c + J.a - 1]));
                for (q = 0; q < c; q++) if (C.l[q + J.a] != s->l[q] || C.r[q + J.a] != s->r[q]) { same = 0; break; }
                {
                    double wd = mx > 0 ? 20 * log10(mx) : -999.0, wk = mxk > 0 ? 20 * log10(mxk) : -999.0; int click = wd > -40.0 && wk <= -40.0;
                    jw_num(w, "control_window_db", wk); jw_num(w, "control_at_change_db", atk > 0 ? 20 * log10(atk) : -999.0);
                    jw_num(w, "excess_db", wd - wk); jw_bool(w, "pre_identical", same); jw_bool(w, "click", click);
                    if (click) anyclick = 1;
                }
            }
            jw_close(w);
        }
        jw_close(w);
        jw_bool(w, "any_click", anyclick);
        jw_close(w);
        sl_wav_free(&C);
    }

    jw_open(w, "notes", 1);
    jw_str(w, NULL, "levels, bands and the whistle test are of the mix; timbre / attack / movement use onsets where the other part is silent when there are any (timbre_from_clean_onsets)");
    jw_str(w, NULL, "echo / tail figures are of the mix around the hit with the longest gap, not a delay-return stem; delay time is the dotted eighth of the bpm unless --delay-ms");
    jw_str(w, NULL, "rubs REL = note-on to note-off, ALL = note-on to voice end (the old logs' END)");
    if (!s->has_end) jw_str(w, NULL, "the json has no t_end: ALL equals REL and simultaneous notes stop at note-off");
    jw_close(w);
    jw_close(w);
    job_free(&J);
    return 0;
}
