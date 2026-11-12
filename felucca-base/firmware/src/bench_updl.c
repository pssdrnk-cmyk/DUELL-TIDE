/* SPDX-License-Identifier: GPL-3.0-only */
/* Bench builds only (bench.c `bench updl`, tools/tide_measure.py, jam_dev.py measure): the way back from the bench build to the normal firmware.
 * The bench build is made without the updater (FELUCCA_OTA=0: it does not fit the app slot with it), so it cannot stage the update loader itself. But the
 * normal firmware that installed it DID stage the package's Felucca update loader at OTA_AREA (ota.c, 0xE0000, JLFS head "LOADER.BIN" + the loader), and a
 * build without the updater never erases that area (ota_boot_cleanup is the updater's). The loader erases only its record (0xE4F00) when it is done, so the loader
 * is still there when the bench build runs. `bench updl CRC16 yes` checks that the staged loader is whole (head, length, body CRC16 equal to the package's own
 * ota.bin CRC that the host sends, the loader's marker), builds the same UPDATA_PARM record ota.c ota_session builds and enters the loader, which then waits
 * for the host as after any cut-off update (fm1_install.py: "in update mode: the write will be finished").
 * The functions are pure (flash reads through a hook) so tests/ota_test.c can run them against ota.c's own record on a simulated NOR. */
#ifndef BU_FN
#define BU_FN static
#endif
#define BU_AREA 0x000E0000u                              /* ota.c OTA_AREA, OTA_AREA_LEN, OTA_RES: the same places */
#define BU_AREA_LEN 0x5000u
#define BU_RES (BU_AREA + BU_AREA_LEN - 0x100u)
#define BU_PARM_LEN 112u
#define BU_E_READ (-1)                                   /* the codes of bu_check */
#define BU_E_HEAD (-2)
#define BU_E_LEN (-3)
#define BU_E_CRC (-4)
#define BU_E_LOADER (-5)
typedef int (*bu_read_t)(uint32_t off, void *p, uint32_t n);
BU_FN uint32_t bu_rd16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
BU_FN uint32_t bu_rd32(const uint8_t *p) { return bu_rd16(p) | bu_rd16(p + 2) << 16; }
BU_FN uint32_t bu_crc16(const uint8_t *p, uint32_t n, uint32_t c)   /* poly 0x1021, init 0 (ota.c ota_crc16) */
{
    uint32_t i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}
/* the record of ota.c ota_session (the UPDATA_PARM record), byte for byte */
BU_FN void bu_parm(uint8_t *parm)
{
    static const char ID[12] = "ota-FM-1_015";           /* (the loader's own USB identity) */
    uint32_t i, c;
    for (i = 0; i < BU_PARM_LEN; i++)
        parm[i] = 0;
    parm[2] = 0x0D, parm[3] = 0x5A;                      /* 0x5A0D */
    parm[4] = 0x01, parm[5] = 0x5A;                      /* 0x5A01 */
    parm[6] = 0x41, parm[7] = 0x54;                      /* 0x5441 */
    for (i = 0; i < 12u; i++)
        parm[8 + i] = (uint8_t)ID[i];
    parm[72] = (uint8_t)BU_AREA, parm[73] = (uint8_t)(BU_AREA >> 8), parm[74] = (uint8_t)(BU_AREA >> 16), parm[75] = (uint8_t)(BU_AREA >> 24);
    c = bu_crc16(parm + 2, 78, 0);
    parm[0] = (uint8_t)c, parm[1] = (uint8_t)(c >> 8);
}
/* the staged loader whole? want: the CRC16 of the loader body (the package's ota.bin: its data CRC, bytes 2..3). 0, or a BU_E_* */
BU_FN int bu_check(bu_read_t rd, uint32_t want)
{
    uint8_t h[32], b[256];
    static const char NAME[10] = "LOADER.BIN", MARK[16] = "FELUCCA-LOADER-1";
    uint32_t len, i, j, crc = 0, mark = 0;
    if (rd(BU_AREA, h, 32))
        return BU_E_READ;
    for (i = 0; i < 10u; i++)
        if (h[16 + i] != (uint8_t)NAME[i])
            return BU_E_HEAD;
    if (bu_crc16(h + 2, 30, 0) != bu_rd16(h) || bu_rd32(h + 4) != BU_AREA || h[12] != 0x41u)
        return BU_E_HEAD;
    len = bu_rd32(h + 8);
    if (len < 32u + 0x200u || len - 32u > BU_AREA_LEN - 0x200u)
        return BU_E_LEN;
    len -= 32u;
    for (i = 0; i < len; i += j) {
        j = len - i > sizeof b ? (uint32_t)sizeof b : len - i;
        if (rd(BU_AREA + 32u + i, b, j))
            return BU_E_READ;
        crc = bu_crc16(b, j, crc);
        if (i == 0) {                                    /* the loader's own header and marker (ota.c ota_stage: crt0_ldr.S's first literals) */
            uint32_t k, m;
            if (j < 0x80u || bu_crc16(b + 2, 30, 0) != bu_rd16(b) || bu_rd32(b + 8) != 0x01C0A800u)
                return BU_E_LOADER;
            for (k = 0x20; k + 16u <= 0x80u && !mark; k++) {
                for (m = 0; m < 16u && b[k + m] == (uint8_t)MARK[m]; m++)
                    ;
                mark = m == 16u;
            }
        }
    }
    if (!mark)
        return BU_E_LOADER;
    return crc == want ? 0 : BU_E_CRC;
}
