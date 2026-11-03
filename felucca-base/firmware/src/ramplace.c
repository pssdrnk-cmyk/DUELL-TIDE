/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL_RAMPLACE=1 (two-core stage 1a, developer builds only; felucca.c includes this file only then). Part 2's voice code runs from two
 * unused RAM ranges instead of the flash: range X (0x01C00000.., the render side of seven engines, tools/ram_fns_x.py) and range Z
 * (after the pool, the rest of the voice path, tools/ram_fns_z.py), linked there by firmware/app_ram.ld, loaded from the flash image.
 * Still everything on core 0: this build proves that the code can live and run there with the same sound.
 *
 * The boot pack (TIDE size work, 3 November 2026): the flash image does not hold the four RAM load images (.ram_text, .data, .ramz,
 * .ramx) as they are. tools/build.py replaces them, from _boot_pack (= _rt_load, the first of them, right after .text), with ONE raw
 * LZMA stream of the four one after the other (lc 1, lp 1, pb 1; 73.6 KB -> 50.5 KB at 47106ca) behind a header: magic "BLZ1", the
 * unpacked length, the stream's length, then per section its RAM address, length and word hash (bp_hash). bp_unpack decodes the whole
 * stream into the pool (its probabilities, then the output: the pool is cleared after it), then copies each section to its address
 * and compares its hash. Plain loops, no call out of the flash (memset and memcpy are in range Z, not there yet).
 *
 * ramplace_copy: in main.c fm1_cstart right after the boot guard, before anything that could call X or Z code (tools/elfcheck.py follows
 * every call made before it). A start that dies in it counts as a failed one. All four sections: main.c copies neither .data nor
 * .ram_text in this build.
 * ramplace_boot: after the start-up copies, X and Z compared with their hashes (and the unpack's result for all four); a difference: all
 * unpacked and copied again and compared again (twice). Still different: the RAM does not hold the code, nothing could run it (every
 * caller jumps to the RAM address; there is no second copy in the flash to run instead), so the unit goes to the update mode at once (as
 * the boot guard does after two failed starts): the DUELL installer puts the normal build back from there.
 * ramplace_check: the same comparison while the unit runs (MENU > ABOUT, once a second): finds code that something overwrote after boot. */
extern uint32_t _rx_start[], _rx_end[], _rz_start[], _rz_end[], _pool_start[], _pool_end[];
extern const uint32_t _boot_pack[];

/* The variables in a section of their own: LLVM's GlobalMerge leaves globals with a section alone, so the other globals merge as in the
 * build without DUELL_RAMPLACE and every moved function is the same object code as there (tools/ram_place.py --compare checks it) */
#define RP_VAR __attribute__((section(".bss.ramplace")))
static uint8_t rp_boot_copies RP_VAR;           /* copies the start needed beyond the first (0: the first was right) */
static uint8_t rp_state RP_VAR;                 /* the last ramplace_check: 0 OK, bit 0 range X differs, bit 1 range Z differs */
static uint32_t rp_about_t RP_VAR;              /* ABOUT's last check (fm1_ms; ui_menu.c about_sig) */

/* the header (tools/build.py boot_pack writes it): the sections in this order */
#define BP_MAGIC 0x315A4C42u                    /* "BLZ1" */
#define BP_NSEC 4u
enum { BP_RT, BP_DATA, BP_RZ, BP_RX };
#define BP_HDR (3u + 3u * BP_NSEC)              /* words: magic, unpacked, packed; per section address, length, hash */
#define BP_SEC(i, f) (_boot_pack[3u + 3u * (i) + (f)])

static uint32_t bp_hash(const uint32_t *d, const uint32_t *e)   /* FNV-1a over words (build.py bp_hash) */
{
    uint32_t h = 0x811C9DC5u;
    while (d < e)
        h = (h ^ *d++) * 0x01000193u;
    return h;
}

/* The LZMA decoder (after Igor Pavlov's public-domain LzmaSpec.cpp): lc 1, lp 1, pb 1, the dictionary is the whole output */
#define BP_LC 1u
#define BP_LP 1u
#define BP_PB 1u
#define BP_NPS (1u << BP_PB)
typedef struct {
    uint16_t choice, choice2, low[BP_NPS][8], mid[BP_NPS][8], high[256];
} bp_len_t;
typedef struct {                                /* every member uint16_t: bp_lzma sets them all as one array */
    uint16_t lit[0x300u << (BP_LC + BP_LP)];
    uint16_t slot[4][64], dec[115], align[16];
    uint16_t is_match[12u << BP_PB], is_rep[12], rep_g0[12], rep_g1[12], rep_g2[12], rep0_long[12u << BP_PB];
    bp_len_t len, rep_len;
} bp_probs_t;
typedef struct {
    const uint8_t *in, *end;
    uint32_t range, code;
} bp_rc_t;

static uint32_t bp_bit(bp_rc_t *rc, uint16_t *p)
{
    uint32_t b = (rc->range >> 11) * *p, r;
    if (rc->code < b) {
        rc->range = b;
        *p = (uint16_t)(*p + ((2048u - *p) >> 5));
        r = 0;
    } else {
        rc->range -= b;
        rc->code -= b;
        *p = (uint16_t)(*p - (*p >> 5));
        r = 1;
    }
    if (rc->range < (1u << 24)) {
        rc->range <<= 8;
        rc->code = (rc->code << 8) | (rc->in < rc->end ? *rc->in++ : 0u);   /* (past the end: zeros; the hashes find a short stream) */
    }
    return r;
}

static uint32_t bp_tree(bp_rc_t *rc, uint16_t *p, uint32_t n)
{
    uint32_t m = 1, k;
    for (k = 0; k < n; k++)
        m = (m << 1) + bp_bit(rc, p + m);
    return m - (1u << n);
}

static uint32_t bp_rtree(bp_rc_t *rc, uint16_t *p, uint32_t n)
{
    uint32_t m = 1, s = 0, k, b;
    for (k = 0; k < n; k++) {
        b = bp_bit(rc, p + m);
        m = (m << 1) + b;
        s |= b << k;
    }
    return s;
}

static uint32_t bp_len(bp_rc_t *rc, bp_len_t *l, uint32_t ps)
{
    if (!bp_bit(rc, &l->choice))
        return bp_tree(rc, l->low[ps], 3);
    if (!bp_bit(rc, &l->choice2))
        return 8u + bp_tree(rc, l->mid[ps], 3);
    return 16u + bp_tree(rc, l->high, 8);
}

static uint32_t bp_dist(bp_rc_t *rc, bp_probs_t *pr, uint32_t len)
{
    uint32_t slot = bp_tree(rc, pr->slot[len < 3u ? len : 3u], 6), nd, d, k;
    if (slot < 4u)
        return slot;
    nd = (slot >> 1) - 1u;
    d = (2u | (slot & 1u)) << nd;
    if (slot < 14u)
        return d + bp_rtree(rc, pr->dec + d - slot, nd);
    for (k = 0; k < nd - 4u; k++) {             /* the direct bits */
        rc->range >>= 1;
        rc->code -= rc->range;
        d += (((rc->code >> 31) ^ 1u) << (nd - 1u - k));
        rc->code += rc->range & (0u - (rc->code >> 31));
        if (rc->range < (1u << 24)) {
            rc->range <<= 8;
            rc->code = (rc->code << 8) | (rc->in < rc->end ? *rc->in++ : 0u);
        }
    }
    return d + bp_rtree(rc, pr->align, 4);
}

/* out[0..size) from in[0..n): 0 when the stream decoded to exactly size bytes */
static uint32_t bp_lzma(uint8_t *out, uint32_t size, const uint8_t *in, uint32_t n, bp_probs_t *pr)
{
    bp_rc_t rc;
    uint16_t *p = (uint16_t *)pr;
    uint32_t pos = 0, state = 0, r0 = 0, r1 = 0, r2 = 0, r3 = 0, ps, len, k, sym, mb, b;
    for (k = 0; k < sizeof(*pr) / 2u; k++)
        p[k] = 1024;
    if (n < 5u || in[0])
        return 1;
    rc.in = in + 5;
    rc.end = in + n;
    rc.range = 0xFFFFFFFFu;
    rc.code = ((uint32_t)in[1] << 24) | ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 8) | in[4];
    while (pos < size) {
        ps = pos & (BP_NPS - 1u);
        if (!bp_bit(&rc, &pr->is_match[(state << BP_PB) + ps])) {            /* a literal */
            p = &pr->lit[0x300u * (((pos & ((1u << BP_LP) - 1u)) << BP_LC) + (pos ? out[pos - 1u] >> (8u - BP_LC) : 0u))];
            sym = 1;
            if (state >= 7u) {
                mb = out[pos - r0 - 1u];
                do {
                    b = (mb >> 7) & 1u;
                    mb <<= 1;
                    k = bp_bit(&rc, p + ((1u + b) << 8) + sym);
                    sym = (sym << 1) | k;
                } while (k == b && sym < 0x100u);
            }
            while (sym < 0x100u)
                sym = (sym << 1) | bp_bit(&rc, p + sym);
            out[pos++] = (uint8_t)sym;
            state = state < 4u ? 0u : state < 10u ? state - 3u : state - 6u;
            continue;
        }
        if (!pos)
            return 2;
        if (bp_bit(&rc, &pr->is_rep[state])) {
            if (!bp_bit(&rc, &pr->rep_g0[state])) {
                if (!bp_bit(&rc, &pr->rep0_long[(state << BP_PB) + ps])) {   /* the short rep: one byte */
                    state = state < 7u ? 9u : 11u;
                    out[pos] = out[pos - r0 - 1u];
                    pos++;
                    continue;
                }
            } else {
                if (!bp_bit(&rc, &pr->rep_g1[state]))
                    k = r1;
                else {
                    if (!bp_bit(&rc, &pr->rep_g2[state]))
                        k = r2;
                    else {
                        k = r3;
                        r3 = r2;
                    }
                    r2 = r1;
                }
                r1 = r0;
                r0 = k;
            }
            len = bp_len(&rc, &pr->rep_len, ps);
            state = state < 7u ? 8u : 11u;
        } else {
            r3 = r2;
            r2 = r1;
            r1 = r0;
            len = bp_len(&rc, &pr->len, ps);
            state = state < 7u ? 7u : 10u;
            r0 = bp_dist(&rc, pr, len);
            if (r0 == 0xFFFFFFFFu)                  /* the end marker */
                break;
        }
        if (r0 >= pos)
            return 3;
        for (len += 2u; len && pos < size; len--, pos++)
            out[pos] = out[pos - r0 - 1u];
    }
    return pos == size ? 0u : 4u;
}

