/* SPDX-License-Identifier: GPL-3.0-only */
/* b9 (B9-STORE): sounds saved before b8 load as they were saved. b8 widened ANALOG's WAVE (0..6 -> 0..8, ACID / ACSQ), so it takes 4 bits instead of 3 in the packed
 * values of a patch record and of an End B block (morph.c MBV); the stored form said nothing of it. Now a record names its width (PTF_W4, MBV_W4) and one without it
 * is read as the firmware before b8 wrote it.
 * The OLD records are real ones: tests/store_xver_fixture.h, written by tests/store_xver_gen.c on fork/b7 (one sound of every engine, ANALOG with every WAVE 0..6 and
 * the values after it, ANALOG with an End B that moves WAVE), with the values b7 loaded from them (the part's values, the extension slots, End B's values).
 *   A  the old records go into the store (up_put_raw): they stay as they are, and every one loads to b7's values
 *   B  new records: ACID and ACSQ (and a value after them) round-trip; their End B block with WAVE 8; both carry the width flags
 *   C  one chunk holds old and new records side by side; a new save beside the old ones and over one of them leaves every other record as it was, all equal
 *   D  an old sound loaded and saved again: the new record holds the same values
 *   E  an old End B block through the project / multi path (morph_load_part_v) gives the same End B values
 *   build/host/store_xver_test            (DUELL/host_tests.sh "storexver") */
#define PATCH_TEST_MAIN patch_main_unused
#include "patch_test.c"
#include "store_xver_fixture.h"
#include "store_xver8_fixture.h"

static void snap(uint32_t slot, int16_t *v)         /* what slot's record loads to: as the fixtures were taken */
{
    morph_part_t *mp;
    uint32_t i;
    power_cycle();
    memset(trk[0].p, 0, sizeof trk[0].p);
    if (up_load(slot) != 0) {
        memset(v, 0x55, XV_NVAL * sizeof *v);
        return;
    }
    mp = morph_ready(0);
    for (i = 0; i < P_COUNT; i++)
        v[i] = trk[0].p[i];
    for (i = 0; i < NX; i++)
        v[P_COUNT + i] = XV(&trk[0], i);
    for (i = 0; i < EXT_P_LEN; i++)
        v[P_COUNT + NX + i] = mbv_wid(i) ? (int16_t)*morph_bval(mp, i) : 0;
}
static const struct xv_case *case_of(uint32_t slot)
{
    uint32_t i;
    for (i = 0; i < XV_NCASES; i++)
        if (xv_cases[i].slot == slot)
            return &xv_cases[i];
    return 0;
}
/* how many of the old records (slots in `skip`'s complement) no longer load to b7's values / are no longer the bytes put in; first few named */
static uint32_t old_bad(uint32_t skip, uint32_t *bytes_bad)
{
    uint32_t i, n = 0, nb = 0, j;
    int16_t v[XV_NVAL];
    for (i = 0; i < XV_NCASES; i++) {
        const struct xv_case *c = &xv_cases[i];
        const uint8_t *raw;
        if (c->slot == skip)
            continue;
        snap(c->slot, v);
        if (memcmp(v, c->val, sizeof v)) {
            if (n++ < 5u)
                printf("store_xver:   slot %u (engine %u) loads to other values\n", c->slot, c->engine);
            for (j = 0; j < XV_NVAL; j++)
                if (v[j] != c->val[j]) {
                    printf("store_xver:     first difference at value %u: %d, was %d\n", j, v[j], c->val[j]);
                    break;
                }
        }
        raw = pt_raw(c->slot);
        if (!raw || pt_rd16(raw) != c->len || memcmp(raw + 4, c->rec + 4, c->len - 4u))
            nb++;
    }
    *bytes_bad = nb;
    return n;
}
static void analog_new(uint32_t wave, uint32_t slot)
{
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 1u);
    trk[0].p[P_E0] = (int16_t)wave;
    trk[0].p[P_E1] = 55;
    trk[0].p[P_E2] = 66;
    trk[0].p[P_E3] = 12;
    trk[0].p[P_E4] = 31;
    trk[0].p[P_E5] = param_desc_of(0u, P_E5)->max;   /* (its top: the range differs by engine) */
    trk[0].p[P_REL] = 47;
    trk[0].p[P_ATK] = 3;
    ck("a new ANALOG sound stores", up_store_trk(&trk[0], slot, "NEW ANALOG", 0) == 0);
}
static int analog_loads(uint32_t slot, uint32_t wave)
{
    const int16_t *p = trk[0].p;
    power_cycle();
    memset(trk[0].p, 0, sizeof trk[0].p);
    if (up_load(slot) != 0)
        return 0;
    if (p[P_E0] != (int)wave || p[P_E1] != 55 || p[P_E2] != 66 || p[P_E3] != 12 || p[P_E4] != 31 || p[P_E5] != param_desc_of(0u, P_E5)->max || p[P_REL] != 47 || p[P_ATK] != 3) {
        printf("store_xver:   slot %u loads WAVE %d DTN %d MIX %d NOIS %d E4 %d E5 %d REL %d ATK %d\n", slot, p[P_E0], p[P_E1], p[P_E2], p[P_E3], p[P_E4], p[P_E5], p[P_REL], p[P_ATK]);
        return 0;
    }
    return 1;
}

