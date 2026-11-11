/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: the shared header of the four commands (render, measure, rules, sweep). Plain C99, no firmware headers: lane A (render, sweep) and
 * lane B (measure, rules) are separate translation units that meet here.
 *   sl_render.c   the only file that includes the firmware (through tests/tide_roles_test.c, as the old render tools did): sl_render_main
 *   sl_sweep.c    sl_sweep_main: renders each point in its own process (`soundlab render`), measures it, prints the table
 *   sl_main.c     the dispatcher; the weak fall-backs for the lane B functions below until sl_metrics.c / sl_rules.c are built
 *   sl_metrics.c, sl_rules.c, ...   lane B: sl_measure_main, sl_rules_main, sl_measure_run, sl_sweep_print */
#ifndef SL_H
#define SL_H

typedef struct { char name[64]; double v; } sl_kv_t;      /* one flat scalar metric, e.g. "level.lufs" */
extern const char *sl_self;                              /* the program's own path (sl_main.c): sweep and `render --multi all` run it again for each render */

int sl_render_main(int argc, char **argv);               /* argv[0] = "render" */
int sl_sweep_main(int argc, char **argv);                /* argv[0] = "sweep" */

/* lane B */
int sl_measure_main(int argc, char **argv);              /* argv[0] = "measure", argv[1] = PREFIX, then [--from BAR] [--to BAR]; writes PREFIX.metrics.json and prints the one-line summary; 0 = ok */
int sl_rules_main(int argc, char **argv);                /* argv[0] = "rules", argv[1] = PREFIX */
/* the same as measure, quiet: fills kv with the flat scalar metrics (at most kvmax), returns their count, -1 on an error. from_bar / to_bar < 0: the whole file */
int sl_measure_run(const char *prefix, double from_bar, double to_bar, sl_kv_t *kv, int kvmax);
/* prints the sweep table: label (e.g. "M02 DUB K4"), the npts points (detents), the index of the 0 point (-1 if none), per point its metrics; values and delta against the 0 point */
void sl_sweep_print(const char *label, int npts, const double *points, int zero_idx, sl_kv_t **kv, const int *nkv);

#endif
