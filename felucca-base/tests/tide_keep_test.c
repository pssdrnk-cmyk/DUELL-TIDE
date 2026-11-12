/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE: KEEP and the FM6 destination saved with End B (docs/design/MIGRATION_1_1_PLAN.md §4.9.1, §4.10; tide.h H9, H14).
 *   fmt      MBV_FMT: an FM6 part whose End B is a MORPH TO sound with another algorithm round-trips through a patch, a multi and the
 *            project (the autosave's path): the values, the effective FM6 patch and the engine's patch at A, the
 *            middle and B (the engine's patch: see hear); a stale check sum or a foreign target falls back to the blended bytes; blocks without a target stay 1.1's
 *   keep     KEEP (SEQ TOOLS, G4) at A, the middle and B: the sound as it played becomes the Original (values; the engine's FM6
 *            patch, the AMP ENV offsets baked into the carriers and the knobs on its numbers), one undo takes back sound and pattern; the pattern half as the SEQ fader played
 *   store    TIDE's settings record (tide_store.c): defaults, a damaged / short / other-version record, the 2 s debounce, never
 *            while playing or erasing, ERASE DATA wipes it
 *   limit    126 distinct saved patches, the 127th refused with nothing written; 128 browse slots */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int nor_fail, nor_ops;                    /* (a failing flash: every erase / program refused, counted) */
static int st_erase(uint32_t off) { nor_ops++; if (nor_fail) return -1; memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    nor_ops++;
    if (nor_fail || (off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidekeep: FAIL %s\n", s); bad++; } } while (0)

static void ablocks(uint32_t n) { static int32_t o[2 * CTL]; while (n--) mix_block(o, CTL); }
static void fresh(void)
{
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    flash_ok = 1;
    pt_boot();
    frame();
    song.sel = 0;
}
/* the FM6 factory sounds' algorithms: a and b of different ones (b's ratios differ too), -1 none */
static int alg_of(uint32_t k)
{
    static uint8_t fv[FP_SIZE + 1u];
    const ds_sound_t *s = ds_get(ENGI_FM6, k);
    return s && !fm6_slot_patch((uint32_t)clamp(ds_value(s, P_E7, 0), 0, 255), fv) ? fv[FP_ALG] & 31 : -1;
}
static int sound_a = -1, sound_b = -1;
static void pick(void)
{
    uint32_t a, b;
    for (a = 0; a < ds_count(ENGI_FM6) && sound_a < 0; a++)
        for (b = 0; b < ds_count(ENGI_FM6); b++)
            if (b != a && alg_of(a) >= 0 && alg_of(b) >= 0 && alg_of(a) != alg_of(b)) {
                sound_a = (int)a, sound_b = (int)b;
                break;
            }
}
/* part 0 plays FM6 sound a, End B MORPH TO sound b */
static void setup(void)
{
    set_engine_of(&trk[0], ENGI_FM6);
    trk[0].engine = ENGI_FM6;
    apply_preset_to(&trk[0], (uint32_t)sound_a);
    frames(64);
    ablocks(16);
    morph_ready(0);
    morph_to_sound(0, 1u, (uint32_t)sound_b);
    mph[0].sm_r = 32768;
}
static void fader(uint32_t v)
{
    morph_do(MQ_FADER, 1u, v);
    mph[0].sm_r = 32768;
    frame(); /* S2 plans changed destinations in the main loop before rendering. */
    ablocks(4);
}
/* what part 0 sounds like at the fader: its values, the effective FM6 patch, the engine's patch (fm6_eff: the blend, the target, the
 * AMP ENV knobs and macros on it: what every FM6 voice renders from). A rendered note is not compared: two renders of one state differ
 * here (the part's filter / LFO state runs on between notes), so a sample hash would test the harness, not the sound */
typedef struct { int16_t v[EXT_P_LEN]; uint8_t fp[FP_SIZE + 1u], eng[FP_SIZE]; } heard_t;
static void hear(heard_t *h)
{
    memset(h, 0, sizeof *h);
    morph_view_all(0, h->v);
    tide_fm6_effective(0, h->fp);
    ablocks(2);
    memcpy(h->eng, fm6_eff[0].p, FP_SIZE);
}
static int same(const heard_t *a, const heard_t *b)
{
    if (memcmp(a, b, sizeof *a)) {
        for (uint32_t i=0;i<EXT_P_LEN;i++) if(a->v[i]!=b->v[i]) { printf("tidekeep diff value %u: %d / %d\n",i,a->v[i],b->v[i]); break; }
        for (uint32_t i=0;i<FP_SIZE;i++) if(a->fp[i]!=b->fp[i]) { printf("tidekeep diff target byte %u: %u / %u\n",i,a->fp[i],b->fp[i]); break; }
        for (uint32_t i=0;i<FP_SIZE;i++) if(a->eng[i]!=b->eng[i]) { printf("tidekeep diff engine byte %u: %u / %u\n",i,a->eng[i],b->eng[i]); break; }
        printf("tidekeep context fader=%u side=%u tp=%u plan=%u sound=%u need=%u\n",mph[0].fader,mxs[0].side,mxs[0].tp,mxs[0].planned,mxs[0].sounding,mxs[0].need);
    }
    return !memcmp(a->v, b->v, sizeof a->v) && !memcmp(a->fp, b->fp, FP_SIZE) && !memcmp(a->eng, b->eng, FP_SIZE);
}
static const uint32_t AT[3] = {0u, 64u, 127u};

static void other_sound(void) { apply_preset_to(&trk[0], (uint32_t)(sound_a + 1) % ds_count(ENGI_FM6)); frames(32); ablocks(8); }
static int target_back(void) { return mph[0].tok[1][mph[0].top[1]] == (uint8_t)(sound_b + 1) && morph_fmto_k[0][1] == (uint8_t)(sound_b + 1); }

static void t_fmt(void)
{
    static heard_t want[3], got;
    static uint8_t blk[MBV_MAX], bad_blk[MBV_MAX];
    int16_t ref[EXT_P_LEN];
    uint32_t i, n, n0, ok;
    project_t pr;
    static project_store_t rec;

    fresh();
    setup();
    for (i = 0; i < 3u; i++) {
        fader(AT[i]);
        hear(&want[i]);
    }
    fader(127);
    hear(&got);
    CHECK("MORPH TO b: B sounds b's algorithm", (want[2].fp[FP_ALG] & 31) == alg_of((uint32_t)sound_b) && (want[0].fp[FP_ALG] & 31) == alg_of((uint32_t)sound_a));

    /* the block: the target at its end */
    memset(ref, 0, sizeof ref);
    for (i = 0; i < P_COUNT; i++)
        ref[i] = motion_base_value(&trk[0], i);
    for (i = 0; i < NX; i++)
        ref[XW_BASE + i] = (int16_t)clamp(XV(&trk[0], i), -64, 127);
    n = morph_bv_make(0, ref, blk);
    CHECK("MBV_FMT: written for a MORPH TO end, 3 bytes at the end", n > 3u && (blk[0] & MBV_FMT) && blk[n - 3u] == (uint8_t)(sound_b + 1) &&
          morph_bv_size(blk, MBV_MAX, ENGI_FM6) == n && morph_bv_size(blk, n, ENGI_FM6) == n);
    CHECK("MBV_FMT: exact length (one byte short is no block)", morph_bv_size(blk, n - 1u, ENGI_FM6) == 0u);
    CHECK("MBV_FMT: FM6 only", morph_bv_size(blk, MBV_MAX, ENGI_FM6 == 0u ? 1u : 0u) == 0u);
    memcpy(bad_blk, blk, sizeof blk);
    bad_blk[n - 3u] = 0;
    CHECK("MBV_FMT: target 0 is no block", morph_bv_size(bad_blk, MBV_MAX, ENGI_FM6) == 0u);
    memcpy(bad_blk, blk, sizeof blk);
    bad_blk[0] |= 16u;       /* (8 is MBV_W4, b9) */
    CHECK("MBV: an unknown flag is no block", morph_bv_size(bad_blk, MBV_MAX, ENGI_FM6) == 0u);
    CHECK("MBV_MAX, PT_WORST, PT_K re-checked: 198, 580, 6 a chunk; the project's End B as big", MBV_MAX == 198u && PT_WORST == 580u && PT_K == 6u &&
          PROJ_BV_MAX == MBV_MAX && PT_WORST * PT_K + PT_HDR <= PT_CAP);

    /* a patch */
    CHECK("patch: saved", up_patch_save_part(0, 5) == 0);
    other_sound();
    CHECK("patch: loaded", up_load(5) == 0);
    frames(16);
    CHECK("patch: the target is back", target_back());
    for (i = 0, ok = 1; i < 3u; i++) {
        fader(AT[i]);
        hear(&got);
        ok &= same(&got, &want[i]);
    }
    CHECK("patch: A, the middle and B sound as saved (values, FM6 patch, the engine's patch)", ok);

    /* a multi */
    fader(127);
    CHECK("multi: saved", project_save(3) == 0);
    other_sound();
    project_load(MIDX(3));
    frames(16);
    CHECK("multi: the target is back", target_back());
    for (i = 0, ok = 1; i < 3u; i++) {
        fader(AT[i]);
        hear(&got);
        ok &= same(&got, &want[i]);
    }
    CHECK("multi: A, the middle and B sound as saved", ok);

    /* the project file (the autosave's record) */
    project_capture(&pr);
    CHECK("project: End B with the target", pr.bvn[0] == n && !memcmp(pr.bv[0], blk, n));
    CHECK("project: packed and read back", proj_pack(&rec, &pr) && proj_import(&proj_scratch, rec.raw, (int)PROJ_STORE_SIZE));
    other_sound();
    CHECK("project: restored", !project_restore_runtime(&proj_scratch));
    frames(16);
    CHECK("project: the target is back", target_back());
    for (i = 0, ok = 1; i < 3u; i++) {
        fader(AT[i]);
        hear(&got);
        ok &= same(&got, &want[i]);
    }
    CHECK("project: A, the middle and B sound as saved", ok);

    /* a check sum that no longer matches (a user voice bank changed under the SLOT): the blended bytes only, as 1.1 */
    {
        uint8_t fh[NFM];
        fresh();
        setup();
        memcpy(fh, mph[0].fh[1][mph[0].top[1]], NFM);
        memcpy(bad_blk, blk, sizeof blk);
        bad_blk[n - 2u] ^= 0x55u;
        apply_preset_to(&trk[0], (uint32_t)sound_a);
        frames(8);
        morph_ldv = bad_blk;
        morph_rebase_part(0);
        morph_ldv = 0;
        CHECK("stale check sum: End B loads without the target", mph[0].tok[1][mph[0].top[1]] == 0u);
        CHECK("  .. its blended FM6 bytes as saved", !memcmp(fh, mph[0].fh[1][mph[0].top[1]], NFM));
        fader(127);
        CHECK("  .. B keeps A's algorithm", (fm6_eff[0].p[FP_ALG] & 31) == alg_of((uint32_t)sound_a));
        memcpy(bad_blk, blk, sizeof blk);
        bad_blk[n - 3u] = (uint8_t)(sound_a + 1);     /* (another sound's index with b's check sum) */
        apply_preset_to(&trk[0], (uint32_t)sound_a);
        frames(8);
        morph_ldv = bad_blk;
        morph_rebase_part(0);
        morph_ldv = 0;
        CHECK("a target whose patch is not the one saved: refused", mph[0].tok[1][mph[0].top[1]] == 0u);
    }

    /* no target: the block is 1.1's, byte for byte the same length and flags */
    fresh();
    setup();
    morph_roll_cfg(0, 1u, 1234u, 60u, morph_cfg_of(0));
    n0 = morph_bv_make(0, ref, blk);
    CHECK("no MORPH TO end: no MBV_FMT, the block 1.1 reads", n0 && !(blk[0] & MBV_FMT) && morph_bv_size(blk, n0, ENGI_FM6) == n0 &&
          !(blk[0] & ~(MBV_PINS | MBV_FMB | MBV_W4)));
}

static void t_keep(void)
{
    static heard_t want, got, orig;
    static uint8_t fp0[FP_SIZE + 1u];
    static int16_t p0[EXT_P_LEN];
    uint32_t i;
    for (i = 0; i < 3u; i++) {
        fresh();
        setup();
        fader(0);
        hear(&orig);
        memcpy(fp0, fm6_patch[0], sizeof fp0);
        memcpy(p0, trk[0].p, sizeof p0);
        fader(AT[i]);
        hear(&want);
        tools_key(TL_KEEP);
        frames(16);
        if (!AT[i]) {
            CHECK("KEEP at A, End A the Original: nothing kept, said why", msg_is("NOTHING TO KEEP") && !memcmp(fp0, fm6_patch[0], FP_SIZE) &&
                  !memcmp(p0, trk[0].p, sizeof p0));
            continue;
        }
        CHECK(AT[i] == 64u ? "KEEP at the middle: SOUND KEPT, the fader at A" : "KEEP at B: SOUND KEPT, the fader at A",
              msg_is("SOUND KEPT") && mph[0].fader == 0u);
        hear(&got);
        CHECK("  .. the engine plays the patch that sounded (the carriers' envelopes with the AMP ENV offsets baked in)", !memcmp(got.eng, want.eng, FP_SIZE));
        CHECK("  .. the AMP ENV knobs on the kept patch's numbers", got.v[P_ATK] == fm6_env_value(&trk[0], 0) && got.v[P_DEC] == fm6_env_value(&trk[0], 1) &&
              got.v[P_SUS] == fm6_env_value(&trk[0], 2) && got.v[P_REL] == fm6_env_value(&trk[0], 3));
        got.v[P_E7] = want.v[P_E7];                      /* (SLOT: the factory patch it equals, or OWN) */
        got.v[P_ATK] = want.v[P_ATK], got.v[P_DEC] = want.v[P_DEC], got.v[P_SUS] = want.v[P_SUS], got.v[P_REL] = want.v[P_REL];
        CHECK("  .. every other value as it sounded", !memcmp(got.v, want.v, sizeof got.v));
        CHECK("  .. SLOT: OWN unless a factory patch equals it", trk[0].p[P_E7] == (int16_t)fm6_slot[0]);
        undo_swap();
        frames(16);
        CHECK("  .. SAVE held (undo): the Original sound back", !memcmp(fp0, fm6_patch[0], FP_SIZE) && !memcmp(p0, trk[0].p, P_COUNT * sizeof p0[0]));
    }
    /* the pattern half (from JAM's t_keep): the SEQ MORPH's variation becomes the pattern, one undo */
    {
        static step_t want_s[NSTEP], before[NSTEP];
        uint32_t len = 16;
        track_t *t;
        fresh();
        t = TSEL;
        for (i = 0; i < NSTEP; i++)
            t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0, 0};
        for (i = 0; i < len; i += 2u)
            t->step[i] = (step_t){{(uint8_t)(60 + i % 7u)}, 1, ST_NOTE, 0, 100, 0};
        t->p[P_SLEN] = (int16_t)len;
        memcpy(before, t->step, sizeof before);
        seqm_set_amount(0, 80);
        seqm_roll(0);
        seqm_set_fader(0, 127);
        for (i = 0; i < len; i++)
            want_s[i] = *seq_play_step(t, i, &want_s[i]);
        tools_key(TL_KEEP);
        CHECK("KEEP: the pattern is what played, the SEQ fader at A", msg_is("SEQ KEPT") && !memcmp(want_s, t->step, len * sizeof want_s[0]) &&
              seqm_fader_of(0) == 0u && memcmp(before, t->step, sizeof before));
        undo_swap();
        CHECK("  .. undo: the pattern as before", !memcmp(before, t->step, sizeof before));
    }
}

static void t_store(void)
{
    uint8_t b[8], w[7] = {1, 2, 3, 0, 1, 1, 2};
    fresh();
    memset(tide_tail, 0, TIDE_TAIL_N);
    memset(tide_tail_saved, 0, TIDE_TAIL_N);
    CHECK("store: a new unit has no record (the defaults)", !tide_store_load(b, 7));
    tide_store_mark(w, 7);
    CHECK("store: a change is in the record at once (a backup reads it)", tide_store_load(b, 7) && !memcmp(b, w, 7));
    fm1_ms += 1000u;
    tide_store_poll();
    CHECK("store: nothing saved before 2 s", tide_store_dirty && !persist_pending);
    song.playing = 1;
    fm1_ms += 1500u;
    tide_store_poll();
    CHECK("store: never while playing", tide_store_dirty && !persist_pending);
    song.playing = 0;
    tide_store_poll();
    CHECK("store: saved once quiet (settings_save: written at once, nothing sounds)", !tide_store_dirty && !persist_pending);
    {
        persist_t p;
        CHECK("store: .. and written into the settings record's tail", !persist_pending && st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p &&
              !memcmp(p.map_tail + TIDE_TAIL_AT, "TIDE", 4) && p.map_tail[TIDE_TAIL_AT + 4] == 7u && !memcmp(p.map_tail + TIDE_TAIL_AT + 5, w, 7) && !p.map_tail[0] && !p.map_tail[31]);
        p.map_tail[TIDE_TAIL_AT + 1] = 'X';              /* a damaged mark: no record */
        memset(tide_tail, 0xEE, TIDE_TAIL_N);
        settings_import(&p, sizeof p);
        CHECK("store: a damaged mark is not taken (the unit's own stays)", tide_tail[0] == 0xEEu);
        memset(tide_tail, 0, TIDE_TAIL_N);
        CHECK("store: no record: the defaults", !tide_store_load(b, 7));
        p.map_tail[TIDE_TAIL_AT + 1] = 'I';
        p.map_tail[TIDE_TAIL_AT + 4] = 5;                /* a short record */
        settings_import(&p, sizeof p);
        CHECK("store: a short record: the defaults", !tide_store_load(b, 7));
        p.map_tail[TIDE_TAIL_AT + 4] = 7;
        settings_import(&p, sizeof p);
        CHECK("store: a whole one: read", tide_store_load(b, 7) && !memcmp(b, w, 7));
    }
    /* a failing write is paced, not repeated at every poll (F04): one attempt, then a retry after the pacing interval (settings_poll) */
    w[0] = 2;
    tide_store_mark(w, 7);
    fm1_ms += 2500u;
    nor_fail = 1; nor_ops = 0;
    tide_store_poll();
    for (b[0] = 0; b[0] < 100; b[0]++)
        settings_poll();
    CHECK("store: a failed write is tried once for 100 polls", nor_ops == 1 && persist_pending == 2u);
    nor_ops = 0;
    fm1_ms += 2500u;
    settings_poll();
    CHECK("store: .. tried again after the pacing interval", nor_ops == 1 && persist_pending == 2u);
    nor_fail = 0;
    fm1_ms += 2500u;
    settings_poll();
    CHECK("store: .. a success clears it", !persist_pending && tide_store_load(b, 7) && b[0] == 2u);
    CHECK("store: ERASE DATA runs", erase_data_run() == 0);
    CHECK("store: .. and wipes it", !tide_store_load(b, 7));
    tide_store_mark(w, 7);
    fm1_ms += 3000u;
    tide_store_poll();
    CHECK("store: nothing queued while the erase's restart is due", tide_store_dirty && !persist_pending);
    erase_at = 0;
}

static void t_limit(void)
{
    uint32_t k, ok = 1, n = 0;
    fresh();
    for (k = 0; k < 126u; k++)
        ok &= up_store(k, 0) == 0;
    for (k = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    CHECK("limit: 126 distinct patches saved", ok && n == 126u);
    CHECK("limit: the 127th refused (PT_FULL)", up_store(126, 0) == PT_FULL && !up_used(126));
    for (k = 0, n = 0; k < UP_SLOTS; k++)
        n += (uint32_t)up_used(k);
    CHECK("  .. nothing else changed", n == 126u);
    ui.svs = 126; patch_save_do(0);
    CHECK("limit: panel save reports STORE FULL", msg_is("STORE FULL") && !up_used(126));
    up_ui_named(2, 126, "FULL", CAT_PAD);
    CHECK("limit: named save reports STORE FULL", msg_is("STORE FULL") && !up_used(126));
    CHECK("  .. an overwrite still fits", up_store(10, "AGAIN") == 0 && up_used(10));
    CHECK("limit: 128 slots to browse", UP_SLOTS == 128u);
}

static void t_keep_macros(void)
{
    uint8_t want[FP_SIZE];
    int32_t dt[6];
    for (uint32_t i=0; i<2; i++) {
        fresh(); setup(); fader(64);
        trk[0].p[P_E1]=2; trk[0].p[P_E2]=-9; trk[0].p[P_E3]=17;
        trk[0].p[P_E4]=i ? -24 : 24;
        trk[0].p[P_E5]=2; trk[0].p[P_E6]=27;
        trk[0].p[P_ATK]=48; trk[0].p[P_DEC]=73;
        ablocks(4);
        memcpy(want, fm6_eff[0].p, sizeof want);
        want[FP_ALG]=fm6_eff[0].alg; want[FP_FB]=fm6_eff[0].fb;
        memcpy(dt,fm6_eff[0].dt,sizeof dt);
        tools_key(TL_KEEP); frames(8); ablocks(4);
        uint8_t got[FP_SIZE]; memcpy(got,fm6_eff[0].p,sizeof got);
        got[FP_ALG]=fm6_eff[0].alg; got[FP_FB]=fm6_eff[0].fb;
        CHECK("KEEP preserves nonzero macros and AMP ENV", !memcmp(want,got,sizeof want));
        CHECK("KEEP preserves carrier detune", !memcmp(dt,fm6_eff[0].dt,sizeof dt));
    }
}

static void t_keep_live(void)
{
    uint8_t want[FP_SIZE], got[FP_SIZE];
    fresh(); setup(); fader(0);
    trk_note_on(&trk[0],60,100); ablocks(8);
    fader(127);
    for (uint32_t i=0;i<200 && !fm6_xt[0].on;i++) ablocks(1);
    CHECK("live KEEP fixture enters a bridge", fm6_xt[0].on && mxs[0].tp > XW0 && mxs[0].tp < XW1);
    /* Compare the underlying morph tone, before the temporary bridge/wash.
     * A saved patch captures tone parameters, not voice phases or an FX tail. */
    MORPH_LOCK();
    (void)morph_begin(&trk[0], 0);
    uint8_t on=fm6_xt[0].on; fm6_xt[0].on=0; fm6_eff[0].ok=0;
    fm6_sync(&trk[0]); memcpy(want,fm6_eff[0].p,sizeof want);
    want[FP_ALG]=fm6_eff[0].alg; want[FP_FB]=fm6_eff[0].fb;
    fm6_xt[0].on=on; fm6_eff[0].ok=0;
    morph_end(&trk[0]);
    MORPH_UNLOCK();
    tools_key(TL_KEEP);
    CHECK("KEEP resets crossing position", mxs[0].tp == 0 && mxs[0].side == 0 && !fm6_xt[0].on);
    uint32_t dirty=0;
    for(uint32_t i=0;i<FM6_POLY;i++) {
        fm6_note_t *n=(fm6_note_t*)ENG_MEM(0)+i;
        dirty |= n->xs | n->xr;
    }
    CHECK("KEEP returns all borrowed operators", !dirty && !fm6_xt[0].dirty);
    ablocks(4);
    memcpy(got,fm6_eff[0].p,sizeof got);
    got[FP_ALG]=fm6_eff[0].alg; got[FP_FB]=fm6_eff[0].fb;
    if (memcmp(want,got,sizeof want)) for(uint32_t i=0;i<FP_SIZE;i++) if(want[i]!=got[i]) {printf("live KEEP byte %u: %u / %u\n",i,want[i],got[i]);break;}
    CHECK("live KEEP preserves the underlying morph tone", !memcmp(want,got,sizeof want));
    trk_note_off(&trk[0],60);

    fresh(); setup(); fader(0); trk_note_on(&trk[0],60,100); ablocks(8); fader(127);
    for (uint32_t i=0;i<200 && !fm6_xt[0].on;i++) ablocks(1);
    apply_preset_to(&trk[0],(uint32_t)sound_a);
    frames(1);
    CHECK("a loaded sound starts with a fresh crossing", mxs[0].tp == 0 && mxs[0].side == 0 && !fm6_xt[0].on);
    CHECK("a loaded sound replans its own endpoints", !mxs[0].planned || mxs[0].esig == morph_xesig(0));
    ablocks(4); trk_note_off(&trk[0],60);
}

static void t_keep_held_b(void)
{
    int32_t base[6], phase[6];
    fresh(); setup();
    /* Deliberately different endpoint frequencies: the factory pair alone may
     * use identical ratios and conceal a retune back to A during KEEP. */
    for(uint32_t k=0;k<6;k++) {
        uint32_t id=k*FP_OP+FP_FC, a=fm6_patch[0][id];
        morph_fmto[0][1][id]=(uint8_t)(a==31u ? 30u : a+1u);
    }
    morph_build(&mph[0]); mph[0].fser++; mph[0].cvalid=0; morph_xplan(0);
    fader(0);
    trk_note_on(&trk[0],60,100); ablocks(8); fader(127); ablocks(1800);
    uint32_t slot=0;
    while(slot<FM6_POLY && !trk[0].v[slot].active) slot++;
    CHECK("held B fixture finishes its crossing", slot<FM6_POLY && !fm6_xt[0].on && mxs[0].side==1);
    if(slot==FM6_POLY) return;
    fm6_note_t *n=(fm6_note_t*)ENG_MEM(0)+slot;
    memcpy(base,n->base,sizeof base);
    for(uint32_t k=0;k<6;k++) phase[k]=n->op[k].phase;
    tools_key(TL_KEEP);
    CHECK("KEEP at B keeps the held voice alive", n->live && trk[0].v[slot].active);
    CHECK("KEEP at B preserves held-note frequencies", !memcmp(base,n->base,sizeof base));
    uint32_t same=1;
    for(uint32_t k=0;k<6;k++) same &= phase[k]==n->op[k].phase;
    CHECK("KEEP at B preserves held-note phases", same);
    trk_note_off(&trk[0],60);
}

/* 1.0.0-b8, SAVE after JOURNEY: the SAVE dialog's note (tide_ui.c tide_save_note, drawn by ui_draw.c draw_confirm) when the part's heard end is a factory sound MORPH TO or
 * JOURNEY put there; none at A, none after KEEP, and with BOTH when either part is. note_drawn: whether it paints anything on a blank card for ui.confirm_trk (2 = BOTH) */
static int note_drawn(uint32_t confirm_trk)
{
    ui.confirm_trk = confirm_trk;
    cv_begin(208, 30, T_BG);
    tide_save_note(208);
    for (uint32_t i = 0; i < 208u * 30u; i++)
        if (cv_px[i] != swap16(T_BG))
            return 1;
    return 0;
}

static void t_savenote(void)
{
    fresh(); setup(); fader(0);
    CHECK("SAVE note: not shown at A", !note_drawn(0));
    fader(127);
    CHECK("shown with the heard end moved (part 1)", note_drawn(0));
    CHECK("not for part 2, which is not moved", !note_drawn(1));
    CHECK("BOTH: shown when part 1 is moved", note_drawn(2));
    tools_key(TL_KEEP);
    CHECK("after KEEP: gone", !note_drawn(0));

    fresh(); setup(); fader(127); other_sound();
    CHECK("a part loaded with another sound: gone", !note_drawn(0));

    /* part 2 moved only */
    {
        fresh(); setup();
        set_engine_of(&trk[1], ENGI_FM6);
        trk[1].engine = ENGI_FM6;
        apply_preset_to(&trk[1], (uint32_t)sound_a);
        frames(64);
        ablocks(16);
        morph_ready(1);
        morph_to_sound(1, 1u, (uint32_t)sound_b);
        mph[1].sm_r = 32768;
        morph_do(MQ_FADER, 2u, 127u);
        frame();
        ablocks(4);
        CHECK("part 2 moved: shown for part 2", note_drawn(1));
        CHECK("not for part 1", !note_drawn(0));
        CHECK("BOTH: shown when only part 2 is moved", note_drawn(2));
    }
}

int main(void)
{
    pick();
    CHECK("two FM6 factory sounds with different algorithms", sound_a >= 0 && sound_b >= 0);
    if (sound_a < 0)
        return 1;
    t_fmt();
    t_keep();
    t_keep_macros();
    t_keep_live();
    t_keep_held_b();
    t_savenote();
    t_store();
    t_limit();
    printf("tidekeep: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
