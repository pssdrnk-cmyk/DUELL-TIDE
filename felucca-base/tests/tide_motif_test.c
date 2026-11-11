/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE: the motif under automatic MUSIC WNDR (lane M: M01-M04, M08; owner decisions D1, D2, D4). Two scenes, a dub skank (chords on
 * the "and"s) over a bass, and house stabs (the "and"s plus two pushes) over an offbeat bass, each 128 bars at 120 BPM with STYLE DUB's
 * rows (LEAD, SYNC SOFT, ARC RISE, POLY GENTLE) and PEAK's (LEAD, PUSH, ARC DROP):
 *   anchors  every bar but a drop's keeps at least max(2, 3/5) of the Original's hits where they were (one identity floor across
 *            DENS-, phrase gaps, moves, LEAD and ARC thinning); an offbeat skank keeps all four of its "and"s
 *   drift    no hit is ever more than two 16ths from its stored step (D1)
 *   answer   CALL AND ANSWER: a part listens (anchors and beats) on its bars, never a pad or a bass, never both parts at once
 *   dynamics the hits play louder in ARC's peak than in its breakdown, and a LEAD follower's bars softer than its leading bars (D4) */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if ((off & 0xFFu) + n > 256u)
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
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidemotif: FAIL %s\n", s); bad++; } } while (0)

static void put_hit(track_t *t, uint32_t i, uint32_t n, const uint8_t *notes, uint32_t vel)
{
    uint32_t k;
    t->step[i].time = ST_NOTE;
    t->step[i].n = (uint8_t)n;
    for (k = 0; k < n; k++)
        t->step[i].note[k] = notes[k];
    t->step[i].vel = (uint8_t)vel;
}
static uint32_t popc(uint32_t x) { uint32_t n = 0; for (; x; x >>= 1) n += x & 1u; return n; }
/* a factory sound of part p's engine in category cat (-1: none) */
static int32_t sound_of(uint32_t p, uint32_t cat)
{
    uint32_t e = trk[p].eng_req, j;
    for (j = 0; j < ds_count(e); j++)
        if (ds_cat_of(e, j) == cat)
            return (int32_t)j;
    return -1;
}
static uint32_t orig[2], g_amode = AM_OFF;   /* (g_amode: the parts' ARP mode; AM_SEQ_ID: D1, the velocity arp_seq_notes plays) */
/* scene 0: dub skank + bass; 1: house stabs + offbeat bass; 2: a single-note lead line + the skank */
static void setup(uint32_t scene)
{
    static const uint8_t chord[3] = {57, 60, 64}, bass[1] = {45}, fifth[1] = {52}, lead[1] = {69};
    uint32_t p, i;
    ui_power_on();
    frame();
    gl.on = gl.armed = 0;
    transport_req = 0;
    song.playing = song.rec = 0;
    mods_off = 0;
    duell_mode = DM_STEREO;
    usb.config = 1;
    memset(&midi_clock, 0, sizeof midi_clock);
    song.g[G_CLOCK] = 0, song.g[G_BPM] = 240, song.g[G_SWING] = 50;
    midi_pch[0] = 0, midi_pch[1] = 1;
    memset(sqm, 0, sizeof sqm);
    memset(&rhythm, 0, sizeof rhythm);
    for (p = 0; p < 2; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        for (i = 0; i < NSTEP; i++)
            t->step[i].time = ST_REST;
        t->p[P_SLEN] = 16, t->p[P_SDIV] = 2, t->p[P_ARATE] = 2, t->p[P_AMODE] = (int16_t)g_amode, t->p[P_APROB] = 127;
        t->p[P_AOCT] = 1, t->p[P_AGATE] = 64, t->p[P_SQNT] = 1, t->p[P_MUTE] = 0, t->p[P_LEVEL] = 100;
        t->nheld = 0, t->seq_active = 1;
        sqm[p].amt_x = 0;
    }
    if (scene == 0) {
        for (i = 2; i < 16u; i += 4u)
            put_hit(&trk[0], i, 3, chord, 100);
        put_hit(&trk[1], 0, 1, bass, 110), put_hit(&trk[1], 7, 1, bass, 90), put_hit(&trk[1], 10, 1, fifth, 90), put_hit(&trk[1], 12, 1, bass, 100);
    } else if (scene == 1) {
        put_hit(&trk[0], 2, 3, chord, 100), put_hit(&trk[0], 6, 3, chord, 90), put_hit(&trk[0], 7, 3, chord, 80);
        put_hit(&trk[0], 10, 3, chord, 100), put_hit(&trk[0], 14, 3, chord, 90), put_hit(&trk[0], 15, 3, chord, 80);
        for (i = 2; i < 16u; i += 4u)
            put_hit(&trk[1], i, 1, i == 10u ? fifth : bass, 100);
    } else {
        static const uint32_t at[7] = {0, 3, 5, 6, 9, 11, 14};
        for (i = 0; i < 7u; i++)
            put_hit(&trk[0], at[i], 1, lead, 100);
        for (i = 2; i < 16u; i += 4u)
            put_hit(&trk[1], i, 3, chord, 100);
    }
    for (p = 0; p < 2; p++)
        for (orig[p] = 0, i = 0; i < 16u; i++)
            if (sqm_hit(&trk[p].step[i]))
                orig[p] |= 1u << i;
}
/* the velocity an ARP SEQ part's step i starts its notes at (seq.c arp_seq_notes, held C4; 0: a rest) */
static uint32_t arp_vel_at(track_t *t, uint32_t i)
{
    static const uint32_t list[1] = {60};
    uint32_t k = t->arp_idx, vel = 0;
    uint8_t out[4];
    t->arp_idx = i;
    if (!arp_seq_notes(t, list, 1, out, &vel))
        vel = 0;
    t->arp_idx = k;
    return vel;
}
typedef struct { uint32_t kept_min, drift, bars, thin_bars, vel[5], nvel[5], vlead, nlead, vfol, nfol; } stats_t;
/* n bars of automatic WNDR (both parts, 16-bar trips, WAIT 2) from PLAY with TURNS, SYNC, ARC as asked (cat >= 0: part p on a sound of
 * that category); part p's statistics */
