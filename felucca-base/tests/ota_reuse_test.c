/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* ota.c's staging buffers (the UFW header and the two 512-B blocks share one 1 KiB space): a synthetic package
 * (own loader and the official one) is pulled through ota_session() against a fake host and a simulated NOR, over
 * every update and error path, one line per scenario:  name rc code requests committed hash(staging area).
 * Expectations are checked here, and the group (host_tests.sh otareuse) builds this file against the tree's ota.c
 * and against the ota.c of OTA_BASE (the buffers as they were) and requires the two listings to be identical.
 *   cc -o ota_reuse_test tests/ota_reuse_test.c && ./ota_reuse_test  [-DOTA_SRC='"path/to/ota.c"']
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)   /* (the sources' .pool sections) */
#define FELUCCA_ID "FM-1_900"
#define FELUCCA_OTA_DRYRUN 0
#ifndef OTA_SRC
#define OTA_SRC "../firmware/src/ota.c"
#endif

static uint8_t nor[0x100000];
static uint8_t *logical;
static uint32_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, mute_after, prog_n, prog_fail_at, erase_n, erase_fail_at, bad_erase;
static int committed, nosuccess;
static int32_t last_code;
static uint8_t *corrupt_at;

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg\n"); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (++requests > mute_after) return 0;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        memset(m + 14, 0, len);
        if (!nosuccess) memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && corrupt_at >= logical + addr && corrupt_at < logical + addr + len)
            m[14 + (corrupt_at - (logical + addr))] ^= 0x01;
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms += 1; }
static int ota_erase(uint32_t off)
{
    if (off < 0xE0000u || off >= 0xE5000u || (off & 0xFFFu)) { bad_erase++; return -8; }
    if (++erase_n == erase_fail_at) return -8;
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < 0xE0000u || off + n > 0xE5000u) { bad_erase++; return -8; }
    if (++prog_n == prog_fail_at) return -8;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm) { (void)parm; committed = 1; }

#include OTA_SRC

#define FLASH_OFF 0x400u
#define OTA_OFF 0x4400u

/* synthetic package: UFW header with flash.bin (type 0, 16 KiB of noise) and ota.bin (type 100) */
static void build(int official)
{
    uint32_t len = official ? OTA_LDR_LEN : 0x1234u, i, x = 12345u, pre;
    uint8_t *h, *o, *e;
    logical_len = OTA_OFF + 0x20u + len;
    logical = calloc(1, logical_len);
    h = logical;
    for (i = 0; i < 0x4000u; i++) { x = x * 1103515245u + 12345u; logical[FLASH_OFF + i] = (uint8_t)(x >> 16); }
    o = logical + OTA_OFF;
    for (i = 0; i < len; i++) { x = x * 1103515245u + 12345u; o[0x20 + i] = (uint8_t)(x >> 16); }
    /* inner head (CRC over bytes 2..31, the load address 0x01C0A800 at +8) */
    memset(o + 0x20, 0, 32);
    ota_wr32(o + 0x20 + 4, 0x4000u);
    ota_wr32(o + 0x20 + 8, 0x01C0A800u);
    o[0x20 + 12] = 0x41;
    ota_wr16(o + 0x20, ota_crc16(o + 0x22, 30, 0));
    if (!official)
        memcpy(o + 0x48, "FELUCCA-LOADER-1", 16);
    else {                       /* the official loader's two CRCs: fix the last two body bytes, then outer bytes 14,15 */
        uint32_t a, b2;
        pre = ota_crc16(o + 0x20, len - 2u, 0);
        for (a = 0; a < 65536u; a++) {
            o[0x20 + len - 2] = (uint8_t)a; o[0x20 + len - 1] = (uint8_t)(a >> 8);
            if (ota_crc16(o + 0x20 + len - 2, 2, pre) == OTA_LDR_DCRC) break;
        }
        if (a == 65536u) { printf("no body fix\n"); exit(1); }
        memset(o, 0, 32);
        ota_wr16(o + 2, OTA_LDR_DCRC);
        ota_wr32(o + 4, 0x20u);
        ota_wr32(o + 8, len);
        o[12] = 0x41;
        for (b2 = 0; b2 < 65536u; b2++) {
            ota_wr16(o + 14, b2);
            if (ota_crc16(o + 2, 30, 0) == OTA_LDR_HCRC) break;
        }
        if (b2 == 65536u) { printf("no head fix\n"); exit(1); }
        ota_wr16(o, OTA_LDR_HCRC);
    }
    if (!official) {
        ota_wr16(o + 2, ota_crc16(o + 0x20, len, 0));
        ota_wr32(o + 4, 0x20u);
        ota_wr32(o + 8, len);
        o[12] = 0x41;
        ota_wr16(o, ota_crc16(o + 2, 30, 0));
    }
    /* UFW header: 2 entries, CRC over the (still ciphered) entry list, then the head ciphered */
    memset(h, 0, 0x400);
    ota_wr16(h + 8, 2);
    e = h + 0x40;
    ota_wr16(e, 0); ota_wr32(e + 8, FLASH_OFF); ota_wr32(e + 12, 0x4000u);
    ota_wr16(e + 0x50, 100); ota_wr32(e + 0x50 + 8, OTA_OFF); ota_wr32(e + 0x50 + 12, len + 0x20u);
    ota_jl_enc(e, 0x50); ota_jl_enc(e + 0x50, 0x50);
    ota_wr16(h + 2, ota_crc16(h + 0x40, 2 * 0x50u, 0));
    ota_wr16(h, ota_crc16(h + 2, 0x3E, 0));
    ota_jl_enc(h, 0x40);
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + FLASH_OFF, 0x4000);              /* the device's flash head == the package's */
    rx_full = 0; requests = 0; committed = 0; last_code = 0; now_ms = 0;
    mute_after = 0xFFFFFFFFu; corrupt_at = NULL; bad_erase = 0; nosuccess = 0;
    prog_n = erase_n = 0; prog_fail_at = erase_fail_at = 0;
}

