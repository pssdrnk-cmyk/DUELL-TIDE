/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's pattern ends and sound pairs (audit E12): the bounds that decide how a generation sounds, as statistical properties over many
 * seeds, not one trace. firmware/src/seq_morph.c sqm_build (the dials DENS / SYNC / ACC / VOIC varying the Original) and
 * firmware/src/morph_complement.c sound_complement_pick (the second part's sound leaves the first one room):
 *   density   an end has at least one hit and never more than two hits more or fewer than the Original; a dial at 0 changes nothing; the same
 *             end built twice is the same (nothing accumulates); the anchors (downbeats, the motif's identity floor) stay hits
 *   still     (C4, roles spec) an Original of no hit or one hit builds to itself for every seed and every dial, and tarc_cap(0..2) is the count itself
 *             in every ARC phase, so a drone, a texture or a two-chord pad gains no onset; three hits and more still have room
 *   register  no dial moves a note to another pitch class; VOIC moves a note an octave at most, so every note stays within an octave of
 *             the Original's lowest and highest and inside 24..108
 *   lane      (C1, roles spec) whatever the dials, VOIC (both paths) and the PITCH moves do, every note of every end lies in the Original's lane: from its
 *             lowest hit note (but not above C3, 48: a pad, a skank, a lead never gets a note under C3) to its highest plus an octave; a bass scene at 36..43 never
 *             goes under 36, a scene at 60..70 never under 48; each lane scene must move notes, and reach the lane's edge, so the bound is not met by doing nothing
 *   room      sound_complement_pick: with the peer muted, WILD, or the musical rolls off, it keeps the first sound; it only ever returns one
 *             of the two candidates, the one with the higher contrast score against the other part (and across many pairs it
 *             picks both); under ARC's peak or breakdown it returns the brighter or the softer of the two */
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
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidebuild: FAIL %s\n", s); bad++; } } while (0)

enum { LEN = 16 };
static uint32_t rs = 12345u;
static uint32_t rnd(uint32_t n) { rs = rs * 1664525u + 1013904223u; return (rs >> 8) % n; }

/* a random 16-step Original: 4..12 attacks, notes of C major from C2 (a bass note now and then) to C6, a third of them triads */
static void make_pat(step_t *p)
{
    static const uint8_t MAJ[7] = { 0, 2, 4, 5, 7, 9, 11 };
    uint32_t i, hits = 4u + rnd(9u), k;
    memset(p, 0, LEN * sizeof *p);
    for (k = 0; k < hits; k++) {
        i = rnd(LEN);
        p[i].time = ST_NOTE;
        p[i].n = rnd(3u) ? 1u : 3u;
        p[i].vel = 100;
        p[i].note[0] = (uint8_t)(36u + 12u * rnd(5u) + MAJ[rnd(7u)]);
        p[i].note[1] = (uint8_t)(p[i].note[0] + 4u);
        p[i].note[2] = (uint8_t)(p[i].note[0] + 7u);
    }
}
static uint32_t count_hits(const step_t *p) { uint32_t i, n = 0; for (i = 0; i < LEN; i++) n += (uint32_t)sqm_hit(&p[i]); return n; }

static void t_build(void)
{
    uint32_t round, i, k;
    uint32_t density_bad = 0, floor_bad = 0, anchor_bad = 0, zero_bad = 0, again_bad = 0, class_bad = 0, range_bad = 0, builds = 0, differed = 0;
    morph.cfg &= ~(uint32_t)MC_PIT;
    for (round = 0; round < 3u; round++) {
        morph_musical = round == 2u ? 1 : 0;           /* round 2: the voice-leading path of VOIC */
        for (k = 0; k < 600u; k++) {
            step_t p[LEN], a[LEN], b[LEN], z[LEN];
            sqm_end_t x;
            uint32_t amt = 25u + 25u * rnd(4u), hp, ha, lo = 255, hi = 0, pcs = 0;
            make_pat(p);
            hp = count_hits(p);
            for (i = 0; i < LEN; i++) {
                uint32_t m;
                if (!sqm_hit(&p[i]))
                    continue;
                for (m = 0; m < p[i].n; m++) {
                    pcs |= 1u << (p[i].note[m] % 12u);
                    lo = p[i].note[m] < lo ? p[i].note[m] : lo;
                    hi = p[i].note[m] > hi ? p[i].note[m] : hi;
                }
            }
            sqm_rand(&x, 1u + rnd(65535u), amt);
            sqm_build(p, LEN, &x, amt, a);
            sqm_build(p, LEN, &x, amt, b);
            builds++;
            ha = count_hits(a);
            density_bad += ha + 2u < hp || ha > hp + 2u;
            floor_bad += ha < 1u;
            again_bad += memcmp(a, b, sizeof a) != 0;
            differed += memcmp(a, p, sizeof a) != 0;
            for (i = 0; i < LEN; i++) {
                uint32_t m;
                if (sqm_anchor(p, LEN, i) && sqm_hit(&p[i]))
                    anchor_bad += !sqm_hit(&a[i]);
                if (!sqm_hit(&a[i]))
                    continue;
                for (m = 0; m < a[i].n && m < 4u; m++) {
                    uint32_t nn = a[i].note[m];
                    class_bad += !((pcs >> (nn % 12u)) & 1u);
                    range_bad += nn + 12u < lo || nn > hi + 12u || nn < 24u || nn > 108u;
                }
            }
            memset(&x.d, 0, sizeof x.d);               /* every dial at 0: the Original, bit for bit */
            sqm_build(p, LEN, &x, amt, z);
            zero_bad += memcmp(z, p, sizeof z) != 0;
        }
    }
    printf("builds %u, ends that differ from the Original %u (density %u, floor %u, anchors %u, zero %u, again %u, class %u, range %u)\n", builds, differed,
           density_bad, floor_bad, anchor_bad, zero_bad, again_bad, class_bad, range_bad);
    CHECK("density: an end never has more than two hits more or fewer than the Original", !density_bad);
    CHECK("density: an end keeps at least one hit", !floor_bad);
    CHECK("density: the anchors stay hits", !anchor_bad);
    CHECK("density: every dial at 0 is the Original", !zero_bad);
    CHECK("density: the same end built twice is the same (computed from the Original every time)", !again_bad);
    CHECK("density: the dials change the pattern in most builds", differed > builds / 2u);
    CHECK("register: no dial moves a note to a pitch class the Original does not have", !class_bad);
    CHECK("register: every note stays within an octave of the Original's range and inside 24..108", !range_bad);
}

/* ---- lane (C1): the Original's register is the bound of every generated note */
/* kind 0: a skank, triads and dyads at 60..70 (lane 48..82); 1: a bass at 36..43 (lane 36..55); 2: a pad / lead from 50 to 62 (lane 48..74) */
static void make_lane_pat(step_t *p, uint32_t kind)
{
    static const uint8_t SK[7] = { 60, 62, 64, 65, 67, 69, 70 }, BS[5] = { 36, 38, 40, 41, 43 }, PD[8] = { 50, 52, 53, 55, 57, 59, 60, 62 };
    uint32_t i, k, hits = 5u + rnd(7u);
    memset(p, 0, LEN * sizeof *p);
    for (k = 0; k < hits; k++) {
        i = k ? rnd(LEN) : 0u;                        /* (the first hit is on step 0 and on the scene's lowest note, so the lane's floor is known) */
        p[i].time = ST_NOTE;
        p[i].vel = 100;
        if (kind == 0u) {
            p[i].n = 2u + rnd(2u), p[i].note[0] = SK[k ? rnd(7u) : 0u], p[i].note[1] = (uint8_t)(p[i].note[0] + 3u + rnd(2u)), p[i].note[2] = (uint8_t)(p[i].note[0] + 7u);
        } else if (kind == 1u) {
            p[i].n = rnd(3u) ? 1u : 2u, p[i].note[0] = BS[k ? rnd(5u) : 0u], p[i].note[1] = (uint8_t)(p[i].note[0] + 7u);
        } else {
            p[i].n = rnd(2u) ? 1u : 3u, p[i].note[0] = PD[k ? rnd(8u) : 0u], p[i].note[1] = (uint8_t)(p[i].note[0] + 4u), p[i].note[2] = (uint8_t)(p[i].note[0] + 7u);
        }
    }
}
static void t_lane(void)
{
    uint32_t kind, k, i, m, builds = 0, moved = 0, edge = 0, out_of_lane = 0, under_floor = 0, pit_runs = 0;
    static const int32_t floor_of[3] = { 48, 36, 48 };      /* (the scenes' lowest notes 60, 36, 50; a lane starting above C3 reaches down to 48) */
    ui_power_on();
    frame();
    scale_g = 1;
    root_g = 0;
    for (kind = 0; kind < 3u; kind++)
        for (k = 0; k < 1200u; k++) {
            step_t p[LEN], a[LEN];
            sqm_end_t x;
            uint32_t amt = 25u + 25u * rnd(4u), pit = k & 1u;
            int32_t top = 0;
            make_lane_pat(p, kind);
            for (i = 0; i < LEN; i++)
                for (m = 0; sqm_hit(&p[i]) && m < p[i].n; m++)
                    top = p[i].note[m] > top ? p[i].note[m] : top;
            top += 12;
            morph_musical = (int)((k >> 1) & 1u);
            if (pit) morph.cfg |= MC_PIT; else morph.cfg &= ~(uint32_t)MC_PIT;
            sqm_rand(&x, 1u + rnd(65535u), amt);
            for (i = 0; i < SD_N; i++)
                x.d[i] = (int8_t)(rnd(2u) ? (int)amt : -(int)amt);   /* every dial at its full amt, either way */
            for (i = 0; i < LEN; i++)
                sqm_pitch_set(&x, i, pit ? (int32_t)rnd(25u) - 12 : 0);
            sqm_build(p, LEN, &x, amt, a);
            builds++, pit_runs += pit;
            for (i = 0; i < LEN; i++) {
                if (!sqm_hit(&a[i]))
                    continue;
                for (m = 0; m < a[i].n && m < 4u; m++) {
                    int32_t nn = a[i].note[m];
                    out_of_lane += nn < floor_of[kind] || nn > top;
                    under_floor += kind == 0u && nn < 48;
                    edge += nn == floor_of[kind] || nn == top;
                    moved += a[i].note[m] != p[i].note[m];
                }
            }
        }
    printf("lane: %u builds (%u with PITCH moves), %u notes moved, %u at the lane's edge, outside the lane %u, a skank under C3 %u\n", builds, pit_runs, moved, edge, out_of_lane, under_floor);
    morph.cfg &= ~(uint32_t)MC_PIT;
    CHECK("lane: no note of any end lies outside [the Original's lowest (not above 48), its highest + 12]", !out_of_lane);
    CHECK("lane: a skank at 60..70 never gets a note under C3", !under_floor);
    CHECK("lane: the dials and the pitch moves do change notes in the lane scenes", moved > builds);
    CHECK("lane: notes reach the lane's edge (the bound is tested, not avoided)", edge > 50u);
}

/* ---- still (C4): a sustained part holds still */
static void t_still(void)
{
    uint32_t k, hits, ph, t, arc, bad_build = 0, bad_cap = 0, bad_room = 0, builds = 0, i;
    morph.cfg &= ~(uint32_t)MC_PIT;
    for (hits = 0; hits <= 1u; hits++)
        for (k = 0; k < 300u; k++) {
            step_t p[LEN], o[LEN];
            sqm_end_t x;
            uint32_t amt = 25u + 25u * rnd(4u);
            memset(p, 0, sizeof p);
            if (hits) {                                /* one chord, or one note, on any step (a held pad often has its tie after it) */
                i = rnd(LEN);
                p[i].time = ST_NOTE;
                p[i].n = rnd(2u) ? 1u : 3u;
                p[i].vel = 100;
                p[i].note[0] = (uint8_t)(48u + rnd(24u)), p[i].note[1] = (uint8_t)(p[i].note[0] + 3u), p[i].note[2] = (uint8_t)(p[i].note[0] + 7u);
                if (rnd(2u))
                    p[(i + 1u) % LEN].time = ST_TIE;
            }
            morph_musical = (int)rnd(2u);
            sqm_rand(&x, 1u + rnd(65535u), amt);
            for (i = 0; i < SD_N; i++)
                x.d[i] = (int8_t)(rnd(2u) ? (int)amt : -(int)amt);   /* every dial at its full amt, either way: the hardest case */
            sqm_build(p, LEN, &x, amt, o);
            builds++;
            bad_build += memcmp(o, p, sizeof o) != 0;
        }
    for (arc = 0; arc <= 2u; arc++)
        for (ph = AP_RISE; ph <= AP_RECOVER; ph++)
            for (t = 0; t <= 255u; t += 5u) {
                sqm_arc = (uint8_t)arc, sqm_auto = 1, tarc.phase = (uint8_t)ph, tarc.t = (uint8_t)t;
                for (hits = 0; hits <= 2u; hits++)
                    bad_cap += tarc_cap(hits) != hits;
                bad_room += tarc_cap(3u) <= 3u || tarc_cap(8u) <= 8u;
            }
    sqm_arc = 0;
    printf("still: %u builds of an Original with 0 or 1 hit, %u changed; tarc_cap(0..2) off %u, tarc_cap(3) and (8) without room %u\n", builds, bad_build, bad_cap, bad_room);
    CHECK("still: an Original of 0 or 1 hit builds to itself for every seed and dial (C4)", !bad_build);
    CHECK("still: tarc_cap(0), (1), (2) is the count itself in every arc phase (C4)", !bad_cap);
    CHECK("still: three hits and more keep the room above them", !bad_room);
}

/* ---- room */
static void t_room(void)
{
    uint32_t e, n, k, took_a = 0, took_b = 0, wrong = 0, neither = 0, peer_bad = 0, wild_bad = 0, off_bad = 0, seed4_bad = 0, bright_bad = 0, soft_bad = 0, arcn = 0;
    ui_power_on();
    frame();
    e = trk[0].eng_req;
    n = ds_count(e);
    CHECK("room: the engine has sounds to choose between", n >= 8u);
    morph_musical = 1;
    morph.cfg &= ~(uint32_t)MC_WILD;
    sqm_arc = 0;
    for (k = 0; k < 400u; k++) {
        uint32_t a = rnd(n), b = rnd(n), seed = 4u * (1u + rnd(1000u)) + 1u + rnd(3u), r;   /* seed & 3 != 0 */
        trk[1].p[P_MUTE] = 0;
        trk[1].p[P_LEVEL] = 100;
        r = sound_complement_pick(0, e, a, b, seed);
        neither += r != a && r != b;
        if (r == b && b != a) {
            took_b++;
            wrong += sound_candidate_score(0, e, b) < sound_candidate_score(0, e, a);
        } else {
            took_a++;
            wrong += sound_candidate_score(0, e, a) < sound_candidate_score(0, e, b);
        }
        seed4_bad += sound_complement_pick(0, e, a, b, seed & ~3u) != a;
        trk[1].p[P_MUTE] = 1;
        peer_bad += sound_complement_pick(0, e, a, b, seed) != a;
        trk[1].p[P_MUTE] = 0;
        morph.cfg |= MC_WILD;
        wild_bad += sound_complement_pick(0, e, a, b, seed) != a;
        morph.cfg &= ~(uint32_t)MC_WILD;
        morph_musical = 0;
        off_bad += sound_complement_pick(0, e, a, b, seed) != a;
        morph_musical = 1;
        /* ARC: the brighter at the peak, the softer in the breakdown (when the two differ) */
        sqm_arc = 1; sqm_auto = 3; tarc.run = 1;
        if (sound_cand_bright(e, a) != sound_cand_bright(e, b)) {
            uint32_t br = sound_cand_bright(e, a) > sound_cand_bright(e, b) ? a : b, so = br == a ? b : a;
            arcn++;
            tarc.phase = AP_PEAK;
            bright_bad += sound_complement_pick(0, e, a, b, seed) != br;
            tarc.phase = AP_BREAK;
            soft_bad += sound_complement_pick(0, e, a, b, seed) != so;
        }
        sqm_arc = 0; tarc.run = 0;
    }
    printf("room: %u picks, the second candidate %u, the first %u; %u pairs with ARC\n", k, took_b, took_a, arcn);
    CHECK("room: the pick is always one of the two candidates", !neither);
    CHECK("room: the pick never has the lower contrast score of the two", !wrong);
    CHECK("room: over many pairs both candidates are picked (a bias, not a constant)", took_a > 20u && took_b > 20u);
    CHECK("room: a seed with its low two bits 0 keeps the first sound", !seed4_bad);
    CHECK("room: with the peer muted the first sound stays", !peer_bad);
    CHECK("room: WILD keeps the first sound", !wild_bad);
    CHECK("room: with the musical rolls off the first sound stays", !off_bad);
    CHECK("room: ARC's peak picks the brighter of two different sounds", arcn > 20u && !bright_bad);
    CHECK("room: ARC's breakdown picks the softer", !soft_bad);
}

int main(void)
{
    t_build();
    t_lane();
    t_still();
    t_room();
    printf("tidebuild: %d checks, %d failed\n", checks, bad);
    return bad != 0;
}