static stats_t run(uint32_t scene, uint32_t bars, uint32_t turns, uint32_t sync, uint32_t arc, int32_t cat1, uint32_t p)
{
    stats_t st;
    uint32_t b, n = bars * 4u * (uint32_t)FS * 60u / 240u / CTL + 1u, last = 0xFFFFFFFFu;
    memset(&st, 0, sizeof st);
    st.kept_min = 99;
    setup(scene);
    if (cat1 >= 0) {
        int32_t j = sound_of(p, (uint32_t)cat1);
        if (j < 0) {
            printf("tidemotif: no sound of category %d on part %u's engine\n", cat1, p + 1);
            st.bars = 0;
            return st;
        }
        trk[p].preset = (uint8_t)j, trk[p].user = 0;   /* (the role is the Original's category; the voice itself does not matter here) */
    }
    sqm_vary_all = 0, sqm_turns = (uint8_t)turns, sqm_sync = (uint8_t)sync, sqm_arc = (uint8_t)arc, sqm_poly = 1;
    morph_musical = 1;
    morph.cfg = (uint8_t)(morph.cfg & ~MC_PIT);
    gl.target = GT_BOTH, gl.mode = GM_WANDER, gl.time = 4u, gl.wait = 2u;
    seq_start();
    song.playing = 1;
    glide_start(1u, 3u, 0u);
    for (b = 0; b < n; b++) {
        fm1_ms = 10u + (uint32_t)((uint64_t)(b + 1u) * CTL * 1000u / FS);
        events_block(CTL);
        if ((b % ((uint32_t)FS / CTL / 60u + 1u)) == 0)
            frame();
        if ((uint32_t)sqm_bar != last) {
            uint32_t bar = (uint32_t)sqm_bar + sqm_bar_turn, m = 0, i, fol, drop;
            int32_t d;
            last = (uint32_t)sqm_bar;
            st.bars++;
            drop = TARC_DROP_BAR(bar);
            fol = (uint32_t)sqm_listens(p, bar);                  /* (CALL AND ANSWER's listening bars: seq_morph.c) */
            for (i = 0; i < 16u; i++) {
                step_t buf;
                const step_t *s = seq_play_step(&trk[p], i, &buf);
                uint32_t v;
                if (s->time != ST_NOTE || !s->n)
                    continue;
                m |= 1u << i;
                v = s->flags & SF_ACCENT ? 127u : s->vel ? s->vel : 96u;
                if (g_amode == AM_SEQ_ID)
                    v = arp_vel_at(&trk[p], i);
                if (sqm_arc && tarc.run)
                    st.vel[tarc.phase % 5u] += v, st.nvel[tarc.phase % 5u]++;
                if (sqm_auto == 3u && !sqm_arc) {
                    if (fol) st.vfol += v, st.nfol++;
                    else st.vlead += v, st.nlead++;
                }
            }
            if (!drop && popc(m & orig[p]) < st.kept_min)
                st.kept_min = popc(m & orig[p]);
            if (fol && (sqm_thin[p][0] & orig[p] & ~m))
                st.thin_bars++;
            for (i = 0; i < 16u; i++)
                if ((d = rhythm_offset(p, i)) < 0 ? (uint32_t)-d > st.drift : (uint32_t)d > st.drift)
                    st.drift = (uint32_t)(d < 0 ? -d : d);
        }
    }
    song.playing = 0;
    return st;
}
static uint32_t floor_of(uint32_t hits) { uint32_t f = (hits * 3u + 4u) / 5u; return f < 2u ? 2u : f; }

