/* JOURNEY's role families (tide_ui.c JFAM): a pad roams the pads, keys and the rest of its family and never a bass; an empty
 * family falls back to the engine; every category is mapped. Prints eligible destinations per engine and family, the walk of
 * tide_jrny_memory_test.c (before: the sound's own category, after: its family). */
#define UI_TEST_NO_MAIN 1
#define JP_TEST 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static const char *const FAMN[4] = {"HARMONIC", "BASS", "LEAD/PLUCK", "PERC/FX"};

static void setup(uint32_t e, uint32_t j)
{
    ui_power_on(); frame();
    song.sel = 0; morph.target = MT_SELECTED;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], j);
    frame();
    jh_clear(0); jh_clear(1);
}


/* a transition cur -> to of engine e is a measured bad pair (jrny_pairs.h) */
static int pair_bad(uint32_t e, uint32_t x, uint32_t y)
{
    uint32_t a = x < y ? x : y, b = x ^ y ^ a, w = e << 12 | a << 6 | b, i;
    for (i = 0; i < JP_N; i++)
        if (JP_BAD[i] == w)
            return 1;
    return 0;
}
/* JOURNEY walks of every engine and family, the skip off and on: transitions made / on a bad pair, and the walks that stalled */
static void pair_walks(uint32_t *moves, uint32_t *bad, uint32_t *stalls, uint32_t off)
{
    uint32_t e, f, j, i, x;
    jp_off = (uint8_t)off;
    *moves = *bad = *stalls = 0;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (f = 0; f < 4u; f++) {
            uint32_t first = ds_count(e);
            for (j = 0; j < ds_count(e); j++)
                if (fam_of(ds_cat_of(e, j)) == f && j < first)
                    first = j;
            if (first == ds_count(e))
                continue;
            setup(e, first);
            morph.gen = 991u + e * 4u + f; fm1_ms = 0;
            for (i = 0, x = 1u; i < 48u; i++, x ^= 1u) {
                int32_t cur = morph_to_of(0, x ^ 1u) >= 0 ? morph_to_of(0, x ^ 1u) : (int32_t)first, t;
                tide_journey(1u, x, 1u, 0u);
                t = morph_to_of(0, x);
                if (t < 0) {
                    ++*stalls;
                    continue;
                }
                ++*moves;
                *bad += pair_bad(e, (uint32_t)cur, (uint32_t)t);
            }
        }
    }
    jp_off = 0;
}
int main(void)
{
    uint32_t e, c, j, f, mapped_ok = 1, longest = 0;
    uint32_t pad_walks = 0, pad_bass = 0, pad_other_cat = 0, fb_walks = 0, fb_out = 0;
    for (c = 0; c < CAT_N; c++)
        mapped_ok &= JFAM[c] <= FAM_PERC;
    CHECK("families: every category is mapped", mapped_ok);
    CHECK("families: a future category counts as HARMONIC", fam_of(CAT_N) == FAM_HARM && fam_of(200u) == FAM_HARM);
    CHECK("families: pads, keys harmonic; bass its own; lead and pluck together; perc and fx together",
          fam_of(CAT_PAD) == FAM_HARM && fam_of(CAT_KEYS) == FAM_HARM && fam_of(CAT_BASS) == FAM_BASS &&
          fam_of(CAT_LEAD) == fam_of(CAT_PLUCK) && fam_of(CAT_PERC) == fam_of(CAT_FX) && fam_of(CAT_LEAD) != FAM_HARM);
    for (e = 0; e < NENGINES; e++) {
        uint32_t fc[4] = {0}, cc[CAT_N] = {0}, sum_b[4] = {0}, sum_a[4] = {0}, cnt[4] = {0}, any = 0;
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (j = 0; j < ds_count(e); j++) {
            c = ds_cat_of(e, j);
            CHECK("families: factory category in range", c < CAT_N);
            fc[fam_of(c)]++, cc[c % CAT_N]++;
        }
        for (f = 0; f < 4u; f++)
            longest = fc[f] > longest ? fc[f] : longest;
        for (j = 0; j < ds_count(e); j++) {   /* destinations reachable from sound j: its group less itself */
            c = ds_cat_of(e, j);
            f = fam_of(c);
            sum_b[f] += cc[c % CAT_N] - 1u, sum_a[f] += fc[f] - 1u, cnt[f]++;
        }
        for (f = 0; f < 4u; f++)
            if (cnt[f]) {
                if (!any++)
                    printf("engine %u:", e);
                printf("  %s %u sounds, reach before %u.%u after %u.%u;", FAMN[f], cnt[f], sum_b[f] / cnt[f], sum_b[f] * 10u / cnt[f] % 10u,
                       sum_a[f] / cnt[f], sum_a[f] * 10u / cnt[f] % 10u);
            }
        if (any)
            printf("\n");
        /* a pad walks its family and never a bass; a lone family sound leaves the family (the engine's fallback) */
        for (j = 0; j < ds_count(e); j++) {
            uint32_t i, x = 1u, lone = fc[fam_of(ds_cat_of(e, j))] == 1u;
            if (!lone && ds_cat_of(e, j) != CAT_PAD)
                continue;
            setup(e, j);
            morph.gen = 77u + j; fm1_ms = 0;
            for (i = 0; i < 24u; i++, x ^= 1u) {
                int32_t t;
                tide_journey(1u, x, 1u, 0u);
                t = morph_to_of(0, x);
                if (t < 0)
                    continue;
                if (lone) {
                    fb_walks++;
                    fb_out += fam_of(ds_cat_of(e, (uint32_t)t)) != fam_of(ds_cat_of(e, j));
                } else {
                    pad_walks++;
                    pad_bass += ds_cat_of(e, (uint32_t)t) == CAT_BASS;
                    pad_other_cat += ds_cat_of(e, (uint32_t)t) != CAT_PAD;
                }
            }
        }
    }
    printf("pad walks %u picks: bass %u, other categories %u; lone-family walks %u picks, outside the family %u; largest family %u\n",
           pad_walks, pad_bass, pad_other_cat, fb_walks, fb_out, longest);
    CHECK("families: a pad never reaches a bass", pad_walks && !pad_bass);
    CHECK("families: a pad reaches other categories of its family (keys, ..)", pad_other_cat > 0);
    CHECK("families: a lone family sound still moves (fallback to the engine)", !fb_walks || fb_out > 0);
    CHECK("families: no family outgrows the pick list", longest <= 64u);
    {
        uint32_t m0, b0, s0, m1, b1, s1, i;
        int sorted = 1, same = 1;
        pair_walks(&m0, &b0, &s0, 1u);
        pair_walks(&m1, &b1, &s1, 0u);
        for (i = 1; i < JP_N; i++)
            sorted &= JP_BAD[i - 1u] < JP_BAD[i];
        for (i = 0; i < JP_N; i++) {
            uint32_t pe = JP_BAD[i] >> 12, pa = JP_BAD[i] >> 6 & 63u, pb = JP_BAD[i] & 63u;
            same &= pe < NENGINES && pa < pb && pb < ds_count(pe) && fam_of(ds_cat_of(pe, pa)) == fam_of(ds_cat_of(pe, pb));
        }
        printf("bad-pair skip (%u pairs listed): %u JOURNEY moves, on a bad pair without the skip %u (%u%%), with it %u (%u%%); stalls %u / %u\n",
               (unsigned)JP_N, m1, b0, m0 ? b0 * 100u / m0 : 0u, b1, m1 ? b1 * 100u / m1 : 0u, s0, s1);
        CHECK("pairs: the list is sorted and names same-family pairs of one engine", sorted && same);
        CHECK("pairs: fewer moves on a bad pair than without the skip", b1 < b0);
        CHECK("pairs: the skip never stalls a walk", !s0 && !s1);
    }
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
