/* acmp ref.bin new.bin : segment-wise audio comparison of two DMP dumps (records: int32 id, int32 sample) */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
typedef struct { int32_t id, v; } rec_t;
static rec_t *load(const char *p, size_t *n) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET); rec_t *r = malloc(s); *n = fread(r, 8, s / 8, f); fclose(f); return r; }
typedef struct { size_t a, b; } seg_t;
static seg_t *segs(rec_t *r, size_t n, int *ns) { seg_t *s = malloc(sizeof(seg_t) * 100000); int k = 0; size_t i, st = 0; for (i = 1; i <= n; i++) if (i == n || abs(r[i].id - r[i - 1].id) > 5) { s[k].a = st; s[k].b = i; k++; st = i; } *ns = k; return s; }
static double db(double x) { return x > 1e-12 ? 20 * log10(x) : -240; }
static void fft(double *re, double *im, int n) { int i, j, k, m; for (i = 1, j = 0; i < n; i++) { int bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; } }
  for (m = 2; m <= n; m <<= 1) { double a = -2 * M_PI / m; for (k = 0; k < n; k += m) for (j = 0; j < m / 2; j++) { double wr = cos(a * j), wi = sin(a * j); double xr = re[k + j + m / 2] * wr - im[k + j + m / 2] * wi, xi = re[k + j + m / 2] * wi + im[k + j + m / 2] * wr; re[k + j + m / 2] = re[k + j] - xr; im[k + j + m / 2] = im[k + j] - xi; re[k + j] += xr; im[k + j] += xi; } } }
#define NB 5
static const double BE[NB + 1] = {0, 250, 2000, 6000, 12000, 22050};
static void bands(const double *x, size_t n, double *e) { static double re[2048], im[2048]; size_t f, i; int b; for (b = 0; b < NB; b++) e[b] = 0; for (f = 0; f + 2048 <= n; f += 2048) { for (i = 0; i < 2048; i++) { re[i] = x[f + i] * (0.5 - 0.5 * cos(2 * M_PI * i / 2047)); im[i] = 0; } fft(re, im, 2048); for (i = 1; i < 1024; i++) { double hz = i * 44100.0 / 2048; for (b = 0; b < NB; b++) if (hz >= BE[b] && hz < BE[b + 1]) e[b] += re[i] * re[i] + im[i] * im[i]; } } }
typedef struct { double *x; size_t n; } ch_t;
static int split(rec_t *r, seg_t s, ch_t *c) { int ids[8], nid = 0, k; size_t i; for (i = s.a; i < s.b; i++) { for (k = 0; k < nid && ids[k] != r[i].id; k++) {} if (k == nid && nid < 8) ids[nid++] = r[i].id; }
  int inter = nid == 1 && ids[0] >= 100000; int nc = inter ? 2 : nid; for (k = 0; k < nc; k++) { c[k].x = malloc(sizeof(double) * (s.b - s.a)); c[k].n = 0; }
  for (i = s.a; i < s.b; i++) { if (inter) k = (i - s.a) & 1; else for (k = 0; ids[k] != r[i].id; k++); c[k].x[c[k].n++] = r[i].v / 32768.0; } return nc; }
int main(int argc, char **argv)
{
    size_t na, nb; rec_t *A = load(argv[1], &na), *B = load(argv[2], &nb); int sa, sb, k, c; seg_t *SA = segs(A, na, &sa), *SB = segs(B, nb, &sb);
    int verbose = argc > 3;
    printf("records %zu / %zu, segments %d / %d\n", na, nb, sa, sb);
    double worst_res = -240, worst_env = 0, worst_band = 0, worst_lvl = 0; int nexact = 0, ncmp = 0;
    for (k = 0; k < sa && k < sb; k++) {
        ch_t ca[8], cb[8]; int nca = split(A, SA[k], ca), ncb = split(B, SB[k], cb);
        size_t i, same = 0, m = SA[k].b - SA[k].a; if (SB[k].b - SB[k].a < m) m = SB[k].b - SB[k].a;
        for (i = 0; i < m; i++) same += A[SA[k].a + i].v == B[SB[k].a + i].v;
        int exact = same == m && SA[k].b - SA[k].a == SB[k].b - SB[k].a;
        for (c = 0; c < nca && c < ncb; c++) {
            size_t n = ca[c].n < cb[c].n ? ca[c].n : cb[c].n; double ea = 0, eb = 0, ed = 0, pa = 0, pb = 0; size_t ta = 0, tb = 0;
            for (i = 0; i < n; i++) { double a = ca[c].x[i], b = cb[c].x[i]; ea += a * a; eb += b * b; ed += (a - b) * (a - b); if (fabs(a) > pa) pa = fabs(a); if (fabs(b) > pb) pb = fabs(b); if (fabs(a) >= 4 / 32768.0) ta = i; if (fabs(b) >= 4 / 32768.0) tb = i; }
            double ra = sqrt(ea / n), rb = sqrt(eb / n), res = db(sqrt(ed / n)) - db(ra);
            double envmax = 0; size_t w; for (w = 0; w + 4410 <= n; w += 4410) { double xa = 0, xb = 0; for (i = w; i < w + 4410; i++) { xa += ca[c].x[i] * ca[c].x[i]; xb += cb[c].x[i] * cb[c].x[i]; } xa = db(sqrt(xa / 4410)); xb = db(sqrt(xb / 4410)); if (xa > -60 && fabs(xa - xb) > 6 && getenv("ENVW")) printf("  win %zu (%.2f s) %.1f -> %.1f dB\n", w, w / 44100.0, xa, xb); if (xa > -60 && fabs(xa - xb) > envmax) envmax = fabs(xa - xb); }
            double ba[NB], bb[NB], bmax = 0; int b; bands(ca[c].x, n, ba); bands(cb[c].x, n, bb); double tot = 0; for (b = 0; b < NB; b++) tot += ba[b];
            for (b = 0; b < NB; b++) if (ba[b] > tot * 1e-4 && ba[b] > 0) { double d = fabs(10 * log10((bb[b] + 1e-30) / ba[b])); if (d > bmax) bmax = d; }
            if (ra < 1e-9 && rb < 1e-9) continue;
            ncmp++; if (exact) nexact++;
            if (!exact) { if (res > worst_res) worst_res = res; if (envmax > worst_env) worst_env = envmax; if (bmax > worst_band) worst_band = bmax; if (fabs(db(rb) - db(ra)) > worst_lvl) worst_lvl = fabs(db(rb) - db(ra)); }
            if (verbose || !exact)
                printf("seg %3d id %6d ch%d n %7zu %s rms %6.1f/%6.1f dB (d %+5.2f) peak %6.1f/%6.1f res %6.1f dB env %4.1f band %4.1f tail %6.3f/%6.3f s\n", k, A[SA[k].a].id, c, n, exact ? "EXACT" : "diff ", db(ra), db(rb), db(rb) - db(ra), db(pa), db(pb), res, envmax, bmax, ta / 44100.0, tb / 44100.0);
        }
    }
    printf("SUMMARY: %d channel-segments, %d bit-exact; worst of the rest: level %.2f dB, residual %.1f dB under the signal, 100 ms envelope %.1f dB, band energy %.1f dB\n", ncmp, nexact, worst_lvl, worst_res, worst_env, worst_band);
    return 0;
}
