#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static FILE *dmp_f; static int dmp_sync = -1; static const char *dmp_lastf; static int dmp_fid;
static inline int32_t dmp_rec(const char *file, int line, int32_t v)
{
    int32_t r[2];
    if (!dmp_f) { const char *p = getenv("DMP_FILE"); if (!p) return v; dmp_f = fopen(p, getenv("DMP_SYNC") ? "ab" : "wb"); if (!dmp_f) return v; }
    if (file != dmp_lastf) { dmp_lastf = file; dmp_fid = strstr(file, "mode_scn") ? 100000 : 0; }
    r[0] = line + dmp_fid; r[1] = v; fwrite(r, 4, 2, dmp_f); if (dmp_sync < 0) dmp_sync = getenv("DMP_SYNC") != 0; if (dmp_sync) fflush(dmp_f);
    return v;
}
#define DMP(v) dmp_rec(__FILE__, __LINE__, (int32_t)(v))
