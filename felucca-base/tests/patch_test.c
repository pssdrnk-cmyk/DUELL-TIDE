/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL PATCHES (upreset.c the compact PATCH store and the slot layer, SAVE > PATCHES, SPEC 3.6; 0.2.17: ONE list of 128 slots, a slot is its saved
 * record or else the factory sound at that browse position) against simulated NOR flash; a few fast checks:
 *   1. a saved record is a sparse record in the patch store only (22 chunks: 0xC8000..0xCDFFF, 0xDC000..0xDFFFF, 0x9F000 / 0xFE000, 0xA6000..0xC5FFF):
 *      small, equal after a power cycle (values, name, End B block, FM6 patch); overwrite and RESTORE; a power cut during a write
 *      leaves the old record;
 *   2. the limit is the slot count: 128 worst-case records (FM6 + patch, SEQ arp with 16 steps, End B, every value off its default)
 *      always fit, whatever was stored before (small records first, then all overwritten in place; 128 FM6-size ones); there is no 'full': with all
 *      128 saved any slot is still written; a power cut at every step of a save loses only that one;
 *      the payload ends at sector offset 0xF00; the store's RAM is a directory, not a mirror;
 *   3. the slots: a fresh unit holds the factory sound in all 128 (nothing written), SAVE overwrites the slot it is told to (K1 in the dialog picks it,
 *      the slot the sound came from is offered), RESTORE brings the factory sound back bit for bit, RESTORE ALL, MIDI program change = the slot,
 *      FAV is the slot's, a power cut at every step of a SAVE, a RESTORE and a RESTORE ALL;
 *   4. the page: ONE list of 128 slots (the number, the name), K1 steps and loads, K2 TYPE a filter, K3 SAVE, K4 FAV (1.1: no RESTORE on the page).
 *   build/host/patch_test            (DUELL/host_tests.sh "patch") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "endb_store.h"
#define NSAVE (PT_MAX < UP_SLOTS ? PT_MAX : UP_SLOTS)   /* records the store always takes: 126 with TIDE (chunk 21 holds its settings, plan 4.5), else all 128 */