/* the four sections unpacked to RAM; the result: a bit (1 << BP_RT..BP_RX) for each that is not right (all four when the stream is not) */
__attribute__((noinline)) static uint32_t bp_unpack(void)   /* (not inlined: tools/elfcheck.py finds it by its load of _boot_pack) */
{
    bp_probs_t *pr = (bp_probs_t *)_pool_start;
    uint8_t *out = (uint8_t *)_pool_start + ((sizeof(bp_probs_t) + 3u) & ~3u);
    uint32_t size = _boot_pack[1], i, off = 0, bad = 0, *s, *d, *e;
    if (_boot_pack[0] != BP_MAGIC || size > (uint32_t)((uint8_t *)_pool_end - out)
        || bp_lzma(out, size, (const uint8_t *)(_boot_pack + BP_HDR), _boot_pack[2], pr))
        return 0xFu;
    for (i = 0; i < BP_NSEC; i++) {
        s = (uint32_t *)(out + off);
        d = (uint32_t *)BP_SEC(i, 0);
        e = d + BP_SEC(i, 1) / 4u;
        off += BP_SEC(i, 1);
        if (bp_hash(s, s + BP_SEC(i, 1) / 4u) != BP_SEC(i, 2)) {
            bad |= 1u << i;
            continue;
        }
        while (d < e)
            *d++ = *s++;
    }
    return bad;
}

