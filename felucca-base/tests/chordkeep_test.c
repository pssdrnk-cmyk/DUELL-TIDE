/* SPDX-License-Identifier: GPL-3.0-only */
/* The player's chord keys (SCL > CHORD: TYPE = CHRD, VOICING = VOIC) in the unit's real situations (owner: "I can't seem to enable auto chord").
 *   KEEP     a sound load leaves the part's CHRD / VOIC alone unless the sound sets a chord of its own (the pads and BEHIVE: DSF_KEYS; a stored patch saved with a chord on);
 *            a chord a sound lent goes back with the next load, unless the player changed it since; a patch saved with the chord keys OFF leaves the player's
 *   NAMED    the five chord-named dub pack sounds (DUB CHORD, DARK MIN7, SOUL STAB, LOW STAB, ICE STAB) set CHRD MIN7, VOIC OPEN
 *   FIRST    the first start's factory multi (M04 DUB CHORD, TIDE on): K1 turns TYPE with the MORPH fader at A, mid way, at B and while a glide runs; a TIDE JRNY of many trips
 *            and its arrivals leave CHRD / VOIC / VOICE alone; the sound loads on the parts keep the player's TYPE
 *   HINTS    the CHORD page (K4 = the part's VOICE): "NEEDS POLY" with a chord on and the part not POLY, "SET A SCALE" with DIA3 / DIA7 on the CHROMATIC scale, the last
 *            chord's note names; the keyboard and the K1 line as before; what the page showed before is still there
 * Built and run by host_tests.sh (name: chordkeep): the UI sources against ui_test.c's stub display and knobs; PPMs of the page go to the directory in argv[1] (the pictures of the report). */
#define UI_TEST_NO_MAIN 1
#define UI_TEXT_CAPTURE 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    printf("chordkeep: %-120s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad++;
    return ok;
}

/* the main loop and the audio together (tide_controls_test.c run): 16 ms frames with the audio blocks that fall in them */
static void run(uint32_t ms)
{
    static uint32_t acc;
    uint32_t i;
    for (i = 0; i < ms / 16u; i++) {
        int32_t out[2 * CTL];
        acc += (uint32_t)FS * 16u;
        while (acc >= 1000u * CTL) {
            acc -= 1000u * CTL;
            mix_block(out, CTL);
        }
        frame();
    }
}
static uint32_t sound_named(uint32_t eng, const char *name)
{
    uint32_t j;
    for (j = 0; j < ds_count(eng); j++)
        if (str_eq(ds_name(eng, j), name))
            return j;
    return 0xFFFFu;
}
static uint32_t first_start(void)                       /* the unit as a new one starts: LAYER, M04 DUB CHORD (TIDE_FIRST_MULTI) */
{
    ui_power_on();
    frame();
    duell_mode_force(DM_LAYER);
    project_load(TIDE_FIRST_MULTI);
    run(200);
    song.sel = 0;
    morph.target = MT_SELECTED;
    return 0;
}

/* audit G11: the chord-named sounds of the dub pack set their chord keys (MIN7, OPEN), as their audition was made with held Cm7 chords */
static void named_chords(void)
{
    static const char *const nm[5] = {"DUB CHORD", "DARK MIN7", "SOUL STAB", "LOW STAB", "ICE STAB"};
    uint32_t e, k, found = 0, set = 0;
    ui_power_on();
    for (k = 0; k < 5u; k++)
        for (e = 0; e < NENGINES; e++) {
            uint32_t j;
            if (!ds_count(e) || (j = sound_named(e, nm[k])) == 0xFFFFu)
                continue;
            init_engine_of(&trk[0], e);
            trk[0].p[P_CHRD] = CH_OFF;
            trk[0].p[P_VOIC] = VC_CLOSE;
            apply_preset_to(&trk[0], j);
            found++;
            set += (ds_get(e, j)->flags & DSF_KEYS) && trk[0].p[P_CHRD] == CH_MIN7 && trk[0].p[P_VOIC] == VC_OPEN;
            if (trk[0].p[P_CHRD] != CH_MIN7)
                printf("  %s: CHRD %d VOIC %d\n", nm[k], trk[0].p[P_CHRD], trk[0].p[P_VOIC]);
        }
    expect("DUB CHORD, DARK MIN7, SOUL STAB, LOW STAB and ICE STAB each set CHRD MIN7 and VOIC OPEN", found == 5u && set == 5u);
}

