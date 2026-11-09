/* SPDX-License-Identifier: GPL-3.0-only */
/* M3: JOURNEY's destination memory (tide_ui.c jh_*): the last four targets of a part are avoided where the group has others,
 * relaxed oldest first where it has not; RAM only; cleared when a session starts and on HOME.
 * J1 (audit 2026-11-09, owner 9 November): the memory now acts inside the Original's nearest sounds in level and colour (tide_ui.c
 * jctx_near, jrny_ctx.h): every pick is one of the Original's JCTX_KEEP + 2 nearest of its class (the two ends being left are out of each
 * pick), the sound still moves on (never a sound of the two picks before; three or more sounds over a walk), a group of two as before. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

#define STEPS 48u

/* part 0 on the first sound of group (e, role family) as its Original; returns the group's size */
static uint32_t setup(uint32_t e, uint32_t cat)
{
    uint32_t j, n = 0, first = 0;
    for (j = 0; j < ds_count(e); j++)
        if (fam_of(ds_cat_of(e, j)) == cat) {
            if (!n++)
                first = j;
        }
    ui_power_on(); frame();
    song.sel = 0; SEL_BOTH = 0;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], first);
    frame();
    jh_clear(0); jh_clear(1);
    return n;
}

/* STEPS arrivals of a JOURNEY on part 0, alternating the end; the sound each reached. hist 0: the memory emptied before every pick
 * (the code as it was). Returns the steps run before something failed (STEPS: all) */
static uint32_t walk(uint32_t seed, int hist, int *seq, uint32_t *same_as_current)
{
    uint32_t i, x = 1u;
    morph.gen = seed; fm1_ms = 0;
    for (i = 0; i < STEPS; i++, x ^= 1u) {
        int32_t cur = morph_to_of(0, x ^ 1u);
        if (!hist)
            jh_clear(0);
        tide_journey(1u, x, 1u, 0u);
        seq[i] = morph_to_of(0, x);
        if (seq[i] < 0)
            return i;
        if (seq[i] == cur)
            ++*same_as_current;
    }
    return STEPS;
}
static uint32_t revisits(const int *seq, uint32_t w)   /* a pick equal to one of the w before it */
{
    uint32_t i, j, r = 0;
    for (i = 1; i < STEPS; i++)
        for (j = i > w ? i - w : 0u; j < i; j++)
            if (seq[j] == seq[i]) {
                r++;
                break;
            }
    return r;
}

