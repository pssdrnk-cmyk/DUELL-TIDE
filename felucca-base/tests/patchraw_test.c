/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 0.2.19's PATCH_RAW (editor_patchraw.c, upreset.c up_put_raw / up_raw_get; EDITOR_PROTOCOL.md "PATCH_RAW (84)") against simulated NOR flash:
 *   1. a fresh unit: every slot holds its factory sound (INFO saved 0, READ refused);
 *   2. READ gives the stored record whole with its slot and revision bytes zero, in 112 byte pieces, its CRC-32; the biggest record there is (an FM6 sound with
 *      its patch, End B, the SEQ arp block with 16 steps) travels and comes back bit for bit in another slot, decodes the same (values, name, patch, arp block)
 *      and loads the same sound; only the chunk's own sectors are written;
 *   3. a second write to a slot is a newer revision (a rename through the bytes), a different slot keeps the others;
 *   4. refusal while the sequencer plays (rc 3, nothing written; READ still works), malformed and stale transfers (arguments, order, CRC, a missing part, no begin,
 *      a USB reset, 15 s, a backup transfer that took the staging RAM), records the store would not read (rc 1 at commit, nothing written);
 *   5. a flash failure (rc 4, the slot as it was) and a power cut at every step of a write: the old record or the new one whole, the other records untouched;
 *   6. a bank: the worst record written to every slot the store takes (128; TIDE 126) through PATCH_RAW, each read back identical, and a rewrite of all of them.
 *   build/host/patchraw_test            (DUELL/host_tests.sh "patchraw") */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"
#include "endb_store.h"
#define NSAVE (PT_MAX < UP_SLOTS ? PT_MAX : UP_SLOTS)   /* records the store always takes: 126 with TIDE (chunk 21 holds its settings, plan 4.5), else all 128 */