static void t_motif(void)
{
    static const struct { uint32_t scene, turns, sync, arc; const char *name; } C[4] = {
        {0, 2, 1, 1, "dub, STYLE DUB"}, {0, 2, 2, 2, "dub, STYLE PEAK"}, {1, 2, 1, 1, "house, STYLE DUB"}, {1, 2, 2, 2, "house, STYLE PEAK"}};
    uint32_t c, p;
    char msg[160];
    for (c = 0; c < 4u; c++)
        for (p = 0; p < 2u; p++) {
            stats_t st = run(C[c].scene, 128, C[c].turns, C[c].sync, C[c].arc, -1, p);
            printf("tidemotif: %s part %u: %u bars, original hits kept per bar min %u of %u, drift max %u\n", C[c].name, p + 1, st.bars,
                   st.kept_min, popc(orig[p]), st.drift);
            snprintf(msg, sizeof msg, "%s part %u: every bar keeps max(2, 3/5) of the Original's hits in place (%u of %u)", C[c].name, p + 1, st.kept_min, popc(orig[p]));
            CHECK(msg, st.bars > 100u && st.kept_min >= floor_of(popc(orig[p])));
            snprintf(msg, sizeof msg, "%s part %u: no hit drifts more than two 16ths (%u)", C[c].name, p + 1, st.drift);
            CHECK(msg, st.drift <= 2u);
            if (orig[p] == 0x4444u) {
                snprintf(msg, sizeof msg, "%s part %u: the skank keeps all four of its offbeats (%u)", C[c].name, p + 1, st.kept_min);
                CHECK(msg, st.kept_min == 4u);
            }
        }
}
/* CALL AND ANSWER (b7, replaces LEAD): a part that is neither a bass nor a pad listens on its bars (anchors and beats, two voices, softer) */
static void t_lead(void)
{
    stats_t a, b, c;
    a = run(0, 64, 2, 1, 0, CAT_PAD, 0);                 /* the skank on a pad, no ARC */
    printf("tidemotif: ANSWER skank on a pad: %u listening bars thinned\n", a.thin_bars);
    CHECK("a pad never listens: its skank plays whole", a.bars > 50u && !a.thin_bars && !sqm_thin[0][0]);
    b = run(0, 64, 2, 1, 0, CAT_PLUCK, 0);               /* the skank on a pluck: chords, every hit an anchor */
    CHECK("a chord skank on a pluck listens with its anchors whole (two voices, no hit dropped)", b.bars > 50u && !sqm_thin[0][0]);
    c = run(2, 64, 2, 1, 0, CAT_LEAD, 0);                /* a 7-note lead line on a lead sound */
    printf("tidemotif: ANSWER lead line: %u listening bars thinned, vel speaking %u listening %u\n", c.thin_bars,
           c.nlead ? c.vlead / c.nlead : 0u, c.nfol ? c.vfol / c.nfol : 0u);
    CHECK("a lead line drops its optional hits on its listening bars", c.thin_bars > 2u);
    CHECK("a listening part plays softer than when it speaks (by more than 15 %)", c.nfol && c.nlead && (uint64_t)c.vfol * 100u * c.nlead < (uint64_t)c.vlead * 85u * c.nfol);
    {   /* who opens swaps (by the bar and the seed); the two parts never both listen in a bar, both play bar 4 of 4; against a bass only bar 3 */
        uint32_t sd, bar, o0 = 0, o1 = 0, both = 0, b4 = 0, one = 0;
        sqm_auto = 3, tarc_thin = 0, sqm_lead = 3;
        for (sd = 1; sd <= 8u; sd++) {
            morph.seed = (uint16_t)(sd * 4099u);
            for (bar = 0; bar < 128u; bar++) {
                uint32_t l0 = (uint32_t)sqm_listens(0, bar), l1 = (uint32_t)sqm_listens(1, bar);
                if (!(bar & 3u))
                    o0 += l0, o1 += l1;
                both += l0 && l1, b4 += (bar & 3u) == 3u && (l0 || l1), one += (bar & 3u) != 3u && !(l0 || l1);
            }
        }
        printf("tidemotif: ANSWER opener over 256 4-bar groups: part 1 %u, part 2 %u\n", o0, o1);
        CHECK("ANSWER swaps its opener, never both listen, both play bar 4, one listens on bars 1-3", o0 && o1 && !both && !b4 && !one);
        sqm_lead = 1;
        for (bar = 0, one = 0; bar < 64u; bar++)
            one += (uint32_t)sqm_listens(0, bar) != ((bar & 3u) == 2u) || sqm_listens(1, bar);
        CHECK("against a bass or a pad the other part listens on bar 3 of 4 alone", !one);
        tarc_thin = 1;
        CHECK("no listening while the arc thins", !sqm_listens(0, 2u) && !sqm_listens(1, 1u));
        tarc_thin = 0;
    }
    a = run(0, 64, 2, 1, 0, CAT_BASS, 1);                /* part 2's bass line on a bass */
    CHECK("a bass never listens", !a.thin_bars && !sqm_thin[1][0]);
}
static void t_dynamics(void)
{
    stats_t st = run(0, 192, 2, 1, 1, -1, 0);
    uint32_t pk = st.nvel[AP_PEAK] ? st.vel[AP_PEAK] / st.nvel[AP_PEAK] : 0u, br = st.nvel[AP_BREAK] ? st.vel[AP_BREAK] / st.nvel[AP_BREAK] : 0u;
    printf("tidemotif: ARC velocity: peak %u, breakdown %u\n", pk, br);
    CHECK("ARC: the peak plays louder than the breakdown (by more than 20 %)", pk && br && br * 120u < pk * 100u);
    CHECK("ARC: the shape never pushes a velocity past 127", pk <= 127u);
    g_amode = AM_SEQ_ID;                                 /* D1: the same on ARP SEQ parts (nearly every factory multi) */
    st = run(0, 192, 2, 1, 1, -1, 0);
    pk = st.nvel[AP_PEAK] ? st.vel[AP_PEAK] / st.nvel[AP_PEAK] : 0u, br = st.nvel[AP_BREAK] ? st.vel[AP_BREAK] / st.nvel[AP_BREAK] : 0u;
    printf("tidemotif: ARC velocity on ARP SEQ: peak %u, breakdown %u\n", pk, br);
    CHECK("ARC on ARP SEQ: the peak plays louder than the breakdown (by more than 20 %)", pk && br && br * 120u < pk * 100u);
    {   /* an accent at ARC OFF still starts at 127 */
        uint32_t v;
        setup(0);
        sqm_arc = 0, sqm_dyn = 0;
        trk[0].step[2].flags |= SF_ACCENT;
        song.playing = 1;
        v = arp_vel_at(&trk[0], 2);
        song.playing = 0;
        printf("tidemotif: ARP SEQ accent at ARC OFF: %u\n", v);
        CHECK("ARP SEQ: an accented step at ARC OFF starts at 127", v == 127u);
    }
    g_amode = AM_OFF;
}
int main(void)
{
    t_motif();
    t_lead();
    t_dynamics();
    printf("tidemotif: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