/* --- KEEP: a sound load and the player's chord keys --- */
static void keep(void)
{
    track_t *t;
    uint32_t j, plain = 0, keyed = 0, n = 0, bees = 0, ok;
    uint32_t en;
    ui_power_on();
    t = &trk[0];
    en = t->eng_req % NENGINES;
    t->p[P_CHRD] = CH_DIA3;
    t->p[P_VOIC] = VC_OPEN;
    for (j = 0; j < ds_count(en); j++) {                 /* every factory sound of the engine: none sets a chord, or the part's own goes back */
        const ds_sound_t *s = ds_get(en, j);
        if (s->flags & DSF_KEYS) {
            keyed++;
            continue;
        }
        apply_preset_to(t, j);
        n++;
        plain += t->p[P_CHRD] == CH_DIA3 && t->p[P_VOIC] == VC_OPEN;
        if (t->p[P_CHRD] != CH_DIA3 || t->p[P_VOIC] != VC_OPEN)
            printf("  sound %u %s: CHRD %d VOIC %d flags %u\n", (unsigned)j, s->name, t->p[P_CHRD], t->p[P_VOIC], s->flags);
    }
    expect("a factory sound without a chord of its own leaves TYPE and VOICING alone (every one of the engine's)", n >= 8u && plain == n);
    j = sound_named(en, "BEHIVE");
    expect("BEHIVE exists on the first engine and sets a chord of its own (DSF_KEYS)", j != 0xFFFFu && (ds_get(en, j)->flags & DSF_KEYS));
    apply_preset_to(t, j);
    bees = t->p[P_CHRD] != CH_DIA3;
    expect("a sound that sets a chord (BEHIVE) lends it as before", bees && t->p[P_CHRD] != CH_OFF);
    apply_preset_to(t, (j + 1u) % ds_count(en));
    expect("  the next sound without one gives the player's DIA3 / OPEN back", t->p[P_CHRD] == CH_DIA3 && t->p[P_VOIC] == VC_OPEN);
    apply_preset_to(t, j);
    t->p[P_CHRD] = CH_MIN7;                               /* the player changes it while it is lent */
    apply_preset_to(t, (j + 1u) % ds_count(en));
    expect("  changed by the player after the lend: it stays (MIN7)", t->p[P_CHRD] == CH_MIN7);
    t->p[P_CHRD] = CH_OFF;
    apply_preset_to(t, (j + 1u) % ds_count(en));
    expect("  the player's OFF is the player's too", t->p[P_CHRD] == CH_OFF);

    /* a stored patch: saved with a chord on it brings it, saved with OFF it leaves the player's */
    ui_power_on();
    t = &trk[0];
    t->p[P_CHRD] = CH_SUS4;
    t->p[P_VOIC] = VC_INV1;
    ok = up_patch_save_part(0, 30u) == 0;
    t->p[P_CHRD] = CH_OFF;
    t->p[P_VOIC] = VC_CLOSE;
    ok &= up_patch_save_part(0, 31u) == 0;
    t->p[P_CHRD] = CH_DIA7;
    t->p[P_VOIC] = VC_BASS;
    sl_load(31u);
    expect("a stored patch saved with the chord keys OFF leaves the player's DIA7 / +OCT", ok && t->p[P_CHRD] == CH_DIA7 && t->p[P_VOIC] == VC_BASS);
    sl_load(30u);
    expect("a stored patch saved with SUS4 / INV1 brings them (a chord the patch sets: as before)", t->p[P_CHRD] == CH_SUS4 && t->p[P_VOIC] == VC_INV1);
    sl_load(31u);
    expect("  and the next one without a chord gives the player's DIA7 / +OCT back", t->p[P_CHRD] == CH_DIA7 && t->p[P_VOIC] == VC_BASS);
    sl_load(97u);
    expect("a stored-slot factory sound without a chord (#98) keeps it too", t->p[P_CHRD] == CH_DIA7 && t->p[P_VOIC] == VC_BASS);
    undo_swap();
    undo_swap();
    expect("undo and redo of a load leave the chord keys where the player had them (param_kept)", t->p[P_CHRD] == CH_DIA7 && t->p[P_VOIC] == VC_BASS);
    (void)keyed;
}

