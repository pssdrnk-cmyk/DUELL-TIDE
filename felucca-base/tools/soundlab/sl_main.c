/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: the dispatcher. `soundlab render|measure|rules|sweep ...` (see README.md). Plain C99, no firmware.
 * The weak functions below are fall-backs for lane B's: the real ones in sl_metrics.c / sl_rules.c replace them at link time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sl.h"

const char *sl_self = "soundlab";                      /* argv[0]: sweep and `render --multi all` start the program again for each render */

__attribute__((weak)) int sl_measure_main(int argc, char **argv)
{
    (void)argc, (void)argv;
    fprintf(stderr, "soundlab measure: not built into this binary (sl_metrics.c is missing)\n");
    return 2;
}
__attribute__((weak)) int sl_rules_main(int argc, char **argv)
{
    (void)argc, (void)argv;
    fprintf(stderr, "soundlab rules: not built into this binary (sl_rules.c is missing)\n");
    return 2;
}
__attribute__((weak)) int sl_measure_run(const char *prefix, double from_bar, double to_bar, sl_kv_t *kv, int kvmax)
{
    (void)prefix, (void)from_bar, (void)to_bar, (void)kv, (void)kvmax;
    fprintf(stderr, "soundlab: measure is not built into this binary (sl_metrics.c is missing)\n");
    return -1;
}
/* the plain table: a row a metric, a column a point, then the deltas against the 0 point */
__attribute__((weak)) void sl_sweep_print(const char *label, int npts, const double *points, int zero_idx, sl_kv_t **kv, const int *nkv)
{
    int i, j, m;
    printf("%s sweep (detents)\n%-40s", label, "metric");
    for (i = 0; i < npts; i++)
        printf(" %10.0f", points[i]);
    printf("\n");
    for (m = 0; m < nkv[0]; m++) {
        printf("%-40s", kv[0][m].name);
        for (i = 0; i < npts; i++) {
            double v = 0.0 / 0.0;
            for (j = 0; j < nkv[i]; j++)
                if (!strcmp(kv[i][j].name, kv[0][m].name)) {
                    v = kv[i][j].v;
                    break;
                }
            printf(" %10.3f", v);
        }
        printf("\n");
    }
    (void)zero_idx;
}

static void usage(void)
{
    puts("soundlab: one sound-check tool (felucca-base/tools/soundlab/README.md)\n"
         "  soundlab render  --multi N | --sound N  [options]  --out PREFIX    -> PREFIX.wav, PREFIX.json   (soundlab render with no option: all options)\n"
         "  soundlab measure PREFIX [--from BAR --to BAR]                     -> PREFIX.metrics.json\n"
         "  soundlab rules   PREFIX                                           -> PASS / FAIL / UNSURE per measurable rule of work/b9/MUSICAL_RULES_B9.md\n"
         "  soundlab sweep   --multi N --knob SET:K [--points -32,-16,0,16,32] [render options]   -> renders and measures each point, prints the table");
}
int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage();
        return argc < 2 ? 2 : 0;
    }
    sl_self = argv[0];
    if (!strcmp(argv[1], "render"))
        return sl_render_main(argc - 1, argv + 1) == 0 ? 0 : 1;
    if (!strcmp(argv[1], "sweep"))
        return sl_sweep_main(argc - 1, argv + 1);
    if (!strcmp(argv[1], "measure"))
        return sl_measure_main(argc - 1, argv + 1);
    if (!strcmp(argv[1], "rules"))
        return sl_rules_main(argc - 1, argv + 1);
    fprintf(stderr, "soundlab: unknown command %s\n", argv[1]);
    usage();
    return 2;
}
