/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the bench build's way back (firmware/src/bench_updl.c, bench.c `bench updl`): the normal firmware's ota_session() stages a synthetic package's Felucca loader into a simulated
 * NOR (tests/ota_reuse_test.c's harness), the way the real unit was left after the bench build was installed (the bench build has no updater and never erases the area; the loader erased only its
 * record sector). Then: the record bu_parm builds is ota.c's own, byte for byte (the flash copy ota_session wrote at 0xE4F00); bu_check accepts the staged loader against the package's own
 * loader CRC (ota.bin's data CRC) with and without the record sector; and every damaged or foreign state is refused: wrong CRC, a flipped body bit, a flipped head bit, no head, a head
 * whose length is out of range, the loader's own header damaged, a loader without Felucca's marker (the official one). Nothing is written by bu_check (the NOR is hashed before and after).
 *   cc -o bench_updl_test tests/bench_updl_test.c && ./bench_updl_test */
#define main reuse_main
#include "ota_reuse_test.c"
#undef main
#include "../firmware/src/bench_updl.c"

static int nor_rd(uint32_t off, void *p, uint32_t n)
{
    if (off + n > sizeof nor)
        return -1;
    memcpy(p, nor + off, n);
    return 0;
}
static uint32_t nor_hash(void)
{
    uint32_t i, h = 2166136261u;
    for (i = 0; i < sizeof nor; i++)
        h = (h ^ nor[i]) * 16777619u;
    return h;
}
static int t_bad;
static void expect(const char *what, int got, int want)
{
    int ok = got == want;
    printf("%-64s %d (want %d) %s\n", what, got, want, ok ? "ok" : "FAIL");
    t_bad += !ok;
}
static void stage(int official)               /* the unit after the normal firmware's OTA session and the loader's own cleanup */
{
    free(logical);
    build(official);
    reset_dev();
    if (ota_session() != 0 || !committed) {
        printf("ota_session did not stage\n");
        exit(1);
    }
}

int main(void)
{
    uint8_t parm[BU_PARM_LEN];
    uint32_t want, h0, i;
    stage(0);
    want = ota_rd16(logical + OTA_OFF + 2);                /* ota.bin's data CRC: what the host sends */
    bu_parm(parm);
    expect("the record is ota.c's own (flash copy at 0xE4F00)", memcmp(parm, nor + 0xE4F00, BU_PARM_LEN) != 0, 0);
    expect("record: CRC over bytes 2..79, tag 0x5441, loader at 0xE0000, identity", bu_rd16(parm) == ota_crc16(parm + 2, 78, 0) && bu_rd16(parm + 6) == 0x5441u && bu_rd32(parm + 72) == 0xE0000u &&
           !memcmp(parm + 8, "ota-FM-1_015", 12), 1);
    expect("the layout constants are ota.c's", BU_AREA == OTA_AREA && BU_AREA_LEN == OTA_AREA_LEN && BU_RES == OTA_RES, 1);
    h0 = nor_hash();
    expect("staged loader accepted against the package's loader CRC", bu_check(nor_rd, want), 0);
    expect("bu_check writes nothing", nor_hash() == h0, 1);
    memset(nor + 0xE4000, 0xFF, 0x1000);                   /* the loader's own cleanup after a finished install: its record sector erased */
    expect("..also after the loader erased its record (the bench build's situation)", bu_check(nor_rd, want), 0);
    expect("a different loader CRC is refused", bu_check(nor_rd, want ^ 1u), BU_E_CRC);
    nor[0xE0100] ^= 0x04;
    expect("a flipped body bit is refused", bu_check(nor_rd, want), BU_E_CRC);
    nor[0xE0100] ^= 0x04;
    nor[0xE0000 + 20] ^= 0x01;                             /* the head's name */
    expect("a flipped head bit is refused", bu_check(nor_rd, want), BU_E_HEAD);
    nor[0xE0000 + 20] ^= 0x01;
    expect("restored: accepted again", bu_check(nor_rd, want), 0);
    {   /* a head with a valid CRC and a length out of range */
        uint8_t save[32];
        memcpy(save, nor + 0xE0000, 32);
        ota_wr32(nor + 0xE0008, 0x6000u);
        ota_wr16(nor + 0xE0000, ota_crc16(nor + 0xE0002, 30, 0));
        expect("a length past the staging area is refused", bu_check(nor_rd, want), BU_E_LEN);
        ota_wr32(nor + 0xE0008, 0x100u);
        ota_wr16(nor + 0xE0000, ota_crc16(nor + 0xE0002, 30, 0));
        expect("a length under the loader's own head is refused", bu_check(nor_rd, want), BU_E_LEN);
        memcpy(nor + 0xE0000, save, 32);
    }
    nor[0xE0020 + 1] ^= 0x01;                              /* the loader's own header */
    expect("the loader's own header damaged is refused", bu_check(nor_rd, want), BU_E_LOADER);
    nor[0xE0020 + 1] ^= 0x01;
    expect("restored again: accepted", bu_check(nor_rd, want), 0);
    for (i = 0; i < 0x5000u; i++)
        nor[0xE0000 + i] = 0xFF;
    expect("no staged loader (the area erased: a normal boot's cleanup) is refused", bu_check(nor_rd, want), BU_E_HEAD);
    stage(1);
    expect("the official loader (no Felucca marker) is refused", bu_check(nor_rd, ota_rd16(logical + OTA_OFF + 2)), BU_E_LOADER);
    printf("%s\n", t_bad || bad ? "BENCH UPDL TEST FAILED" : "bench updl test passed");
    return t_bad != 0 || bad != 0;
}