/* --- FIRST: the first start's state --- */
static void show_parts(char *b, size_t n)
{
    snprintf(b, n, "CHRD %d/%d VOIC %d/%d VOICE %d/%d", trk[0].p[P_CHRD], trk[1].p[P_CHRD], trk[0].p[P_VOIC], trk[1].p[P_VOIC], trk[0].p[P_VOICE], trk[1].p[P_VOICE]);
}
static void first(void)
{
    char b0[80], b1[80];
    int16_t c0;
    uint32_t i;
    first_start();
    expect("first start: M04 DUB CHORD; part 1 plays MIN7 chords (POLY), part 2 is LEGATO (a root alone)", trk[0].p[P_CHRD] == CH_MIN7 && trk[0].p[P_VOICE] == V_POLY &&
           trk[1].p[P_VOICE] == V_LEGATO);
    go_page(GR_CHORD);
    run(100);
    c0 = trk[0].p[P_CHRD];
    turn(EN_K1, 1);
    expect("the CHORD page's K1 turns TYPE with the MORPH fader at A (MIN7 -> SUS4)", trk[0].p[P_CHRD] == c0 + 1);
    morph_push(MQ_FADER, 0u, 127u);
    run(200);
    turn(EN_K1, -1);
    expect("  at B (MF_NEVER does not block it)", mph[0].fader == 127u && trk[0].p[P_CHRD] == c0);
    morph_push(MQ_FADER, 0u, 60u);
    run(200);
    turn(EN_K1, 1);
    expect("  half way between the ends", mph[0].fader == 60u && trk[0].p[P_CHRD] == c0 + 1);
    morph_push(MQ_FADER, 0u, 0u);
    run(100);
    turn(EN_K1, -1);
    turn(EN_K1, -9);
    expect("  down to OFF and back up to DIA3 (the list is OFF, DIA3, DIA7, MAJ ..)", trk[0].p[P_CHRD] == CH_OFF);
    turn(EN_K1, 1);
    expect("  DIA3", trk[0].p[P_CHRD] == CH_DIA3);
    turn(EN_K2, 1);
    expect("  K2 VOICING OPEN", trk[0].p[P_VOIC] == VC_OPEN);

    /* TIDE: JRNY on both parts, many trips: the player's chord stays, and so does the voice mode */
    trk[1].p[P_CHRD] = CH_DIA7;
    show_parts(b0, sizeof b0);
    transport_req = 2u;
    run(48);
    morph.target = MT_BOTH;
    gl.mode = GM_JOURNEY; gl.target = GT_BOTH; gl.time = 0; gl.wait = 0; gl.on = gl.armed = 0;
    song.g[G_BPM] = 120;
    glide_launch(1u, morph_mask(0), 0);
    run(96);
    {
        uint32_t same = 1, arrivals = 0, last = 0;
        for (i = 0; i < 400u; i++) {
            run(160);
            show_parts(b1, sizeof b1);
            same &= str_eq(b0, b1);
            if (mph[0].fader != last && (mph[0].fader == 0u || mph[0].fader >= 120u))
                arrivals++;
            last = mph[0].fader;
        }
        expect("TIDE JRNY (both parts, 1 bar a trip, 64 s): CHRD, VOIC and VOICE of both parts never move; the glide keeps running", same && gl.on == 3u && arrivals > 5u);
    }
    glide_stop();
    transport_req = 2u;
    run(48);
    morph.target = MT_SELECTED;
    {   /* while a glide runs a K1 turn on the CHORD page still lands (nothing re-asserts the old value at the next block) */
        song.sel = 0;
        gl.mode = GM_JOURNEY; gl.target = GT_BOTH; gl.on = gl.armed = 0;
        morph.target = MT_BOTH;
        glide_launch(1u, morph_mask(0), 0);
        run(800);
        go_page(GR_CHORD);
        run(50);
        c0 = trk[0].p[P_CHRD];
        turn(EN_K1, 1);
        run(3000);
        expect("a K1 turn while a JRNY glide is running lands and stays", gl.on && trk[0].p[P_CHRD] == c0 + 1);
        glide_stop();
        transport_req = 2u;
        run(48);
    }
}

