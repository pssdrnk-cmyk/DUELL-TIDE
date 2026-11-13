/* SPDX-License-Identifier: GPL-3.0-only */
/* Stand-alone front end for the metrics half (lane B), so measure / rules work without the renderer:
 *   soundlab_measure measure PREFIX [...]    soundlab_measure rules PREFIX [...] */
#include "sl_lab.h"
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "measure")) return sl_cmd_measure(argc - 2, argv + 2);
    if (argc >= 2 && !strcmp(argv[1], "rules")) return sl_cmd_rules(argc - 2, argv + 2);
    if (argc >= 4 && !strcmp(argv[1], "sweep-table")) {   /* sweep-table NAME POINT:PREFIX ... (the table of a sweep whose points are rendered and measured already) */
        int n = argc - 3, i; const char *pre[32]; double pt[32]; static char buf[32][600];
        if (n > 32) n = 32;
        for (i = 0; i < n; i++) {
            const char *c = strchr(argv[3 + i], ':');
            if (!c) { fprintf(stderr, "sweep-table: POINT:PREFIX expected\n"); return 2; }
            pt[i] = atof(argv[3 + i]); snprintf(buf[i], sizeof buf[i], "%s", c + 1); pre[i] = buf[i];
        }
        return sl_sweep_table(argv[2], pre, pt, n);
    }
    fprintf(stderr, "usage: soundlab_measure measure PREFIX [--from BAR --to BAR] [--control CTL] [--delay-ms T]\n       soundlab_measure rules PREFIX [--from BAR --to BAR] [--lufs-window LO,HI]\n");
    return 2;
}
