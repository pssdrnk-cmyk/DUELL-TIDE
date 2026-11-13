/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot pack's decoder (firmware/src/ramplace.c bp_lzma, bp_hash) on the host: each file given is a boot pack (tools/bootpack.py;
 * tests/bootpack_gen.py writes them: the device build's own and synthetic ones). Checks: the stream decodes to exactly the unpacked
 * length and every section's hash is the header's; a cut stream, flipped bytes and a wrong length never decode to the right hashes and
 * never write past the output (a guard after it).
 * Built and run by DUELL/host_tests.sh (name: bootpack) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct { uint32_t pending; } bootguard;
static void fm1_enter_uboot(void) {}
uint32_t _rx_start[1], _rx_end[1], _rz_start[1], _rz_end[1], _pool_start[1], _pool_end[1];
const uint32_t _boot_pack[1];
#include "../firmware/src/ramplace.c"

static int bad, ncheck;
static void expect(const char *f, const char *w, int ok) { ncheck++; if (!ok) { printf("bootpack: %s: FAIL %s\n", f, w); bad++; } }

#define GUARD 64u
/* decode pk (len bytes) with the firmware's decoder: 0 when every section's hash is right; -1 when the guard after the output changed */
static int decode(const uint8_t *pk, size_t len, uint32_t size_delta)
{
    uint32_t hdr[BP_HDR], size, i, off = 0, ok = 1;
    uint8_t *out;
    bp_probs_t *pr = malloc(sizeof *pr);
    memcpy(hdr, pk, sizeof hdr);
    size = hdr[1] + size_delta;
    out = malloc(size + GUARD);
    memset(out + size, 0xA5, GUARD);
    if (bp_lzma(out, size, pk + 4u * BP_HDR, len - 4u * BP_HDR < hdr[2] ? (uint32_t)(len - 4u * BP_HDR) : hdr[2], pr))
        ok = 0;
    for (i = 0; ok && i < BP_NSEC; i++) {
        if (off + hdr[4 + 3 * i] > size || bp_hash((uint32_t *)(out + off), (uint32_t *)(out + off + hdr[4 + 3 * i])) != hdr[5 + 3 * i])
            ok = 0;
        off += hdr[4 + 3 * i];
    }
    for (i = 0; i < GUARD; i++)
        if (out[size + i] != 0xA5)
            ok = 2;
    free(out);
    free(pr);
    return ok == 2 ? -1 : ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    int a, k;
    for (a = 1; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        uint8_t *pk;
        long len;
        uint32_t hdr[BP_HDR];
        if (!f) {
            printf("bootpack: %s: FAIL cannot open\n", argv[a]);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        len = ftell(f);
        fseek(f, 0, SEEK_SET);
        pk = malloc((size_t)len);
        if (fread(pk, 1, (size_t)len, f) != (size_t)len)
            return 1;
        fclose(f);
        memcpy(hdr, pk, sizeof hdr);
        expect(argv[a], "magic", hdr[0] == BP_MAGIC);
        expect(argv[a], "the pack decodes to its sections", decode(pk, (size_t)len, 0) == 0);
        if (hdr[2] >= 64u)                      /* (a tiny stream's last third is only the end marker and the flush) */
            expect(argv[a], "a cut stream does not", decode(pk, (size_t)len - hdr[2] / 3u, 0) == 1);
        expect(argv[a], "one byte more expected does not", decode(pk, (size_t)len, 4) == 1);
        expect(argv[a], "one word less expected does not (and stays inside)", decode(pk, (size_t)len, (uint32_t)-4) == 1);
        for (k = 0; hdr[2] >= 64u && k < 24; k++) {   /* flipped bytes over the stream: a wrong result (and no write past the output) */
            size_t at = 4u * BP_HDR + 6u + (size_t)k * (hdr[2] - 12u) / 24u;
            uint8_t keep = pk[at];
            char w[64];
            pk[at] ^= (uint8_t)(1u << (k & 7));
            snprintf(w, sizeof w, "flipped byte at %zu", at);
            expect(argv[a], w, decode(pk, (size_t)len, 0) == 1);
            pk[at] = keep;
        }
        printf("bootpack: %s: %u B -> %u B\n", argv[a], hdr[1], hdr[2]);
        free(pk);
    }
    printf("bootpack: %d checks, %d failed\n", ncheck, bad);
    return bad != 0;
}