int main(void)
{
    uint32_t e, cat, groups = 0, big = 0, small = 0, stalls = 0, cur = 0, rv_h = 0, rv_n = 0, big_rv = 0, picks = 0;
    uint32_t seen[NENGINES][32] = {{0}}, size_hist[8] = {0};
    int a[STEPS], b[STEPS], c[STEPS];
    uint32_t one_same = 0, one_n = 0, det = 1, stuck = 0, jn_groups = 0, jn_picks = 0, jn_far = 0;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (cat = 0; cat < 4u; cat++) {
            uint32_t n, i, ra, rb, dist;
            uint32_t j;
            for (n = j = 0; j < ds_count(e); j++)
                n += fam_of(ds_cat_of(e, j)) == cat;
            if (n < 2u || seen[e][cat]++)               /* (a sound alone in its family has no walk: it rolls itself, tide_jrny_family_test.c) */
                continue;
            groups++;
            
            setup(e, cat);
            stalls += walk(0x1234u + e * 32u + cat, 1, a, &cur) != STEPS;
            setup(e, cat);
            walk(0x1234u + e * 32u + cat, 1, c, &cur);
            for (i = 0; i < STEPS; i++)
                det &= a[i] == c[i];
            setup(e, cat);
            stalls += walk(0x1234u + e * 32u + cat, 0, b, &cur) != STEPS;
            for (i = dist = 0; i < 2u * STEPS; i++) {   /* the sounds the walks really reached: the eligible ones (the peer part holds some) */
                int v = i < STEPS ? a[i] : b[i - STEPS];
                uint32_t q;
                for (q = 0; q < i && (q < STEPS ? a[q] : b[q - STEPS]) != v; q++)
                    ;
                dist += q == i;
            }
            size_hist[n > 7u ? 7u : n]++;
            ra = revisits(a, 2u); rb = revisits(b, 4u);
            {                                     /* J1: the picks are among the Original's nearest of its class (tide_ui.c jctx_near) */
                uint32_t o = trk[0].preset, q, rank, nd = 0, dd[64];
                const uint8_t *row = JRNY_CTX + DS_ENG_AT[e];
                uint32_t cls = tide_cls(ds_value(ds_get(e, o), P_ATK, TP[P_ATK].def), ds_value(ds_get(e, o), P_SUS, TP[P_SUS].def));
                for (j = 0; j < ds_count(e) && nd < 64u; j++)
                    if (j != o && fam_of_sound(e, j) == fam_of_sound(e, o) &&
                        tide_cls(ds_value(ds_get(e, j), P_ATK, TP[P_ATK].def), ds_value(ds_get(e, j), P_SUS, TP[P_SUS].def)) == cls) {
                        dd[nd++] = jctx_dist(row[o], row[j]);
                    }
                if (nd > JCTX_KEEP + 2u) {            /* (a class large enough for the bound to mean something) */
                    jn_groups++;
                    for (i = 0; i < STEPS; i++) {
                        uint32_t d = jctx_dist(row[o], row[(uint32_t)a[i]]);
                        if (a[i] == (int)o)
                            continue;
                        for (q = rank = 0; q < nd; q++)
                            rank += dd[q] < d;
                        jn_far += rank >= JCTX_KEEP + 2u;
                        jn_picks++;
                    }
                }
            }
            if (getenv("JM_DUMP")) {
                printf("e%u c%u n%u rv %u/%u:", e, cat, n, ra, rb);
                for (i = 0; i < 16u; i++)
                    printf(" %d", a[i]);
                printf("\n");
            }
            rv_h += ra; rv_n += rb;
            picks += STEPS - 1u;
            if (n >= 5u) {                        /* (J1: the walk stays among the Original's nearest, so it reaches fewer than five) */
                big++;
                big_rv += ra;
                stuck += dist < 3u;
            } else
                small++;
            if (n == 2u) {                        /* one candidate: the same picks with and without the memory */
                one_n++;
                for (i = 0, j = 1; i < STEPS; i++)
                    j &= a[i] == b[i];
                one_same += j;
            }
        }
    }
    printf("groups %u (5+ eligible sounds: %u, fewer: %u); category sizes 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7+:%u\n", groups, big, small,
           size_hist[1], size_hist[2], size_hist[3], size_hist[4], size_hist[5], size_hist[6], size_hist[7]);
    printf("revisits within 4 picks over %u picks: without the memory %u, with it %u (groups of 5+: %u)\n",
           picks, rv_n, rv_h, big_rv);
    CHECK("jrny memory: every group walks STEPS arrivals without a stall or a roll", !stalls);
    CHECK("jrny memory: seeded runs repeat", det);
    printf("J1: %u classes larger than %u: %u picks, %u outside the Original's nearest %u; groups of 5+ reaching fewer than 3 sounds: %u\n",
           jn_groups, JCTX_KEEP + 2u, jn_picks, jn_far, JCTX_KEEP + 2u, stuck);
    CHECK("jrny memory: groups of five or more never pick a sound of the two picks before (the ends being left)", big && !big_rv);
    CHECK("jrny memory: groups of five or more reach three or more sounds (the sound keeps moving, J1)", !stuck);
    CHECK("jrny J1: every pick is among the Original's nearest of its class in level and colour", jn_groups && jn_picks && !jn_far);
    CHECK("jrny memory: a group of two (one candidate) picks exactly as before", one_n && one_same == one_n);
    CHECK("jrny memory: never the sound just reached", !cur);

    /* a session start clears it, and so does HOME */
    for (e = 0; e < NENGINES && !(eng_ok(e) && ds_count(e) > 6u); e++)
        ;
    setup(e, fam_of(ds_cat_of(e, 0)));
    tide_journey(1u, 1u, 1u, 0u);
    CHECK("jrny memory: a pick is remembered", jh[0][0] != 0 && jh_eng[0] == e);
    gl.mode = GM_JOURNEY; gl.on = gl.armed = 0; gl.target = GT_SEQ;
    glide_start(0u, 1u, 0u);
    CHECK("jrny memory: a new session starts it empty", !jh[0][0] && !jh[0][3]);
    gl.on = gl.armed = 0;
    tide_journey(1u, 1u, 1u, 0u);
    tide_home(1u);
    CHECK("jrny memory: HOME clears it", !jh[0][0]);
    /* B18: a JRNY start gives End B a target on each part that has none, decided per part (part 2 already has one) */
    setup(e, fam_of(ds_cat_of(e, 0)));
    set_engine_of(&trk[1], e); apply_preset_to(&trk[1], 0); frame();
    morph_to_sound(1, 1, 1);
    {
        int32_t keep = morph_to_of(1, 1);
        gl.mode = GM_JOURNEY; gl.on = gl.armed = 0; gl.target = GT_SOUND;
        glide_start(1u, 3u, 0u);
        CHECK("jrny: a start with part 2 already targeted still targets part 1 (B18)", keep >= 0 && morph_to_of(0, 1) >= 0 && morph_to_of(1, 1) == keep);
    }
    /* B16: the draws follow from the session seed alone, whenever they are made */
    {
        uint32_t s1, s2, s3, s4;
        morph.gen = 77; fm1_ms = 1;      s1 = morph_new_seed();
        fm1_ms = 999999;                 s2 = morph_new_seed();
        morph.gen = 77; fm1_ms = 5;      s3 = morph_new_seed();
        fm1_ms = 12345;                  s4 = morph_new_seed();
        CHECK("seeds: the same session seed gives the same draws at any time (B16)", s1 == s3 && s2 == s4 && s1 != s2);
    }
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
