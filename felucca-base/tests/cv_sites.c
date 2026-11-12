/* scratch (b10 ui-c): which cv_begin sites draw a picture taller than a band without CV_BANDS, over every page / layer / menu */
#define UI_TEST_NO_MAIN 1
#define CV_SITE_TRACE 1
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static struct { char f[40]; int line; unsigned h; unsigned long n; int band; } sites[256];
static int nsites, in_band;
static void cv_site_note(const char *file, int line, unsigned h, int inb)
{
    int i; if (inb) return; const char *b = strrchr(file, '/') ? strrchr(file, '/') + 1 : file; const char *b2 = strrchr(b, 92); if (b2) b = b2 + 1;
    for (i = 0; i < nsites; i++) if (sites[i].line == line && !strcmp(sites[i].f, b)) { sites[i].n++; if (h > sites[i].h) sites[i].h = h; return; }
    if (nsites < 256) { strncpy(sites[nsites].f, b, 39); sites[nsites].line = line; sites[nsites].h = h; sites[nsites].n = 1; nsites++; }
}
#include "ui_test.c"
static const char *const FAMN[] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "TRK", "?"};
int main(void)
{
    uint32_t e, i, l, g;
    static const struct { uint8_t b; } LY[] = {{B_GLO}, {B_SCL}, {B_EDIT}, {B_LFO}, {B_ENV}, {B_ARP}};
    for (g = 0; g < 3u; g++)
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        if (g && e > 2u) continue;
        for (i = 0; i < NPAGES; i++) {
            ui_power_on(); song.sel = (uint8_t)(g & 1u); set_engine_of(TSEL, e); go_main_to(HP_MORPH); frames(64);
            if (!page_visible(i)) continue;
            ui.page = (uint8_t)i; page_entered(); frames(160);
            ui.force = 1; frame();
        }
        for (l = 0; l < NELEM(LY); l++) {
            ui_power_on(); song.sel = 0; set_engine_of(TSEL, e); go_main_to(HP_MORPH); frames(64);
            btn_down(LY[l].b); frames(480); btn_up(LY[l].b); frames(64);
        }
        if (e == 0) { ui_power_on(); go_main_to(HP_MORPH); frames(64); ui.menu = 1; ui.force = 1; frames(160); ui.menu = 0; }
    }
    for (i = 0; i < (uint32_t)nsites; i++) if (sites[i].h > 68u) printf("%-14s:%-5d tallest %3u  calls %lu\n", sites[i].f, sites[i].line, sites[i].h, sites[i].n);
    return 0;
}
