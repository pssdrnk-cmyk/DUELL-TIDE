/* SPDX-License-Identifier: GPL-3.0-only */
/* JOURNEY's role families (tide_ui.c JFAM): a pad roams the pads and keys and never a bass, a hit or a texture; a sound alone in its
 * family stays in its role (a roll of itself); a USER record roams its own category's family; every category is mapped; no factory
 * sound sits in OTHER (the junk drawer that made pads reach hits); the texture family (C3, D39) is the FX sounds with the DSF_TEXT flag, every
 * other FX a one-shot (family NOISE). Prints eligible destinations per engine and family, the walk of
 * tide_jrny_memory_test.c (before: the sound's own category, after: its family). Metric 4.6 (C2, TIDE_ROLES_AND_MULTIS_SPEC.md): the walk also keeps the
 * Original's attack class (PAD, STAB, KEYS by ATK and SUS), and a family with no other sound of the class still turns (any class). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

static const char *const FAMN[FAM_N] = {"HARMONIC", "BASS", "LEAD/PLUCK", "PERC", "TEXT", "NOISE"};

static void setup(uint32_t e, uint32_t j)
{
    ui_power_on(); frame();
    song.sel = 0; morph.target = MT_SELECTED;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], j);
    frame();
    jh_clear(0); jh_clear(1);
}

/* factory sound `name`: its engine and index; 0 when there is none */
static int find_sound(const char *name, uint32_t *pe, uint32_t *pj)
{
    uint32_t e, j;
    for (e = 0; e < NENGINES; e++)
        for (j = 0; j < ds_count(e); j++)
            if (!strcmp(ds_name(e, j), name)) {
                *pe = e, *pj = j;
                return 1;
            }
    return 0;
}
/* factory sound j of engine e: its attack class (tide_ui.c tide_cls: 2 PAD, 1 STAB, 0 KEYS) */
static uint32_t cls_of(uint32_t e, uint32_t j)
{
    const ds_sound_t *s = ds_get(e, j);
    return tide_cls(ds_value(s, P_ATK, TP[P_ATK].def), ds_value(s, P_SUS, TP[P_SUS].def));
}
/* `n` JOURNEY arrivals from factory sound (e, j); the targets that are not of class `cls` or not in the family of the start go to *off, the arrivals that
 * reached any target to *got */
static void walk_cls(uint32_t e, uint32_t j, uint32_t n, uint32_t cls, uint32_t *off, uint32_t *got)
{
    uint32_t i, x = 1u;
    *off = *got = 0;
    setup(e, j);
    morph.gen = 5150u + e * 64u + j; fm1_ms = 0;
    for (i = 0; i < n; i++, x ^= 1u) {
        int32_t t;
        tide_journey(1u, x, 1u, 0u);
        t = morph_to_of(0, x);
        if (t < 0 || (uint32_t)t >= ds_count(e))
            continue;
        (*got)++;
        *off += cls_of(e, (uint32_t)t) != cls || fam_of_sound(e, (uint32_t)t) != fam_of_sound(e, j);
    }
}
/* 32 JOURNEY arrivals from factory sound `name`; counts the targets that are flagged (the TEXT family) and the distinct targets */
static void walk32(const char *name, uint32_t *flagged, uint32_t *unflagged, uint32_t *distinct)
{
    uint32_t e = 0, j = 0, i, x = 1u, seen[64] = {0};
    *flagged = *unflagged = *distinct = 0;
    if (!find_sound(name, &e, &j))
        return;
    setup(e, j);
    morph.gen = 4242u + j; fm1_ms = 0;
    for (i = 0; i < 32u; i++, x ^= 1u) {
        int32_t t;
        tide_journey(1u, x, 1u, 0u);
        t = morph_to_of(0, x);
        if (t < 0 || (uint32_t)t >= ds_count(e))
            continue;
        if (ds_get(e, (uint32_t)t)->flags & DSF_TEXT)
            (*flagged)++;
        else
            (*unflagged)++;
        if ((uint32_t)t < 64u && !seen[t])
            seen[t] = 1u, (*distinct)++;
    }
}