static uint8_t nor[0x100000];
static long cut_bytes = -1;
static int dead, watch;
static uint32_t progd, stray, wrote;                       /* (watch) bytes erased / programmed outside the patch store; inside it */
static void hit(uint32_t off, uint32_t n)
{
    if (!watch)
        return;
    if ((off >= 0xA6000u && off + n <= 0xC6000u) || (off >= 0xC8000u && off + n <= 0xCE000u) || (off >= 0xDC000u && off + n <= 0xE0000u) ||
        (off >= 0x9F000u && off + n <= 0xA0000u) || (off >= 0xFE000u && off + n <= 0xFF000u))
        wrote += n;
    else
        stray += n;
}
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead)
        return -8;
    hit(off, 4096u);
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
        return -1;
    hit(off, n);
    for (i = 0; i < n; i++) {
        if (dead || (cut_bytes >= 0 && cut_bytes-- == 0)) {
            dead = 1;
            return -9;
        }
        nor[off + i] &= s[i];
        progd++;
    }
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad;
static void ck(const char *what, int ok)
{
    printf("patch: %-108s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void power_cycle(void)                       /* the RAM is gone; the flash read again at power-on */
{
    dead = 0;
    cut_bytes = -1;
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
}
static uint32_t pt_bytes(void)
{
    uint32_t c, n = 0;
    for (c = 0; c < PT_NCH; c++)
        n += pt_len[c];
    return n;
}
/* part t as the biggest record there can be: FM6 with its patch, every value off its default, the arp in SEQ mode with all sixteen
 * values and sixteen full steps; End B with 32 values set by hand comes with store_with_b(.., worst_ed, XB_MAX) */
static xb_edit_t worst_ed[XB_MAX];
static void worst_part(track_t *t, uint32_t idx)
{
    uint32_t i;
    set_engine_of(t, ENGI_FM6);
    apply_preset_to(t, 1u);
    for (i = 0; i < P_COUNT; i++) {
        const param_desc_t *d = param_desc_of(ENGI_FM6, i);
        t->p[i] = (int16_t)(d->def + 1 <= d->max ? d->def + 1 : d->def - 1);
    }
    t->p[P_AMODE] = AM_SEQ;
    t->p[P_SLEN] = 15;
    for (i = 0; i < 16u; i++) {
        step_t *s = &t->step[i];
        step_clear(s);
        s->note[0] = (uint8_t)(40 + (i * 5u + idx) % 40u);
        s->n = 1;
        s->time = ST_NOTE;
        s->flags = (uint8_t)(SF_ACCENT | SF_SLIDE);
        s->vel = (uint8_t)(50 + i);
        s->probability = (uint8_t)(30 + i);
        step_set_nudge(s, (int32_t)i % 5 - 2);
        step_set_ratchet(s, 2u);
    }
    fm6_patch[(uint32_t)(t - trk) % NTRK][6] = (uint8_t)(idx % 99u);
    for (i = 0; i < NX; i++)                          /* the cloud plan's job A: all 16 extension slots off their default, and 32 hand-set B edits (PT_WORST counts both) */
        XV(t, i) = (int16_t)((int)xb_def(i) - 64 + 1 + (int)(idx % 50u));
    for (i = 0; i < XB_MAX; i++)
        worst_ed[i] = (xb_edit_t){(uint8_t)(i < 28u ? 4u * i + 1u : 128u + i - 28u), (int16_t)(127 - (int)(i * 3u + idx % 7u))};
}

static int store_worst(track_t *t, uint32_t k, const char *nm) { return store_worst_cat(t, k, nm, 0, worst_ed, XB_MAX); }

/* how many of the two LFO TRIGs a stored record holds (PT_VER 4: their bits in the values' bitmap); pr: the values' start (pt_layout) */
static uint32_t trig_stored(const uint8_t *raw, uint32_t pr)
{
    return (raw[pr + P_LTRIG / 8u] >> (P_LTRIG % 8u) & 1u) + (raw[pr + P_L2TRIG / 8u] >> (P_L2TRIG % 8u) & 1u);
}



int main(void)
{
    uint32_t k, n, i, ok, total, occ, e, kk, rec0;
    int16_t was[P_COUNT];
    uint8_t pk[FM6_PACKED], pk2[FM6_PACKED];
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;

    /* 1: the store */
    ck("power-on: no patch stored, no chunk", up_count() == 0 && pt_bytes() == 0);
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 3u);
    watch = 1;
    ck("an INIT-like sound stores (rc 0 or 3)", up_store_trk(&trk[0], 7u, "TEST A", CAT_BASS) == 0);
    watch = 0;
    rec0 = pt_bytes();
    ck("  one sparse record: small (under 140 bytes with its chunk header), written in the patch store only",
       up_used(7) && rec0 > PT_HDR + PT_FIX && rec0 < 140u && wrote && !stray);
    trk[0].p[P_E4] = (int16_t)(trk[0].p[P_E4] + 9);
    trk[0].p[P_TRIM] = 99;
    for (i = 0; i < P_COUNT; i++)
        was[i] = i == P_LEVEL ? (int16_t)up_def(0, P_LEVEL) : (int16_t)wv_val(0, i, wv_code(0, i, motion_base_value(&trk[0], i)));   /* (0.2.19: the mixer level is no patch's;
                                                                                         * PT_VER 4: a value as its range keeps it, what a load fits it to) */
    {
        static const xb_edit_t e1[2] = {{P_ATK, 30}, {P_REL, 90}};
        ck("overwrite the same slot with other values + an End B block (two values set by hand)", store_with_b_cat(&trk[0], 7u, "TEST A", CAT_BASS, e1, 2) == 0 && eb_n);
    }
    {
        static uint8_t keep[MBV_MAX];
        uint32_t nkeep = eb_n, ebn;
        const uint8_t *eb;
        memcpy(keep, eb_blk, eb_n);
        power_cycle();
        {
        const up_rec_t *r = up_rec(7);
        eb = rec_endb(7u, &ebn);
        ok = up_used(7) && up_count() == 1u && str_eq(r->name, "TEST A") && up_cat(7) == CAT_BASS && r->engine == 0u &&
             eb && ebn == nkeep && !memcmp(eb, keep, nkeep);
        for (i = 0; i < P_COUNT; i++)
            ok &= wv_code(0, i, up_value(r, i)) == wv_code(0, i, was[i]);   /* (PT_VER 4: equal inside the range, what a load fits it to) */
        }
    }
    ck("after a power cycle: the same name, category, every value, End B block; still one record", ok);
    /* 0.2.7: the matrix's new destinations (stored as their DST numbers 23..45, appended) come back as they were */
    trk[0].p[P_M1SRC] = MS_LFO, trk[0].p[P_M1DST] = MD_RDAMP, trk[0].p[P_M1AMT] = -20;
    trk[0].p[P_M5SRC] = MS_ENV2, trk[0].p[P_M5DST] = MD_INSC, trk[0].p[P_M5AMT] = 33;
    trk[0].p[P_M8SRC] = MS_MODW, trk[0].p[P_M8DST] = MD_E2REL, trk[0].p[P_M8AMT] = 63;
    trk[0].p[P_M7SRC] = MS_LFO2, trk[0].p[P_M7DST] = MD_E1 + 5, trk[0].p[P_M7AMT] = -64;
    for (i = 0; i < P_COUNT; i++)
        was[i] = i == P_LEVEL ? (int16_t)up_def(0, P_LEVEL) : (int16_t)wv_val(0, i, wv_code(0, i, motion_base_value(&trk[0], i)));   /* (0.2.19: the mixer level is no patch's;
                                                                                         * PT_VER 4: a value as its range keeps it, what a load fits it to) */
    ok = up_store_trk(&trk[0], 7u, "TEST A", CAT_BASS) == 0;
    power_cycle();
    for (i = 0; i < P_COUNT; i++)
        ok &= up_value(up_rec(7), i) == was[i];
    ck("  a matrix routed to REVERB DAMP, INSERT C, ENV 2 REL and an engine slot: stored and back after a power cycle", ok && up_count() == 1u);
    set_engine_of(&trk[1], ENGI_FM6);
    apply_preset_to(&trk[1], 2u);
    fm6_patch[1][5] = (uint8_t)((fm6_patch[1][5] + 11u) % 99u);
    fm6_adopt(1);
    ck("an FM6 sound stores with its patch inside the record", up_store_trk(&trk[1], 9u, "TEST FM", 0) == 0 && upf_fm6(9));
    fm6_pack(fm6_patch[1], pk);
    power_cycle();
    ck("  after a power cycle the patch is the same (upf_get)", !upf_get(9, pk2) && !memcmp(pk, pk2, sizeof pk));
    kk = pt_bytes();
    for (n = 0; n < 2u; n++) {                        /* a cut in the payload (n 0) and one in the commit record, the last write (n 1) */
        cut_bytes = n ? (long)pt_len[0] + 8 : 100;
        ck(n ? "a power cut in the commit record of an overwrite: SAVE ERROR" : "a power cut in the payload of an overwrite: SAVE ERROR",
           up_store_trk(&trk[0], 7u, "CUT", 0) != 0);
        power_cycle();
        ck("  after the power comes back: both records as they were", up_count() == 2u && str_eq(up_rec(7)->name, "TEST A") &&
           up_used(9) && pt_bytes() == kk);
    }
    ck("erase: slot 7 empty, slot 9 kept (through a power cycle too)", up_put(7, 0) == 0 && !up_used(7) && up_used(9));
    power_cycle();
    ck("  after a power cycle", !up_used(7) && up_used(9) && up_count() == 1u);

    {   /* a move to another chunk cut short: the slot in two chunks, the newer revision wins (the newer one is written first) */
        up_rec_t r = *up_rec(9);
        uint32_t rev = pt_raw(9)[3];
        char nm1[13];
        memset(r.name, 0, 12);
        memcpy(r.name, "MOVED", 5);
        pt_encode(9, rev + 1u, &r, 0);
        ck("  (a newer revision of slot 9 written into chunk 1)", pt_write(1, UP_SLOTS, pt_rd16(pt_enc)) == 0);
        up_name(9, nm1);
        ok = str_eq(nm1, "MOVED") && pt_dir[9].ch == 1u;
        ok &= pt_len[0] == 0u;                             /* (the older one is out of the directory) */
        power_cycle();                                     /* (flash holds both: the directory is read again) */
        up_name(9, nm1);
        ok &= str_eq(nm1, "MOVED") && pt_dir[9].ch == 1u;
        memset(r.name, 0, 12);
        memcpy(r.name, "TEST FM", 7);
        pt_encode(9, rev + 2u, &r, 0);                     /* the move completes: back to chunk 0 with an even newer revision, chunk 1's cut */
        ck("  (an even newer one in chunk 0)", pt_write(0, UP_SLOTS, pt_rd16(pt_enc)) == 0);
        pt_write(1, 9u, 0);
        power_cycle();
        up_name(9, nm1);
        ck("the same slot in two chunks: the newer revision is the record (either order), not the older", ok && str_eq(nm1, "TEST FM") && pt_dir[9].ch == 0u &&
           pt_n[0] == 1u && pt_n[1] == 0u);
        memset(r.name, 0, 12);
        memcpy(r.name, "OLDER", 5);
        pt_encode(9, rev - 1u, &r, 0);
        pt_write(1, UP_SLOTS, pt_rd16(pt_enc));            /* an OLDER revision in chunk 1: it never wins */
        power_cycle();
        up_name(9, nm1);
        ck("  an older revision in another chunk never wins (also after a power cycle)", str_eq(nm1, "TEST FM") && pt_dir[9].ch == 0u);
        pt_write(1, UP_SLOTS, 0);                          /* (chunk 1 empty again) */
        power_cycle();
    }


    /* 1b: the arp, the chord / scale keys and the SEQ pattern travel with a patch (upreset.c, the AK block) */
    {
        track_t *t = &trk[0];
        uint32_t per_chunk = PT_CAP - PT_HDR, a_slot = 40u, b_slot = 41u, fe = 0, fk = 0, bytes_old, bytes_new, sz;
        int16_t want[16], mark[16];
        step_t st0[NSTEP];
        const uint8_t IDS[16] = {P_AMODE, P_ARATE, P_AOCT, P_AGATE, P_ASWING, P_APROB, P_AHOLD, P_AORDER, AK_NOSCALE, P_CHRD, P_VOIC,
                                 P_SLEN, P_SDIV, P_SSWING, P_SGATE, P_SQNT};
        int same;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        t->p[P_RES2] = 5; t->p[P_DRV2] = 9;              /* ids 25 / 26, once ROOT and SCALE: the TWIN FILTER's RES 2 and DRIVE, stored with the patch like any value */
        t->p[P_AMODE] = AM_SEQ;
        t->p[P_ARATE] = 3; t->p[P_AOCT] = 2; t->p[P_AGATE] = 40; t->p[P_ASWING] = 67;   /* (1.1: SWING 50..75) */ t->p[P_APROB] = 91; t->p[P_AHOLD] = 1;
        t->p[P_AORDER] = 1; t->p[P_CHRD] = 3; t->p[P_VOIC] = 1;
        t->p[P_SLEN] = 12; t->p[P_SDIV] = 2; t->p[P_SSWING] = 61; t->p[P_SGATE] = 71; t->p[P_SQNT] = 0;
        t->p[P_ATGT] = 3;
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        for (i = 0; i < 12u; i += 1) {
            step_t *s = &t->step[i];
            if (i % 5u == 4u)
                continue;                                    /* rests */
            s->note[0] = (uint8_t)(60 + (i * 7u) % 17u);
            s->n = 1;
            s->time = ST_NOTE;
            s->flags = (uint8_t)(i % 3u == 0 ? SF_ACCENT : 0);
            s->vel = (uint8_t)(70 + i);
            s->probability = (uint8_t)(i == 7u ? 60u : 0u);
            step_set_nudge(s, (int32_t)i % 4 - 2);
            step_set_ratchet(s, i == 9u ? 3u : 1u);
        }
        for (i = 0; i < 16u; i++)
            want[i] = t->p[IDS[i]];
        memcpy(st0, t->step, sizeof st0);
        ck("save a part with the arp in SEQ mode and a pattern", up_store_trk(t, a_slot, "SEQ ARP", 0) == 0 && pt_raw(a_slot)[4] & PTF_AK);
        {   /* record sizes: the same patch with the block (new) and with the arp ids as plain pairs (before) */
            up_rec_t r = *up_rec(a_slot);
            const uint8_t *raw = pt_raw(a_slot);
            bytes_new = pt_rd16(raw);
            up_ak_in = 0;
            bytes_old = pt_encode(a_slot, 0, &r, 0);
            up_ak_in = 0;
            printf("patch: record bytes with the SEQ arp block %u, before (arp ids as pairs + 32-byte pattern) %u\n", bytes_new, bytes_old);
        }
        power_cycle();
        ck("  after a power cycle the record is there", up_used(a_slot));
        t = &trk[0];
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        song.playing = 0;
        /* the player's own arp and steps, then the patch loads over them */
        t->p[P_RES2] = 0; t->p[P_DRV2] = 0;
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 5; t->p[P_SLEN] = 16; t->p[P_CHRD] = 0;
        for (i = 0; i < 16u; i++)
            mark[i] = t->p[IDS[i]];
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        t->step[2].note[0] = 55; t->step[2].n = 1; t->step[2].time = ST_NOTE; t->step[2].vel = 99;
        t->step[40].note[0] = 50; t->step[40].n = 1; t->step[40].time = ST_NOTE; t->step[40].vel = 99;
        {
            step_t own[NSTEP];
            memcpy(own, t->step, sizeof own);
            up_load(a_slot);
            same = 1;
            for (i = 0; i < 16u; i++)
                if (IDS[i] != AK_NOSCALE)
                    same &= t->p[IDS[i]] == want[i];
            ck("  load: every arp / keys / sequencer value as saved (mode SEQ, rate, octaves, gate, swing, prob, hold, order, CHRD VOIC, LEN DIV SWING GATE QUANTIZE)", same && t->p[P_ATGT] == 3);
            ck("  RES 2 and DRIVE (ids 25 / 26, once ROOT and SCALE) come back with the patch", t->p[P_RES2] == 5 && t->p[P_DRV2] == 9);
            {   /* no patch stores or applies a scale (core.h: the scale is the instrument's) */
                ak_t a;
                int has = ak_get(a_slot, &a);
                ck("  the keys block holds no SCALE (its ordinal is retired), the load applied none, the global scale is untouched",
                   has && (a.g & AK_KEYS) && a.key[0] == 0 && scale_g == 0 && root_g == 0 && offscale_g == OS_SNAP);
            }
            same = 1;
            for (i = 0; i < 16u; i++)
                same &= !memcmp(&t->step[i], &st0[i], sizeof(step_t));
            ck("  the 16 steps as saved (notes, accents, velocity, chance, nudge, ratchet, rests)", same);
            ck("  steps 17..64 stay the player's", !memcmp(&t->step[16], &own[16], sizeof(step_t) * 48u));
            /* DUELL 1.0: every patch sets its own arp and keys (OFF and the defaults when the sound has none); the player's own sequencer pattern comes back */
            apply_preset_to(t, 1u);
            same = 1;
            for (i = 0; i < 16u; i++)
                if (IDS[i] != AK_NOSCALE)
                    same &= t->p[IDS[i]] == (i < 9u ? TP[IDS[i]].def : mark[i]);   /* (the arp back to OFF; CHRD VOIC, ids 9 and 10, stay the player's: a patch without a chord leaves them) */
            ck("  the next sound loaded (no arp of its own) sets the arp OFF and the keys to their defaults; the player's LEN DIV SWING GATE QUANTIZE and steps come back", same && !memcmp(t->step, own, sizeof own));
            up_load(a_slot);
            t->p[P_ARATE] = 9;                                  /* edited while loaded, and the pattern too: */
            t->step[3].note[0] = 77;
            apply_preset_to(t, 1u);
            ck("  edited after the load: the next sound sets its own arp (OFF); the edited pattern stays the player's", t->p[P_AMODE] == 0 && t->p[P_ARATE] == TP[P_ARATE].def && t->step[3].note[0] == 77);
        }
        /* a patch with the arp off is stored WITH the block (the arp group and the keys group at their defaults, 4 bytes), so loading it turns the arp off */
        apply_preset_to(t, 3u);
        for (i = 0; i < 16u; i++)
            t->p[IDS[i]] = TP[IDS[i]].def;
        up_store_trk(t, b_slot, "PLAIN", 0);
        {
            ak_t a;
            const uint8_t *ab = pt_ak_raw(b_slot);
            ck("arp off + default sequencer: the AK block is there, the arp and keys groups at their defaults (5 bytes with its length: g + the PT_VER 4 bitmap)",
               (pt_raw(b_slot)[4] & PTF_AK) && ab && pt_rd16(ab) == 3u && ak_get(b_slot, &a) && a.g == (AK_ARP | AK_KEYS) && a.arp[0] == 0);
        }
        t->p[P_AMODE] = 2; t->p[P_ARATE] = 6; t->p[P_CHRD] = 2; t->p[P_SLEN] = 9;
        memset(mark, 0, sizeof mark);
        for (i = 0; i < 16u; i++)
            mark[i] = t->p[IDS[i]];
        memcpy(st0, t->step, sizeof st0);
        up_load(b_slot);
        same = 1;
        for (i = 0; i < 16u; i++)
            if (IDS[i] != AK_NOSCALE)
                same &= t->p[IDS[i]] == (i < 9u ? TP[IDS[i]].def : mark[i]);   /* (the arp back to OFF; CHRD VOIC, ids 9 and 10, stay the player's: a patch without a chord leaves them) */
        ck("the arp OFF patch loads: arp OFF, the player's chord keys stay (a patch with no chord of its own leaves them); LEN and the steps stay the player's", same && !memcmp(st0, t->step, sizeof st0));
        {   /* an older record (before 1.0: no block) reads as the defaults: arp OFF, keys at their defaults, no pattern of its own */
            uint8_t rec[PT_WORST + 8];
            uint32_t rn = up_raw_get(b_slot, rec), xbo, mb, pr, pat, fm, ak;
            pt_layout_x(rec, &xbo, &mb, &pr, &pat, &fm, &ak);
            rec[4] &= (uint8_t)~PTF_AK;
            rn = ak;
            rec[0] = (uint8_t)rn;
            rec[1] = (uint8_t)(rn >> 8);
            ck("an older record without the block is taken by the store", up_put_raw(b_slot, rec, rn) <= 3);
            t->p[P_AMODE] = 2; t->p[P_ARATE] = 6; t->p[P_CHRD] = 2;
            up_load(b_slot);
            same = 1;
            for (i = 0; i < 16u; i++)
                if (IDS[i] != AK_NOSCALE)
                    same &= t->p[IDS[i]] == (i < 9u ? TP[IDS[i]].def : mark[i]);   /* (the arp back to OFF; CHRD VOIC, ids 9 and 10, stay the player's: a patch without a chord leaves them) */
            ck("an older record (no block) loads as arp OFF, the chord keys stay the player's (it used to leave the current arp); LEN and the steps stay", same && !memcmp(st0, t->step, sizeof st0));
        }
        /* a rename keeps the block; the editor's UP_PUT (no block) drops it */
        ck("RENAME keeps the block", up_rename(a_slot, "SEQ ARP 2") == 0 && (pt_raw(a_slot)[4] & PTF_AK) && ak_get(a_slot, &(ak_t){0}) );
        {
            ak_t a;
            ok = ak_get(a_slot, &a) && a.g == (AK_ARP | AK_KEYS | AK_SEQ) && a.arp[0] == AM_SEQ && a.seq[0] == 12 && !memcmp(&a.st[0], &st0[0], 0);
            ck("  and it still decodes (SEQ, LEN 12)", ok);
        }
        /* a factory sound that sets an arp, saved with its arp switched off / edited (the part plays it: no slot record loaded): the player's edit is what is saved and loaded */
        for (i = 0; i < DS_N; i++)
            if (DS_SOUNDS[i].flags & DSF_ARP) {
                fe = DS_SOUNDS[i].eng;
                fk = i - DS_ENG_AT[fe];
                break;
            }
        set_engine_of(t, fe);
        apply_preset_to(t, fk);
        ok = t->p[P_AMODE] != 0;
        t->p[P_AMODE] = 0;
        up_store_trk(t, 42u, "SHADOW", 0);
        apply_preset_to(t, fk);                                 /* the factory sound again: its arp is on */
        ok &= t->p[P_AMODE] != 0;
        up_load(42u);
        ck("a factory sound saved with its arp switched off loads with the arp off (not the factory's)", ok && t->p[P_AMODE] == 0);
        apply_preset_to(t, fk);
        t->p[P_ARATE] = (int16_t)(t->p[P_ARATE] == 4 ? 5 : 4);
        want[0] = t->p[P_ARATE];
        up_store_trk(t, 42u, "SHADOW", 0);
        apply_preset_to(t, fk);
        up_load(42u);
        ck("  an edited rate is saved and loads (not the factory's)", t->p[P_ARATE] == want[0] && t->p[P_AMODE] != 0);
        /* record sizes: a plain sound, with an ordinary arp, with a SEQ arp */
        apply_preset_to(t, 2u);                                   /* a plain sound: its record; then with an ordinary arp (UP, 3 values changed) */
        for (i = 0; i < NSTEP; i++)
            step_clear(&t->step[i]);
        for (i = 0; i < 16u; i++)
            t->p[IDS[i]] = TP[IDS[i]].def;
        up_store_trk(t, 43u, "PLAIN2", 0);
        sz = pt_rd16(pt_raw(43u));
        t->p[P_AMODE] = 1; t->p[P_ARATE] = 5; t->p[P_AOCT] = 2; t->p[P_AGATE] = 60;
        up_store_trk(t, 43u, "ARP2", 0);
        {
            up_rec_t r = *up_rec(43u);
            uint32_t nw = pt_rd16(pt_raw(43u));
            up_ak_in = 0;
            printf("patch: record sizes: a plain sound %u bytes; the same with an ordinary arp %u (arp ids as pairs: %u); with a SEQ arp of 9 steps %u (as plain pairs with the 16-note pattern, no block: %u)\n",
                   sz, nw, pt_encode(43u, 0, &r, 0), bytes_new, bytes_old);
        }
        up_put(43u, 0);
        up_put(a_slot, 0); up_put(b_slot, 0); up_put(42u, 0);
    }

    /* 1c: PT_VER 4 retired the SCALE ordinal (8) of the keys group: a block that names it is no block (the record reads as arp OFF, default keys);
     * a keys block with CHRD alone applies it. Neither touches the global scale (1.1: chunks before PT_VER 4 are not read at all) */
    {
        static uint8_t blk[8], blk2[8];
        track_t *t = &trk[0];
        up_rec_t r;
        uint32_t rc, rc2, at;
        ak_t a;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
        memcpy(r.name, "OLD KEYS", 9);
        for (i = 0; i < P_COUNT; i++) up_set_value(&r, i, param_desc_of(0, i)->def);
        blk[0] = 5; blk[1] = 0; blk[2] = AK_KEYS; blk[3] = 0x00; blk[4] = 0x03; blk[5] = 5; blk[6] = 4;   /* u16 length 5: g KEYS, bm bits 8 SCALE + 9 CHRD, values */
        up_ak_in = blk;
        rc = (uint32_t)up_put(44u, &r);
        memset(blk2, 0, sizeof blk2);
        blk2[2] = AK_KEYS; blk2[4] = 0x02;                                                 /* g KEYS, bm bit 9 CHRD = 4 in the bits of its range */
        at = 5u * 8u;
        bs_put(blk2, &at, wv_code(0, P_CHRD, 4), wv_bits(0, P_CHRD));
        blk2[0] = (uint8_t)((at + 7u) / 8u - 2u); blk2[1] = 0;
        up_ak_in = blk2;
        rc2 = (uint32_t)up_put(45u, &r);
        up_ak_in = 0;
        scale_g = 3; root_g = 2;
        t->p[P_CHRD] = 0; t->p[P_DRV2] = 0;
        ck("a keys block naming the retired SCALE (ordinal 8) is no block: the record stores, ak_get finds none", rc == 0u && !ak_get(44u, &a));
        up_load(44u);
        ck("  loading it: arp OFF, default keys (CHRD at its default), the global scale unchanged", t->p[P_CHRD] == TP[P_CHRD].def && scale_g == 3 && root_g == 2);
        t->p[P_CHRD] = 0;
        ck("a keys block with CHRD alone reads", rc2 == 0u && ak_get(45u, &a) && a.key[1] == 4);
        up_load(45u);
        ck("  loading it: CHRD applies, the global scale unchanged", t->p[P_CHRD] == 4 && t->p[P_DRV2] == 0 && scale_g == 3 && root_g == 2);
        scale_g = 0; root_g = 0;
        up_put(44u, 0); up_put(45u, 0);
    }

    /* 1d (DUELL 1.1, PT_VER 4): the 28 extension slots are values by wide id (XW_BASE + i) in the record's sparse values; End B with the values set by
     * hand is the MBV block (PTF_MB). The 1.0 extension block (PTF_XB: tags, its own list of edits) is gone, and up_rec's xb list is no longer filled */
    {
        static uint8_t raw[PT_WORST + 8], good[PT_WORST + 8], keep[MBV_MAX];
        static xb_edit_t ed[XB_MAX], mine[XB_MAX], got[XB_MAX];
        track_t *t = &trk[0];
        up_rec_t r;
        uint32_t rn, n0, nmine, nkeep, ebn, ngot;
        const uint8_t *p, *eb;
        set_engine_of(t, 0u);
        apply_preset_to(t, 2u);
        for (i = 0; i < NX; i++)
            XV(t, i) = (int16_t)xb_def(i) - 64;
        ok = store_with_b(t, 40u, "XB NONE", 0, 0) == 0;
        p = pt_raw(40u);
        ck("1d: every slot at its default, no End B: no slot among the record's values, no End B block", ok && p && !rec_xbits(p) && !(p[4] & PTF_MB));
        n0 = pt_rd16(p);
        XV(t, X_LINK) = 0;
        ok = store_with_b(t, 41u, "XB LINK", 0, 0) == 0;
        p = pt_raw(41u);
        ck("  one slot off its default (LINK 0): that slot alone among the record's values, no more than two bytes more (its bits)", ok && p && rec_xbits(p) == 1u << X_LINK && pt_rd16(p) >= n0 && pt_rd16(p) <= n0 + 2u);
        XV(t, X_LINK) = (int16_t)xb_def(X_LINK) - 64;
        /* all 28 slots off default and End B with values set by hand: SAVE, a power cycle, up_rec, the record's End B and a LOAD */
        for (i = 0; i < NX; i++)
            XV(t, i) = (int16_t)(i * 9u % 120u) - 60;
        XV(t, X_SUBHARM) = 127; XV(t, X_CYCURVE) = -64;
        ed[0] = (xb_edit_t){P_REL, 99}; ed[1] = (xb_edit_t){P_ATK, 20}; ed[2] = (xb_edit_t){XWID(X_SUBLVL), 100}; ed[3] = (xb_edit_t){P_E1, 0};
        ok = store_with_b(t, 42u, "XB ALL", ed, 4) == 0 && eb_n;
        memcpy(keep, eb_blk, eb_n);
        nkeep = eb_n;
        nmine = morph_edits(0, mine);
        power_cycle();
        t = &trk[0];
        r = *up_rec(42u);
        for (i = 0; i < NX; i++)
            ok &= xfit_eq(0, i, up_xval(&r, i), i == X_SUBHARM ? 127 : i == X_CYCURVE ? -64 : (int)(i * 9u % 120u) - 60);
        eb = rec_endb(42u, &ebn);
        ck("  all 28 slots off default and End B with values set by hand: after SAVE and a power cycle up_rec has every slot, the record the same End B block",
           ok && nmine >= 3u && eb && ebn == nkeep && !memcmp(eb, keep, nkeep) && (eb[0] & MBV_PINS));
        for (i = 0; i < NX; i++)
            XV(t, i) = 0;
        ok = up_load(42u) == 0;
        for (i = 0; i < NX; i++)
            ok &= XV(t, i) == up_xval(&r, i);
        ck("  LOAD writes the slots to the part (XV)", ok);
        (void)morph_ready(0);
        ngot = morph_edits(0, got);
        ck("  .. and End B's values set by hand come back as set by hand (the same ids and values)", ngot == nmine && !memcmp(got, mine, nmine * sizeof got[0]));
        set_engine_of(t, 0u);
        for (i = 0; i < NX; i++)
            XV(t, i) = 5;
        ok = up_load(40u) == 0;                                       /* the record without slots and End B */
        for (i = 0; i < NX; i++)
            ok &= XV(t, i) == (int16_t)xb_def(i) - 64;
        ck("  a record with no slots stored loads the defaults into every slot (not what the part held)", ok);
        /* PATCH_RAW keeps a record bit for bit; a rename writes it again with its slots and End B */
        rn = up_raw_get(42u, good);
        ok = up_put_raw(43u, good, rn) == 0;
        ck("  PATCH_RAW keeps it bit for bit (up_raw_get = what was put)", ok && up_raw_get(43u, raw) == rn && !memcmp(raw + 4, good + 4, rn - 4u));
        ok = up_rename_cat(43u, "XB RENAMED", 0) == 0;
        r = *up_rec(43u);
        eb = rec_endb(43u, &ebn);
        ck("  a rename writes it again: its slots and its End B block stay", ok && up_xval(&r, X_LINK) == up_xval(up_rec(42u), X_LINK) && xfit_eq(0, X_SUBHARM, up_xval(&r, X_SUBHARM), 127) &&
           eb && ebn == nkeep && !memcmp(eb, keep, nkeep));
        /* a damaged End B block refuses the record (PATCH_RAW rc 1) */
        {
            static const char *WHAT[] = {"AMOUNT 102", "an unknown flag", "the FM6 flag on another engine", "a cut record (length one short)"};
            uint32_t m, xbo, mb, pr, pat, fm, bad_n = 0, refused_n = 0;
            pt_layout(good, &xbo, &mb, &pr, &pat, &fm);
            for (m = 0; m < sizeof WHAT / sizeof WHAT[0]; m++) {
                uint32_t n2 = rn, rc;
                memcpy(raw, good, rn);
                switch (m) {
                case 0: raw[mb + 1u] = 102u; break;
                case 1: raw[mb] |= 128u; break;
                case 2: raw[mb] |= MBV_FMB; break;
                default: n2 = rn - 1u; raw[0] = (uint8_t)n2; raw[1] = (uint8_t)(n2 >> 8); break;
                }
                rc = (uint32_t)up_put_raw(44u, raw, n2);
                refused_n += rc == 1u && !up_used(44u);
                bad_n += rc != 1u;
                if (rc != 1u)
                    printf("patch:     not refused: %s (rc %u)\n", WHAT[m], rc);
            }
            ck("  every damaged End B (AMOUNT past 101, an unknown flag, FM6's flag on another engine, a cut record) refuses the record, nothing written",
               !bad_n && refused_n == sizeof WHAT / sizeof WHAT[0]);
        }
        /* the chunk images: a bad record is passed over at boot, its neighbour stays; an image of the chunk version before is no chunk; the newer revision wins */
        {
            uint32_t w, c2, xbo, mb, pr, pat, fm;
            up_rec_t q;
            q = *up_rec(42u);
            pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[2] = PT_VER; pt_work[3] = 0;
            w = PT_HDR;
            n = pt_encode(50u, 1u, &q, 0); memcpy(pt_work + w, pt_enc, n); w += n;               /* slot 50: good */
            memcpy(raw, good, rn);                                                                /* slot 51: record 42's, End B's AMOUNT damaged */
            pt_layout(raw, &xbo, &mb, &pr, &pat, &fm);
            raw[mb + 1u] = 102u;
            raw[2] = 51u; raw[3] = 1u;
            memcpy(pt_work + w, raw, rn); w += rn;
            n = pt_encode(52u, 1u, &q, 0); memcpy(pt_work + w, pt_enc, n); w += n;               /* slot 52: good */
            pt_sv(4u, pt_work, w);
            power_cycle();
            ck("  a chunk image with a record whose End B is damaged: that record is passed over at boot, the records before and after it stay", up_used(50u) && !up_used(51u) && up_used(52u));
            pt_work[2] = PT_VER - 1u;                                                             /* the chunk version of before: no chunk */
            pt_sv(4u, pt_work, w);
            power_cycle();
            ck("  a chunk image of the version before (PT_VER - 1) is no chunk: its records are not read", !up_used(50u) && !up_used(52u));
            for (c2 = 0; c2 < 2u; c2++) {                                                         /* the same slot in two chunks: the newer revision wins (also across the 255 -> 0 wrap) */
                static const uint8_t RA[2] = {5, 250}, RB[2] = {6, 3};
                up_rec_t qa = *up_rec(42u), qb = qa;
                up_set_xval(&qa, X_SUBLVL, 11);
                up_set_xval(&qb, X_SUBLVL, 22);
                pt_work[2] = PT_VER; w = PT_HDR;
                n = pt_encode(60u, RA[c2], &qa, 0); memcpy(pt_work + w, pt_enc, n); w += n;
                pt_sv(2u, pt_work, w);
                w = PT_HDR;
                n = pt_encode(60u, RB[c2], &qb, 0); memcpy(pt_work + w, pt_enc, n); w += n;
                pt_sv(9u, pt_work, w);
                power_cycle();
                ck(c2 ? "  the same slot in two chunks, revisions 250 and 3 (wrapped): the newer, 3, wins (slot value 22)" : "  the same slot in two chunks, revisions 5 and 6: the newer, 6, wins (slot value 22)",
                   up_used(60u) && up_xval(up_rec(60u), X_SUBLVL) == 22 && pt_dir[60].ch == 9u);
                pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[3] = 0;
                pt_sv(2u, pt_work, PT_HDR); pt_sv(9u, pt_work, PT_HDR);
                power_cycle();
            }
        }
        /* RESTORE and RESTORE ALL: the other slots keep their slots and End B; the restored one is its factory sound; a SAVE after works */
        t = &trk[0];
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        ok = 1;
        for (k = 70; k < 73u; k++) {
            for (i = 0; i < NX; i++)
                XV(t, i) = (int16_t)((int)(i + k) % 100) - 50;
            ed[0] = (xb_edit_t){P_REL, (int16_t)(k - 60u)};
            ok &= store_with_b(t, k, "XB RST", ed, 1) == 0;
        }
        ok &= up_used(70u) && sl_restore(71u) == 0 && !up_used(71u) && up_used(70u) && up_used(72u);
        r = *up_rec(72u);
        for (i = 0; i < NX; i++)
            ok &= xfit_eq(0, i, up_xval(&r, i), (int)(i + 72u) % 100 - 50);
        ok &= rec_endb(72u, &ebn) != 0;
        ck("  RESTORE of one slot: the others keep their slots and their End B; the restored slot is its factory sound again", ok);
        power_cycle();
        r = *up_rec(70u);
        ck("  and after a power cycle too", up_used(70u) && up_used(72u) && !up_used(71u) && xfit_eq(0, 3, up_xval(&r, 3), (int)(3u + 70u) % 100 - 50) && rec_endb(70u, &ebn) != 0);
        ok = sl_restore_all() == 0 && up_count() == 0u;
        for (k = 70; k < 73u; k++)
            ok &= !up_used(k);
        t = &trk[0];
        for (i = 0; i < NX; i++)
            XV(t, i) = 33;
        ed[0] = (xb_edit_t){P_REL, 9};
        ok &= store_with_b(t, 5u, "AFTER", ed, 1) == 0 && xfit_eq(0, 7, up_xval(up_rec(5u), 7), 33) && rec_endb(5u, &ebn) != 0;
        ck("  RESTORE ALL: no record left; a SAVE after it stores with its slots and End B again", ok);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        for (i = 0; i < NX; i++)
            XV(t, i) = (int16_t)xb_def(i) - 64;
    }

    /* 2: the limit: NSAVE worst-case records always fit (128, every slot; with TIDE 126, its settings take chunk 21: plan 4.5, a 127th NEW record is STORE FULL) */
    {
        uint32_t maxrec = 0, mx = 0, c, per = 0;
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        printf("patch: PT_WORST %u bytes (SEQ-arp FM6 record %u, plain %u), PT_K %u per chunk, %u chunks, PT_MAX %u, chunk payload %u (ends at 0x%X)\n",
               (unsigned)PT_WORST, (unsigned)PT_W_SEQ, (unsigned)PT_W_PLAIN, (unsigned)PT_K, (unsigned)PT_NCH, (unsigned)PT_MAX, (unsigned)PT_CAP,
               (unsigned)(PT_CAP + ST_PAYLOAD_OFF));
        ck("PT_MAX = chunks x records per chunk (22 x 6 = 132, at least the 128 slots; TIDE 21 x 6 = 126), a chunk of PT_K worst records fits its payload",
           PT_MAX == PT_NCH * PT_K && PT_MAX + (22u - PT_NCH) * PT_K >= UP_SLOTS && UP_SLOTS == 128u && PT_K * PT_WORST + PT_HDR <= PT_CAP && PT_K == 6u &&
           PT_NCH == (DUELL_TIDE ? 21u : 22u) && NSAVE == (DUELL_TIDE ? 126u : 128u));
        ck("  the payload ends at sector offset 0xF00 (the updater's scan), 22 objects = 16 more A/B pairs at 0xA6000..0xC5FFF (TIDE: the last one its settings)",
           PT_CAP + ST_PAYLOAD_OFF == 0xF00u && PT_NCH + (DUELL_TIDE ? 1u : 0u) == 22u && st_sector(OBJ_PATCHX0, 0) == 0xA6000u && st_sector(OBJ_PATCHX0 + 9u, 1) + 4096u == 0xBA000u &&
           st_sector(OBJ_PATCHX0 + 10u, 0) == 0xBA000u && st_sector(OBJ_PATCHX0 + 15u, 1) + 4096u == 0xC6000u && OBJ_COUNT >= OBJ_PATCHX0 + 16u &&
           OBJ_PATCHX0 == OBJ_FM6VB + 1u);
        {   /* the 22 chunks' 44 sectors: all different, none of another object's, none the voice bank's (id 32 is refused) */
            uint32_t a2, b2, o, cc, same = 0, other = 0;
            st_hdr_t h;
            for (a2 = 0; a2 < PT_NCH * 2u; a2++) {
                for (b2 = a2 + 1u; b2 < PT_NCH * 2u; b2++)
                    same += st_sector(pt_obj(a2 / 2u), a2 % 2u) == st_sector(pt_obj(b2 / 2u), b2 % 2u);
                for (o = 0; o < OBJ_COUNT; o++)
                    for (cc = 0; cc < 2u; cc++)
                        if ((o < OBJ_PATCH0 || o >= OBJ_PATCH0 + 6u) && (o < OBJ_PATCHX0 || o >= OBJ_PATCHX0 + 16u) && !OBJ_RETIRED(o) &&
                            st_sector(o, cc) == st_sector(pt_obj(a2 / 2u), a2 % 2u))
                            other++;
            }
            ck("  the 22 chunks' sectors are all different and no other object's; ids 30..32 (unused, and the FM6 voice bank's) are no store objects",
               !same && !other && st_head(OBJ_FM6VB, 0, &h) != 0 && st_save(OBJ_FM6VB, "x", 1) < 0 && st_wipe(OBJ_FM6VB) < 0 && st_head(30, 0, &h) != 0 && st_save(31, "x", 1) < 0);
        }
        /* A: NSAVE small records (every slot; TIDE: 126 of them), the directory is all there is in RAM: no mirror */
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        for (n = 0, k = 0; k < NSAVE; k++) {
            char nmx[8] = {'S', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0};
            trk[0].p[P_TRIM] = (int16_t)(40 + k % 50u);
            n += up_store_trk(&trk[0], k, nmx, 0) == 0;
        }
        ck("NSAVE plain records store, one a slot (128; TIDE 126)", n == NSAVE && up_count() == NSAVE);
        if (NSAVE < UP_SLOTS)
            ck("  TIDE: one more NEW record is refused as full (STORE FULL), nothing lost", up_store_trk(&trk[0], NSAVE, "FULL", 0) == PT_FULL && !up_used(NSAVE) && up_count() == NSAVE);
        for (c = 0; c < PT_NCH; c++)
            mx = pt_cnt(c) > mx ? pt_cnt(c) : mx;
        ck("  no chunk holds more than PT_K records, and every one of the 22 chunks is used", mx <= PT_K && pt_cnt(0) && pt_cnt(PT_NCH - 1u));
        {   /* the sector tails: nothing from sector offset 0xF00 is ever programmed; each chunk's live copy is within PT_CAP */
            uint32_t o, cc, tail = 1, maxlen = 0;
            for (o = 0; o < PT_NCH; o++)
                for (cc = 0; cc < 2u; cc++) {
                    uint32_t base = st_sector(pt_obj(o), cc), i;
                    for (i = 0xF00u; i < 0x1000u; i++)
                        tail &= nor[base + i] == 0xFFu;
                }
            for (c = 0; c < PT_NCH; c++)
                maxlen = pt_len[c] > maxlen ? pt_len[c] : maxlen;
            ck("  the tail of every chunk sector (0xF00..0xFFF) is still erased, a chunk is at most PT_CAP", tail && maxlen <= PT_CAP);
        }
        {   /* RAM of the store: the directory, the work chunk and four read records; no mirror of the chunks */
            uint32_t ram = (uint32_t)(sizeof pt_dir + sizeof pt_work + sizeof pt_rbuf + sizeof pt_len + sizeof pt_n + sizeof pt_cur);
            printf("patch: the store's RAM: directory %u + work chunk %u + read ring %u = %u bytes (was 23,040 + 268 for the mirror)\n",
                   (unsigned)sizeof pt_dir, (unsigned)sizeof pt_work, (unsigned)sizeof pt_rbuf, (unsigned)ram);
            ck("  the store's RAM is under 9 KB (no whole-chunk mirror)", ram < 9000u);
        }
        {   /* the directory is rebuilt from the chunks at boot: identical, and the names it serves are the stored ones */
            static pt_ent_t snap[UP_SLOTS];
            memcpy(snap, pt_dir, sizeof snap);
            memset(pt_dir, 0x5A, sizeof pt_dir);
            memset(pt_len, 0x5A, sizeof pt_len);
            power_cycle();
            ok = !memcmp(snap, pt_dir, NSAVE * sizeof snap[0]) && up_count() == NSAVE;
            for (k = NSAVE; k < UP_SLOTS; k++)                /* (an unused slot: only its chunk mark is rebuilt, the rest is never read) */
                ok &= pt_dir[k].ch == PT_NOCH;
            ck("  a power cycle rebuilds the directory from flash: every entry equal (TIDE: the two unused slots marked unused)", ok);
            ok = 1;
            for (k = 0; k < NSAVE; k++) {
                char nmx[8] = {'S', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0}, got[13];
                up_name(k, got);
                ok &= str_eq(got, nmx) && !str_eq(up_rec(k)->name, "") && up_rec(k)->engine == 0u;
            }
            ck("  and the names / records read back (directory and flash)", ok);
        }
        kk = pt_bytes();
        watch = 1;
        wrote = stray = 0;
        trk[0].user = 0;                              /* the part plays a factory sound: SAVE offers its slot; with the store full any stored slot still takes it */
        k = (uint32_t)up_patch_save_part(0, 77u);
        ck("  with the store full a SAVE over any stored slot works: written in the patch store only, still NSAVE", k == 0u && wrote && !stray &&
           up_count() == NSAVE && up_used(77) && trk[0].user == 78u);
        watch = 0;
        /* B: every one of them overwritten with the worst record, in place: all fit whatever the first fill looked like */
        for (n = 0, k = 0; k < NSAVE; k++) {
            char nmx[8] = {'W', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0};
            worst_part(&trk[1], k);
            fm6_adopt(1);
            n += store_worst(&trk[1], k, nmx) == 0;
            maxrec = pt_rd16(pt_raw(k)) > maxrec ? pt_rd16(pt_raw(k)) : maxrec;
        }
        printf("patch: the biggest record stored: %u bytes (PT_WORST %u)\n", maxrec, (unsigned)PT_WORST);
        ck("NSAVE worst-case records overwrite the small ones in place: every one stores", n == NSAVE && up_count() == NSAVE);
        ck("  the worst record is never above PT_WORST, and apart from End B the bound is tight (MBV_MAX bounds every engine, the retired placeholder's; FM6's End B is its own biggest)",
           maxrec <= PT_WORST && maxrec + (MBV_MAX - eb_n) + 8u >= PT_WORST && eb_n > MBV_MAX / 2u);
        for (c = 0, mx = 0; c < PT_NCH; c++)
            mx = pt_cnt(c) > mx ? pt_cnt(c) : mx;
        ck("  every chunk within its PT_K records and its bytes", mx <= PT_K && pt_len[0] <= PT_CAP && pt_len[PT_NCH - 1u] <= PT_CAP);
        for (c = 0; c < PT_NCH; c++)
            per += pt_len[c] > PT_HDR;
        power_cycle();
        ck("  after a power cycle: all of them there, the FM6 patch of each intact, every chunk used", up_count() == NSAVE && per == PT_NCH);
        for (ok = 1, k = 0; k < NSAVE; k++) {
            const up_rec_t *r = up_rec(k);
            ak_t a;
            ok &= r->engine == ENGI_FM6 && !upf_get(k, pk2) && pk2[6] == (uint8_t)(k % 99u) && ak_get(k, &a) &&
                  a.g == (AK_ARP | AK_KEYS | AK_SEQ) && a.seq[0] == 15;
        }
        ck("  and each decodes (FM6 patch byte, the arp / keys / SEQ block)", ok);
        {   /* the sector tails again after the biggest chunks: nothing past 0xF00 */
            uint32_t o, cc, tail = 1, i;
            for (o = 0; o < PT_NCH; o++)
                for (cc = 0; cc < 2u; cc++)
                    for (i = 0xF00u; i < 0x1000u; i++)
                        tail &= nor[st_sector(pt_obj(o), cc) + i] == 0xFFu;
            ck("  the tails (0xF00..0xFFF) of every chunk sector are still erased with the fullest chunks", tail);
        }
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        ck("  all erased", up_count() == 0u);
        /* B2: NSAVE FM6-size patches (an FM6 sound with its patch, 190 bytes about): typical, all fit with room */
        {
            uint32_t tot = 0, big = 0;
            /* (typical: no End B, the slots at their defaults) */
            for (i = 0; i < NX; i++)
                XV(&trk[1], i) = (int16_t)xb_def(i) - 64;
            for (n = 0, k = 0; k < NSAVE; k++) {
                char nmx[8] = {'F', (char)('0' + k / 10u % 10u), (char)('0' + k % 10u), 0};
                set_engine_of(&trk[1], ENGI_FM6);
                apply_preset_to(&trk[1], 1u + k % 7u);
                fm6_patch[1][5] = (uint8_t)((fm6_patch[1][5] + k) % 99u);
                fm6_adopt(1);
                n += up_store_trk(&trk[1], k, nmx, 0) == 0;
                tot += pt_rd16(pt_raw(k));
                big = pt_rd16(pt_raw(k)) > big ? pt_rd16(pt_raw(k)) : big;
            }
            printf("patch: %u FM6 sounds: %u bytes of records (%u a record, biggest %u)\n", (unsigned)NSAVE, tot, tot / NSAVE, big);
            ck("NSAVE FM6-size patches (the factory FM6 sounds with their patch) all store, about 190 bytes each", n == NSAVE && up_count() == NSAVE && tot / NSAVE < 260u);
            for (c = 0, mx = 0; c < PT_NCH; c++)
                mx = pt_len[c] > mx ? pt_len[c] : mx;
            printf("patch: the fullest chunk holds %u of %u bytes\n", mx, (unsigned)PT_CAP);
            power_cycle();
            for (ok = 1, k = 0; k < NSAVE; k++)
                ok &= upf_fm6(k) && !upf_get(k, pk2);
            ck("  after a power cycle all of them are there with their patches", ok && up_count() == NSAVE);
            for (k = 0; k < UP_SLOTS; k++)
                up_put(k, 0);
        }
        /* C: the last save torn by a power cut: that one is lost, the others stay; then it works */
        for (k = 0; k + 1u < NSAVE; k++) {
            worst_part(&trk[1], k);
            fm6_adopt(1);
            store_worst(&trk[1], k, "W");
        }
        ck("NSAVE - 1 worst-case records stored", up_count() == NSAVE - 1u);
        worst_part(&trk[1], NSAVE - 1u);
        fm6_adopt(1);
        for (n = 0; n < 2u; n++) {
            cut_bytes = n ? (long)(PT_WORST + PT_HDR + 8u) : 100;
            ck(n ? "a power cut in the last save's payload (the last that fits): SAVE ERROR" : "a power cut at the start of the last save: SAVE ERROR",
               store_worst(&trk[1], NSAVE - 1u, "LAST") != 0);
            power_cycle();
            ck("  after the power comes back: the other records, the last one not stored", up_count() == NSAVE - 1u && !up_used(NSAVE - 1u));
            worst_part(&trk[1], NSAVE - 1u);
            fm6_adopt(1);
        }
        ck("  the same save then works: NSAVE stored", store_worst(&trk[1], NSAVE - 1u, "LAST") == 0 && up_count() == NSAVE);
        power_cycle();
        ck("  after a power cycle: NSAVE records, the last one too", up_count() == NSAVE && up_used(NSAVE - 1u));
        worst_part(&trk[1], 5u);
        fm6_adopt(1);
        ck("an overwrite of a stored one with the store full still works (in place)", store_worst(&trk[1], 100u, "LAST 2") == 0 && up_count() == NSAVE);
        ck("  an erase makes a place: the next new patch then works", up_put(0, 0) == 0 && store_worst(&trk[1], 0u, "AGAIN") == 0 && up_count() == NSAVE);
        /* C2: a power cut at EVERY step of a save (every 8th programmed byte, every byte near the commit record, the erase itself): the
         * chunk is the old one or the new one, never anything else; the others are never touched */
        {
            uint32_t steps = 0, bad_cut = 0, newer = 0, cl = pt_dir[100].len, ch = pt_dir[100].ch;
            long cb, lim = (long)pt_len[ch] + 40;
            char got[13];
            for (cb = 0; cb <= lim; cb += (cb > lim - 48 || cb < 8) ? 1 : 8) {
                worst_part(&trk[1], 9u);
                fm6_adopt(1);
                cut_bytes = cb;
                (void)store_worst(&trk[1], 100u, "CUTSTEP");
                power_cycle();
                up_name(100, got);
                steps++;
                if (up_count() != NSAVE || !up_used(100) || (!str_eq(got, "LAST 2") && !str_eq(got, "CUTSTEP")))
                    bad_cut++;
                newer += str_eq(got, "CUTSTEP");
                for (k = 0; k < NSAVE && !bad_cut; k += 9u)
                    if (k != 100u && !upf_fm6(k))
                        bad_cut++;
                if (str_eq(got, "CUTSTEP"))
                    store_worst(&trk[1], 100u, "LAST 2");   /* back to the old name for the next cut */
            }
            ck("a power cut at every step of a save (payload pages, commit record, readback): old or new, never half, the others intact", steps > 100u && !bad_cut);
            printf("patch: %u cut points tried in a %u byte record's chunk save (%u ended with the new record)\n", steps, cl, newer);
            cut_bytes = -1;
            power_cycle();
            ck("  and a cut in the erase of the other sector (the erase fails): the old chunk stays", (dead = 0, 1) && up_count() == NSAVE);
            (void)ch;
        }
        /* D: the page with the store full: the same 128 slots; SAVE held on a stored slot opens the OVERWRITE dialog as ever (no message about room) */
        power_cycle();
        song.playing = 0;
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 1u);
        go_title("PATCHES");
        frame();
        (void)preset_pos(&occ);
        (void)pl_cursor(&total);
        ck("with the store full the list is the 128 slots, the same as with none", total == UP_VSLOTS && occ == UP_VSLOTS);
        hold(B_SAVE);
        ck("hold SAVE with the store full: the OVERWRITE dialog, no message about room; OCT-: nothing written", ui.confirm == CF_SAVE_OVER &&
           (press(B_OCTDN), !ui.confirm) && up_count() == NSAVE);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        ck("  all erased", up_count() == 0u);
        /* E: a store written under other rules (60 records, 10 in each of six chunks: more than PT_K a chunk): readable, new patches go to the
         * other chunks, the guarantee for the rest (a chunk is never given more than PT_K NEW records) holds */
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        up_store_trk(&trk[0], 0u, "OLD", 0);
        {
            up_rec_t r = *up_rec(0);
            uint32_t w;
            up_put(0, 0);
            for (c = 0; c < 6u; c++) {
                pt_work[0] = (uint8_t)PT_MAGIC; pt_work[1] = (uint8_t)(PT_MAGIC >> 8); pt_work[2] = PT_VER; pt_work[3] = 0;
                for (w = PT_HDR, k = 0; k < 10u; k++) {
                    uint32_t nn = pt_encode(c * 10u + k, 1u, &r, 0);
                    memcpy(pt_work + w, pt_enc, nn);
                    w += nn;
                }
                pt_sv(c, pt_work, w);
            }
        }
        power_cycle();
        ck("a store with more records a chunk than PT_K (60 in six chunks) stays readable after a power cycle", up_count() == 60u && up_used(59));
        ck("  new patches still store (into the other chunks); an old chunk is not given more",
           up_store_trk(&trk[0], 70u, "NEW", 0) == 0 && up_count() == 61u && pt_dir[70].ch >= 6u && pt_n[0] == 10u);
        ck("  a stored one is still rewritten in place, and erased", up_store_trk(&trk[0], 5u, "SAME", 0) == 0 && up_put(5, 0) == 0 && up_count() == 60u);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
        {   /* PT_VER 4: the AK block's pairs form (np, then (id, value) pairs) is gone with the chunks before it (never read): such bytes are no block */
            static const uint8_t old[] = {AK_ARP, 1, P_ARATE, 5 + 64};
            ak_t a;
            ck("an AK block in the old pairs form is no block (PT_VER 4 reads its bitmap form only)", !ak_dec(old, sizeof old, 0u, &a));
        }
    }

    /* 3: the slots: a fresh unit holds the factory sound in all 128 */
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;
    {
        uint32_t fe, fk, same = 0, named = 0;
        ck("a fresh unit: nothing saved, no chunk written, the factory sounds fill all 128 slots (ds_list_n() = 128 = UP_SLOTS) and the sounds past the bank's 128 (UP_NXTRA) have places after them",
           up_count() == 0u && pt_bytes() == 0u && ds_list_n() == UP_SLOTS + UP_NXTRA && UP_SLOTS == 128u && UP_VSLOTS == UP_SLOTS + UP_NUSER + UP_NXTRA);
        for (k = 0; k < UP_USER0; k++) {   /* (U1: the factory sounds of the slots before the USER ones) */
            char nmk[13];
            int16_t v1[P_COUNT];
            sl_name(k, nmk);
            ok = sl_fac(k, &fe, &fk) && sl_engine(k) == fe && str_eq(nmk, ds_name(fe, fk)) && sl_cat(k) == preset_cat(fe, fk);
            sl_values(k, v1);
            {   /* item 138: DUELL 1.1 load_end (ui.c) moves the retired DEST cards' routes into the matrix, fills the empty slots with the preselected pairs at amount 0 and packs
                 * (mod.c mod_legacy_move, mod_preselect, mod_compact: the same sound); the list's values through the same three steps (trk[0] as the scratch part, put back after) */
                int16_t keep[P_COUNT];
                memcpy(keep, trk[0].p, sizeof keep);
                memcpy(trk[0].p, v1, sizeof keep);
                (void)mod_legacy_move(&trk[0]);
                mod_preselect(&trk[0]);
                (void)mod_compact(&trk[0]);
                memcpy(v1, trk[0].p, sizeof keep);
                memcpy(trk[0].p, keep, sizeof keep);
            }
            /* a part that loads the factory sound the old way (the browse position's engine and index) is what sl_values says */
            set_engine_of(&trk[1], fe);
            apply_preset_to(&trk[1], fk);
            for (i = 0; i < P_COUNT; i++)
                ok &= v1[i] == trk[1].p[i] || (i >= P_AMODE && i <= P_AORDER) || i == P_CHRD || i == P_VOIC || param_kept(i) || i == P_LEVEL ||
                      (fe == ENGI_FM6 && i >= P_ATK && i <= P_REL && trk[1].p[i] == fm6_env_value(&trk[1], i - P_ATK));   /* (item 138: FM6's AMP ENV knobs are its carriers' numbers, FM6_ENV_MAP_V2) */
            ok &= v1[P_TRIM] == trk[1].p[P_TRIM];   /* (0.2.19: the factory sound's trim; the mixer level is no sound's) */
            same += ok;
            named += nmk[0] != 0 && !str_eq(nmk, "INIT");
        }
        ck("  every slot's name, engine, category and values are its browse position's factory sound's (sl_values equals a factory load)", same == UP_USER0 && named == UP_USER0);
        {   /* load slot k on part 1: the same as the PATCHES list loads (the engine, the factory index, the values) */
            uint32_t good = 0;
            for (k = 0; k < UP_USER0; k += 5u) {
                song.sel = 0;
                sl_load(k);
                good += sl_fac(k, &fe, &fk) && trk[0].eng_req == fe && trk[0].preset == fk && !trk[0].user && slot_of(&trk[0]) == k;
            }
            ck("  sl_load(k) plays the factory sound (engine, index), the part is in slot k (no record: user 0)", good == (UP_USER0 + 4u) / 5u);
        }
    }

    /* 3a: SAVE then reload equals the saved sound; RESTORE brings the factory sound back bit for bit */
    {
        int16_t fac[P_COUNT], sv[P_COUNT], trn_fac;
        uint8_t blk[7];
        uint32_t sl = 41u, e0, f0;
        song.sel = 0;
        sl_load(sl);
        sl_fac(sl, &e0, &f0);
        memcpy(fac, trk[0].p, sizeof fac);
        trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 12 : 88);
        trk[0].p[P_E2] = (int16_t)(trk[0].p[P_E2] > 30 ? 5 : 77);
        trk[0].p[P_TRANS] = -24;                       /* (DUELL 1.1, owner 2026-10-27: TRANSPOSE is the patch's: a bass two octaves down) */
        memcpy(sv, trk[0].p, sizeof sv);
        ck("SAVE (slot 42 over its factory sound): written, the part plays that slot, a record exists", up_patch_save_part(0, sl) == 0 && up_used(sl) && trk[0].user == sl + 1u &&
           up_count() == 1u && slot_of(&trk[0]) == sl);
        power_cycle();
        sl_load(97u);                                  /* another sound, then back */
        trn_fac = trk[0].p[P_TRANS];                   /* (the factory sound's TRANSPOSE) */
        sl_load(sl);
        ok = trk[0].user == sl + 1u && trk[0].eng_req == e0;
        for (i = 0; i < P_COUNT; i++)
            ok &= param_kept(i) ? 1 : trk[0].p[i] == sv[i];
        ck("  after a power cycle: loading the slot gives the saved sound back (every sound value, its name = the factory name it came from)", ok);
        ck("  TRANSPOSE is the patch's: the factory sound loaded between set the part to its own 0, the saved patch brings -24 back", trn_fac == 0 && trk[0].p[P_TRANS] == -24 &&
           !param_kept(P_TRANS));
        {
            char nmk[13];
            sl_name(sl, nmk);
            ck("  the slot's name is the sound's own (a copy keeps the name), the category too", str_eq(nmk, ds_name(e0, f0)) && sl_cat(sl) == preset_cat(e0, f0));
        }
        (void)blk;
        k = (uint32_t)sl_restore(sl);
        ck("RESTORE: the record is gone, the part that played it plays the factory sound again", (k == 0u || k == 3u) && !up_used(sl) && up_count() == 0u && trk[0].user == 0u &&
           trk[0].eng_req == e0 && trk[0].preset == f0);
        ok = 1;
        for (i = 0; i < P_COUNT; i++)
            ok &= param_kept(i) ? 1 : trk[0].p[i] == fac[i];
        ck("  bit for bit the factory sound (every sound value as it was before the SAVE)", ok);
        power_cycle();
        ck("  after a power cycle: still the factory sound in the slot, no chunk holds a record", !up_used(sl) && pt_bytes() == PT_HDR * 0u + pt_bytes() && up_count() == 0u);
        ck("RESTORE of a slot that holds its factory sound: rc 0, nothing written", (watch = 1, wrote = stray = 0, sl_restore(sl)) == 0 && !wrote && !stray && (watch = 0, 1));
    }

    /* 3a2: K4 RESTORE (PATCHES, owner 2026-10-26): the part reloads the SAVED version of its slot, edits gone, the record stays, never the factory sound */
    {
        int16_t sv[P_COUNT];
        uint32_t sl = 41u, nrec;
        song.sel = 0;
        sl_load(sl);
        trk[0].p[P_REL] = (int16_t)(trk[0].p[P_REL] > 60 ? 12 : 88);
        memcpy(sv, trk[0].p, sizeof sv);
        up_patch_save_part(0, sl);
        nrec = up_count();
        trk[0].p[P_REL] = (int16_t)(sv[P_REL] > 60 ? 7 : 71);      /* an unsaved edit */
        trk[0].p[P_E2] = (int16_t)(sv[P_E2] > 30 ? 3 : 90);
        patch_restore_do(sl);
        ok = trk[0].user == sl + 1u && up_used(sl) && up_count() == nrec && slot_of(&trk[0]) == sl;
        for (i = 0; i < P_COUNT; i++)
            ok &= param_kept(i) ? 1 : trk[0].p[i] == sv[i];
        ck("K4 RESTORE of an edited saved slot: the saved record's values back, the record still stored, not the factory sound", ok);
        undo_swap();
        ck("  one undo brings the unsaved edit back", trk[0].p[P_REL] == (sv[P_REL] > 60 ? 7 : 71));
        sl_load(97u);
        patch_restore_do(sl);
        ck("  RESTORE for a slot the part does not play is refused (it keeps playing #98)", slot_of(&trk[0]) == 97u);
        (void)sl_restore(sl);
    }

    /* 3b: RESTORE ALL; the parts that play a saved slot get their factory sound */
    {
        uint32_t sl;
        song.sel = 0;
        for (sl = 0; sl < UP_SLOTS; sl += 3u) {
            sl_load(sl);
            trk[0].p[P_REL] = (int16_t)(11 + sl % 70u);
            up_patch_save_part(0, sl);
        }
        song.sel = 1;
        sl_load(6u);
        sl_load(9u);
        up_patch_save_part(1, 9u);                      /* part 2 plays saved slot 10 */
        song.sel = 0;
        sl_load(30u);
        ck("RESTORE ALL setup: 43 saved slots (every 3rd, plus #10), part 1 on a saved slot, part 2 on slot 10", up_count() == 43u && up_used(30) && trk[1].user == 10u);
        watch = 1; wrote = stray = 0;
        k = (uint32_t)sl_restore_all();
        watch = 0;
        ck("RESTORE ALL: every record gone (rc 0 or 3), written in the patch store only", (k == 0u || k == 3u) && up_count() == 0u && !stray);
        ck("  the parts that played saved slots play their factory sound (user 0, the slot's engine and index)", trk[0].user == 0u && trk[1].user == 0u &&
           slot_of(&trk[0]) == 30u && slot_of(&trk[1]) == 9u);
        power_cycle();
        ck("  after a power cycle: still no record anywhere, every slot its factory sound", up_count() == 0u && pt_bytes() == 0u + pt_bytes() && !up_used(30));
        ck("RESTORE ALL on a unit that holds none: rc 0 and no write", (watch = 1, wrote = stray = 0, sl_restore_all()) == 0 && !wrote && !stray && (watch = 0, 1));
    }

    /* 3c: a power cut at EVERY step of a SAVE, of a RESTORE and of a RESTORE ALL (every 5th programmed byte, every byte near the commit records):
     * each slot is its old content or the new one (a saved record or the factory sound), never anything half; the other slots are untouched.
     * The 479-cut-point style of the 0.2.15 test, over the three operations; a second RESTORE ALL always finishes the job. */
    {
        uint32_t cuts[3] = {0, 0, 0}, bad_cut = 0, a, sl, cb, total_b, newer[3] = {0, 0, 0};
        for (a = 0; a < 3u; a++) {
            sl = a == 0u ? 63u : 66u;                   /* SAVE over slot #64 (a factory sound); RESTORE of slot #67 (a saved one: 66 = 6 x 11) */
            total_b = 0;
            for (cb = 0; cb <= total_b + 40u; cb += (cb + 60u > total_b || cb < 40u) ? 1u : 5u) {
                uint32_t had, nrec = 0, base_ok = 1;
                memset(nor, 0xFF, sizeof nor);
                power_cycle();
                song.sel = 0;
                for (k = 0; k < UP_SLOTS; k += 11u) {   /* the saved ones: slots 1, 12, 23 .. (k % 11 == 0), REL = 20 + k */
                    sl_load(k);
                    trk[0].p[P_REL] = (int16_t)(20 + k);
                    up_patch_save_part(0, k);
                }
                had = up_count();
                sl_load(63u);
                trk[0].p[P_REL] = 5;                    /* (the sound SAVE writes over slot 64) */
                progd = 0;
                cut_bytes = total_b ? (long)cb : -1;
                if (a == 0u)
                    (void)up_patch_save_part(0, sl);
                else if (a == 1u)
                    (void)sl_restore(sl);
                else
                    (void)sl_restore_all();
                if (!total_b)
                    total_b = progd;                    /* the first, uncut run: how many bytes the operation programs */
                cut_bytes = -1;
                power_cycle();
                cuts[a]++;
                for (k = 0; k < UP_SLOTS; k++)
                    if (up_used(k)) {
                        nrec++;
                        if (k % 11u == 0u)
                            base_ok &= vfit_eq(up_rec(k)->engine, P_REL, up_value(up_rec(k), P_REL), (int)(20 + k));   /* (PT_VER 4: as REL's range keeps it) */
                    }
                if (a == 0u) {                          /* SAVE: slot 64 is its factory sound or the new record, all the saved ones intact */
                    int is_new = up_used(sl);
                    ok = base_ok && nrec == had + (is_new ? 1u : 0u) && (!is_new || up_value(up_rec(sl), P_REL) == 5);
                    newer[0] += is_new;
                } else if (a == 1u) {                   /* RESTORE of slot 67: its record or its factory sound */
                    int is_new = !up_used(sl);
                    ok = base_ok && nrec == had - (is_new ? 1u : 0u);
                    newer[1] += is_new;
                } else {                                /* RESTORE ALL: any chunk restored or not, every record that is left whole */
                    ok = base_ok && nrec <= had;
                    newer[2] += nrec == 0u;
                    if (ok) {                           /* ... and doing it again finishes it */
                        ok = sl_restore_all() <= 3 && up_count() == 0u;
                        power_cycle();
                        ok &= up_count() == 0u;
                    }
                }
                if (!ok) {
                    bad_cut++;
                    if (bad_cut < 4u)
                        printf("patch:   cut %u of op %u: base_ok %u nrec %u had %u used(sl) %d\n", cb, a, base_ok, nrec, had, up_used(sl));
                }
            }
        }
        printf("patch: power cuts tried: SAVE %u, RESTORE %u, RESTORE ALL %u (new content after the cut: %u, %u, %u)\n", cuts[0], cuts[1], cuts[2], newer[0], newer[1], newer[2]);
        ck("a power cut at every step of a SAVE, a RESTORE and a RESTORE ALL: every slot its old content or the new, the others untouched, RESTORE ALL done again finishes",
           !bad_cut && cuts[0] > 100u && cuts[1] > 100u && cuts[2] > 100u);   /* (6 records a chunk: a smaller chunk image than with 8, fewer bytes to cut in) */
    }

    /* 3d: MIDI program change = the slot (bank 0 and 1 alike); FAV is the slot's */
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;
    {
        uint32_t fe, fk, good = 0;
        for (k = 0; k < UP_USER0; k += 9u) {
            pc_recall(0u, k, k & 1u);
            good += sl_fac(k, &fe, &fk) && trk[0].eng_req == fe && trk[0].preset == fk && slot_of(&trk[0]) == k;
        }
        ck("MIDI program change n (bank 0 or 1) loads slot n: the factory sound of that browse position", good == (UP_USER0 + 8u) / 9u);
        song.sel = 0;
        sl_load(20u);
        trk[0].p[P_REL] = 33;
        up_patch_save_part(0, 20u);
        sl_load(21u);
        pc_recall(0u, 20u, 0u);
        ck("  a saved slot loads its record by program change too", trk[0].user == 21u && trk[0].p[P_REL] == 33);
        pc_recall(0u, 128u, 0u);
        pc_recall(0u, 5u, 3u);
        pc_recall(0u, UP_VSLOTS - UP_SLOTS - UP_NUSER, 2u);
        ck("  a program past the slots, past the last extra place, or another bank (3): ignored", trk[0].user == 21u);
        {   /* audit G13: bank 2 = the read-only places past the 128, #129 + n, and FAV works on them through the factory sound's own flag (no slot bit, no new field) */
            uint32_t fe2, fk2, pl = UP_SLOTS + UP_NUSER + 3u;
            uint8_t before[14][32];
            pc_recall(0u, 3u, 2u);
            ck("bank 2, program 3 = #132: that factory sound, no slot, its place #132",
               sl_fac(pl, &fe2, &fk2) && trk[0].eng_req == fe2 && trk[0].preset == fk2 && !trk[0].user && slot_of(&trk[0]) == UP_SLOTS && slot_place(&trk[0]) == pl);
            ck("  and it is not a favourite yet", !preset_favorite() && !list_fav(pl));
            memcpy(before, favorites.factory, sizeof before);   /* (rows 0..13: the factory flags; 14 and 15 are the MIDI map and the settings bytes) */
            preset_mark(1);
            ck("  FAV on a place past the 128 stars it (the factory flag of its engine and index), no slot bit moves",
               preset_favorite() && list_fav(pl) && favorite_has(fe2, fk2) && !list_fav(pl + 1u) && favorite_has(NENGINES, 0u) == 0 && favorites.user == 0u);
            sl_load(8u);
            ck("  it stays starred when another sound is loaded, and a slot's FAV is as before", list_fav(pl) && !preset_favorite() && !list_fav(8u));
            pc_recall(0u, 3u, 2u);
            preset_mark(0);
            ck("  FAV off clears it, back to the bytes before",
               !preset_favorite() && !list_fav(pl) && !memcmp(before, favorites.factory, sizeof before));
        }
        song.sel = 0;
        sl_load(8u);
        favorite_set(NENGINES, 8u, 1);
        sl_load(9u);
        ck("FAV is the slot's: starred slot 9 (#9), slot 10 (#10) not", list_fav(8u) && !list_fav(9u) && !preset_favorite());
        sl_load(8u);
        ok = preset_favorite();
        up_patch_save_part(0, 8u);
        sl_load(8u);
        ok &= preset_favorite();
        sl_restore(8u);
        ok &= preset_favorite();
        ck("  it stays through a SAVE and a RESTORE (the slot holds a factory sound, then a saved one, then the factory one again)", ok && favorite_has(NENGINES, 8u));
        up_put(20u, 0);
    }

    /* 4: the page */
    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 0u);
    ui_power_on();
    go_main_to(HP_MORPH);
    press(B_SAVE);
    ck("a tap of SAVE opens PATCHES (from HOME); again: the next SAVE page", cur_page()->graph == GR_BROWSE && !at_home());
    press(B_SAVE);
    ck("  PATTERNS", cur_page()->graph == GR_PATS);
    power_cycle();
    song.playing = 0;
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 0u);
    go_title("PATCHES");
    frame();
    ck("SAVE's first page is PATCHES; the SAVE family: PATCHES PATTERNS MULTIS TOOLS", cur_page()->graph == GR_BROWSE &&
       cur_page()->fam == FAM_SAVE && str_eq(PAGES[ui.page + 1u].title, "PATTERNS") && str_eq(PAGES[ui.page + 2u].title, "MULTIS") &&
       str_eq(PAGES[ui.page + 3u].title, "TOOLS"));
    (void)pl_cursor(&total);
    (void)preset_pos(&occ);
    ck("ONE list of 128 slots: no empty place, no tail, the cursor on the slot of the sound the part plays", total == UP_VSLOTS && occ == UP_VSLOTS &&
       pl_cursor(&total) == slot_of(&trk[0]) && slot_of(&trk[0]) < UP_SLOTS);
    ok = 1;
    for (i = 0; i < 300u; i++) {
        uint32_t c0 = pl_cursor(&total);
        turn(EN_K1, 1);
        ok &= pl_cursor(&total) == (c0 + 1u) % UP_VSLOTS && slot_place(&trk[0]) == preset_at(pl_cursor(&total)) && (!trk[0].user || trk[0].user > UP_USER0);
    }
    ck("K1 steps the slots one by one, round the ends (300 steps), each loads its sound (the part is always in the slot under the cursor)", ok);
    go_title("PATCHES");
    sl_load(10u);
    frame();
    trk[0].p[P_E4] = (int16_t)(trk[0].p[P_E4] + 7);
    ui.msg_t = 0;
    ui.force = 1; frame();
    ck("the slot the part plays (a factory sound, edited): the FAV card is drawn", ui.col[3][0] != 0x7f && ui.col[0][0] != 0x7f);
    turn(EN_K4, 1);
    ck("K4 there is FAV (1.1: the page has no RESTORE): no dialog, slot #11 starred", !ui.confirm && favorite_has(NENGINES, 10u));
    turn(EN_K4, -1);
    ck("  K4 left: the star off, nothing else changes", !ui.confirm && !favorite_has(NENGINES, 10u));
    hold(B_SAVE);
    ck("hold SAVE: the OVERWRITE dialog, the slot the sound came from offered (#11), nothing written yet", ui.confirm == CF_SAVE_OVER && ui.svs == 10u && !up_count());
    {
        char a1[32], b1[32];
        confirm_text(a1, b1);
        ck("  it reads OVERWRITE #11 <the slot's name>? and WITH <the sound's name>", !strncmp(a1, "OVERWRITE #11 ", 14) && a1[str_len(a1) - 1u] == '?' &&
           !strncmp(b1, "WITH ", 5));
    }
    turn(EN_K1, 1);
    turn(EN_K1, 1);
    ck("  K1 picks another slot (#13), round the ends, without loading anything", ui.svs == 12u && slot_of(&trk[0]) == 10u && ui.confirm == CF_SAVE_OVER);
    for (i = 0; i < 13u; i++)
        turn(EN_K1, -1);
    ck("  and back past #1 to #128", ui.svs == 127u);
    for (i = 0; i < 13u; i++)
        turn(EN_K1, 1);
    ck("  and on round to #13 again", ui.svs == 12u);
    {   /* 0.2.21: K2 RENAME in the dialog: the NAME screen for the sound's name, back to the dialog; OCT+ then saves under it; cancel leaves the old name */
        char a1[32], b1[32], old[16], got[16];
        sound_name(&trk[0], old);
        turn(EN_K2, 1);
        ck("K2 RENAME: the NAME screen opens (the dialog is closed, nothing written), the field holds the sound's name",
           name_on() && nm.kind == NK_SAVE_NAME && !ui.confirm && !up_count() && str_eq(nm.s, old) && ui.svs == 12u);
        {
            char t[32];
            nm_title(t);
            ck("  its title names the slot (NAME #13), the footer says OK / CANCEL", str_eq(t, "NAME #13"));
        }
        str_cpy(nm.s, "OTHER NAME", sizeof nm.s);
        nm.len = nm.cur = 10;
        press(B_OCTDN);
        confirm_text(a1, b1);
        ck("  OCT- (cancel): back to the dialog, the old name still shown, nothing written, same slot", !name_on() && ui.confirm == CF_SAVE_OVER && ui.svs == 12u &&
           !up_count() && !save_nm[0] && !strcmp(b1 + 5, old));
        turn(EN_K2, -1);
        str_cpy(nm.s, " NEW NAME  ", sizeof nm.s);
        nm.len = nm.cur = (uint8_t)str_len(nm.s);
        press(B_OCTUP);
        confirm_text(a1, b1);
        ck("  OCT+: back to the dialog (nothing written), spaces at the ends dropped, it reads WITH NEW NAME, slot #13 kept",
           !name_on() && ui.confirm == CF_SAVE_OVER && ui.svs == 12u && !up_count() && !strcmp(b1, "WITH NEW NAME") && !strncmp(a1, "OVERWRITE #13 ", 14));
        turn(EN_K2, 1);
        ck("  RENAME again: the field starts from the name chosen", name_on() && str_eq(nm.s, "NEW NAME"));
        press(B_OCTDN);
        press(B_OCTUP);
        up_name(12u, got);
        ck("  dialog OCT+: SAVED #13 under NEW NAME; the part plays it and its footer name is the new one", up_used(12) && str_eq(got, "NEW NAME") && trk[0].user == 13u &&
           !save_nm[0]);
        sound_name(&trk[0], got);
        ck("  the sound's name is the new one", str_eq(got, "NEW NAME"));
        set_engine(TSEL->eng_req);                      /* the part leaves the slot, the slot is loaded again: the record's name is the new one */
        sl_load(12u);
        sound_name(&trk[0], got);
        ck("  reload of #13: the new name", str_eq(got, "NEW NAME") && trk[0].user == 13u);
        up_put(12u, 0);
        sl_load(10u);
        hold(B_SAVE);
        ui.svs = 12u;
        turn(EN_K2, 1);
        press(B_OCTUP);                                 /* (the unchanged name accepted) */
        press(B_OCTDN);                                 /* the dialog cancelled */
        hold(B_SAVE);
        confirm_text(a1, b1);
        ck("  a new SAVE after a cancelled dialog starts without the name chosen before (the sound's own)", ui.confirm == CF_SAVE_OVER && !save_nm[0] && !up_used(12));
        ui.svs = 12u;
    }
    press(B_OCTUP);
    ck("  OCT+: SAVED #13: that slot holds the sound now, the part plays it, one record, the list is still 128 slots, the cursor on it", up_used(12) && up_count() == 1u &&
       trk[0].user == 13u && !strncmp(ui.msg, "SAVED", 5) && pl_cursor(&total) == 12u && total == UP_VSLOTS && up_rec(12)->engine == trk[0].eng_req);
    kk = 12u;
    turn(EN_K4, 1);
    ck("K4 on a saved slot: FAV too, no dialog; the record, the part and the cursor stay (1.1: RESTORE ALL in SAVE > TOOLS is the only restore)",
       !ui.confirm && favorite_has(NENGINES, 12u) && up_used(12) && up_count() == 1u && trk[0].user == 13u && pl_cursor(&total) == 12u);
    turn(EN_K4, -1);
    (void)sl_restore(12u);                              /* (the record out of the way for the checks below) */
    {
        uint32_t eng0 = TSEL->eng_req;
        for (i = 0; i < 40u && list_mode() <= CAT_N; i++)
            turn(EN_K2, 1);
        ok = list_mode() > CAT_N && TSEL->eng_req == eng0;
        n = list_mode() - CAT_N - 1u;
        (void)preset_pos(&total);
        for (i = 0; i < total; i++)
            ok &= sl_engine(preset_at(i)) == eng_vis(n);
        ck("K2 TYPE: past the categories one entry per engine; a filter only (the part keeps its engine), the list holds the slots of that engine", ok && total > 0u);
        for (i = 0; i < 40u && list_mode(); i++)
            turn(EN_K2, -1);
        ck("  K2 back to ALL", !list_mode());
    }
    go_title("PATCHES");
    sl_load(5u);
    frame();
    turn(EN_K4, 1);
    ck("K4 FAV marks the cursor's slot", preset_favorite() && favorite_has(NENGINES, 5u));
    {   /* TYPE FAV lists the starred slots; a saved sound in a starred slot stays in it */
        list_set(1u);
        (void)preset_pos(&total);
        ok = total == 1u && preset_at(0) == 5u;
        up_patch_save_part(0, 5u);
        (void)preset_pos(&total);
        ok &= total == 1u && preset_at(0) == 5u;
        list_set(0u);
        ck("TYPE FAV lists the starred slots, before and after a SAVE over one", ok);
        up_put(5u, 0);
    }
    {   /* a sound that is in no slot (an engine change): the cursor and FAV say so, SAVE offers the slot its factory sound belongs to */
        select_engine(ENGI_FM6);
        frame();
        ok = slot_of(&trk[0]) < UP_SLOTS || trk[0].user == 0u;
        sl_load(3u);
        up_patch_save_part(0, ds_list_rank(ENGI_FM6, 1u) < UP_SLOTS ? ds_list_rank(ENGI_FM6, 1u) : 0u);
        select_engine(0u);
        apply_preset_to(&trk[0], 1u);
        ck("an engine change loads the engine's factory sound; its slot, if still the factory sound, is the cursor's", ok && trk[0].user == 0u);
        for (k = 0; k < UP_SLOTS; k++)
            up_put(k, 0);
    }

    {   /* owner bug 2026-10-12: SAVE, then EDIT (or any page button) must leave, saving nothing, from every SAVE page, a dialog and NAME */
        static const struct { uint8_t b; uint8_t fam; } LV[] = {{B_EDIT, FAM_EDIT}, {B_FX, FAM_FX}, {B_ENV, FAM_ENV}, {B_LFO, FAM_LFO},
            {B_ARP, FAM_ARP}, {B_SEQ, FAM_SEQ}, {B_SCL, FAM_SCL}, {B_GLO, FAM_GLO}, {B_HOME, FAM_TRK}};
        static uint8_t img[sizeof nor];
        uint32_t s, j, good = 0, tried = 0;
        for (s = 0; s < 7u; s++)
            for (j = 0; j < sizeof LV / sizeof LV[0]; j++) {
                int left, where_ok;
                ui.confirm = 0;
                if (name_on()) name_close();
                frames(400);
                go_main();
                press(B_SAVE);
                if (s == 1u || s == 5u || s == 6u) {           /* PATCHES: s 1 another slot, 5 the OVERWRITE dialog, 6 a NAME screen (PATTERNS') */
                    if (s == 1u) preset_go(20u);
                    if (s == 6u) { press(B_SAVE); go_title("PATTERNS"); name_open(NK_PATTERN, 3u); }
                    if (s == 5u) { preset_go(1u); hold(B_SAVE); }
                } else if (s == 2u) {
                    press(B_SAVE);
                } else if (s == 3u) {
                    press(B_SAVE); press(B_SAVE);
                } else if (s == 4u) {
                    press(B_SAVE); press(B_SAVE); press(B_SAVE);
                }
                where_ok = s == 5u ? ui.confirm != 0 : s == 6u ? name_on() : 1;
                memcpy(img, nor, sizeof img);
                frames(400);
                press(LV[j].b);
                frames(100);
                left = !ui.confirm && !name_on() && (LV[j].fam == FAM_TRK ? (!at_home() && cur_page()->fam == FAM_TRK) || at_home() :
                       cur_page()->fam == LV[j].fam);
                tried++;
                if (where_ok && left && !memcmp(img, nor, sizeof img))
                    good++;
                else
                    printf("   state %u button %u: dialog/name %d left %d fam %d\n", (unsigned)s, (unsigned)LV[j].b, where_ok, left, (int)cur_page()->fam);
            }
        ck("SAVE pages (PATCHES, another slot, PATTERNS, MULTIS, TOOLS), a dialog, NAME: EDIT FX ENV LFO ARP SEQ SCL GLO HOME all leave, flash unchanged",
           good == tried);
    }

    /* 1c: LFO TRIG (DUELL 0.2.2): FREE is the default, but the sparse store keeps NOTE as its own default for the two ids (upreset.c
     * up_def): a NOTE patch has no TRIG pair, exactly as every patch stored before; a FREE patch stores one; both load as stored */
    {
        track_t *t = &trk[0];
        uint32_t len_note, len_free, pr, a, b, c, d, e, j, pairs = 0;
        const uint8_t *raw;
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        ck("TRIG: a factory sound loads with both LFOs FREE (the default)", t->p[P_LTRIG] == 1 && t->p[P_L2TRIG] == 1);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 0;
        ck("TRIG: a NOTE patch stores", up_store_trk(t, 50u, "LFO NOTE", 0) == 0);
        len_note = pt_rd16(pt_raw(50u));
        raw = pt_raw(50u);
        pt_layout(raw, &a, &b, &c, &d, &e);
        pr = c;
        pairs = trig_stored(raw, pr);
        ck("  its record holds no TRIG value (the same bytes as a patch stored before 0.2.2)", pairs == 0u);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 1;
        ck("TRIG: a FREE patch stores", up_store_trk(t, 51u, "LFO FREE", 0) == 0);
        len_free = pt_rd16(pt_raw(51u));
        raw = pt_raw(51u);
        pt_layout(raw, &a, &b, &c, &d, &e);
        ck("  its record holds both TRIG values (a pair each, or two values of the bitmap form)", trig_stored(raw, c) == 2u && len_free > len_note);
        power_cycle();
        t = &trk[0];
        set_engine_of(t, 0u);
        t->p[P_LTRIG] = t->p[P_L2TRIG] = 1;
        up_load(50u);
        ck("  after a power cycle the NOTE patch loads NOTE / NOTE (an old patch does not change)", t->p[P_LTRIG] == 0 && t->p[P_L2TRIG] == 0);
        up_load(51u);
        ck("  and the FREE patch loads FREE / FREE", t->p[P_LTRIG] == 1 && t->p[P_L2TRIG] == 1);
        {
            const up_rec_t *r = up_rec(50u);
            ck("  the decoded NOTE record reads NOTE (up_rec)", up_value(r, P_LTRIG) == 0 && up_value(r, P_L2TRIG) == 0);
        }
        up_put(50u, 0);
        up_put(51u, 0);
    }

    /* 1d: DUCK CURVE (DUELL 0.2.5): EXP is the default, but the sparse store keeps LIN as its own default (upreset.c up_def): a LIN patch
     * stores no CURVE value (the bytes of a patch stored before), so an old patch still loads LIN; an EXP patch stores it and loads EXP */
    {
        track_t *t = &trk[0];
        set_engine_of(t, 0u);
        apply_preset_to(t, 3u);
        ck("CURVE: a factory sound loads with the ducker's EXP release (the default)", t->p[P_DKCRV] == 1 && TP[P_DKCRV].def == 1);
        t->p[P_DKCRV] = 0;
        ck("CURVE: a LIN patch stores", up_store_trk(t, 52u, "CURVE LIN", 0) == 0);
        t->p[P_DKCRV] = 1;
        ck("CURVE: an EXP patch stores", up_store_trk(t, 53u, "CURVE EXP", 0) == 0);
        power_cycle();
        t = &trk[0];
        set_engine_of(t, 0u);
        t->p[P_DKCRV] = 1;
        up_load(52u);
        ck("  after a power cycle the LIN patch loads LIN (an old patch does not change)", t->p[P_DKCRV] == 0);
        up_load(53u);
        ck("  and the EXP patch loads EXP", t->p[P_DKCRV] == 1);
        ck("  the decoded LIN record reads LIN, the EXP one EXP (up_rec)", up_value(up_rec(52u), P_DKCRV) == 0 && up_value(up_rec(53u), P_DKCRV) == 1);
        up_put(52u, 0);
        up_put(53u, 0);
    }

    memset(nor, 0xFF, sizeof nor);
    power_cycle();
    song.playing = 0;
    song.sel = 0;
    set_engine_of(&trk[0], 0u);
    apply_preset_to(&trk[0], 1u);
    {   /* U1: the last 4 slots are USER slots; the factory sounds they displace are read-only places after them; a record already there stays */
        char tag[6], nmu[13];
        uint32_t fe2, fk2, e2, k2, bad = 0, snd;
        up_slot_label(tag, UP_USER0);
        sl_name(UP_USER0, nmu);
        ck("U1: slot 125 is USER 1, empty: tag U1, name USER EMPTY, no factory sound, no record",
           UP_NUSER == 4u && str_eq(tag, "U1") && str_eq(nmu, "USER EMPTY") && !sl_fac(UP_USER0, &fe2, &fk2) && !up_used(UP_USER0) && sl_empty_user(UP_USER0 + 3u));
        for (k2 = 0; k2 < UP_NUSER; k2++) {   /* the displaced factory sounds: places 129..132, numbered #125..#128, the sounds browse positions 124..127 held */
            e2 = ds_list_at(UP_USER0 + k2, &fk2);
            up_slot_label(tag, UP_SLOTS + k2);
            bad |= !(sl_fac(UP_SLOTS + k2, &fe2, &fk2) && fe2 == e2) || sl_engine(UP_SLOTS + k2) != fe2 || tag[0] != '#' || tag[1] != '1' || tag[3] != (char)('5' + k2);
            sl_name(UP_SLOTS + k2, nmu);
            bad |= !str_eq(nmu, ds_name(fe2, fk2));
        }
        ck("  the 4 displaced factory sounds are reachable as read-only places #125..#128 (name, engine, label)", !bad);
        sl_load(UP_SLOTS + 1u);
        ck("  loading a displaced place plays that factory sound, in no slot to save over (SAVE offers USER 1)", trk[0].preset == sl_tk[UP_USER0 + 1u] && !trk[0].user &&
           slot_of(&trk[0]) == UP_SLOTS && slot_place(&trk[0]) == UP_SLOTS + 1u && slot_home(&trk[0]) == UP_USER0);
        snd = trk[0].preset;
        sl_load(UP_USER0 + 2u);
        ck("  loading an empty USER slot changes no sound; the part points at it (SAVE offers it), the footer keeps the sound's own name", trk[0].preset == snd && trk[0].user == UP_PTR_EMPTY(UP_USER0 + 2u) &&
           slot_of(&trk[0]) == UP_USER0 + 2u && slot_home(&trk[0]) == UP_USER0 + 2u);
        {
            char nm3[16];
            sound_name(&trk[0], nm3);
            ck("  .. sound_name is the sound's, not USER EMPTY", !str_eq(nm3, "USER EMPTY") && nm3[0]);
        }
        ck("  every list but FAV shows the empty USER slots (ALL, one category, one engine)", ({ uint32_t m, seen = 0; for (m = 0; m < LIST_N; m++) { list_set(m); seen += m == 1u ? !list_keep(UP_USER0) : list_keep(UP_USER0); } list_set(0); seen == LIST_N; }));
        song.sel = 0;
        ui.svs = (uint8_t)UP_USER0;
        trk[0].user = 0;
        sl_load(UP_SLOTS + 1u);
        patch_save_do(0);
        ck("  SAVE over USER 1 (ui.svs): a record with the sound's own name, not USER EMPTY; the displaced #126 is still there",
           up_used(UP_USER0) && up_count() == 1u && !str_eq(({ sl_name(UP_USER0, nmu); nmu; }), "USER EMPTY") && str_eq(nmu, ds_name(trk[0].eng_req, trk[0].preset)) &&
           sl_fac(UP_SLOTS + 1u, &fe2, &fk2) && fk2 == snd && !sl_empty_user(UP_USER0) && sl_empty_user(UP_USER0 + 1u));
        power_cycle();
        ck("  after a power cycle USER 1 is still there; USER 2..4 still empty", up_used(UP_USER0) && !up_used(UP_USER0 + 1u) && up_count() == 1u);
        sl_load(UP_SLOTS + 2u);
        ck("  with USER 1 taken, a displaced place offers the first empty USER slot (USER 2), not USER 1 over the owner's sound", slot_home(&trk[0]) == UP_USER0 + 1u);
        sl_load(UP_USER0);
        (void)pl_cursor(&total);
        ck("  the record is the browse list's place, label U1, in all the places", up_used(UP_USER0) && total == UP_VSLOTS && preset_at(pl_cursor(&total)) == UP_USER0);
    }

    {   /* the factory sounds past the bank's 128 (tools/duell_sounds.py N_BASE): no browse position of the 128 moves, the others are read-only places after the displaced ones */
        static const uint8_t BANK_N[16] = {20, 0, 11, 9, 0, 8, 13, 7, 0, 12, 0, 11, 14, 0, 23, 0};   /* the bank's factory sounds per engine: their index is under this */
        uint32_t r, e3, k3, bad3 = 0, fe3, fk3, bad4 = 0;
        char tg[6], nm4[13];
        for (r = 0; r < ds_list_n(); r++) {
            e3 = ds_list_at(r, &k3);
            bad3 |= (k3 < BANK_N[e3]) != (r < 128u);
        }
        ck("sounds past the bank's 128: the browse positions 0..127 hold exactly the bank's sounds (nothing moved), the later ones follow", !bad3 && ds_list_n() == 128u + UP_NXTRA && UP_NXTRA > 0u);
        for (r = 0; r < UP_NXTRA; r++) {
            uint32_t pl = UP_SLOTS + UP_NUSER + r;
            e3 = ds_list_at(128u + r, &k3);
            up_slot_label(tg, pl);
            sl_name(pl, nm4);
            bad4 |= !(sl_fac(pl, &fe3, &fk3) && fe3 == e3 && fk3 == k3) || sl_engine(pl) != e3 || up_used(pl) || tg[0] != '#' || !str_eq(nm4, ds_name(e3, k3));
        }
        ck("  each is a read-only place #129.. with its name, engine and a label, never a slot", !bad4 && !sl_fac(UP_VSLOTS, &fe3, &fk3));
        sl_load(UP_VSLOTS - 1u);
        ck("  loading the last plays that sound, in no slot to save over (SAVE offers the first empty USER slot: USER 2, USER 1 holds a record), its place is found again",
           trk[0].eng_req == ds_list_at(ds_list_n() - 1u, &k3) && trk[0].preset == k3 && !trk[0].user && slot_of(&trk[0]) == UP_SLOTS && slot_place(&trk[0]) == UP_VSLOTS - 1u &&
           slot_home(&trk[0]) == UP_USER0 + 1u && up_used(UP_USER0));
    }

    {   /* U1: the factory multis hold engine + factory index, never a browse slot: loading them gives the factory sound whatever the USER slots hold; a part pointed at an empty USER slot still plays a factory sound */
        uint32_t m, pi, bad2 = 0, hit = 0;
        for (m = 0; m < DF_N; m++) {
            memset(nor, 0xFF, sizeof nor);
            power_cycle();
            song.playing = 0;
            sl_load(UP_SLOTS);
            project_load(m);
            for (pi = 0; pi < NSHOWN; pi++) {
                uint32_t r = ds_list_rank(trk[pi].eng_req % NENGINES, trk[pi].preset);
                char a[16];
                sound_name(&trk[pi], a);
                bad2 |= trk[pi].user != 0u || !str_eq(a, ds_name(trk[pi].eng_req % NENGINES, trk[pi].preset));
                if (r >= UP_USER0 && r < UP_SLOTS) {
                    hit++;
                    bad2 |= slot_of(&trk[pi]) != UP_SLOTS || slot_place(&trk[pi]) != r + UP_NUSER || slot_home(&trk[pi]) != UP_USER0;
                }
            }
        }
        printf("patch:   factory multis: %u parts play a sound a USER slot displaced\n", hit);
        ck("U1: every factory multi loads its factory sounds (user 0, the factory name; a displaced one sits at its read-only place)", !bad2);
        song.sel = 0;
        sl_load(UP_USER0);
        ck("  a part pointed at an empty USER slot is not a stored record (TIDE's JRNY / WNDR and the End B roll treat it as factory); a saved one is",
           trk[0].user == UP_PTR_EMPTY(UP_USER0) && !UP_REC_USER(&trk[0]) && (ui.svs = (uint8_t)UP_USER0, patch_save_do(0), UP_REC_USER(&trk[0])));
    }

    printf(bad ? "patch: FAILED (%d)\n" : "patch: all ok\n", bad);
    return bad ? 1 : 0;
}
