/* SPDX-License-Identifier: GPL-3.0-only */
/* i32rec out.bin file.i32 ... : int32 L/R pair dumps (MULTI_DUMP, SOUND_DUMP) -> the records acmp reads (id 100000 + file number, sample), one segment a file */
#include <stdio.h>
#include <stdint.h>
int main(int argc, char **argv)
{
    FILE *o = fopen(argv[1], "wb");
    int i;
    for (i = 2; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        int32_t r[2], v;
        if (!f)
            continue;
        r[0] = 100000 + 20 * (i - 2);
        while (fread(&v, 4, 1, f) == 1) {
            r[1] = v;
            fwrite(r, 4, 2, o);
        }
        fclose(f);
    }
    fclose(o);
    return 0;
}
