/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab measure, the voice-log half: onset groups, rhythm / timbre / movement / attack per part (AN = reel analyse.py),
 * simultaneous notes (M18 bed, AN nv_both) and rubs between parts (M18 rub, RB rubs). Times are samples of the measured range. */
#include "sl_int.h"
#include <stdlib.h>
#include <string.h>

#define NA NAN
static double mtof(int m) { return 440.0 * pow(2.0, (m - 69) / 12.0); }
static double med(const double *a, int n) { return n ? sl_median(a, (size_t)n) : NA; }
static double meanv(const double *a, int n) { int i; double s = 0; if (!n) return NA; for (i = 0; i < n; i++) s += a[i]; return s / n; }
static double stdv(const double *a, int n) { int i; double mu = meanv(a, n), s = 0; if (!n) return NA; for (i = 0; i < n; i++) s += (a[i] - mu) * (a[i] - mu); return sqrt(s / n); }

static int cmp_v(const void *a, const void *b) {
    const voice *x = (const voice *)a, *y = (const voice *)b;
    if (x->s0 != y->s0) return x->s0 < y->s0 ? -1 : 1;
    return x->note - y->note;
}
int sl_part_voices(const sig *s, int part, voice **out) {
    int i, k = 0; voice *a = (voice *)malloc(sizeof(voice) * (size_t)(s->nv + 1));
    for (i = 0; i < s->nv; i++) if (s->v[i].part == part) a[k++] = s->v[i];
    qsort(a, (size_t)k, sizeof(voice), cmp_v);
    *out = a; return k;
}
int sl_make_groups(const voice *a, int k, long tol, group *g) {
    int i, ng = 0;
    for (i = 0; i < k; i++) {
        if (ng && a[i].s0 - g[ng - 1].t <= tol) g[ng - 1].cnt++;
        else { g[ng].t = a[i].s0; g[ng].first = i; g[ng].cnt = 1; ng++; }
    }
    return ng;
}
int sl_lowest(const voice *a, const group *g) {
    int i, lo = 999;
    for (i = 0; i < g->cnt; i++) if (a[g->first + i].note < lo) lo = a[g->first + i].note;
    return lo;
}
static const voice *lowest_voice(const voice *a, const group *g) {
    int i, b = 0;
    for (i = 1; i < g->cnt; i++) if (a[g->first + i].note < a[g->first + b].note) b = i;
    return &a[g->first + b];
}
static int other_sounds(const sig *s, int part, long t0, long t1) {
    int i;
    for (i = 0; i < s->nv; i++) if (s->v[i].part != part && s->v[i].s0 <= t1 && s->v[i].se >= t0) return 1;
    return 0;
}

/* AN harm(): amplitude (dB) of harmonics 1..16 of f0 in a Hann window of N samples centred on t */
static int harm(const sig *s, long t, double f0, int N, double *out) {
    long a = t - N / 2; int k, h = N / 2 + 1; double *w, *re, *im, bw = (double)s->sr / N;
    if (a < 0) a = 0;
    if (a + N > s->n) return 0;
    w = (double *)malloc(sizeof(double) * (size_t)N); re = (double *)malloc(sizeof(double) * (size_t)h); im = (double *)malloc(sizeof(double) * (size_t)h);
    for (k = 0; k < N; k++) w[k] = s->m[a + k] * sl_hann((size_t)k, (size_t)N);
    sl_rfft(w, (size_t)N, re, im);
    for (k = 1; k <= 16; k++) {
        double f = k * f0, c, r, mx = 0; int lo, hi, j;
        if (f > 18000) { out[k - 1] = -200.0; continue; }
        c = f / bw; r = 0.025 * c; if (r < 1.5) r = 1.5;
        lo = (int)(c - r > 0 ? c - r : 0); hi = (int)(c + r) + 1; if (hi > h) hi = h;
        for (j = lo; j < hi; j++) { double v = sqrt(re[j] * re[j] + im[j] * im[j]); if (v > mx) mx = v; }
        out[k - 1] = 20.0 * log10(mx + 1e-12);
    }
    free(w); free(re); free(im); return 1;
}
static double hcent(const double *e) {   /* harmonic centroid of the 16 amplitudes (dB) */
    double sp = 0, sk = 0; int k;
    for (k = 0; k < 16; k++) { double p = pow(10.0, e[k] / 10.0); sp += p; sk += (k + 1) * p; }
    return sk / sp;
}
static double emax(const double *e) { double m = -1e9; int k; for (k = 0; k < 16; k++) if (e[k] > m) m = e[k]; return m; }

