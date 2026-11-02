/* SPDX-License-Identifier: GPL-3.0-only */
/* Focused host regression for the shared GLO knob-jump transition. */
#define UI_TEST_NO_MAIN
#define UI_TEXT_CAPTURE
#include "ui_test.c"

static uint32_t glo_page(uint32_t scope, uint32_t graph, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (PAGES[i].fam == FAM_GLO && (scope == UINT32_MAX || PAGES[i].scope == scope) &&
            (graph == UINT32_MAX || PAGES[i].graph == graph) &&
            (id == UINT32_MAX || PAGES[i].id[0] == id))
            return i;
    return NPAGES;
}

static uint32_t knob_test_page(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (str_eq(PAGES[i].title, title))
            return i;
    return NPAGES;
}

static void prepare_jump(uint32_t from)
{
    ui_power_on();
    memset(kr_on, 0, sizeof kr_on);
    memset(kr_t, 0, sizeof kr_t);
    ui.page = (uint8_t)from;
    ui.fam_last[FAM_GLO] = (uint8_t)from;
    ui.hot_t = 40;
    ui.entry_open = 1;
    ui.act = 1;
    ui.perf = 1;
    fm1_ms = 1234u;
}

static int transition_ok(uint32_t page, uint32_t knob)
{
    knob &= 3u;
    return ui.page == page && ui.fam_last[FAM_GLO] == page && kr_on[knob] &&
           kr_t[knob] == fm1_ms && !ui.perf && !ui.hot_t && !ui.entry_open && !ui.act;
}

int main(void)
{
    int bad = 0;
    uint32_t from = knob_test_page("SEND FX");
    uint32_t fx = glo_page(SC_GLOBAL, UINT32_MAX, FXSUM_TO[0]);
    uint32_t sidechain = glo_page(UINT32_MAX, GR_SCHAIN, UINT32_MAX);
    uint32_t modules = glo_page(SC_MODS, UINT32_MAX, UINT32_MAX);

    ui_power_on();
    ui.page = (uint8_t)from;
    page_entered();
    turn(EN_K1, 1);
    bad += check("panel K1 from SEND FX commits the mapped GLO page and detent quiet state",
                 ui.page == fx && ui.fam_last[FAM_GLO] == fx && kr_on[0] && kr_t[0] == fm1_ms);
    {
        int16_t chorus = song.g[G_CRATE];
        turn(EN_K1, 1);
        bad += check("the in-flight K1 detent is swallowed on the new page", song.g[G_CRATE] == chorus && kr_on[0]);
        frames(KR_QUIET_MS + 32u);
        turn(EN_K1, 1);
        bad += check("K1 edits the GLO page after the quiet interval", song.g[G_CRATE] != chorus);
    }

    prepare_jump(knob_test_page("DUCK 1/2"));
    schain_jump(3u, 1);
    bad += check("MIDI DUCK setup link commits SIDECHAIN and masks knob index", transition_ok(sidechain, 3u));

    prepare_jump(knob_test_page("FX"));
    off_knob(2u, 1);
    bad += check("OFF-module link commits MODULES and suppresses the initiating knob tail", transition_ok(modules, 2u));

    prepare_jump(from);
    fxsum_knob(1u, 0);
    bad += check("zero-step SEND FX input remains a no-op", ui.page == from && !kr_on[1]);

    printf(bad ? "knob jump: %d FAILED\n" : "knob jump: all passed\n", bad);
    return bad ? 1 : 0;
}
