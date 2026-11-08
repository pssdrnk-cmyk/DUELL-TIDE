/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE K4 (1.0.0-b6; firmware/src/twocore_run.c tcb_*, docs/TWOCORE_STAGE1.md section 9): the 16 factory multis rendered on two cores with the reverb job, bit for bit
 * against the one-core render. Each multi in a child process of its own (the firmware keeps generator states a load does not reset), the way tests/budget_test.c renders them:
 * MODE RECALL on, loaded, 40 halves to settle, MORPH target B and the fader in the middle, four notes held on each part, 160 halves of which the last 80 are hashed.
 *   one core   core 1 not started (the record says why: no start armed): the firmware's one-core path
 *   two cores  core 1 a host thread running the real tc_c1_step loop with the real job (part 2 and the reverb), the computer hal (tests/twocore_hal_host.h). The clock
 *              stands still within a half (the harness sets it at each half's start), so every job is in time: this is the data separation and the arithmetic, not the timing
 *              (tests/twocore_sim.c has the late, very late, silent, refused and hung cases)
 * Checks: the two hashes are equal for every multi; in STEREO and LAYER the reverb went to core 1 (posted > 0, every posted job in time, nothing late, silent or refused) and
 * part 2 too; in DUEL (bus B on core 0) no reverb job at all; core 1 is up at the end, no strike. Built and run by host_tests.sh (name: k4multis) */
#define DUELL_TWOCORE 1
#define TC_AUTO_ARM 0
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
#include <pthread.h>
#include <sched.h>
#include "memfast.h"
void *memset(void *d, int c, size_t n) { return mf_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, size_t n) { return mf_memcpy(d, s, (unsigned)n); }
static void fm1_irq_off(void);
static void fm1_irq_on(void);
#include "ui_test.c"
#include "scenes.h"
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { const uint8_t *s = src; uint32_t i; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

#if !TCB_ON
#error "k4_multis_test: K4 is off in this build (DUELL_TIDE=0 or DUELL_K4=0)"
#endif

static int fails;
static void kchk(const char *what, int ok)
{
    printf("k4multis: %-4s %s\n", ok ? "ok" : "FAIL", what);
    fails += !ok;
}
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
static void load_multi(uint32_t i)
{
    power();
    ui_prefs2 |= PREF_MODE_RECALL >> 16;
    if (i == 15u)
        scene_load(15, "M16_STRESS_TEST");              /* (as budget_test.c: slot 16 of the measurements is the STRESS TEST scene) */
    else
        project_load(i);
    frame();
}

/* core 1: the real waiting loop until the run ends */
static volatile uint32_t quit, c1_ready;
static void *core1(void *arg)
{
    tc_c1_t c;
    (void)arg;
    tc_host_is_c1 = 1;
    tc_c1_init(&c);
    __atomic_store_n(&c1_ready, 1u, __ATOMIC_RELEASE);
    while (__atomic_load_n(&tc_host_live, __ATOMIC_ACQUIRE) && !__atomic_load_n(&quit, __ATOMIC_ACQUIRE))
        tc_c1_step(&c);
    return 0;
}
static pthread_t thr;
static int thr_on;
static void start_core1(void)
{
    thr_on = 1;
    pthread_create(&thr, 0, core1, 0);
}

/* a block as audio.c gives it: at a half's start the main loop's pass (tc_watch, tc_trim_poll) and the half's clock */
static uint32_t nb;
static void blk(int32_t *o)
{
    if (nb % TCR_BLOCKS == 0u) {
        __atomic_store_n(&tc_host_ticks, 24000u + nb / TCR_BLOCKS * TCR_HALF_TICKS, __ATOMIC_RELAXED);
        tc_watch(__atomic_load_n(&tc_host_ticks, __ATOMIC_RELAXED) / (TC_TICKS_PER_US * 1000u));
        tc_trim_poll();
        tc_half_begin(__atomic_load_n(&tc_host_ticks, __ATOMIC_RELAXED));
    }
    mix_block(o, CTL);
    nb++;
    while (wave_service())
        ;
}

typedef struct { uint64_t h; uint32_t mode, up, failed, posted, in_time, late, vlate, silent, refused, nofit, p2_in, p2_blocks, c1_jobs; } res_t;
static const uint8_t N8[4] = {48, 52, 55, 59};
static void child(uint32_t i, int two, res_t *r)
{
    uint64_t h = 1469598103934665603ull;
    int32_t o[2 * CTL];
    uint32_t b, p, k;
    __atomic_store_n(&tc_host_tick_div, 0u, __ATOMIC_RELAXED);   /* (the clock stands still but for the half starts) */
    __atomic_store_n(&tc_host_ticks, 24000u, __ATOMIC_RELAXED);
    load_multi(i);                                      /* (the load before the start: power() clears the session, not the core) */
    if (two) {
        tc_host_c1_started = start_core1;
        tc_arm();
    }
    tc_boot(1u, 0, 0);
    while (thr_on && !__atomic_load_n(&c1_ready, __ATOMIC_ACQUIRE))
        sched_yield();                                  /* (core 1 in its loop before the first job: it reads the job word as its start) */
    song.master_q12 = 2048;
    for (b = 0; b < 40u * HALF_FRAMES / CTL; b++)
        blk(o);
    morph_set_target(1);
    morph_set_fader(64);
    bnotes[0] = bnotes[1] = 0;
    for (p = 0; p < 2u; p++)
        for (k = 0; k < 4u; k++)
            trk_note_on(&trk[p], (uint8_t)(N8[k] + 7u * p), 110);
    for (b = 0; b < 160u * HALF_FRAMES / CTL; b++) {
        blk(o);
        if (b >= 80u * HALF_FRAMES / CTL)
            for (k = 0; k < 2u * CTL; k++)
                h = (h ^ (uint32_t)o[k]) * 1099511628211ull;
    }
    __atomic_store_n(&quit, 1u, __ATOMIC_RELEASE);
    if (thr_on)
        pthread_join(thr, 0);
    r->h = h, r->mode = dm_split ? DM_SPLIT : duell_mode_req, r->up = tc.up, r->failed = tc.failed;
    r->posted = tcbs.posted, r->in_time = tcbs.in_time, r->late = tcbs.late, r->vlate = tcbs.vlate, r->silent = tcbs.silent, r->refused = tcbs.refused, r->nofit = tcbs.nofit;
    r->p2_in = tcs.in_time, r->p2_blocks = tcs.blocks, r->c1_jobs = tc.job;
}
static int run(uint32_t i, int two, res_t *r)
{
    int fd[2], st = 0;
    pid_t pid;
    memset(r, 0, sizeof *r);
    if (pipe(fd))
        return 0;
    fflush(stdout);
    pid = fork();
    if (!pid) {
        res_t x;
        memset(&x, 0, sizeof x);
        child(i, two, &x);
        _exit(write(fd[1], &x, sizeof x) == (ssize_t)sizeof x ? 0 : 1);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof *r) != (ssize_t)sizeof *r)
        r->h = 0;
    close(fd[0]);
    waitpid(pid, &st, 0);
    return WIFEXITED(st) && WEXITSTATUS(st) == 0 && r->h != 0;
}

int main(void)
{
    uint32_t i, eq = 0, k4 = 0, duel = 0;
    char what[400];
    for (i = 0; i < 16u; i++) {
        res_t a, b;
        int ok = run(i, 0, &a) && run(i, 1, &b);
        snprintf(what, sizeof what, "multi %2u (%s): two cores with the reverb job == one core (%016llx / %016llx); reverb jobs %u in time %u late %u very late %u silent %u refused %u no fit %u; part 2 on core 1 in %u of %u blocks; up %u",
                 i + 1u, a.mode == DM_SPLIT ? "DUEL" : a.mode == DM_LAYER ? "LAYER" : "STEREO", (unsigned long long)a.h, (unsigned long long)b.h, b.posted, b.in_time, b.late, b.vlate, b.silent, b.refused, b.nofit, b.p2_in, b.p2_blocks, b.up);
        kchk(what, ok && a.h == b.h && b.up && !b.failed && !a.posted);
        eq += ok && a.h == b.h;
        if (b.mode == DM_SPLIT) {
            duel++;
            snprintf(what, sizeof what, "multi %2u (DUEL): no reverb job (bus B and bus A stay on core 0)", i + 1u);
            kchk(what, b.posted == 0);
        } else {
            k4 += b.posted > 0;
            snprintf(what, sizeof what, "multi %2u: the reverb went to core 1 and every job came back in time (posted %u, in time %u), part 2 too (%u of %u blocks)", i + 1u, b.posted, b.in_time, b.p2_in, b.p2_blocks);
            kchk(what, b.posted > 0 && b.in_time == b.posted && !b.late && !b.vlate && !b.silent && !b.refused && b.p2_in > b.p2_blocks / 2u);
        }
    }
    printf("k4multis: %u of 16 multis bit for bit; %u STEREO / LAYER multis with the reverb on core 1, %u DUEL\n", eq, k4, duel);
    printf("k4multis: %d FAIL\n", fails);
    return fails != 0;
}