/* choose the clean subset of (value, clean) pairs when it is not empty (the other part silent around the onset), else all */
static double pick_med(const double *v, const int *cl, int n, int *used_clean) {
    double *t = (double *)malloc(sizeof(double) * (size_t)(n + 1)); int i, m = 0, c = 0; double r;
    for (i = 0; i < n; i++) if (cl[i] && !getenv("SL_ALL_ONSETS")) t[m++] = v[i];   /* SL_ALL_ONSETS: test hook, use every onset */
    if (m) c = 1; else { for (i = 0; i < n; i++) t[m++] = v[i]; }
    r = med(t, m); free(t);
    if (used_clean) *used_clean = c;
    return r;
}

void sl_part_metrics(const sig *s, int part, partm *o) {
    voice *a; int k = sl_part_voices(s, part, &a), i, ng; group *g; long tol = (long)(0.005 * s->sr + 0.5);
    double *tmp;
    {   /* defaults */
        double *p = (double *)o; size_t q; memset(o, 0, sizeof *o);
        for (q = 1; q < offsetof(partm, timbre_clean) / sizeof(double); q++) p[q] = NA;
    }
    o->nnotes = k;
    if (!k) { free(a); return; }
    g = (group *)malloc(sizeof(group) * (size_t)k); ng = sl_make_groups(a, k, tol, g);
    o->ngroups = ng;
    tmp = (double *)malloc(sizeof(double) * (size_t)(k + ng + 4));
    for (i = 0; i < k; i++) tmp[i] = a[i].note;
    o->note_med = med(tmp, k); o->note_p90 = sl_percentile(tmp, (size_t)k, 90.0);
    o->high = 0; o->low = 999;
    for (i = 0; i < k; i++) { if (a[i].note > o->high) o->high = a[i].note; if (a[i].note < o->low) o->low = a[i].note; }
    for (i = 0; i < k; i++) tmp[i] = (double)(a[i].s1 - a[i].s0) * 1000.0 / s->sr;
    o->note_len_med_ms = med(tmp, k);

    /* ---- AN rhythm() ---- */
    if (s->nbar >= 3) {
        double *d = (double *)malloc(sizeof(double) * (size_t)s->nbar), blen, step;
        double *odd = (double *)malloc(sizeof(double) * (size_t)(ng + 1)), *even = (double *)malloc(sizeof(double) * (size_t)(ng + 1));
        double *gates = (double *)malloc(sizeof(double) * (size_t)k), *snd = (double *)malloc(sizeof(double) * (size_t)k), *vel = (double *)malloc(sizeof(double) * (size_t)k);
        double *gapf = (double *)malloc(sizeof(double) * (size_t)(ng + 1));
        int no = 0, ne = 0, off16 = 0, nio = 0, io1 = 0, blip = 0, nblip = 0, ngf = 0, seen[128];
        memset(seen, 0, sizeof seen);
        for (i = 0; i + 1 < s->nbar; i++) d[i] = (double)(s->bs[i + 1] - s->bs[i]);
        blen = med(d, s->nbar - 1); step = blen / 16.0;
        o->onsets_bar = (double)ng / s->nbar;
        { double nn = 0; for (i = 0; i < ng; i++) nn += g[i].cnt; o->notes_onset = nn / ng; }
        for (i = 0; i < ng; i++) {
            long t = g[i].t; int b = 0, j; double p, ip;
            for (j = 0; j < s->nbar; j++) if (s->bs[j] <= t) b = j;
            p = (double)(t - s->bs[b]) / step; ip = rint(p);
            if (((long)ip) % 2 != 0) { odd[no++] = p - ip; off16++; } else even[ne++] = p - ip;
            if (i + 1 < ng) { double ioi = (double)(g[i + 1].t - t) / step; nio++; if (fabs(ioi - 1) < 0.25) io1++; }
        }
        o->share_16th_off = 100.0 * off16 / ng;
        o->ioi1_share = nio ? 100.0 * io1 / nio : 0.0;
        o->swing_pct = (no && ne) ? (meanv(odd, no) - meanv(even, ne)) * 100.0 : 0.0;
        { int nd = 0; double vmin = 1e9, vmax = -1e9;
          for (i = 0; i < k; i++) {
              gates[i] = (double)(a[i].s1 - a[i].s0) / step; snd[i] = (double)(a[i].se - a[i].s0) / step; vel[i] = a[i].vel;
              if (a[i].vel >= 0 && a[i].vel < 128 && !seen[a[i].vel]) { seen[a[i].vel] = 1; nd++; }
              if (vel[i] < vmin) vmin = vel[i];
              if (vel[i] > vmax) vmax = vel[i];
          }
          o->vel_distinct = nd; o->vel_min = vmin; o->vel_max = vmax; o->vel_sd = stdv(vel, k); }
        o->gate_med_steps = med(gates, k); o->sound_med_steps = med(snd, k);
        for (i = 0; i + 1 < ng; i++) {   /* blip: sounding length (with release) under half the gap to the next onset */
            double ioi = (double)(g[i + 1].t - g[i].t) / step, mx = 0; int j;
            for (j = 0; j < g[i].cnt; j++) { double e = (double)(a[g[i].first + j].se - a[g[i].first + j].s0); if (e > mx) mx = e; }
            mx /= step; nblip++;
            gapf[ngf++] = mx / ioi < 1.0 ? mx / ioi : 1.0;
            if (mx < 0.5 * ioi) blip++;
        }
        o->blip_pct = nblip ? 100.0 * blip / nblip : 0.0;
        o->gapfill_med = ngf ? med(gapf, ngf) : 1.0;
        { int nj = 0, c12 = 0, c7 = 0; double sum = 0;
          for (i = 0; i + 1 < ng; i++) { int dd = sl_lowest(a, &g[i + 1]) - sl_lowest(a, &g[i]); if (dd < 0) dd = -dd; nj++; sum += dd; if (dd == 12) c12++; if (dd == 7) c7++; }
          o->jump_oct_pct = nj ? 100.0 * c12 / nj : 0; o->jump_5th_pct = nj ? 100.0 * c7 / nj : 0; o->jump_mean = nj ? sum / nj : 0; }
        free(d); free(odd); free(even); free(gates); free(snd); free(vel); free(gapf);
    }

    /* ---- AN timbre(): the lowest note of each onset, harmonics 1..16 at 60 ms (and again at 300 ms for the tone move) ---- */
    {
        double *h30 = (double *)malloc(sizeof(double) * (size_t)ng), *oe = (double *)malloc(sizeof(double) * (size_t)ng), *os = (double *)malloc(sizeof(double) * (size_t)ng);
        double *h1 = (double *)malloc(sizeof(double) * (size_t)ng), *hc = (double *)malloc(sizeof(double) * (size_t)ng);
        double *mv = (double *)malloc(sizeof(double) * (size_t)ng); int *cl = (int *)malloc(sizeof(int) * (size_t)ng), *cm = (int *)malloc(sizeof(int) * (size_t)ng);
        int n1 = 0, nm = 0, used = 0, u2 = 0;
        for (i = 0; i < ng; i++) {
            const voice *lv = lowest_voice(a, &g[i]); double f0 = mtof(lv->note), e[16], po[16], tot = 0, odd, even; int kk, cnt = 0;
            long c = g[i].t + (long)(0.06 * s->sr);
            if (!harm(s, c, f0, 4096, e) || emax(e) < -80) continue;
            for (kk = 0; kk < 16; kk++) { po[kk] = pow(10.0, e[kk] / 10.0); tot += po[kk]; if (e[kk] - emax(e) > -30) cnt++; }
            odd = po[2] + po[4] + po[6] + po[8] + po[10]; even = po[1] + po[3] + po[5] + po[7] + po[9];
            h30[n1] = cnt; oe[n1] = 10.0 * log10(odd / (even + 1e-20) + 1e-20); os[n1] = 100.0 * odd / (odd + even + 1e-30);
            h1[n1] = 10.0 * log10(po[0] / tot); hc[n1] = hcent(e);
            cl[n1] = !other_sounds(s, part, c - 2048, c + 2048);
            n1++;
            {
                long tl = g[i].t + (long)(0.30 * s->sr); double l2[16];
                if (lv->s1 > tl + 2048 && harm(s, tl, f0, 4096, l2) && emax(l2) > -90) {
                    mv[nm] = fabs(hcent(l2) - hcent(e)) / hcent(e) * 100.0;
                    cm[nm] = cl[n1 - 1] && !other_sounds(s, part, tl - 2048, tl + 2048);
                    nm++;
                }
            }
        }
        if (n1) {
            o->h30 = pick_med(h30, cl, n1, &used); o->oddeven_db = pick_med(oe, cl, n1, NULL); o->odd_share_pct = pick_med(os, cl, n1, NULL);
            o->h1_share_db = pick_med(h1, cl, n1, NULL); o->hcentroid = pick_med(hc, cl, n1, NULL);
            o->timbre_clean = used; o->timbre_n = n1;
        }
        if (nm) o->move_pct = pick_med(mv, cm, nm, &u2);
        free(h30); free(oe); free(os); free(h1); free(hc); free(mv); free(cl); free(cm);
    }

    /* ---- AN movement(): harmonic centroid inside the note, 20 ms against 150 ms; the spread over a held note ---- */
    {
        double *ch = (double *)malloc(sizeof(double) * 64), *cv = (double *)malloc(sizeof(double) * 64); int *cc = (int *)malloc(sizeof(int) * 64), *cc2 = (int *)malloc(sizeof(int) * 64);
        int nch = 0, ncv = 0;
        for (i = 0; i < ng && i < 48; i++) {
            const voice *lv = lowest_voice(a, &g[i]); double f0 = mtof(lv->note), e1[16], e2[16];
            long t1 = g[i].t + (long)(0.02 * s->sr), t2 = g[i].t + (long)(0.15 * s->sr), en = lv->se - (long)(0.01 * s->sr), rel = lv->s1;
            if (en < t2) t2 = en;
            if (t2 > t1 + (long)(0.04 * s->sr)) {
                if (harm(s, t1, f0, 2048, e1) && emax(e1) >= -85 && harm(s, t2, f0, 2048, e2) && emax(e2) >= -85) {
                    double c1 = hcent(e1), c2 = hcent(e2);
                    ch[nch] = (c2 - c1) / c1 * 100.0; cc[nch] = !other_sounds(s, part, g[i].t, t2); nch++;
                }
            }
            if (rel - g[i].t > (long)(0.6 * s->sr)) {
                double ser[128]; int ns = 0; long t, tend = rel < g[i].t + 4 * (long)s->sr ? rel : g[i].t + 4 * (long)s->sr;
                for (t = g[i].t + (long)(0.3 * s->sr); t < tend && ns < 128; t += (long)(0.05 * s->sr)) {
                    double e[16];
                    if (harm(s, t, f0, 2048, e) && emax(e) >= -85) ser[ns++] = hcent(e);
                }
                if (ns > 5) { cv[ncv] = stdv(ser, ns) / meanv(ser, ns) * 100.0; cc2[ncv] = !other_sounds(s, part, g[i].t + (long)(0.3 * s->sr), tend); ncv++; }
            }
        }
        if (nch) o->env_move_pct = pick_med(ch, cc, nch, NULL);
        if (ncv) o->held_cv_pct = pick_med(cv, cc2, ncv, NULL);
        free(ch); free(cv); free(cc); free(cc2);
    }

    /* ---- AN attack_ms(): onset to 90 % of the note's peak in a 2 ms RMS envelope ---- */
    {
        long w = (long)(0.002 * s->sr + 0.5), j; double *P = (double *)malloc(sizeof(double) * (size_t)(s->n + 1)), *at = (double *)malloc(sizeof(double) * 48); int *ac = (int *)malloc(sizeof(int) * 48), na = 0;
        P[0] = 0;
        for (j = 0; j < s->n; j++) P[j + 1] = P[j] + s->m[j] * s->m[j];
        for (i = 0; i < ng && i < 40; i++) {
            long t = g[i].t, end = t + (long)(0.5 * s->sr), q, first = -1; double pk = 0;
            if (end > s->n) end = s->n;
            if (end - t < 100) continue;
            for (q = t; q < end; q++) {
                long lo = q - w / 2, hi = q + (w - w / 2) - 1; double v;
                if (lo < 0) lo = 0;
                if (hi > s->n - 1) hi = s->n - 1;
                v = sqrt((P[hi + 1] - P[lo]) / (double)w); if (v > pk) pk = v;
            }
            if (pk < 1e-4) continue;
            for (q = t; q < end; q++) {
                long lo = q - w / 2, hi = q + (w - w / 2) - 1; double v;
                if (lo < 0) lo = 0;
                if (hi > s->n - 1) hi = s->n - 1;
                v = sqrt((P[hi + 1] - P[lo]) / (double)w);
                if (v >= 0.9 * pk) { first = q - t; break; }
            }
            if (first < 0) continue;
            at[na] = (double)first / s->sr * 1000.0; ac[na] = !other_sounds(s, part, t, t + (long)(0.1 * s->sr)); na++;
        }
        if (na) o->attack_ms = pick_med(at, ac, na, NULL);
        free(P); free(at); free(ac);
    }
    free(tmp); free(g); free(a);
}