static uint8_t nor[0x100000];
static long cut_bytes = -1;
static int dead, watch;
static uint32_t progd, erases, stray, wrote;
static void hit(uint32_t off, uint32_t n)
{
    if (!watch)
        return;
    if ((off >= 0xA6000u && off + n <= 0xBA000u) || (off >= 0xC8000u && off + n <= 0xCE000u) || (off >= 0xDC000u && off + n <= 0xE0000u) ||
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
    erases++;
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

enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t rep[4096];
static uint32_t rep_n;
static void ed_b(uint32_t v) { if (rep_n < sizeof rep) rep[rep_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j;
        na--;
        uint32_t k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
static uint8_t ed_smp_buf[512] __attribute__((aligned(4)));
static int ed_flash_stop(void) { return transport_busy(); }
#include "../firmware/src/editor_backup.c"
#include "../firmware/src/editor_patchraw.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("patchraw: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void power_cycle(void)
{
    dead = 0;
    cut_bytes = -1;
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    ed_pr_put = ed_bk_put = ed_wt_put = 0;
    transport_req = 0;
    song.playing = 0;
}

static void put32(uint8_t *a, uint32_t v) { for (uint32_t i = 0; i < 5u; i++) a[i] = (uint8_t)((v >> (7u * i)) & 127u); }
static void pack7(uint8_t *a, uint32_t *k, const uint8_t *p, uint32_t n)
{
    while (n) {
        uint32_t g = n > 7u ? 7u : n, m = 0, i;
        for (i = 0; i < g; i++) m |= (uint32_t)(p[i] >> 7) << i;
        a[(*k)++] = (uint8_t)m;
        for (i = 0; i < g; i++) a[(*k)++] = p[i] & 127u;
        p += g;
        n -= g;
    }
}
static uint32_t call(const uint8_t *a, uint32_t n)
{
    rep_n = 0;
    ed_patchraw_handle(ED_PATCH_RAW, a, n);
    return rep_n;
}
static uint32_t wr(const uint8_t *a, uint32_t n) { call(a, n); return rep[2]; }
static uint32_t pr_begin(uint32_t slot, uint32_t len, uint32_t crc)
{
    uint8_t a[9] = {2, (uint8_t)slot, (uint8_t)(len & 127u), (uint8_t)(len >> 7)};
    put32(a + 4, crc);
    return wr(a, sizeof a);
}
static uint32_t pr_data(uint32_t slot, uint32_t off, const uint8_t *p, uint32_t n)
{
    uint8_t a[4 + 160] = {3, (uint8_t)slot, (uint8_t)(off & 127u), (uint8_t)(off >> 7)};
    uint32_t k = 4;
    pack7(a, &k, p, n);
    return wr(a, k);
}
static uint32_t pr_end(uint32_t slot, uint32_t op) { uint8_t a[2] = {(uint8_t)op, (uint8_t)slot}; return wr(a, 2); }
static uint32_t pr_all(uint32_t slot, const uint8_t *rec, uint32_t len)   /* begin, 112 byte pieces, commit: the first failing rc */
{
    uint32_t off, rc = pr_begin(slot, len, st_crc32(rec, len));
    for (off = 0; !rc && off < len; off += 112u)
        rc = pr_data(slot, off, rec + off, len - off > 112u ? 112u : len - off);
    return rc ? rc : pr_end(slot, 4);
}
/* INFO + READ of a slot: 1 saved (rec/len filled, the CRC checked), 0 factory, -1 an error */
static int pr_read(uint32_t slot, uint8_t *rec, uint32_t *len)
{
    uint8_t i0[2] = {0, (uint8_t)slot};
    uint32_t n, crc, off;
    call(i0, 2);
    if (rep[2])
        return -1;
    n = (uint32_t)rep[4] | (uint32_t)rep[5] << 7;
    crc = ed_bk_r32(rep + 6);
    if (!rep[3])
        return n || crc ? -1 : 0;
    for (off = 0; off < n; off += 112u) {
        uint32_t c = n - off > 112u ? 112u : n - off;
        uint8_t r[5] = {1, (uint8_t)slot, (uint8_t)(off & 127u), (uint8_t)(off >> 7), (uint8_t)c};
        call(r, 5);
        if (rep[2] || rep[5] != c || ed_unpack7(rep + 6, rep_n - 6u, rec + off, c) != c)
            return -1;
    }
    *len = n;
    return st_crc32(rec, n) == crc ? 1 : -1;
}
static const uint8_t *flash_rec(uint32_t k) { return pt_raw(k); }
static int same_but_pos(uint32_t k, const uint8_t *rec, uint32_t len)   /* slot k's stored record = rec but bytes 2 and 3 */
{
    const uint8_t *p = flash_rec(k);
    uint32_t i;
    if (!p || pt_rd16(p) != len)
        return 0;
    for (i = 0; i < len; i++)
        if (i != 2u && i != 3u && p[i] != rec[i])
            return 0;
    return p[2] == k;
}

/* part t as the biggest record there can be (patch_test.c worst_part) */
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
    for (i = 0; i < NX; i++)                                    /* the cloud plan's job A: all 16 extension slots off their default, 32 hand-set B edits */
        XV(t, i) = (int16_t)((int)xb_def(i) - 64 + 1 + (int)(idx % 50u));
    for (i = 0; i < XB_MAX; i++)
        worst_ed[i] = (xb_edit_t){(uint8_t)(i < 28u ? 4u * i + 1u : 128u + i - 28u), (int16_t)(127 - (int)(i * 3u + idx % 7u))};
}

int main(void)
{
    static uint8_t A[PT_WORST + 8], B[PT_WORST + 8], C[PT_WORST + 8], snap[0x100000];
    uint32_t la = 0, lb = 0, lc = 0, i, k, ok, e0;
    uint8_t pk[FM6_PACKED], pk2[FM6_PACKED];
    memset(nor, 0xFF, sizeof nor);
    power_cycle();

    /* 1: a fresh unit */
    {
        uint8_t g[5] = {1, 5, 0, 0, 8};
        ck("fresh unit: a slot holds its factory sound: INFO saved 0, length 0, CRC 0", pr_read(5, A, &la) == 0 && rep[3] == 0);
        call(g, 5);
        ck("  READ of it: rc 1 (there is no record; UP_GET gives the factory values)", rep[2] == 1u);
    }

    /* 2: the biggest record there is, read, written to another slot, compared */
    worst_part(&trk[1], 3u);
    fm6_adopt(1);
    ck("the worst record is stored in slot 9 (FM6 + patch, End B at its biggest, SEQ arp block, every value off its default)",
       store_worst_cat(&trk[1], 9u, "WORST ONE", CAT_PAD, worst_ed, XB_MAX) == 0 && upf_fm6(9) && pt_raw(9)[4] & PTF_AK && pt_raw(9)[4] & PTF_MB);
    fm6_pack(fm6_patch[1], pk);
    power_cycle();
    ok = pr_read(9, A, &la) == 1;
    ck("PATCH_RAW READ: the record whole (pieces of 112 bytes, its CRC-32), the length the store holds, slot and revision bytes zero",
       ok && la == pt_dir[9].len && la > 300u && la <= PT_WORST && A[2] == 0 && A[3] == 0 && A[0] == (la & 255u) && A[1] == (la >> 8) && same_but_pos(9, A, la));
    ck("  it is the stored form: name, engine FM6, category PAD, PTF_CB, PTF_FM6, PTF_AK, PTF_MB", !memcmp(A + 7, "WORST ONE", 10) && A[5] == ENGI_FM6 && A[6] == CAT_PAD &&
       (A[4] & (PTF_CB | PTF_FM6 | PTF_AK | PTF_MB)) == (PTF_CB | PTF_FM6 | PTF_AK | PTF_MB));
    watch = 1; wrote = stray = 0;
    memcpy(snap, nor, sizeof nor);
    ck("PATCH_RAW write of that record into slot 70: rc 0", pr_all(70, A, la) == 0);
    watch = 0;
    {
        uint32_t c = pt_dir[70].ch, bytes_other = 0;
        ok = 1;
        for (i = 0; i < sizeof nor; i++) {
            uint32_t s0a = st_sector(pt_obj(c), 0), s0b = st_sector(pt_obj(c), 1);
            if (nor[i] != snap[i] && !((i >= s0a && i < s0a + 4096u) || (i >= s0b && i < s0b + 4096u)))
                bytes_other++;
        }
        ck("  only the sectors of the slot's chunk were written (no other chunk, no other object), all inside the patch store", !bytes_other && wrote && !stray && ok);
    }
    ck("  slot 70 holds the same record bit for bit (bytes 2 and 3 are the unit's: slot 70, revision 0)", same_but_pos(70, A, la) && flash_rec(70)[3] == 0);
    {
        const up_rec_t *r9 = up_rec(9);
        up_rec_t r = *r9;
        const up_rec_t *r70 = up_rec(70);
        ak_t a9, a70;
        int h9 = ak_get(9, &a9), h70 = ak_get(70, &a70);
        ok = !memcmp(r.packed, r70->packed, sizeof r.packed) && !memcmp(r.name, r70->name, 12) && r.engine == r70->engine && !memcmp(r.note, r70->note, 16) &&
             !memcmp(r.flags, r70->flags, 16);
        ck("  decoded the same: every value, name, engine, End B block, pattern", ok);
        ck("  the arp block decodes the same (groups, values, 16 steps)", h9 && h70 && !memcmp(&a9, &a70, sizeof a9));
        ck("  the FM6 patch is the same", !upf_get(70, pk2) && !memcmp(pk, pk2, sizeof pk));
    }
    {   /* loaded into the part: the same sound, the same steps, the same patch */
        int16_t p9[P_COUNT];
        step_t st9[NSTEP];
        uint8_t f9[FP_SIZE + 1u];
        song.sel = 0;
        up_load(9);
        memcpy(p9, trk[0].p, sizeof p9);
        memcpy(st9, trk[0].step, sizeof st9);
        memcpy(f9, fm6_patch[0], sizeof f9);
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 3u);
        up_load(70);
        ok = !memcmp(p9, trk[0].p, sizeof p9) && !memcmp(f9, fm6_patch[0], sizeof f9);
        for (i = 0; i < 16u; i++)
            ok &= !memcmp(&st9[i], &trk[0].step[i], sizeof(step_t));
        ck("  loaded into a part: the same parameters, the same 16 steps, the same FM6 patch as slot 9", ok);
    }
    power_cycle();
    ck("  after a power cycle: both records are there, the same", pr_read(70, B, &lb) == 1 && lb == la && !memcmp(A, B, la) && pr_read(9, C, &lc) == 1 && !memcmp(A, C, la));

    /* 3: a rename through the bytes; a second write is a newer revision */
    memcpy(B, A, la);
    memset(B + 7, 0, 12);
    memcpy(B + 7, "RENAMED", 7);
    ck("a rename through the bytes (name field, new CRC): rc 0, a newer revision, the rest of the record the same",
       pr_all(70, B, la) == 0 && flash_rec(70)[3] == 1u && same_but_pos(70, B, la) && !memcmp(up_rec(70)->name, "RENAMED", 8) && !memcmp(up_rec(9)->name, "WORST ONE", 10));
    {
        char nm[13];
        up_name(70, nm);
        ck("  the directory follows (name RENAMED, engine, category)", !strcmp(nm, "RENAMED") && pt_dir[70].eng == ENGI_FM6 && up_cat(70) == CAT_PAD);
    }
    ck("  the same bytes in another slot are the same patch (slot 3: rc 0, read back equal to slot 70's)", pr_all(3, B, la) == 0 && pr_read(3, C, &lc) == 1 && lc == la && !memcmp(B, C, la));

    /* 4: refusal while the sequencer plays */
    memcpy(snap, nor, sizeof nor);
    e0 = erases;
    transport_req = 1;
    ok = pr_begin(20, la, st_crc32(A, la)) == 3u;
    transport_req = 0;
    ok &= pr_begin(20, la, st_crc32(A, la)) == 0u && pr_data(20, 0, A, 112) == 0u;
    transport_req = 1;
    ok &= pr_data(20, 112, A + 112, 112) == 3u && pr_end(20, 4) == 3u;
    ck("while the sequencer plays (or starts): begin, a piece and the commit are refused (rc 3)", ok);
    ck("  nothing was written (the flash is unchanged, no erase), the slot is still on its factory sound", erases == e0 && !memcmp(snap, nor, sizeof nor) && !up_used(20));
    ok = pr_read(70, C, &lc) == 1 && !memcmp(C + 7, "RENAMED", 7);
    transport_req = 0;
    ck("  INFO and READ work while it plays (they do not touch the flash)", ok);
    {   uint32_t off2, rc2 = 0;
        for (off2 = 112u; !rc2 && off2 < la; off2 += 112u)                 /* (the pieces in order; the record is up to 534 bytes: five pieces) */
            rc2 = pr_data(20, off2, A + off2, la - off2 > 112u ? 112u : la - off2);
        ck("  the transfer goes on after it stops (pieces in order, commit; the commit with the missing parts was refused, rc 3, not 2)",
           !rc2 && pr_end(20, 4) == 0u && same_but_pos(20, A, la));
    }

    /* malformed and stale transfers */
    power_cycle();
    memcpy(snap, nor, sizeof nor);
    e0 = erases;
    {
        uint8_t b1[9] = {2, 0, 0, 3}, b2[8] = {2, 0, 0, 3}, g1[5] = {1, 200, 0, 0, 8}, g2[5] = {0, 128};
        ok = pr_begin(128, la, 0) == 1u && pr_begin(0, 19, 0) == 1u && pr_begin(0, PT_WORST + 1u, 0) == 1u && pr_begin(0, 0, 0) == 1u;
        put32(b1 + 4, 0); b1[8] = 16;
        ok &= wr(b1, 9) == 1u;                                      /* a CRC byte past 15 */
        ok &= wr(b2, 8) == 1u;                                      /* a short begin */
        call(g1, 5); ok &= rep[2] == 1u;                            /* READ of slot 200 */
        call(g2, 2); ok &= rep[2] == 1u;                            /* INFO of slot 128 */
        ok &= pr_data(0, 0, A, 112) == 5u;                          /* data with no begin: stale */
        {
            uint8_t op9[2] = {9, 0}, s1[1] = {4};
            ok &= wr(op9, 2) == 1u;                                 /* an op that is none */
            call(s1, 1); ok &= rep[2] == 1u;                        /* too short: no slot byte */
        }
    }
    ck("bad arguments are refused (rc 1): slot 128, a length under 20 or over the biggest record, a CRC byte over 15, a short begin, an unknown op, READ / INFO of a bad slot; data with no begin is stale (5)", ok && erases == e0);
    ok = pr_begin(0, la, st_crc32(A, la)) == 0u;
    ok &= pr_data(0, 112, A + 112, 112) == 1u;                      /* out of order */
    ok &= pr_data(0, 0, A, 112) == 0u && pr_data(0, 0, A, 112) == 1u;   /* a piece twice */
    ok &= pr_end(0, 4) == 2u;                                       /* commit with a missing part */
    ck("a piece out of order or twice: rc 1; a commit before all bytes arrived: rc 2, nothing written", ok && !up_used(0) && erases == e0);
    ok = pr_begin(0, la, st_crc32(B, la)) == 0u;
    for (i = 0; i < la; i += 112u)
        ok &= pr_data(0, i, A + i, la - i > 112u ? 112u : la - i) == 0u;
    ok &= pr_end(0, 4) == 2u;
    ck("a wrong CRC at commit: rc 2, nothing written", ok && !up_used(0) && erases == e0 && !memcmp(snap, nor, sizeof nor));
    {
        uint8_t big[128] = {0};
        ok = pr_begin(0, 200, 0) == 0u && pr_data(0, 0, A, 112) == 0u && pr_data(0, 112, A + 112, 112) == 1u;   /* more data than announced */
        (void)big;
        ck("  more data than the begin announced: rc 1", ok);
    }
    ok = pr_begin(0, la, st_crc32(A, la)) == 0u && pr_data(0, 0, A, 112) == 0u && pr_end(0, 5) == 0u && pr_data(0, 112, A + 112, 112) == 5u;
    ck("abort (5) ends the transfer: the next piece is stale (5)", ok);
    ok = pr_begin(0, la, st_crc32(A, la)) == 0u;
    usb.resets++;
    ok &= pr_data(0, 0, A, 112) == 5u;
    ok &= pr_begin(0, la, st_crc32(A, la)) == 0u;
    fm1_ms += 15001u;
    ok &= pr_data(0, 0, A, 112) == 5u;
    ck("a USB bus reset or 15 s with no request make the transfer stale (5)", ok);
    {   /* a backup transfer (or a wavetable) that begins after ours takes the staging RAM */
        uint8_t bb[13] = {0, ED_BK_PATCH};
        ok = pr_begin(0, la, st_crc32(A, la)) == 0u && pr_data(0, 0, A, 112) == 0u;
        put32(bb + 2, 20); put32(bb + 7, st_crc32(A, 20)); bb[12] = 5;
        rep_n = 0;
        ed_backup_handle(ED_BACKUP_PUT, bb, 13);
        ok &= rep[2] == 0u;
        ok &= pr_data(0, 112, A + 112, 112) == 5u;
        {
            uint8_t ab[2] = {3, ED_BK_PATCH};
            ed_backup_handle(ED_BACKUP_PUT, ab, 2);
        }
        ok &= pr_begin(0, la, st_crc32(A, la)) == 0u;
        {   /* ours first, then a backup data piece: the backup's transfer is over */
            uint8_t bd[16] = {1, ED_BK_PATCH, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            rep_n = 0;
            ed_backup_handle(ED_BACKUP_PUT, bd, 11);
            ok &= rep[2] == 5u;
        }
        ck("a backup transfer that begins after ours makes ours stale (5), and ours ends a backup transfer (its next piece is stale)", ok && !up_used(0));
    }

    /* records the store would not read: rc 1 at commit, nothing written */
    {
        struct { const char *what; uint32_t at, v; int op; } bads[] = {
            {"the old form (no PTF_CB)", 4, 0, 2}, {"an engine that does not exist", 5, 99, 0}, {"an empty name", 7, 0, 0},
            {"a control character in the name", 8, 7, 0}, {"a damaged End B (AMOUNT past 101)", 0, 0, 3}, {"text after the end of the name", 0, 0, 4},
            {"a length field that is not the record's", 0, 0, 5},
        };
        uint32_t t, pr, a2, b2, c2, d2;
        ok = 1;
        memcpy(snap, nor, sizeof nor);
        e0 = erases;
        for (t = 0; t < sizeof bads / sizeof bads[0]; t++) {
            uint32_t rc;
            memcpy(C, A, la);
            lc = la;
            if (bads[t].op == 2) C[4] &= (uint8_t)~PTF_CB;
            else if (bads[t].op == 0) C[bads[t].at] = (uint8_t)bads[t].v;
            else if (bads[t].op == 3) {
                pt_layout(C, &a2, &pr, &b2, &c2, &d2);                     /* (pr: where End B starts) */
                C[pr + 1u] = 102u;
            } else if (bads[t].op == 4) { memset(C + 7, 0, 12); memcpy(C + 7, "AB", 2); C[12] = 'X'; }
            else if (bads[t].op == 5) { C[0] = (uint8_t)(la - 1u); }
            rc = pr_all(33, C, lc);
            if (rc != 1u) { printf("patchraw:   (%s: rc %u)\n", bads[t].what, rc); ok = 0; }
        }
        ck("a record the store would not read is refused at the commit (rc 1): old form (no unknown flag bit is left: 0.2.19 uses bit 2 as PTF_TR), engine 99, empty name, control character, a damaged End B, text after the name, wrong length field", ok);
        ck("  ..and nothing was written (no erase, the flash is as it was, the slot still holds its factory sound)", erases == e0 && !memcmp(snap, nor, sizeof nor) && !up_used(33));
    }

    /* 5: a flash failure, then a power cut at every step of a write */
    power_cycle();
    memcpy(B, A, la); memset(B + 7, 0, 12); memcpy(B + 7, "NEWER", 5);
    ok = pr_read(70, C, &lc) == 1 && lc == la;
    dead = 1;
    ck("a failing flash (erase refused): the commit answers rc 4", pr_all(70, B, la) == 4u);
    dead = 0;
    power_cycle();
    ck("  the slot is as it was (also after a power-on): the old record whole", ok && pr_read(70, C, &lc) == 1 && same_but_pos(70, C, lc) && !memcmp(up_rec(70)->name, "RENAMED", 8));
    {
        uint32_t cb, steps = 0, newer = 0, broken = 0, total, oc;
        uint8_t old[PT_WORST + 8], other[8][PT_WORST + 8];
        uint32_t olen[8], oslot[8], n_other = 0;
        pr_read(70, old, &lc);
        oc = pt_dir[70].ch;
        for (k = 0; k < UP_SLOTS && n_other < 8u; k++)           /* the records that share its chunk */
            if (k != 70u && pt_dir[k].ch == oc) { oslot[n_other] = k; pr_read(k, other[n_other], &olen[n_other]); n_other++; }
        cut_bytes = 1000000; progd = 0;
        pr_all(70, B, la);                                       /* an uncut run to count the programmed bytes */
        total = progd;
        pr_all(70, old, lc);
        cut_bytes = -1;
        for (cb = 0; cb <= total + 2u; cb += (cb + 40u < total ? 7u : 1u)) {
            uint8_t g[PT_WORST + 8];
            uint32_t gl = 0, j;
            cut_bytes = (long)cb;
            (void)pr_all(70, B, la);
            power_cycle();
            steps++;
            ok = pr_read(70, g, &gl) == 1 && gl == la && (!memcmp(g, old, la) || !memcmp(g, B, la));
            newer += ok && !memcmp(g, B, la);
            for (j = 0; ok && j < n_other; j++) {
                uint8_t o[PT_WORST + 8];
                uint32_t ol = 0;
                ok = pr_read(oslot[j], o, &ol) == 1 && ol == olen[j] && !memcmp(o, other[j], ol);
            }
            if (!ok) broken++;
            pr_all(70, old, lc);                                 /* back to the old record for the next cut */
            power_cycle();
        }
        printf("patchraw: %u cut points tried in a %u byte record's chunk write (%u ended with the new record)\n", steps, la, newer);
        ck("a power cut at every step of a write (payload pages, commit record, readback): the old record or the new one whole, the other records of the chunk untouched",
           steps > 60u && !broken && newer > 0u && newer < steps);
        cut_bytes = -1;
    }

    /* 6: a bank: the worst record into every slot the store takes (128; TIDE 126), read back, rewritten */
    {
        static uint8_t W[UP_SLOTS][PT_WORST + 8], G[PT_WORST + 8];
        uint32_t lw[UP_SLOTS], rc = 0, good = 0, gl, xb0, mb0, pr0, pat0, fm0;
        pt_layout(A, &xb0, &mb0, &pr0, &pat0, &fm0);              /* (where End B is in this record: after the values, PT_VER 4) */
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        for (k = 0; k < UP_SLOTS; k++) {
            memcpy(W[k], A, la);
            lw[k] = la;
            memset(W[k] + 7, 0, 12);
            memcpy(W[k] + 7, "BANK ", 5);
            W[k][12] = (char)('0' + k / 100u);
            W[k][13] = (char)('0' + k / 10u % 10u);
            W[k][14] = (char)('0' + k % 10u);
            W[k][mb0 + 1u] = (uint8_t)(1u + k % 100u);           /* End B's AMOUNT (+ 1, at most 101): the records differ a little */
        }
        for (k = 0; k < NSAVE && !rc; k++)
            rc = pr_all(k, W[k], lw[k]);
        ck("the worst record into every slot the store takes through PATCH_RAW: every write rc 0, whatever the other slots hold", !rc);
        if (NSAVE < UP_SLOTS)
            ck("  TIDE: one more NEW record is refused (the store is full), nothing lost", pr_all(NSAVE, W[NSAVE], lw[NSAVE]) != 0u && !up_used(NSAVE) && up_count() == NSAVE);
        for (k = 0; k < NSAVE; k++)
            good += pr_read(k, G, &gl) == 1 && gl == lw[k] && same_but_pos(k, W[k], lw[k]) && !memcmp(G + 7, W[k] + 7, 12);
        ck("  each slot reads back its own record, the slot bytes the unit's", good == NSAVE);
        power_cycle();
        for (k = 0, good = 0; k < NSAVE; k++)
            good += pr_read(k, G, &gl) == 1 && gl == lw[k] && !memcmp(G + 4, W[k] + 4, lw[k] - 4u);
        ck("  and after a power cycle", good == NSAVE);
        for (k = 0, rc = 0; k < NSAVE && !rc; k++) {
            W[k][7] = 'Z';
            rc = pr_all(k, W[k], lw[k]);
        }
        for (k = 0, good = 0; k < NSAVE; k++)
            good += pr_read(k, G, &gl) == 1 && G[7] == 'Z' && flash_rec(k)[3] >= 1u;
        ck("  a rewrite of all of them (a newer revision each): rc 0, all read back", !rc && good == NSAVE);
        ck("  every slot is saved (UP_LIST's SAVED), RESTORE of one brings its factory sound back and READ then says so", up_used(5) && sl_restore(5) == 0 && pr_read(5, G, &gl) == 0 && pr_read(6, G, &gl) == 1);
    }

    /* 7: End B travels whole (1.1, PT_VER 4: the MBV block with its marks of the values set by hand; the 1.0 extension block is gone): bit for bit
     * through READ and a write into another slot; a damaged End B is refused at COMMIT (rc 1) and the slot is as it was; the record at its longest
     * (PT_WORST) is taken, one byte more is refused at BEGIN */
    {
        static uint8_t R[PT_WORST + 8], X[PT_WORST + 8], keep[MBV_MAX];
        static const xb_edit_t ed[3] = {{P_REL, 90}, {P_ATK, 12}, {XWID(X_SUBLVL), 100}};
        uint32_t lr, lx, xbo, mb, pr, pat, fm, same_n, nkeep, ebn;
        const uint8_t *eb;
        memset(nor, 0xFF, sizeof nor);
        power_cycle();
        set_engine_of(&trk[0], 0u);
        apply_preset_to(&trk[0], 2u);
        for (i = 0; i < NX; i++)
            XV(&trk[0], i) = (int16_t)xb_def(i) - 64;
        XV(&trk[0], X_SUBLVL) = 60;
        ok = store_with_b(&trk[0], 20u, "ENDB", ed, 3) == 0 && eb_n;
        memcpy(keep, eb_blk, eb_n);
        nkeep = eb_n;
        ok &= pr_read(20, R, &lr) == 1 && (R[4] & PTF_MB);
        ok &= pr_all(21, R, lr) == 0 && pr_read(21, X, &lx) == 1 && lx == lr && !memcmp(X + 4, R + 4, lr - 4u);
        ck("a record with End B and values set by hand: PATCH_RAW takes it (rc 0) and gives it back bit for bit", ok);
        ok = pr_all(22, X, lx) == 0 && pr_read(22, R, &lr) == 1 && lr == lx && !memcmp(R + 4, X + 4, lx - 4u);
        eb = rec_endb(22u, &ebn);
        ok &= xfit_eq(0, X_SUBLVL, up_xval(up_rec(22), X_SUBLVL), 60) && eb && ebn == nkeep && !memcmp(eb, keep, nkeep) && (eb[0] & MBV_PINS);
        ck("  and again into another slot (a librarian trip): the same bytes; the slot it knows (SUB LEVEL 60) and End B with its marks", ok);
        /* damaged End B: BEGIN and DATA are fine (the length and CRC are right), COMMIT answers rc 1 and the slot keeps what it had */
        pt_layout(X, &xbo, &mb, &pr, &pat, &fm);
        same_n = 0;
        for (i = 0; i < 4u; i++) {
            uint32_t n2 = lx;
            memcpy(R, X, lx);
            switch (i) {
            case 0: R[mb + 1u] = 102u; break;                         /* AMOUNT past 101 */
            case 1: R[mb] |= 128u; break;                             /* an unknown flag */
            case 2: R[mb] |= MBV_FMB; break;                          /* FM6's bytes on another engine */
            default: n2 = lx - 1u; R[0] = (uint8_t)n2; R[1] = (uint8_t)(n2 >> 8); break;   /* cut one byte short */
            }
            {
                uint32_t rc = pr_begin(23, n2, st_crc32(R, n2)), off;
                for (off = 0; !rc && off < n2; off += 112u)
                    rc = pr_data(23, off, R + off, n2 - off > 112u ? 112u : n2 - off);
                rc = rc ? rc : pr_end(23, 4);
                same_n += rc == 1u && !up_used(23);
                if (rc == 0u)
                    up_put(23, 0);
            }
        }
        ck("a damaged End B (AMOUNT past 101, an unknown flag, FM6's flag on another engine, a record cut short) is refused at COMMIT with rc 1; the slot is not written", same_n == 4u);
        ok = pr_begin(23, PT_WORST + 1u, 0) == 1u && pr_begin(23, PT_FIX - 1u, 0) == 1u && pr_begin(23, PT_WORST, 0) == 0u;
        pr_end(23, 5);
        ck("the length limit follows PT_WORST (577; TIDE 580): BEGIN takes PT_WORST, refuses one byte more and a record shorter than the fixed part",
           ok && PT_WORST == (DUELL_TIDE ? 580u : 577u));
        /* the biggest record the store can write (every slot and value off default, End B at its biggest, FM6, SEQ arp) through PATCH_RAW into every slot was section 6 above */
    }

    printf("patchraw: %s\n", bad ? "FAILED" : "all passed");
    return bad ? 1 : 0;
}
