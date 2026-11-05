/* SPDX-License-Identifier: GPL-3.0-only */
/* mkdir -p for the test programs, without a shell (system("mkdir -p '...'") fails under Windows' cmd.exe, and its mkdir takes one argument): 0 on success. */
#ifndef MKDIRS_H
#define MKDIRS_H
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIRS_ONE(p) _mkdir(p)
#else
#define MKDIRS_ONE(p) mkdir((p), 0755)
#endif
static int mkdirs(const char *path)
{
    char b[1024];
    size_t i, n = strlen(path);
    if (n == 0 || n >= sizeof b)
        return -1;
    memcpy(b, path, n + 1);
    for (i = 1; i <= n; i++)
        if (b[i] == '/' || b[i] == 92 /* backslash */ || b[i] == 0) {
            char c = b[i];
            b[i] = 0;
            if (MKDIRS_ONE(b) && errno != EEXIST)
                return -1;
            b[i] = c;
        }
    return 0;
}
#endif