static uint32_t area_hash(int *clean)
{
    uint32_t i, h = 2166136261u;
    *clean = 1;
    for (i = 0xE0000; i < 0xE5000; i++) { h = (h ^ nor[i]) * 16777619u; if (nor[i] != 0xFF) *clean = 0; }
    return h;
}

static int bad;
static void run(const char *name, int expect_rc, int expect_clean, int expect_commit)
{
    int rc = ota_session(), clean;
    uint32_t h = area_hash(&clean);
    int ok = rc == expect_rc && clean == expect_clean && committed == expect_commit && !bad_erase;
    printf("%-34s rc %d code %d req %u commit %d hash %08x %s\n", name, rc, last_code, requests, committed, h, ok ? "ok" : "FAIL");
    bad += !ok;
}

int main(void)
{
    int official;
    uint32_t top;
    for (official = 0; official < 2; official++) {
        const char *k = official ? "official" : "own";
        char nm[64];
        free(logical);
        build(official);
        top = OTA_OFF;
        reset_dev();
        sprintf(nm, "%s: update", k); run(nm, 0, 0, 1);
        reset_dev(); nosuccess = 1;
        sprintf(nm, "%s: host never says success", k); run(nm, -13, 1, 0);
        reset_dev(); corrupt_at = logical + top + 0x20u + 0x100u;
        sprintf(nm, "%s: body bit flip", k); run(nm, -11, 1, 0);
        reset_dev(); corrupt_at = logical + 4;
        sprintf(nm, "%s: UFW header bit flip", k); run(nm, -2, 1, 0);
        reset_dev(); corrupt_at = logical + top + 5;
        sprintf(nm, "%s: loader head bit flip", k); run(nm, -7, 1, 0);
        reset_dev(); mute_after = 1;
        sprintf(nm, "%s: host gone after 1", k); run(nm, -1, 1, 0);
        reset_dev(); mute_after = 3;
        sprintf(nm, "%s: host gone after 3", k); run(nm, official ? -4 : -6, 1, 0);
        reset_dev(); mute_after = official ? 60u : 6u;
        sprintf(nm, "%s: host gone mid body", k); run(nm, -9, 1, 0);
        reset_dev(); erase_fail_at = 2;
        sprintf(nm, "%s: erase fails", k); run(nm, -8, 1, 0);
        reset_dev(); prog_fail_at = 2;
        sprintf(nm, "%s: body write fails", k); run(nm, -10, 1, 0);
        reset_dev(); prog_fail_at = 100;
        sprintf(nm, "%s: no failure (control)", k); run(nm, 0, 0, 1);
        if (official) {
            reset_dev(); nor[0x1234] ^= 0x10;
            run("official: device head differs", -5, 1, 0);
            reset_dev(); nor[0x3FFF] ^= 0x01;
            run("official: last head byte differs", -5, 1, 0);
        }
    }
    printf("%s\n", bad ? "OTA REUSE TEST FAILED" : "ota reuse test passed");
    return bad != 0;
}
