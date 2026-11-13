/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's own settings record (plan §4.5; D11 of the 2026-11-04 audit): the fields of tide_ui.c (TS_*) live in the settings record's reserved tail
 * (settings_persist.c: map_tail[32..63], "TIDE", the field count, the fields; 27 fields at most), so every editor backup (object 1) holds them and a restore
 * brings them back. They were TIDE's own storage object, OBJ_TIDE, on the A/B sector pair of patch chunk 21 (object 48, 0xC4000 / 0xC5000): that object is read ONCE,
 * by tide_store_load_min when the settings hold no TIDE record (a unit updated from an earlier TIDE build), copied into the tail and never written again, so a patch
 * record a stock 1.1 unit once kept in chunk 21 (D03; the 21-chunk store of TIDE does not list it) is never overwritten by a TIDE setting. ERASE DATA wipes it with the
 * patch chunks (erase_data.c ORDER: OBJ_PATCHX0 + 15) and the settings. Plain 1.1 zeroes the tail at its next settings save. Fields past the ones this build knows
 * (a newer TIDE build's) are kept as they were read (D09). Included after upreset.c / project.c (felucca.c, TIDE hook). Main loop only. */
#define TIDE_STORE_MAX (TIDE_TAIL_N - 5u)
static uint8_t tide_store_dirty;
static uint32_t tide_store_t;

/* the fields as stored: how many of the n were read (at least min; the rest of b is untouched, the caller's defaults), or 0 when there is
 * no valid record (a new unit, ERASE DATA, one shorter than min): an older, shorter record keeps its fields */
static uint32_t tide_store_load_min(uint8_t *b, uint32_t n, uint32_t min)
{
    uint32_t have;
#if FELUCCA_FLASH
    if (!tide_tail_ok(tide_tail)) {                      /* (no record in the settings: the old object 48, once) */
        uint8_t r[5u + TIDE_STORE_MAX];
        int len = st_load(OBJ_TIDE, r, sizeof r);
        if (len >= 5 && r[0] == 'T' && r[1] == 'I' && r[2] == 'D' && r[3] == 'E' && r[4] == 1u) {
            memset(tide_tail, 0, TIDE_TAIL_N);
            memcpy(tide_tail, r, (uint32_t)len);
            tide_tail[4] = (uint8_t)(len - 5);           /* (the old record's length is its field count) */
            settings_save();                             /* (the tail goes to the flash with the next settings save) */
        }
    }
#endif
    if (!tide_tail_ok(tide_tail) || tide_tail[4] < min)
        return 0;
    have = tide_tail[4] < n ? tide_tail[4] : n;
    memcpy(b, tide_tail + 5, have);
    return have;
}
static int tide_store_load(uint8_t *b, uint32_t n) { return tide_store_load_min(b, n, n) != 0u; }
/* the fields changed: kept in the record at once (a backup reads it), saved ~2 s after the last change (tide_store_poll) */
static void tide_store_mark(const uint8_t *b, uint32_t n)
{
    uint32_t keep = tide_tail_ok(tide_tail) ? tide_tail[4] : 0u;
    n = n > TIDE_STORE_MAX ? TIDE_STORE_MAX : n;
    if (!keep)
        memset(tide_tail, 0, TIDE_TAIL_N);
    memcpy(tide_tail, "TIDE", 4);
    tide_tail[4] = (uint8_t)(keep > n ? keep : n);       /* (a newer build's extra fields stay as they were) */
    memcpy(tide_tail + 5, b, n);
    tide_store_dirty = 1;
    tide_store_t = fm1_ms;
}
static void tide_store_poll(void)
{
    if (!tide_store_dirty || fm1_ms - tide_store_t < 2000u || song.playing || erase_busy())
        return;                                          /* (a knob turned writes once; never while playing: the flash write ducks the audio) */
    settings_save();                                     /* (settings_poll writes it when nothing sounds, and retries a failed write) */
    tide_store_dirty = 0;
}
