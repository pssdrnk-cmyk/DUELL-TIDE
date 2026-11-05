/* SPDX-License-Identifier: GPL-3.0-only */
/* JOURNEY's role families (tide_ui.c JFAM): a pad roams the pads and keys and never a bass, a hit or a texture; a sound alone in its
 * family stays in its role (a roll of itself); a USER record roams its own category's family; every category is mapped; no factory
 * sound sits in OTHER (the junk drawer that made pads reach hits). Prints eligible destinations per engine and family, the walk of
 * tide_jrny_memory_test.c (before: the sound's own category, after: its family). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static const char *const FAMN[5] = {"HARMONIC", "BASS", "LEAD/PLUCK", "PERC", "FX"};

static void setup(uint32_t e, uint32_t j)
{
    ui_power_on(); frame();
    song.sel = 0; morph.target = MT_SELECTED;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], j);
    frame();
    jh_clear(0); jh_clear(1);
}

int main(void)
{
    uint32_t e, c, j, f, mapped_ok = 1, longest = 0;
    uint32_t pad_walks = 0, pad_bass = 0, pad_other_cat = 0, pad_off_role = 0, fb_walks = 0, fb_out = 0, other_n = 0, user_walks = 0, user_out = 0;
    for (c = 0; c < CAT_N; c++)
        mapped_ok &= JFAM[c] <= FAM_TEXT;
    CHECK("families: every category is mapped", mapped_ok);
    CHECK("families: a future category counts as HARMONIC", fam_of(CAT_N) == FAM_HARM && fam_of(200u) == FAM_HARM);
    CHECK("families: pads, keys harmonic; bass its own; lead and pluck together; perc and fx together",
          fam_of(CAT_PAD) == FAM_HARM && fam_of(CAT_KEYS) == FAM_HARM && fam_of(CAT_BASS) == FAM_BASS &&
          fam_of(CAT_LEAD) == fam_of(CAT_PLUCK) && fam_of(CAT_LEAD) != FAM_HARM);
    CHECK("families: hits (PERC) and textures (FX) are two families", fam_of(CAT_PERC) != fam_of(CAT_FX) && fam_of(CAT_PERC) != FAM_HARM);
    for (e = 0; e < NENGINES; e++) {
        uint32_t fc[5] = {0}, cc[CAT_N] = {0}, sum_b[5] = {0}, sum_a[5] = {0}, cnt[5] = {0}, any = 0;
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (j = 0; j < ds_count(e); j++) {
            c = ds_cat_of(e, j);
            CHECK("families: factory category in range", c < CAT_N);
            fc[fam_of(c)]++, cc[c % CAT_N]++, other_n += c == CAT_OTHER;
        }
        for (f = 0; f < 5u; f++)
            longest = fc[f] > longest ? fc[f] : longest;
        for (j = 0; j < ds_count(e); j++) {   /* destinations reachable from sound j: its group less itself */
            c = ds_cat_of(e, j);
            f = fam_of(c);
            sum_b[f] += cc[c % CAT_N] - 1u, sum_a[f] += fc[f] - 1u, cnt[f]++;
        }
        for (f = 0; f < 5u; f++)
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
                    pad_off_role += fam_of(ds_cat_of(e, (uint32_t)t)) != FAM_HARM;
                }
            }
        }
    }
    /* a USER record walks its category's family, whatever the engine holds: BASS stays bass, PAD stays harmonic (it used to roam any sound) */
    for (e = 0; e < NENGINES; e++) {
        uint32_t cats[2] = {CAT_PAD, CAT_BASS}, ci;
        if (!eng_ok(e) || ds_count(e) < 4u)
            continue;
        for (ci = 0; ci < 2u; ci++) {
            uint32_t i, x = 1u, n = 0;
            for (j = 0; j < ds_count(e); j++)
                n += fam_of(ds_cat_of(e, j)) == fam_of(cats[ci]);
            if (!n)
                continue;
            setup(e, 0);
            up_store_cat(2u, "TEST", cats[ci]);
            trk[0].user = 3;
            morph.gen = 91u + e; fm1_ms = 0;
            for (i = 0; i < 24u; i++, x ^= 1u) {
                int32_t t;
                tide_journey(1u, x, 1u, 0u);
                t = morph_to_of(0, x);
                if (t < 0)
                    continue;
                user_walks++;
                user_out += fam_of(ds_cat_of(e, (uint32_t)t)) != fam_of(cats[ci]);
            }
        }
    }
    printf("pad walks %u picks: bass %u, other categories %u; lone-family walks %u picks, outside the family %u; largest family %u\n",
           pad_walks, pad_bass, pad_other_cat, fb_walks, fb_out, longest);
    CHECK("families: a pad never reaches a bass", pad_walks && !pad_bass);
    CHECK("families: a pad reaches other categories of its family (keys, ..)", pad_other_cat > 0);
    printf("pad picks outside the harmonic family %u; factory sounds in OTHER %u; user-record walks %u picks, outside the family %u\n", pad_off_role, other_n,
           user_walks, user_out);
    CHECK("families: a pad never reaches a hit or a texture", pad_walks && !pad_off_role);
    CHECK("families: no factory sound is filed under OTHER", !other_n);
    CHECK("families: a sound alone in its family stays in its role (a roll of itself, never another family)", !fb_out);
    CHECK("families: a USER record walks its own category's family", user_walks && !user_out);
    CHECK("families: no family outgrows the pick list", longest <= 64u);
    /* AUTO TIDE reaches the fork's own factory sounds (audit E12): #129..#160 are places 128..159 of the browse list (DS_LIST); walk JOURNEY from every
     * sound of every engine, 24 hops each, and count which of them were ever the target. A sound whose family on its engine has no other member can only
     * be reached by a roll of itself, so it is not required; every other one must come up, and so must a majority of the 32 overall */
    {
        uint8_t reached[DS_N], listed[DS_N], eligible[DS_N];
        uint32_t g, rank, fork_n = 0, fork_hit = 0, fork_elig = 0, elig_hit = 0, unreached = 0;
        memset(reached, 0, sizeof reached);
        memset(eligible, 0, sizeof eligible);
        for (rank = 0; rank < DS_N; rank++)
            listed[DS_LIST[rank]] = (uint8_t)rank;
        for (e = 0; e < NENGINES; e++) {
            uint32_t fc[5] = {0};
            if (!eng_ok(e) || ds_count(e) < 2u)
                continue;
            for (j = 0; j < ds_count(e); j++)
                fc[fam_of(ds_cat_of(e, j))]++;
            for (j = 0; j < ds_count(e); j++) {
                uint32_t i, x = 1u;
                eligible[DS_ENG_AT[e] + j] = fc[fam_of(ds_cat_of(e, j))] > 1u;
                setup(e, j);
                morph.gen = 301u + j; fm1_ms = 0;
                for (i = 0; i < 24u; i++, x ^= 1u) {
                    int32_t t;
                    tide_journey(1u, x, 1u, 0u);
                    t = morph_to_of(0, x);
                    if (t >= 0 && (uint32_t)t < ds_count(e))
                        reached[DS_ENG_AT[e] + (uint32_t)t] = 1u;
                }
            }
        }
        for (g = 0; g < DS_N; g++) {
            if (listed[g] < 128u || listed[g] >= 160u)
                continue;
            fork_n++;
            fork_hit += reached[g];
            fork_elig += eligible[g];
            elig_hit += eligible[g] && reached[g];
            if (eligible[g] && !reached[g]) {
                unreached++;
                printf("not reached: #%u (engine sound %u)\n", listed[g] + 1u, g);
            }
        }
        printf("AUTO TIDE reach: %u of the %u sounds #129..#160 came up as a target, %u of the %u that have a family to arrive from\n", fork_hit, fork_n, elig_hit, fork_elig);
        CHECK("reach: the browse list has the 32 sounds #129..#160", fork_n == 32u);
        CHECK("reach: every sound of #129..#160 with a family to arrive from comes up in JOURNEY's walks", !unreached);
        CHECK("reach: JOURNEY reaches the fork's own sounds (a majority of #129..#160)", fork_hit * 2u > fork_n);
    }
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
