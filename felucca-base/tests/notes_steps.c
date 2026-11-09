/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE 1.0.0-b6 N1 (firmware/src/budget.h DUELL_NOTES_STEP): the notes per part of the 16 factory multis at one step of the NOTES limit, the table of
 * work/measure/b6-notes/UNIT_PLAN.md and CHANGELOG-TIDE. Report only, not a test group: build it once per step (-DDUELL_NOTES_STEP=72 / 76 / 80 / 84) and compare.
 * The factory multis load as MULTI LOAD gives them (MODE RECALL on; tests/budget_test.c's slot 16 is its STRESS TEST scene instead); two states, as budget_test's table: as loaded, and after 40 halves with MORPH on End B and the
 * fader in the middle (the state the unit's GLIDE sweep plays in: the morph's extras cost each voice a little). Both rules: the two-core numbers (core 1 up, how the
 * unit plays) and the one-core ones (core 1 down or its jobs paused). One line a multi:  M01 name mode | loaded 2-core p1 p2  1-core p1 p2 | MORPH 2-core p1 p2  1-core p1 p2 */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#define _FORTIFY_SOURCE 0
#define HOST_IRQ_HOOK 1
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static void power(void)
{
    uint32_t k;
    memset(nor, 0xFF, sizeof nor);
    ui_power_on();
    for (k = 0; k < NTRK; k++)
        memset(fm6_patch[k], 0, sizeof fm6_patch[k]);
    flash_ok = 1;
    multi_boot();
    frame();
}
static void load_multi(uint32_t i)                      /* (the factory multi of slot i + 1, as MULTI LOAD gives it; tests/budget_test.c puts its STRESS TEST scene in slot 16 instead) */
{
    power();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    project_load(i);
    frame();
}
typedef struct { uint32_t n[2][2][2]; uint32_t mode; char name[PROJ_NAME_LEN + 1u]; } row_t;   /* [state][rule: 0 one core, 1 two cores][part] */
static void get(row_t *r, uint32_t st)
{
    bd_t b;
    uint32_t two;
    bd_build(&b);
    for (two = 0; two < 2u; two++)
        budget_solve2(&b, r->n[st][two], two);
}
static void child(uint32_t i, row_t *r)
{
    int32_t o[2 * CTL];
    uint32_t b;
    load_multi(i);
    r->mode = duell_mode_req;
    memcpy(r->name, mu_names[i], sizeof r->name);
    r->name[PROJ_NAME_LEN] = 0;
    get(r, 0);
    for (b = 0; b < 40u * HALF_FRAMES / CTL; b++) {
        mix_block(o, CTL);
        while (wave_service())
            ;
    }
    SEL_BOTH = 1;
    morph_set_fader(64);
    get(r, 1);
}
int main(void)
{
    static const char *const MN[3] = {"DUEL", "STEREO", "LAYER"};
    uint32_t i;
    printf("notes_steps: DUELL_NOTES_STEP %u (BUDGET_LIMIT %u, BUDGET_PEAK %u)\n", (unsigned)DUELL_NOTES_STEP, (unsigned)BUDGET_LIMIT, (unsigned)BUDGET_PEAK);
    printf("notes_steps: no  name             mode   | loaded: 2-core p1 p2  1-core p1 p2 | MORPH: 2-core p1 p2  1-core p1 p2\n");
    for (i = 0; i < 16u; i++) {
        int fd[2], st = 0;
        row_t r;
        pid_t pid;
        memset(&r, 0, sizeof r);
        if (pipe(fd))
            return 1;
        fflush(stdout);
        pid = fork();
        if (!pid) {
            row_t x;
            memset(&x, 0, sizeof x);
            child(i, &x);
            _exit(write(fd[1], &x, sizeof x) == (ssize_t)sizeof x ? 0 : 1);
        }
        close(fd[1]);
        if (read(fd[0], &r, sizeof r) != (ssize_t)sizeof r)
            return 1;
        close(fd[0]);
        waitpid(pid, &st, 0);
        printf("notes_steps: M%02u %-16s %-6s |          %2u %2u          %2u %2u |         %2u %2u          %2u %2u\n", i + 1u, r.name, MN[r.mode % 3u],
               r.n[0][1][0], r.n[0][1][1], r.n[0][0][0], r.n[0][0][1], r.n[1][1][0], r.n[1][1][1], r.n[1][0][0], r.n[1][0][1]);
    }
    return 0;
}
