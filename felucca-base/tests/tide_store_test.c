/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's settings in the settings record's reserved tail (D11 of the 2026-11-04 audit; tide_store.c, settings_persist.c; tests/tide_keep_test.c has the debounce and ERASE DATA):
 *   migrate   a unit updated from an earlier TIDE build: its record in object 48 is read once, copied into the settings record, never written again
 *   chunk21   a patch record a stock 1.1 unit kept in chunk 21 (D03): not read as a TIDE record, and no TIDE setting ever writes that object
 *   forward   fields past the ones this build knows (a newer build's) survive a save, a backup and a restore (D09)
 *   backup    the backup's settings object carries the fields; a restore brings them back, in RAM and in the flash
 *   old       a settings record made by 6fbd939 (before this change; tests/settings_6fbd939.bin, the firmware's own export, tail all zero): accepted, the unit's own TIDE
 *             settings stay, and the next settings save writes them into the flash too */
#define BACKUP_TEST_MAIN backup_main
#include "backup_test.c"
#include "../firmware/src/tide_store.c"

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidestore: FAIL %s\n", s); bad++; } } while (0)

static void fresh(void)
{
    reset();
    memset(tide_tail, 0, TIDE_TAIL_N);
    memset(tide_tail_saved, 0, TIDE_TAIL_N);
    tide_store_dirty = 0;
    tide_tail_new = 0;
    persist_pending = 0;
    erase_at = 0;
    flash_ok = 1;
}
/* the settings record as the flash holds it */
static int flash_rec(persist_t *p) { return st_load(OBJ_SETTINGS, p, sizeof *p) == (int)sizeof *p; }
static int tail_is(const persist_t *p, const uint8_t *fields, uint32_t n)
{
    return !memcmp(p->map_tail + TIDE_TAIL_AT, "TIDE", 4) && p->map_tail[TIDE_TAIL_AT + 4] == n && !memcmp(p->map_tail + TIDE_TAIL_AT + 5, fields, n);
}
static void settle(void)                         /* the 2 s debounce passes, the transport is stopped: the record is written */
{
    fm1_ms += 2500u;
    song.playing = 0;
    tide_store_poll();
    settings_poll();
}
static int upstream_reserve_zero(const persist_t *p)
{
    uint32_t i, z = 1;
    for (i = 0; i < TIDE_TAIL_AT; i++)
        z &= !p->map_tail[i];
    return (int)z;
}

int main(void)
{
    static const uint8_t F[12] = {2, 3, 1, 2, 1, 1, 3, 2, 1, 1, 2, 4}, G[12] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    uint8_t b[16], rec[5 + 12] = {'T', 'I', 'D', 'E', 1};
    persist_t p, q;
    static uint8_t old_sector[0x2000];

    /* migrate: an earlier build's object 48 */
    fresh();
    memcpy(rec + 5, F, 12);
    CHECK("migrate: the old record is in the flash", st_save(OBJ_TIDE, rec, sizeof rec) == 0);
    memcpy(old_sector, nor + 0xC4000, sizeof old_sector);
    memset(b, 0xEE, sizeof b);
    CHECK("migrate: no TIDE record in the settings: the old one is read (12 fields)", tide_store_load_min(b, 12, 7) == 12u && !memcmp(b, F, 12));
    CHECK("  .. and written into the settings record, which the flash then holds", !persist_pending && flash_rec(&p) && tail_is(&p, F, 12) && upstream_reserve_zero(&p));
    CHECK("  .. object 48 itself is not written", !memcmp(old_sector, nor + 0xC4000, sizeof old_sector));
    st_wipe(OBJ_TIDE);
    memset(b, 0xEE, sizeof b);
    CHECK("  .. the next read comes from the settings alone (object 48 gone)", tide_store_load_min(b, 12, 7) == 12u && !memcmp(b, F, 12));
    memset(tide_tail, 0, TIDE_TAIL_N);                  /* a power cycle: the RAM is gone, the flash record is read back (persist_boot) */
    CHECK("  .. after a power cycle (the settings record imported)", flash_rec(&p) && settings_import(&p, sizeof p) && tide_store_load(b, 12) && !memcmp(b, F, 12));
    persist_note(&p);
    persist_base(&q);
    settings_export(&q);
    CHECK("  .. and nothing is written again for it (the exported record equals the saved one)", persist_is_saved(&q));
    fresh();
    rec[4] = 2;                                          /* an object 48 of another version is no record */
    st_save(OBJ_TIDE, rec, sizeof rec);
    CHECK("migrate: another version in object 48: the defaults", tide_store_load_min(b, 12, 7) == 0u && !persist_pending);
    fresh();
    rec[4] = 1;
    st_save(OBJ_TIDE, rec, 5 + 5);
    CHECK("migrate: a record shorter than the fields wanted: the defaults", tide_store_load_min(b, 12, 7) == 0u && !persist_pending);

    /* chunk21: patch records in object 48 */
    fresh();
    {
        uint8_t pt[300];
        memset(pt, 0x5A, sizeof pt);
        pt[0] = 'P';
        pt[1] = 'T';
        pt[2] = 3;
        pt[3] = 0;
        CHECK("chunk21: a patch chunk is in object 48", st_save(OBJ_TIDE, pt, sizeof pt) == 0);
        memcpy(old_sector, nor + 0xC4000, sizeof old_sector);
        CHECK("chunk21: .. it is no TIDE record (the defaults), nothing queued", tide_store_load_min(b, 12, 7) == 0u && !persist_pending);
        tide_store_mark(F, 12);
        settle();
        tide_store_mark(G, 12);
        settle();
        CHECK("chunk21: .. two TIDE settings changes later the chunk is byte for byte the same", !memcmp(old_sector, nor + 0xC4000, sizeof old_sector) && flash_rec(&p) && tail_is(&p, G, 12));
    }

    /* forward: a newer build's fields */
    fresh();
    {
        uint8_t newer[3] = {0xA1, 0xA2, 0xA3}, w[12];
        uint32_t len = 0, crc = 0;
        memcpy(w, F, 12);
        memcpy(tide_tail, "TIDE", 4);
        tide_tail[4] = 15;
        memcpy(tide_tail + 5, F, 12);
        memcpy(tide_tail + 5 + 12, newer, 3);
        w[0] = 3;
        tide_store_mark(w, 12);
        CHECK("forward: this build changes its 12 fields; the record keeps 15", tide_tail[4] == 15u && !memcmp(tide_tail + 5, w, 12) && !memcmp(tide_tail + 17, newer, 3));
        memcpy(&q, ed_bk_settings_rec(), sizeof q);
        CHECK("backup: the exported record holds the fields as they stand now (before any save)", q.map_tail[TIDE_TAIL_AT + 4] == 15u && !memcmp(q.map_tail + TIDE_TAIL_AT + 5, w, 12) &&
              !memcmp(q.map_tail + TIDE_TAIL_AT + 17, newer, 3) && upstream_reserve_zero(&q));
        settle();
        CHECK("forward: saved: the 3 newer fields and the count 15 are in the flash too", flash_rec(&p) && !memcmp(p.map_tail + TIDE_TAIL_AT, q.map_tail + TIDE_TAIL_AT, TIDE_TAIL_N));
        list(1, &len, &crc);
        CHECK("backup: LIST sizes the settings object as the record", len == sizeof(persist_t));

        fresh();
        CHECK("backup: a unit without TIDE settings: the defaults", !tide_store_load(b, 12));
        CHECK("backup: the restore (PUT 1) is accepted", put_all(1, &q, sizeof q, st_crc32(&q, sizeof q)) == 0);
        CHECK("  .. the fields are back in RAM, the newer ones too", tide_store_load(b, 12) && !memcmp(b, w, 12) && tide_tail[4] == 15u && !memcmp(tide_tail + 17, newer, 3) && tide_tail_new == 1u);
        CHECK("  .. and in the flash (the written record is the backup's)", flash_rec(&p) && !memcmp(p.map_tail + TIDE_TAIL_AT, q.map_tail + TIDE_TAIL_AT, TIDE_TAIL_N));
    }

    /* old: a settings record made by 6fbd939 */
    {
        static persist_t o;
        FILE *f = fopen("tests/settings_6fbd939.bin", "rb");
        uint32_t i, z = 1;
        size_t got = f ? fread(&o, 1, sizeof o, f) : 0;
        if (f)
            fclose(f);
        CHECK("old: the 6fbd939 record is read (the size of today's record, the PER4 mark)", got == sizeof o && o.magic == PERSIST_MAGIC);
        for (i = 0; i < sizeof o.map_tail; i++)
            z &= !o.map_tail[i];
        CHECK("  .. its tail is all zero", z);
        fresh();
        CHECK("old: restored onto a unit that has no TIDE settings yet: accepted, the defaults stay", put_all(1, &o, sizeof o, st_crc32(&o, sizeof o)) == 0 && !tide_store_load(b, 12) && !tide_tail_new);
        CHECK("  .. the rest of the record took effect (palette, the compressor word)", settings.palette == UI_DEFAULT_INDEX && cmp_set == (0xC8000000u | 20u | 3u << 6));
        fresh();
        memcpy(tide_tail, "TIDE", 4);
        tide_tail[4] = 12;
        memcpy(tide_tail + 5, F, 12);
        CHECK("old: restored onto a unit with TIDE settings: accepted", put_all(1, &o, sizeof o, st_crc32(&o, sizeof o)) == 0);
        CHECK("  .. the unit keeps its own TIDE settings (the backup has none)", tide_store_load(b, 12) && !memcmp(b, F, 12));
        CHECK("  .. and they are written into the flash with the next settings save", persist_pending == 1u);
        settings_poll();
        CHECK("  .. which wrote them (and the rest of the old record, palette 6)", !persist_pending && flash_rec(&p) && tail_is(&p, F, 12) && p.rsv == (0xC8000000u | 20u | 3u << 6));
    }
    printf("tidestore: %d checks, %d failed\n", checks, bad);
    return bad != 0;
}
