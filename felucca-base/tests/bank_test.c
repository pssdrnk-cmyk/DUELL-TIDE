/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.0: THE FACTORY BANK and the ARP RECALL.
 * 1. The bank (DUELL 1.1: the firmware's own factory sounds, sounds.c from tools/duell_sounds.py with End B as exact values; the 1.0 records file
 *    tests/factory_bank_records.txt holds PT_VER 3 records the store no longer reads, and is not used). For every browse slot the store takes (128; TIDE 126) the
 *    factory sound is loaded through the firmware (sl_load, as PATCHES loads it), saved through the firmware (up_patch_save_part, as SAVE does: End B block, arp /
 *    keys block, the FM6 patch) and loaded back from its slot: the same sound (every value as the record keeps it, the extension slots, End B as it sounds, the
 *    FM6 patch); every record carries End B and the arp and keys block (arp OFF written explicitly).
 * 2. The arp recall: a patch saved with the arp OFF turns the arp OFF when it is loaded after one with the arp ON; a patch with an arp brings it back (all eight values);
 *    a factory sound without an arp turns an arp off; the chord keys the same; a record without the block (written before 1.0) loads as arp OFF, keys at their
 *    defaults; the global MODULES switch still kills the arp.
 *   build/host/bank_test [records-file]          (DUELL/host_tests.sh "bank") */
#include <ctype.h>
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("bank: %-120s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}


/* the fields of a stored slot, the firmware's decode */
typedef struct {
    up_rec_t r;
    uint8_t pk[FM6_PACKED];
    int fm, has_ak;
    ak_t a;
} dec_t;
static void decode(uint32_t k, dec_t *d)
{
    d->r = *up_rec(k);
    d->fm = upf_get(k, d->pk) == 0;
    d->has_ak = ak_get(k, &d->a);
}
static void set_arp(track_t *t, int on)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        t->p[P_AMODE + i] = TP[P_AMODE + i].def;
    if (on) {
        t->p[P_AMODE] = 3;      /* UPDN */
        t->p[P_ARATE] = 5;
        t->p[P_AOCT] = 2;
        t->p[P_AGATE] = 77;
        t->p[P_ASWING] = 59;                          /* (1.1: SWING 50..75) */
        t->p[P_APROB] = 100;
        t->p[P_AHOLD] = 1;
        t->p[P_AORDER] = 1;
    }
}
static int arp_equal(const track_t *t, const int16_t *v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        if (t->p[P_AMODE + i] != v[i])
            return 0;
    return 1;
}

/* part 0 as it sounds now: every value as a record keeps it (PT_VER 4: inside its range), the slots, End B as it sounds, the FM6 patch, the arp */
typedef struct {
    uint32_t eng;
    int16_t v[P_COUNT], x[NX], b[EXT_P_LEN];
    uint8_t pk[FM6_PACKED];
} snd_t;
static void snd_take(snd_t *s)
{
    const track_t *t = &trk[0];
    uint32_t i;
    memset(s, 0, sizeof *s);
    s->eng = t->eng_req;
    for (i = 0; i < P_COUNT; i++)
        s->v[i] = (int16_t)wv_code(s->eng, i, motion_base_value(t, i));
    for (i = 0; i < NX; i++)
        s->x[i] = (int16_t)wv_code(s->eng, XWID(i), XV(t, i));
    (void)morph_ready(0);
    morph_effective_x(0, 1u, s->b);
    for (i = 0; i < EXT_P_LEN; i++)
        s->b[i] = mbv_wid(i) && !mbv_never(i) ? (int16_t)wv_code(s->eng, i, s->b[i]) : 0;
    if (s->eng == ENGI_FM6)
        fm6_pack(fm6_patch[0], s->pk);
}
/* 0: the same sound; else what differs */
static const char *snd_diff(const snd_t *a, const snd_t *b)
{
    uint32_t i;
    if (a->eng != b->eng)
        return "engine";
    for (i = 0; i < P_COUNT; i++)
        if (i != P_LEVEL && a->v[i] != b->v[i])
            return "a value";
    if (memcmp(a->x, b->x, sizeof a->x))
        return "an extension slot";
    if (memcmp(a->b, b->b, sizeof a->b))
        return "End B";
    if (memcmp(a->pk, b->pk, sizeof a->pk))
        return "FM6 patch";
    return 0;
}