/* ---- simultaneous notes: voices alive from note-on to voice end (the old NV count) ---- */
typedef struct { long t; int d, part, note; } ev;
static int cmp_ev(const void *a, const void *b) {
    const ev *x = (const ev *)a, *y = (const ev *)b;
    if (x->t != y->t) return x->t < y->t ? -1 : 1;
    return x->d - y->d;     /* ends (-1) before starts (+1) at the same time */
}
static ev *build_events(const sig *s, int all_mode, int *ne) {
    int i, k = 0; ev *e = (ev *)malloc(sizeof(ev) * (size_t)(2 * s->nv + 2));
    for (i = 0; i < s->nv; i++) {
        long a = s->v[i].s0, b = all_mode ? s->v[i].se : s->v[i].s1;
        if (a >= s->n) continue;
        if (b > s->n) b = s->n;
        if (b < a) b = a;
        e[k].t = a; e[k].d = 1; e[k].part = s->v[i].part; e[k].note = s->v[i].note; k++;
        e[k].t = b; e[k].d = -1; e[k].part = s->v[i].part; e[k].note = s->v[i].note; k++;
    }
    qsort(e, (size_t)k, sizeof(ev), cmp_ev);
    *ne = k; return e;
}

void sl_bed(const sig *s, bedm *b) {
    int ne, i, p; ev *e = build_events(s, 1, &ne); long last = 0; int cnt[SL_NP], nc[SL_NP][128], bestboth = -1;
    double acc[SL_NP], accb = 0, silp[SL_NP], silb = 0;
    memset(cnt, 0, sizeof cnt); memset(nc, 0, sizeof nc); memset(acc, 0, sizeof acc); memset(silp, 0, sizeof silp);
    memset(b, 0, sizeof *b);
    for (i = 0; i <= ne; i++) {
        long t = i < ne ? e[i].t : s->n; int tot = cnt[1] + cnt[2];
        if (t > last) {
            double dt = (double)(t - last);
            for (p = 1; p < SL_NP; p++) { acc[p] += cnt[p] * dt; if (!cnt[p]) silp[p] += dt; if (cnt[p] > b->maxc[p]) b->maxc[p] = cnt[p]; }
            accb += tot * dt; if (!tot) silb += dt;
            if (tot > b->both_max) b->both_max = tot;
            if (tot > bestboth) {
                bestboth = tot;
                for (p = 1; p < SL_NP; p++) { int q; b->nfull[p] = 0; for (q = 0; q < 128 && b->nfull[p] < 16; q++) if (nc[p][q] > 0) b->fullest[p][b->nfull[p]++] = q; }
            }
            last = t;
        }
        if (i < ne && e[i].part >= 1 && e[i].part < SL_NP) {
            cnt[e[i].part] += e[i].d;
            if (e[i].note >= 0 && e[i].note < 128) nc[e[i].part][e[i].note] += e[i].d;
        }
    }
    for (p = 1; p < SL_NP; p++) { b->mean[p] = s->n ? acc[p] / s->n : NA; b->sil[p] = s->n ? 100.0 * silp[p] / s->n : NA; }
    b->both_mean = s->n ? accb / s->n : NA; b->both_sil = s->n ? 100.0 * silb / s->n : NA;
    free(e);
}