static uint32_t ramplace_copy(void)
{
    return bp_unpack();
}

static uint32_t ramplace_check(void)
{
    uint32_t bad = 0;
    if ((uint32_t *)BP_SEC(BP_RX, 0) != _rx_start || bp_hash(_rx_start, _rx_end) != BP_SEC(BP_RX, 2))
        bad |= 1u;
    if ((uint32_t *)BP_SEC(BP_RZ, 0) != _rz_start || bp_hash(_rz_start, _rz_end) != BP_SEC(BP_RZ, 2))
        bad |= 2u;
    return bad;
}

static void ramplace_boot(uint32_t unpacked)    /* (after the .bss clear: rp_boot_copies / rp_state are .bss; unpacked: ramplace_copy's
                                                 * result; a copy again here clears its pool work space after it) */
{
    uint32_t k, bad;
    uint32_t *d;
    for (k = 0; k < 3u; k++) {
        rp_state = (uint8_t)ramplace_check();
        bad = unpacked | rp_state;
        if (!bad)
            break;
        rp_boot_copies++;
        unpacked = bp_unpack();
        for (d = _pool_start; d < _pool_end; d++)
            *d = 0;
    }
    if (bad) {
        bootguard.pending = 0;                  /* (not counted as a failed start: the update mode is meant) */
        fm1_enter_uboot();
    }
}

static uint32_t ramplace_bytes(void)
{
    return (uint32_t)((uintptr_t)_rx_end - (uintptr_t)_rx_start) + (uint32_t)((uintptr_t)_rz_end - (uintptr_t)_rz_start);
}
