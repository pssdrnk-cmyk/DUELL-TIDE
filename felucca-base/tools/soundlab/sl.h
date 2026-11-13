/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: the shared header of the four commands (render, measure, rules, sweep). Plain C99, no firmware headers: lane A (render, sweep) and
 * lane B (measure, rules) are separate translation units that meet here.
 *   sl_render.c   the only file that includes the firmware (tests/tide_roles_test.c as a library, as the old render tools did): sl_render_main
 *   sl_sweep.c    sl_sweep_main: renders each point in its own process (`soundlab render`), measures it, prints the table
 *   sl_main.c     the dispatcher; the weak fall-backs for the lane B functions below until sl_metrics.c / sl_rules.c are built
 *   sl_metrics.c, sl_rules.c, ...   lane B: the four functions below */
#ifndef SL_H
#define SL_H

extern const char *sl_self;                              /* the program's own path (sl_main.c): sweep and `render --multi all` run it again for each render */

int sl_render_main(int argc, char **argv);               /* argv[0] = "render" */
int sl_params_main(int argc, char **argv);               /* the parameter descriptors of every engine (deadparams.py reads them) */
int sl_wavdiff_main(int argc, char **argv);              /* soundlab wavdiff A.wav B.wav: same / diff, rms and centroid change */
int sl_sweep_main(int argc, char **argv);                /* argv[0] = "sweep" */

/* lane B. argv is what follows the subcommand: PREFIX [--from BAR --to BAR] [--control CTLPREFIX]; 0 = ok */
int sl_cmd_measure(int argc, char **argv);
int sl_cmd_rules(int argc, char **argv);
/* writes prefix.metrics.json; from_bar / to_bar < 0: the whole file; control: the control render's prefix or NULL; 0 = ok */
int sl_measure_prefix(const char *prefix, int from_bar, int to_bar, const char *control);
/* prints the sweep table (values and the delta against the 0 point) for the rendered and measured points */
int sl_sweep_table(const char *name, const char *const *prefixes, const double *points, int n);

#endif
