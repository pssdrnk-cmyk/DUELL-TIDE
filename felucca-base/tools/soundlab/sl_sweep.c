/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab sweep: one knob (or any render option) at several points. Each point is rendered by `soundlab render` in a process of its own (the firmware's state
 * is not reset by a second load), measured by lane B's sl_measure_prefix, and the table printed by sl_sweep_table (values and the delta against the 0 point).
 *   soundlab sweep --multi 2 --knob DUB:4 [--points -32,-16,0,16,32] [--out DIR] [render options]
 * The defaults are those of work/b9/knobstr (the knob lane): a held C4 Eb4 G4, 4 bars, the transport running, 6 s of tail; a render option given here replaces them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sl.h"

#define SW_MAXPTS 32

static int sw_has(int argc, char **argv, const char *opt)
{
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], opt))
            return 1;
    return 0;
}
static int sw_sys(const char *cmd)
{
#ifdef _WIN32
    char buf[8200];
    snprintf(buf, sizeof buf, "\"%s\"", cmd);
    return system(buf);
#else
    return system(cmd);
#endif
}
int sl_sweep_main(int argc, char **argv)
{
    char label[96] = "sweep", dir[512] = "", knob[64] = "", pts[256] = "-32,-16,0,16,32", multi[16] = "", cmd[8192], rest[4096] = "";
    double points[SW_MAXPTS];
    char pre[SW_MAXPTS][600];
    const char *prefixes[SW_MAXPTS];
    int npts = 0, i, rc = 0;
    char *tok;
    for (i = 1; i < argc; i++) {
        const char *k = argv[i];
        if (!strcmp(k, "--knob") && i + 1 < argc)
            snprintf(knob, sizeof knob, "%s", argv[++i]);
        else if (!strcmp(k, "--points") && i + 1 < argc)
            snprintf(pts, sizeof pts, "%s", argv[++i]);
        else if (!strcmp(k, "--out") && i + 1 < argc)
            snprintf(dir, sizeof dir, "%s", argv[++i]);
        else if (!strcmp(k, "--multi") && i + 1 < argc) {
            snprintf(multi, sizeof multi, "%s", argv[++i]);
            strncat(rest, " --multi ", sizeof rest - strlen(rest) - 1);
            strncat(rest, multi, sizeof rest - strlen(rest) - 1);
        } else {
            strncat(rest, " \"", sizeof rest - strlen(rest) - 1);
            strncat(rest, argv[i], sizeof rest - strlen(rest) - 1);
            strncat(rest, "\"", sizeof rest - strlen(rest) - 1);
        }
    }
    if (!knob[0] || (!multi[0] && !sw_has(argc, argv, "--sound"))) {
        fprintf(stderr, "usage: soundlab sweep --multi N --knob SET:K [--points -32,-16,0,16,32] [--out DIR] [render options]\n       SET: AUTO STD DUB HSE TECH ACID ELEC AMB, K: 2..4\n");
        return 2;
    }
    if (!dir[0])
        snprintf(dir, sizeof dir, "soundlab_sweep_%s_%s", multi[0] ? multi : "sound", knob);
    for (i = 0; dir[i]; i++)
        if (dir[i] == ':')
            dir[i] = '_';
    snprintf(label, sizeof label, "%s %s", multi[0] ? multi : "sound", knob);
    for (tok = strtok(pts, ","); tok && npts < SW_MAXPTS; tok = strtok(0, ","))
        points[npts++] = atof(tok);
    if (!npts) {
        fprintf(stderr, "soundlab sweep: no points\n");
        return 2;
    }
    snprintf(cmd, sizeof cmd, "mkdir \"%s\" 2>%s", dir, "/dev/null");
#ifdef _WIN32
    snprintf(cmd, sizeof cmd, "if not exist \"%s\" mkdir \"%s\"", dir, dir);
#endif
    sw_sys(cmd);
    for (i = 0; i < npts; i++) {
        char *prefix = pre[i];
        int n;
        snprintf(prefix, 600, "%s/p%+d", dir, (int)points[i]);
        prefixes[i] = prefix;
        n = snprintf(cmd, sizeof cmd, "\"%s\" render%s", sl_self, rest);
        if (!sw_has(argc, argv, "--keys"))
            n += snprintf(cmd + n, sizeof cmd - (size_t)n, " --keys chord:C4,Eb4,G4");
        if (!sw_has(argc, argv, "--bars"))
            n += snprintf(cmd + n, sizeof cmd - (size_t)n, " --bars 4");
        if (!sw_has(argc, argv, "--ringout"))
            n += snprintf(cmd + n, sizeof cmd - (size_t)n, " --ringout 6");
        snprintf(cmd + n, sizeof cmd - (size_t)n, " --knob %s:%+d --out \"%s\"", knob, (int)points[i], prefix);
        if (sw_sys(cmd) != 0) {
            fprintf(stderr, "soundlab sweep: the render of point %+d failed\n", (int)points[i]);
            return 1;
        }
        if (sl_measure_prefix(prefix, -1, -1, 0) != 0) {
            fprintf(stderr, "soundlab sweep: the measurement of point %+d failed\n", (int)points[i]);
            return 1;
        }
    }
    rc = sl_sweep_table(label, prefixes, points, npts);
    return rc;
}