int main(void)
{
    uint32_t e, c, j, f, mapped_ok = 1, longest = 0;
    uint32_t pad_walks = 0, pad_bass = 0, pad_other_cat = 0, pad_off_role = 0, fb_walks = 0, fb_out = 0, other_n = 0, user_walks = 0, user_out = 0;
    for (c = 0; c < CAT_N; c++)
        mapped_ok &= JFAM[c] < FAM_N;
    CHECK("families: every category is mapped", mapped_ok);
    CHECK("families: a future category counts as HARMONIC", fam_of(CAT_N) == FAM_HARM && fam_of(200u) == FAM_HARM);
    CHECK("families: pads, keys harmonic; bass its own; lead and pluck together; perc and fx together",
          fam_of(CAT_PAD) == FAM_HARM && fam_of(CAT_KEYS) == FAM_HARM && fam_of(CAT_BASS) == FAM_BASS &&
          fam_of(CAT_LEAD) == fam_of(CAT_PLUCK) && fam_of(CAT_LEAD) != FAM_HARM);
    CHECK("families: hits (PERC), one-shots (FX) and textures (the TEXT flag) are three families",
          fam_of(CAT_PERC) != fam_of(CAT_FX) && fam_of(CAT_PERC) != FAM_HARM && fam_of(CAT_FX) == FAM_NOISE && fam_of(CAT_PERC) != FAM_TEXT && FAM_TEXT != FAM_NOISE);
    for (e = 0; e < NENGINES; e++) {
        uint32_t fc[FAM_N] = {0}, cc[CAT_N] = {0}, sum_b[FAM_N] = {0}, sum_a[FAM_N] = {0}, cnt[FAM_N] = {0}, any = 0;
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (j = 0; j < ds_count(e); j++) {
            c = ds_cat_of(e, j);
            CHECK("families: factory category in range", c < CAT_N);
            fc[fam_of_sound(e, j)]++, cc[c % CAT_N]++, other_n += c == CAT_OTHER;
        }
        for (f = 0; f < FAM_N; f++)
            longest = fc[f] > longest ? fc[f] : longest;
        for (j = 0; j < ds_count(e); j++) {   /* destinations reachable from sound j: its group less itself */
            c = ds_cat_of(e, j);
            f = fam_of_sound(e, j);
            sum_b[f] += cc[c % CAT_N] - 1u, sum_a[f] += fc[f] - 1u, cnt[f]++;
        }
        for (f = 0; f < FAM_N; f++)
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
            uint32_t i, x = 1u, lone = fc[fam_of_sound(e, j)] == 1u;
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
                    fb_out += fam_of_sound(e, (uint32_t)t) != fam_of_sound(e, j);
                } else {
                    pad_walks++;
                    pad_bass += ds_cat_of(e, (uint32_t)t) == CAT_BASS;
                    pad_other_cat += ds_cat_of(e, (uint32_t)t) != CAT_PAD;
                    pad_off_role += fam_of_sound(e, (uint32_t)t) != FAM_HARM;
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
                n += fam_of_sound(e, j) == fam_of(cats[ci]);
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
                user_out += fam_of_sound(e, (uint32_t)t) != fam_of(cats[ci]);
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
    /* C3, the texture family: eight FX sounds carry the DSF_TEXT flag (gen_duell_sounds.py --check lists the same eight); a texture roams the
     * textures of its engine, a one-shot never reaches one */
    {
        static const char *const TEXTS[8] = {"DUST BED", "TAPE HISS", "MOOR WIND", "WHITE OUT", "NIGHT DRONE", "STORM FRONT", "TAPE DRONE", "FROZEN AIR"};
        uint32_t g, nflag = 0, nonfx = 0, named = 0, fl, un, di, te, tj;
        for (g = 0; g < DS_N; g++) {
            nflag += (DS_SOUNDS[g].flags & DSF_TEXT) != 0u;
            nonfx += (DS_SOUNDS[g].flags & DSF_TEXT) && DS_SOUNDS[g].cat != CAT_FX;
        }
        for (g = 0; g < 8u; g++)
            named += find_sound(TEXTS[g], &te, &tj) && (ds_get(te, tj)->flags & DSF_TEXT) && fam_of_sound(te, tj) == FAM_TEXT;
        printf("texture flag: %u sounds flagged, %u of the 8 named ones found and flagged\n", nflag, named);
        CHECK("texture: eight sounds carry the TEXT flag, all of them FX", nflag == 8u && !nonfx && named == 8u);
        CHECK("texture: an unflagged FX is a one-shot (NOISE), a flagged one a TEXT", find_sound("MODEM", &te, &tj) && fam_of_sound(te, tj) == FAM_NOISE &&
              find_sound("DUST BED", &te, &tj) && fam_of_sound(te, tj) == FAM_TEXT && fam_of(CAT_FX) == FAM_NOISE && fam_of(CAT_OTHER) == FAM_TEXT);
        walk32("DUST BED", &fl, &un, &di);
        printf("DUST BED (a texture): 32 arrivals, %u textures, %u others, %u distinct\n", fl, un, di);
        CHECK("texture: a texture (DUST BED) visits only flagged sounds over 32 arrivals", fl > 0u && !un);
        CHECK("texture: a texture still turns through the textures of its engine (3 others on NOISE)", di >= 3u);
        walk32("MODEM", &fl, &un, &di);
        printf("MODEM (a one-shot): 32 arrivals, %u textures, %u others, %u distinct\n", fl, un, di);
        CHECK("texture: a one-shot (MODEM) never reaches a flagged sound", !fl && un > 0u);
        walk32("NIGHT DRONE", &fl, &un, &di);
        CHECK("texture: a drone (NIGHT DRONE) never reaches a one-shot", !un);
    }
    /* C2, metric 4.6: JOURNEY keeps the Original's attack class. A KEYS part (CHORD ECHO: ATK 3, SUS 34) reaches only keys (ATK under 60, SUS over 20), a PAD part
     * (FOG BANK) only sounds with ATK 60 or more; from every sound of every engine, 12 arrivals stay in the class and the family, except where the family has no
     * other sound of the class: there the part still turns (pass 2: any class of the family) */
    {
        uint32_t off, got, be, bj, sw = 0, swoff = 0, lone = 0, lone_turn = 0, nsnd = 0, g;
        static const char *const NAMES[2] = {"CHORD ECHO", "FOG BANK"};
        for (g = 0; g < 2u; g++) {
            if (!find_sound(NAMES[g], &be, &bj))
                continue;
            walk_cls(be, bj, 32u, cls_of(be, bj), &off, &got);
            printf("%s (class %u): 32 arrivals, %u reached, %u outside its class or family\n", NAMES[g], cls_of(be, bj), got, off);
            if (!g) {
                CHECK("4.6: from a KEYS part (CHORD ECHO, ATK 3) 32 arrivals reach only keys, none with ATK 60 or more", cls_of(be, bj) == 0u && got > 24u && !off);
            } else
                CHECK("4.6: from a PAD part (FOG BANK) 32 arrivals reach only pads", cls_of(be, bj) == 2u && got > 24u && !off);
        }
        for (e = 0; e < NENGINES; e++) {
            uint32_t cnt[FAM_N][3] = {{0}};
            if (!eng_ok(e) || ds_count(e) < 2u)
                continue;
            for (j = 0; j < ds_count(e); j++)
                cnt[fam_of_sound(e, j)][cls_of(e, j)]++;
            for (j = 0; j < ds_count(e); j++) {
                uint32_t fm = fam_of_sound(e, j), cl = cls_of(e, j), fsum = cnt[fm][0] + cnt[fm][1] + cnt[fm][2];
                if (fsum < 2u)
                    continue;                              /* (alone in its family: a roll of itself, the family tests above) */
                walk_cls(e, j, 12u, cl, &off, &got);
                nsnd++;
                if (cnt[fm][cl] > 1u) {                    /* (another sound of its class: the class holds) */
                    sw += got;
                    swoff += off;
                } else {                                   /* (the only one of its class in the family: pass 2, any class) */
                    lone++;
                    lone_turn += got > 0u;
                    if (!got)
                        printf("lone class sound does not turn: engine %u %s\n", e, ds_name(e, j));
                }
            }
        }
        printf("class walks from %u sounds: %u arrivals, %u outside the class; %u sounds alone in their class, %u of them turn\n",
               nsnd, sw, swoff, lone, lone_turn);
        CHECK("4.6: from every sound with a classmate the walk stays in its class and its family", sw > 500u && !swoff);
        CHECK("4.6: a sound alone in its class still turns (any class of the family)", lone > 0u && lone_turn == lone);
    }
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
            uint32_t fc[FAM_N] = {0};
            if (!eng_ok(e) || ds_count(e) < 2u)
                continue;
            for (j = 0; j < ds_count(e); j++)
                fc[fam_of_sound(e, j)]++;
            for (j = 0; j < ds_count(e); j++) {
                uint32_t i, x = 1u;
                eligible[DS_ENG_AT[e] + j] = fc[fam_of_sound(e, j)] > 1u;
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