/* --- HINTS: the CHORD page --- */
static const char *outdir;
static void shot(const char *name)
{
    if (outdir)
        screen_save(outdir, name);
}
static int line_has(const char *s) { return rows_has(128, 152, s); }
static void key_tap(uint32_t k)                        /* key k (key 7 = C4 at the first start's octave) held for a moment */
{
    fm1_in.notes |= 1u << k;
    run(96);
    fm1_in.notes &= ~(1u << k);
    run(96);
}
static void names_of(uint32_t part, char *b)           /* the last chord of the part as the page should say it */
{
    uint32_t i;
    b[0] = 0;
    for (i = 0; i < chord_last[part].n && i < CHORD_MAX; i++) {
        if (i)
            strcat(b, " ");
        strcat(b, N_NOTE[chord_last[part].note[i] % 12u]);
    }
}
static void hints(void)   /* (K3 / K4: the VOICE card is MODE, only while a chord is on and the part is not POLY) */
{
    char nm[16];
    track_t *t;
    first_start();
    t = &trk[0];
    go_page(GR_CHORD);
    run(100);
    shot("1_start_part1_min7");
    expect("first start, part 1 (POLY, MIN7): no hint on the line under the keyboard, no MODE card (only VOICING)", card_hidden(2) && card_hidden(3) && !line_has("NEEDS POLY") && !line_has("SET A SCALE") && !line_has("KEYS PLAY CHORDS IN THE SCALE"));
    expect("  the K1 line says TYPE and the chord's name, K2 the voicing, as before", card_match(0, "TYPE") && rows_has(24, 48, "Cm7") && card_match(1, "VOICING|CLOSE"));
    key_tap(7);
    names_of(0, nm);
    expect("  a key played: the last chord's notes by name under the keyboard (4 notes)", chord_last[0].n == 4u && nm[0] && line_has(nm));
    shot("2_part1_after_a_key");

    t->p[P_CHRD] = CH_DIA3;
    scale_g = 0;
    run(100);
    shot("3_dia3_chromatic");
    expect("DIA3 with the CHROMATIC scale: SET A SCALE", line_has("SET A SCALE"));
    t->p[P_CHRD] = CH_DIA7;
    run(100);
    expect("  DIA7 too", line_has("SET A SCALE"));
    t->p[P_CHRD] = CH_MAJ;
    run(100);
    expect("  not for a fixed shape (MAJ names its own tones)", !line_has("SET A SCALE"));
    t->p[P_CHRD] = CH_DIA3;
    scale_g = 1;
    run(100);
    shot("4_dia3_c_major");
    expect("  and gone once the scale is C MAJOR", !line_has("SET A SCALE") && !line_has("NEEDS POLY"));
    scale_g = 0;

    song.sel = 1;
    go_page(GR_CHORD);
    run(100);
    t = &trk[1];
    t->p[P_AMODE] = 0;                                  /* (part 2 runs an arp in M04: that alone hides STRUM; off, so that the voice mode is the reason) */
    XV(t, X_STRUM) = 0;
    t->p[P_CHRD] = CH_DIA3;
    run(100);
    shot("5_part2_legato_dia3");
    expect("part 2 (LEGATO) with TYPE DIA3: NEEDS POLY (it plays the root alone)", t->p[P_VOICE] == V_LEGATO && line_has("NEEDS POLY"));
    expect("  STRUM is hidden there (it cannot act), so the VOICE card (MODE) moves up to the third place, knob 3 (the cards and knobs close ranks)", !card_hidden(2) && card_hidden(3) && card_match(2, "MODE|LEGATO"));
    turn(EN_K3, -2);
    run(100);
    shot("6_part2_k4_to_poly");
    expect("  that knob down to POLY: the hint is gone, the MODE card leaves (it shows only where the chord needs POLY), STRUM takes the place", t->p[P_VOICE] == V_POLY && !line_has("NEEDS POLY") && card_hidden(3) && card_match(2, "STRUM"));
    t->p[P_VOICE] = V_MONO;
    t->p[P_CHRD] = CH_OFF;
    run(100);
    shot("7_part2_type_off");
    expect("TYPE OFF: the old hint only (no NEEDS POLY, no SET A SCALE, no MODE card) in any voice mode", card_hidden(2) && line_has("KEYS PLAY CHORDS IN THE SCALE") && !line_has("NEEDS POLY") && !line_has("SET A SCALE"));
    t->p[P_VOICE] = V_UNISON;
    t->p[P_CHRD] = CH_MIN7;
    chord_last[1].n = 4;
    chord_last[1].note[0] = 60; chord_last[1].note[1] = 63; chord_last[1].note[2] = 67; chord_last[1].note[3] = 70;
    run(100);
    expect("UNISON with a chord on: NEEDS POLY, and no note names of a chord that was not played", line_has("NEEDS POLY") && !line_has("C D# G A#"));
    t->p[P_VOICE] = V_POLY;
    run(100);
    expect("  back in POLY the last chord's names are there (C D# G A#)", line_has("C D# G A#") && !line_has("NEEDS POLY"));
    t->p[P_VOICE] = V_LEGATO;
    t->p[P_CHRD] = CH_OFF;
    song.sel = 0;
}

