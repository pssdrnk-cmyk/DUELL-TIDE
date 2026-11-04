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
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