/* slot k into part 0 as PATCHES loads it, the engine switch done (as the audio's next blocks finish it: the morph's sound is then the part's) */
static void load_settled(uint32_t k)
{
    sl_load(k);
    trk[0].engine = trk[0].eng_req;
}

int main(int argc, char **argv)
{
    uint32_t n_ok = 0, n_same = 0, n_blocks = 0, k, i, nsave = PT_MAX < UP_SLOTS ? PT_MAX : UP_SLOTS;
    static dec_t dg;
    static snd_t fac, back;
    (void)argc; (void)argv;
    ui_power_on();
    frame();
    song.playing = 0;
    /* 1.1: the factory bank is the firmware's own (sounds.c from tools/duell_sounds.py, End B as exact values); the 1.0 records file
     * (tests/factory_bank_records.txt, PT_VER 3) is no longer read by the store. Every factory sound: loaded, saved through SAVE's path, loaded
     * back from the saved slot: the same sound (every value as the record keeps it, the slots, End B as it sounds, the FM6 patch), and the
     * record carries End B and the arp / keys block (126 slots with TIDE: its store's limit) */
    for (k = 0; k < nsave; k++) {
        const char *why;
        load_settled((k + 61u) % UP_SLOTS);               /* another sound first: nothing of the one before may leak into this one */
        load_settled(k);
        snd_take(&fac);
        if (up_patch_save_part(0, k) != 0 || !up_used(k)) {
            printf("bank: #%u: the firmware did not store it\n", k + 1u);
            bad++;
            continue;
        }
        n_ok++;
        decode(k, &dg);
        n_blocks += (pt_raw(k)[4] & PTF_MB) && dg.has_ak && (dg.a.g & AK_ARP) && (dg.a.g & AK_KEYS);
        load_settled((k + 61u) % UP_SLOTS);
        load_settled(k);
        snd_take(&back);
        if ((why = snd_diff(&fac, &back)) != 0) {
            printf("bank: #%u: loaded from its saved slot it DIFFERS (%s)\n", k + 1u, why);
            bad++;
        } else
            n_same++;
    }
    printf("bank: %u factory sounds saved, %u the same sound loaded back, %u records with End B and the arp / keys block\n", n_ok, n_same, n_blocks);
    ck("every factory sound the store takes (128; TIDE 126) loads and stores through the firmware", n_ok == nsave);
    ck("  loaded back from its saved slot each is the same sound (values, slots, End B, FM6 patch); every record carries End B and the arp / keys block",
       n_same == nsave && n_blocks == nsave);
    for (k = 0; k < UP_SLOTS; k++)
        sl_restore(k);

    /* 2. the arp recall */
    {
        track_t *t = &trk[0];
        int16_t on[8], off[8];
        uint32_t a_on = 0, a_off = 0, s;
        uint8_t rec[PT_WORST + 8];
        uint32_t rn;
        for (i = 0; i < 8u; i++)
            off[i] = TP[P_AMODE + i].def;
        /* find a factory sound with no arp (the bank has several arp sounds: ARP BASS, ARP PLUCK ..) and one that has one */
        for (s = 0; s < 128u; s++) {
            uint32_t e, fk;
            if (!sl_fac(s, &e, &fk))
                continue;
            if (DS_SOUNDS[DS_ENG_AT[e] + fk].flags & DSF_ARP) {
                if (!a_on)
                    a_on = s + 1u;
            } else if (!a_off)
                a_off = s + 1u;
        }
        ck("the bank has a factory sound with an arp and one without", a_on && a_off);
        set_arp(t, 1);
        for (i = 0; i < 8u; i++)
            on[i] = t->p[P_AMODE + i];
        /* slot 10: stored with the arp OFF, slot 11: stored with the arp ON, both from the same sound */
        sl_load(30u);
        set_arp(t, 0);
        ck("save slot 10 with the arp OFF", up_patch_save_part(0, 10u) == 0);
        set_arp(t, 1);
        ck("save slot 11 with the arp ON", up_patch_save_part(0, 11u) == 0);
        ck("  both records carry the arp block (the OFF one too)", ak_get(10u, &dg.a) && (dg.a.g & AK_ARP) && ak_get(11u, &dg.a) && (dg.a.g & AK_ARP) && (dg.a.g & AK_KEYS));
        sl_load(11u);
        ck("load the arp ON patch: its arp is back, all eight values", arp_equal(t, on));
        sl_load(10u);
        ck("load the arp OFF patch after it: the arp is OFF (it used to stay ON)", arp_equal(t, off));
        sl_load(11u);
        sl_load(a_off - 1u);
        ck("a factory sound without an arp, loaded after an arp patch: the arp is OFF", arp_equal(t, off));
        sl_load(11u);
        set_arp(t, 0);
        t->p[P_ARATE] = 7;                                  /* the player changed one value of the lent arp, then: */
        sl_load(10u);
        ck("a load sets the patch's own arp even after the player changed it", arp_equal(t, off));
        sl_load(a_on - 1u);
        ck("a factory sound with an arp sets it", t->p[P_AMODE] != 0);
        sl_load(10u);
        ck("  and the next patch (arp OFF) takes it away again", arp_equal(t, off));
        /* the chord keys */
        sl_load(30u);
        set_arp(t, 0);
        t->p[P_CHRD] = 4;
        t->p[P_VOIC] = 2;
        ck("save slot 12 with chord keys", up_patch_save_part(0, 12u) == 0);
        t->p[P_CHRD] = TP[P_CHRD].def;
        t->p[P_VOIC] = TP[P_VOIC].def;
        ck("save slot 13 without", up_patch_save_part(0, 13u) == 0);
        sl_load(12u);
        ck("load the patch with chord keys: they are back", t->p[P_CHRD] == 4 && t->p[P_VOIC] == 2);
        sl_load(13u);
        ck("load the patch without: the keys are at their defaults (they stayed before)", t->p[P_CHRD] == TP[P_CHRD].def && t->p[P_VOIC] == TP[P_VOIC].def);
        /* a record written before 1.0: no block at all = arp OFF, keys default */
        rn = up_raw_get(11u, rec);
        {
            uint32_t xbo, mb, pr, pat, fm, ak;
            pt_layout_x(rec, &xbo, &mb, &pr, &pat, &fm, &ak);
            ck("the arp ON record has its block last", (rec[4] & PTF_AK) && ak + 2u + pt_rd16(rec + ak) == rn);
            /* the same record with the arp values among its pairs and no block: the pairs are ignored (param_kept), the block's absence means OFF */
            rec[4] &= (uint8_t)~PTF_AK;
            rn = ak;
            rec[0] = (uint8_t)rn;
            rec[1] = (uint8_t)(rn >> 8);
        }
        ck("a record without the block is taken by the store", up_put_raw(14u, rec, rn) == 0 || up_put_raw(14u, rec, rn) == 3);
        sl_load(11u);
        sl_load(14u);
        ck("a record without the arp block (written before 1.0): the arp is OFF, not the previous patch's", arp_equal(t, off));
        /* the MODULES switch still kills every arp */
        sl_load(11u);
        mods_off |= MODS_ARP;
        ck("GLO > MODULES ARP off: the patch's arp is stored but plays as OFF", arp_mode(t) == 0 && arp_equal(t, on));
        mods_off &= (uint8_t)~MODS_ARP;
        ck("  and on again", arp_mode(t) == 3);
    }
    printf("bank: %s\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
