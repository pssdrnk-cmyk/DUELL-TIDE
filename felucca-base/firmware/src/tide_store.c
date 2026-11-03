/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's own settings record (plan §4.5): a storage object of its own, OBJ_TIDE (storage.c), on the A/B sector pair of patch chunk 21
 * (object 48, 0xC4000 / 0xC5000; with DUELL_TIDE the patch store has 21 chunks, upreset.c PT_NCH). Commit-last A/B writes (st_save):
 * a torn write leaves the previous copy. Payload: "TIDE", a version, the fields (tide_ui.c TS_*). ERASE DATA wipes it with the patch
 * chunks (erase_data.c ORDER: OBJ_PATCHX0 + 15). Plain 1.1 reads the object as a patch chunk and rejects it (no "PT" header): it stays
 * until a patch save there overwrites it. Not in the editor's backup. Included after upreset.c / project.c (felucca.c, TIDE hook).
 * Host builds without the flash: RAM only. Main loop only. */
#define TIDE_STORE_VER 1u
#define TIDE_STORE_MAX 32u
static uint8_t tide_store_buf[4u + 1u + TIDE_STORE_MAX], tide_store_dirty;
static uint32_t tide_store_n, tide_store_t;

/* the fields as stored: how many of the n were read (at least min; the rest of b is untouched, the caller's defaults), or 0 when there is
 * no valid record (a new unit, ERASE DATA, a record of another version, one shorter than min): an older, shorter record keeps its fields */
static uint32_t tide_store_load_min(uint8_t *b, uint32_t n, uint32_t min)
{
#if FELUCCA_FLASH
    uint8_t r[sizeof tide_store_buf];
    int len = st_load(OBJ_TIDE, r, sizeof r);
    uint32_t have;
    if (len < 5 + (int)min || r[0] != 'T' || r[1] != 'I' || r[2] != 'D' || r[3] != 'E' || r[4] != TIDE_STORE_VER)
        return 0;
    have = (uint32_t)len - 5u;
    have = have < n ? have : n;
    memcpy(b, r + 5, have);
    return have;
#else
    (void)b;
    (void)n;
    (void)min;
    return 0;
#endif
}
static int tide_store_load(uint8_t *b, uint32_t n) { return tide_store_load_min(b, n, n) != 0u; }
/* the fields changed: saved ~2 s after the last change (tide_store_poll) */
static void tide_store_mark(const uint8_t *b, uint32_t n)
{
    n = n > TIDE_STORE_MAX ? TIDE_STORE_MAX : n;
    tide_store_buf[0] = 'T';
    tide_store_buf[1] = 'I';
    tide_store_buf[2] = 'D';
    tide_store_buf[3] = 'E';
    tide_store_buf[4] = TIDE_STORE_VER;
    memcpy(tide_store_buf + 5, b, n);
    tide_store_n = 5u + n;
    tide_store_dirty = 1;
    tide_store_t = fm1_ms;
}
static void tide_store_poll(void)
{
    if (!tide_store_dirty || fm1_ms - tide_store_t < 2000u || song.playing || erase_busy())
        return;                                          /* (a knob turned writes once; never while playing: the flash write ducks the audio) */
#if FELUCCA_FLASH
    if (!flash_ok || !autosave_quiet())
        return;                                          /* (no flash, or a key / voice sounding: the other autosaves wait the same) */
    if (st_save(OBJ_TIDE, tide_store_buf, tide_store_n)) {
        tide_store_t = fm1_ms;                           /* (failed: tried again after the 2 s again, not at every pass) */
        return;
    }
#endif
    tide_store_dirty = 0;
}