int main(void)
{
    uint32_t i, j, nb, nbad, put_ok = 0, old_flagged = 0, w4 = 0, blocks = 0, blocks_ok = 0, shared = 0;
    int16_t v[XV_NVAL];
    const struct xv_case *c;
    char msg[96];
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;

    /* A: the old records */
    for (i = 0; i < XV_NCASES; i++) {
        c = &xv_cases[i];
        put_ok += up_put_raw(c->slot, c->rec, c->len) == 0;
        old_flagged += (c->rec[4] & PTF_W4) != 0;
    }
    snprintf(msg, sizeof msg, "A: the %d records of the firmware before b8 are accepted", XV_NCASES);
    ck(msg, put_ok == XV_NCASES && !old_flagged);
    nbad = old_bad(0xFFFFu, &nb);
    ck("A: every one loads to the values b7 loaded (parts, extension slots, End B)", !nbad);
    ck("A: and is stored as it was put (the bytes after slot and revision)", !nb);

    /* B: new records */
    analog_new(7u, 60u);
    ck("B: ACID (WAVE 7) record carries PTF_W4", pt_raw(60) && (pt_raw(60)[4] & PTF_W4));
    ck("B: ACID and the values after it round-trip", analog_loads(60u, 7u));
    analog_new(8u, 61u);
    ck("B: ACSQ (WAVE 8) record carries PTF_W4", pt_raw(61) && (pt_raw(61)[4] & PTF_W4));
    ck("B: ACSQ and the values after it round-trip", analog_loads(61u, 8u));
    {
        static const xb_edit_t ed[3] = {{P_E0, 8}, {P_E1, 99}, {P_REL, 20}};
        const uint8_t *blk;
        uint32_t n;
        morph_part_t *mp;
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 1u);
        trk[0].p[P_E0] = 7;
        trk[0].p[P_E1] = 44;
        ck("B: a sound with End B (ACID to ACSQ) stores", store_with_b(&trk[0], 62u, "NEW ACID EB", ed, 3u) == 0 && pt_raw(62) && (pt_raw(62)[4] & PTF_MB));
        power_cycle();
        blk = rec_endb(62u, &n);
        ck("B: its End B block carries MBV_W4 and is a block", blk && n && (blk[0] & MBV_W4) && morph_bv_size(blk, n, 0u) == n);
        ck("B: and its record PTF_W4", (pt_raw(62)[4] & PTF_W4) != 0);
        memset(trk[0].p, 0, sizeof trk[0].p);
        ck("B: it loads", up_load(62u) == 0);
        mp = morph_ready(0);
        ck("B: Original ACID 7 / DTN 44, End B ACSQ 8 / DTN 99", trk[0].p[P_E0] == 7 && trk[0].p[P_E1] == 44 && *morph_bval(mp, P_E0) == 8 && *morph_bval(mp, P_E1) == 99);
    }

    /* C: old and new side by side in the chunks */
    nbad = old_bad(0xFFFFu, &nb);
    ck("C: after the new saves every old record is still equal", !nbad);
    ck("C: and still the bytes it was", !nb);
    for (i = 0; i < XV_NCASES; i++)
        for (j = 60; j <= 62u; j++)
            shared += pt_dir[xv_cases[i].slot].ch == pt_dir[j].ch;
    snprintf(msg, sizeof msg, "C: old and new records share a chunk (%u pairs)", shared);
    ck(msg, shared > 0);
    analog_new(8u, 26u);                              /* over an old ANALOG WAVE 6 record */
    ck("C: a new save over an old record: PTF_W4", pt_raw(26) && (pt_raw(26)[4] & PTF_W4));
    ck("C: it loads WAVE 8 and its values", analog_loads(26u, 8u));
    nbad = old_bad(26u, &nb);
    ck("C: every other old record is still equal", !nbad);
    ck("C: and still the bytes it was", !nb);
    ck("C: and the earlier new ones", analog_loads(60u, 7u) && analog_loads(61u, 8u));
    for (i = 0; i < 128u; i++)
        w4 += pt_raw(i) && (pt_raw(i)[4] & PTF_W4) != 0;
    snprintf(msg, sizeof msg, "C: %u records carry PTF_W4 (4 new)", w4);
    ck(msg, w4 == 4u);

    /* D: an old sound saved again */
    power_cycle();
    c = case_of(23u);
    ck("D: an old ANALOG WAVE 3 sound loads and saves again", up_load(23u) == 0 && up_store_trk(&trk[0], 70u, "RESAVED", 0) == 0);
    ck("D: the new record carries PTF_W4", pt_raw(70) && (pt_raw(70)[4] & PTF_W4));
    snap(70u, v);
    ck("D: and loads to the same values (parts, extension slots)", c && !memcmp(v, c->val, (P_COUNT + NX) * sizeof *v));
    c = case_of(26u);
    (void)c;

    /* E: an old End B block through the project / multi path */
    for (i = 0; i < XV_NCASES; i++) {
        const uint8_t *blk;
        uint32_t n, same = 1;
        morph_part_t *mp;
        c = &xv_cases[i];
        if (!(c->rec[4] & PTF_MB) || c->slot == 26u)
            continue;
        blocks++;
        power_cycle();
        blk = rec_endb(c->slot, &n);
        if (!blk || !n || (blk[0] & MBV_W4) || morph_bv_size(blk, n, c->engine) != n)
            continue;
        memset(trk[0].p, 0, sizeof trk[0].p);
        if (up_load(c->slot) != 0)
            continue;
        morph_load_part_v(0, blk);
        mp = morph_ready(0);
        for (j = 0; j < EXT_P_LEN; j++)
            if (mbv_wid(j) && *morph_bval(mp, j) != c->val[P_COUNT + NX + j])
                same = 0;
        blocks_ok += same;
    }
    snprintf(msg, sizeof msg, "E: %u old End B blocks (no MBV_W4) read through morph_load_part_v give b7's End B", blocks);
    ck(msg, blocks >= 3u && blocks_ok == blocks);

    /* F: records written by b8 rc1 / rc2 (tests/store_xver8_fixture.h, store_xver8_gen.c on fork/b8-acid-integ ebc63ca): ANALOG WAVE in 4 bits, no width flag. The first XV8_PLAIN have no WAVE in
     * them (WAVE 0 in the values and in End B; another engine): the store reads them as b8 did. The others hold WAVE 3, 7, 8 (also in End B): the record does not say its width, so the
     * store reads them as the 3-bit ones of b7 (B9-STORE, "ambiguity"; none of the owner's backups holds one): pinned here is what the store does today: 4 of the 5 are accepted (the WAVE 7 one is refused), the accepted stay the bytes they were, load
     * without harm, and none gives b8's values. */
    {
        enum { XV8_PLAIN = 3 };
        uint32_t plain_ok = 0, plain_bytes = 0, plain_flag = 0, plain_put = 0, amb_put = 0, amb_bytes = 0, amb_eq = 0, amb_load = 0;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        for (i = 0; i < XV8_NCASES; i++) {
            c = &xv8_cases[i];
            if (up_put_raw(c->slot, c->rec, c->len) == 0) {
                if (i < XV8_PLAIN)
                    plain_flag += (c->rec[4] & PTF_W4) != 0, plain_put++;
                else
                    amb_put++;
            } else
                printf("store_xver:   b8 record %u (slot %u): refused by the store\n", i, c->slot);
        }
        for (i = 0; i < XV8_NCASES; i++) {
            const uint8_t *raw;
            c = &xv8_cases[i];
            snap(c->slot, v);
            raw = pt_raw(c->slot);
            if (i < XV8_PLAIN) {
                plain_ok += !memcmp(v, c->val, sizeof v);
                if (memcmp(v, c->val, sizeof v)) {
                    for (j = 0; j < XV_NVAL && v[j] == c->val[j]; j++)
                        ;
                    printf("store_xver:   b8 record %u (slot %u): first difference at value %u: %d, b8 had %d\n", i, c->slot, j, j < XV_NVAL ? v[j] : 0, j < XV_NVAL ? c->val[j] : 0);
                }
                plain_bytes += raw && pt_rd16(raw) == c->len && !memcmp(raw + 4, c->rec + 4, c->len - 4u);
            } else {
                amb_bytes += raw && pt_rd16(raw) == c->len && !memcmp(raw + 4, c->rec + 4, c->len - 4u);
                amb_eq += !memcmp(v, c->val, sizeof v);
                amb_load += v[0] != 0x5555;
                printf("store_xver:   b8 record %u (slot %u): loads %s b8's values\n", i, c->slot, !memcmp(v, c->val, sizeof v) ? "to" : "NOT to");
            }
        }
        snprintf(msg, sizeof msg, "F: the %d b8 rc1 / rc2 records without WAVE are accepted, none carries PTF_W4", XV8_PLAIN);
        ck(msg, plain_flag == 0 && plain_put == XV8_PLAIN);
        ck("F: the records without ANALOG WAVE load to the values b8 loaded (parts, extension slots, End B)", plain_ok == XV8_PLAIN);
        ck("F: and stay the bytes they were", plain_bytes == XV8_PLAIN);
        printf("store_xver:   b8 records with WAVE: %u of %u accepted, %u as put, %u load to b8's values, %u load\n", amb_put, XV8_NCASES - XV8_PLAIN, amb_bytes, amb_eq, amb_load);
        ck("F: known ambiguity (B9-STORE): the b8 records with WAVE are read as b7's 3-bit ones: 4 of 5 are accepted (the ACID one is refused), the accepted stay the bytes put, none loads to "
           "b8's values (a better reading changes these numbers: update them with it)", amb_put == 4u && amb_bytes == amb_put && amb_eq == 0u && amb_load == amb_put);
    }

    printf("store_xver: %d FAIL\n", bad);
    return bad != 0;
}
