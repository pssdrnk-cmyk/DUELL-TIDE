/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab: the dispatcher. `soundlab render|measure|rules|sweep ...` (see README.md). Plain C99, no firmware.
 * The weak functions below are fall-backs for lane B's: the real ones in sl_metrics.c / sl_rules.c replace them at link time. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sl.h"

const char *sl_self = "soundlab";                      /* argv[0]: sweep and `render --multi all` start the program again for each render */

__attribute__((weak)) int sl_cmd_measure(int argc, char **argv)
{
    (void)argc, (void)argv;
    fprintf(stderr, "soundlab measure: not built into this binary (sl_metrics.c is missing)\n");
    return 2;
}
__attribute__((weak)) int sl_cmd_rules(int argc, char **argv)
{
    (void)argc, (void)argv;
    fprintf(stderr, "soundlab rules: not built into this binary (sl_rules.c is missing)\n");
    return 2;
}
__attribute__((weak)) int sl_measure_prefix(const char *prefix, int from_bar, int to_bar, const char *control)
{
    (void)prefix, (void)from_bar, (void)to_bar, (void)control;
    fprintf(stderr, "soundlab: measure is not built into this binary (sl_metrics.c is missing)\n");
    return 2;
}
__attribute__((weak)) int sl_sweep_table(const char *name, const char *const *prefixes, const double *points, int n)
{
    int i;
    printf("%s sweep: rendered and measured (no table: sl_metrics.c is missing)\n", name);
    for (i = 0; i < n; i++)
        printf("  %+.0f  %s\n", points[i], prefixes[i]);
    return 0;
}

static void usage(void)
{
    puts("soundlab: one sound-check tool (felucca-base/tools/soundlab/README.md)\n"
         "  soundlab render  --multi N | --sound N  [options]  --out PREFIX    -> PREFIX.wav, PREFIX.json   (soundlab render with no option: all options)\n"
         "  soundlab measure PREFIX [--from BAR --to BAR]                     -> PREFIX.metrics.json\n"
         "  soundlab rules   PREFIX                                           -> PASS / FAIL / UNSURE per measurable rule of work/b9/MUSICAL_RULES_B9.md\n"
         "  soundlab atlas   [--only multis|sounds|patterns] | --find \"pad dark sub\"    -> tools/soundlab/atlas/ (every sound, multi and pattern measured)\n"
         "  soundlab sweep   --multi N --knob SET:K [--points -32,-16,0,16,32] [render options]   -> renders and measures each point, prints the table");
}
#ifdef _WIN32
#define SL_PY "python"                                   /* (cmd.exe: the venv python.exe comes first on the PATH jam/env.sh sets; a bare python3 is the Store stub) */
#else
#define SL_PY "python3"
#endif
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
        return sl_cmd_measure(argc - 2, argv + 2);
    if (!strcmp(argv[1], "rules"))
        return sl_cmd_rules(argc - 2, argv + 2);
    if (!strcmp(argv[1], "atlas")) {                    /* the atlas builder is tools/soundlab/atlas.py (stdlib Python), run on this program */
        char cmd[8192], dir[4096];
        size_t n, i;
        int k;
        snprintf(dir, sizeof dir, "%s", sl_self);
        for (n = strlen(dir); n > 0 && dir[n - 1] != '/' && dir[n - 1] != '\\'; n--)
            ;
        dir[n] = 0;                                     /* felucca-base/build/soundlab/ */
        snprintf(cmd, sizeof cmd, SL_PY " \"%s../../tools/soundlab/atlas.py\" --exe \"%s\"", dir, sl_self);
        for (k = 2; k < argc; k++) {
            i = strlen(cmd);
            snprintf(cmd + i, sizeof cmd - i, " \"%s\"", argv[k]);
        }
#ifdef _WIN32
        {
            char b[8300];
            snprintf(b, sizeof b, "\"%s\"", cmd);       /* (cmd.exe wants the whole line in one more pair of quotes) */
            return system(b);
        }
#else
        return system(cmd);
#endif
    }
    fprintf(stderr, "soundlab: unknown command %s\n", argv[1]);
    usage();
    return 2;
}
