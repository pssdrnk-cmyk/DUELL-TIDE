/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE's pattern ends and sound pairs (audit E12): the bounds that decide how a generation sounds, as statistical properties over many
 * seeds, not one trace. firmware/src/seq_morph.c sqm_build (the dials DENS / SYNC / ACC / VOIC varying the Original) and
 * firmware/src/morph_complement.c sound_complement_pick (the second part's sound leaves the first one room):
 *   density   an end has at least one hit and never more than two hits more or fewer than the Original; a dial at 0 changes nothing; the same
 *             end built twice is the same (nothing accumulates); the anchors (downbeats, the motif's identity floor) stay hits
 *   register  no dial moves a note to another pitch class; VOIC moves a note an octave at most, so every note stays within an octave of
 *             the Original's lowest and highest and inside 24..108
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
    t_room();
    printf("tidebuild: %d checks, %d failed\n", checks, bad);
    return bad != 0;
}