/* M18 rub(): share of the time both parts sound in which a note of part 1 and one of part 2 are 1 or 13 semitones apart; RB rubs():
 * the octave-equivalent version (minor 2nd, major 7th at any octave). Also within one part. */
void sl_rubs(const sig *s, int all_mode, rubm *r) {
    int ne, i, p, q, j; ev *e = build_events(s, all_mode, &ne); long last = 0; int nc[SL_NP][128];
    double both = 0, bad12 = 0, badoct = 0, w2[SL_NP], wbad[SL_NP];
    memset(nc, 0, sizeof nc); memset(r, 0, sizeof *r); memset(w2, 0, sizeof w2); memset(wbad, 0, sizeof wbad);
    for (i = 0; i < s->nv; i++) {
        int pt = s->v[i].part, nt = s->v[i].note;
        if (pt >= 1 && pt < SL_NP && nt >= 0 && nt < 128) r->sets[pt][nt] = 1;
    }
    for (i = 0; i <= ne; i++) {
        long t = i < ne ? e[i].t : s->n;
        if (t > last) {
            double dt = (double)(t - last); int pa[128], na = 0, pb[128], nb = 0;
            for (j = 0; j < 128; j++) { if (nc[1][j] > 0) pa[na++] = j; if (nc[2][j] > 0) pb[nb++] = j; }
            if (na && nb) {
                int b12 = 0, bo = 0;
                both += dt;
                for (p = 0; p < na; p++) for (q = 0; q < nb; q++) { int d = pa[p] - pb[q]; if (d < 0) d = -d; if (d == 1 || d == 13) b12 = 1; if (d % 12 == 1 || d % 12 == 11) bo = 1; }
                if (b12) bad12 += dt;
                if (bo) badoct += dt;
            }
            for (p = 1; p < SL_NP; p++) {
                int *arr = p == 1 ? pa : pb, cn = p == 1 ? na : nb;
                if (cn >= 2) {
                    int bad = 0;
                    w2[p] += dt;
                    for (j = 0; j + 1 < cn && !bad; j++) for (q = j + 1; q < cn; q++) { int d = arr[q] - arr[j]; if (d == 1 || d == 13) { bad = 1; break; } }
                    if (bad) wbad[p] += dt;
                }
            }
            last = t;
        }
        if (i < ne && e[i].part >= 1 && e[i].part < SL_NP && e[i].note >= 0 && e[i].note < 128) nc[e[i].part][e[i].note] += e[i].d;
    }
    r->rub12_pct = 100.0 * bad12 / (both > 1 ? both : 1); r->rub_oct_pct = 100.0 * badoct / (both > 1 ? both : 1);
    r->both_pct = s->n ? 100.0 * both / s->n : NA;
    for (p = 1; p < SL_NP; p++) {
        r->within_pct[p] = w2[p] > 0 ? 100.0 * wbad[p] / w2[p] : NA;
        for (j = 0; j < 128; j++) if (r->sets[p][j]) r->sets[p][r->nset[p]++] = j;
    }
    /* sets[p] is now the sorted list of notes (the flags below nset are overwritten in order: j >= position, so it is safe) */
    free(e);
}