/* the VOICING card beside STRUM (POLY, no arp: two narrow cards) shows all five voicings whole */
static void voicings(void)
{
    static const char *const SHOWN[5] = {"CLOSE", "OPEN", "1ST INV", "2ND INV", "BASS NOTE"};
    uint32_t v, ok = 1;
    first_start();
    trk[0].p[P_AMODE] = 0;
    XV(&trk[0], X_STRUM) = 0;
    trk[0].p[P_CHRD] = CH_DIA7;
    go_page(GR_CHORD);
    for (v = 0; v < 5u; v++) {
        trk[0].p[P_VOIC] = (int16_t)v;
        run(100);
        if (!card_match(2, "STRUM") || !card_match(1, "VOICING") || !card_has(SHOWN[v])) {
            printf("  voicing %u: the VOICING card does not say %s\n", (unsigned)v, SHOWN[v]);
            ok = 0;
        }
    }
    expect("the VOICING card beside STRUM shows CLOSE, OPEN, 1ST INV, 2ND INV, BASS NOTE (+OCT) whole (the text drawn is the whole name: a cut word would show short)", ok);
    trk[0].p[P_VOIC] = VC_INV1;
    run(100);
    expect("  1ST INV and 2ND INV can be told apart (the card said INVERSION for both)", card_match(1, "VOICING|1ST INV"));
    trk[0].p[P_VOIC] = VC_INV2;
    run(100);
    expect("  .. 2ND INV", card_match(1, "VOICING|2ND INV"));
    trk[0].p[P_VOIC] = VC_CLOSE;
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    keep();
    named_chords();
    first();
    hints();
    voicings();
    printf(bad ? "chordkeep: %d FAILED of %d\n" : "chordkeep: all %d passed\n", bad ? bad : ncheck, ncheck);
    return bad ? 1 : 0;
}
