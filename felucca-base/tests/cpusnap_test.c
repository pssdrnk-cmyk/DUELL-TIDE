/* SPDX-License-Identifier: GPL-3.0-only */
/* K2 (CPU audit): the CPU meter's one owner. ui_menu.c cpu_snap_poll alone reads and clears the audio ISR's peaks (cpu_pk, cpu_el_pk) and folds them into
 * every reader's own window (cpu_win_take: the header's, the console's, the bench's, TEST / ABOUT's), so no reader can take another's peak away.
 *   SAME     two readers see the same peak for the same ISR peak (the first one's take leaves the second its own)
 *   OWNER    after a poll the ISR's counters are 0 (cleared by the owner only); the snapshot (seq, sum, n, pk, el_pk, miss) is filled together and changes only with a poll
 *   LOST     a peak the ISR publishes between a reader's take and the next is in the next take, and a second peak in the same poll is not dropped; the peak
 *            published at the moment the interrupts go off (host_irq_hook) is in that very poll
 *   WINDOWS  a window keeps the longest of the polls since ITS last take and starts again after it
 *   METER    TEST / ABOUT's PEAK is the elapsed peak, the average the render's
 *   HEAD     TIDE: the header's elapsed peak and miss count (cpu_head): held 4 s, the miss count from the last clear (TEST's RESET: cpum.base), REC red once a period was missed
 * Built and run by DUELL/host_tests.sh (name: cpusnap): the UI sources against ui_test.c's stubs. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, ncheck;
static int expect(const char *what, int ok)
{
    ncheck++;
    if (!ok) {
        printf("cpusnap: FAIL %s\n", what);
        bad++;
    }
    return ok;
}
static uint32_t late_el;                               /* the peak the "ISR" publishes while the interrupts go off */
static void isr_hook(int on)
{
    if (!on && late_el) {
        if (late_el > cpu_el_pk)
            cpu_el_pk = late_el;
        late_el = 0;
    }
}
int main(void)
{
    uint32_t pk, el, pk2, el2, seq;
    cpu_pk = cpu_el_pk = cpu_sum = cpu_n = cpu_miss = 0;
    memset(&cpu_snap, 0, sizeof cpu_snap);
    memset(cpu_wpk, 0, sizeof cpu_wpk);
    memset(cpu_wel, 0, sizeof cpu_wel);
    /* SAME */
    cpu_pk = 2100u; cpu_el_pk = 2500u; cpu_n = 40u; cpu_sum = 40u * 1500u; cpu_miss = 3u;
    cpu_win_take(CPU_W_CON, &pk, &el);
    cpu_win_take(CPU_W_HEAD, &pk2, &el2);
    expect("same: the console's take sees the ISR's peaks (2100 render, 2500 elapsed)", pk == 2100u && el == 2500u);
    expect("same: the header's take sees the same peaks after the console's", pk2 == 2100u && el2 == 2500u);
    cpu_win_take(CPU_W_BENCH, &pk2, &el2);
    expect("same: so does a third reader", pk2 == 2100u && el2 == 2500u);
    /* OWNER */
    expect("owner: the ISR's peaks are cleared by the poll", cpu_pk == 0u && cpu_el_pk == 0u);
    expect("owner: the snapshot has the counters and the misses", cpu_snap.n == 40u && cpu_snap.sum == 60000u && cpu_snap.miss == 3u);
    seq = cpu_snap.seq;
    cpu_pk = 700u; cpu_el_pk = 900u;
    expect("owner: the snapshot does not move without a poll", cpu_snap.seq == seq && cpu_pk == 700u);
    cpu_snap_poll();
    expect("owner: a poll bumps seq, fills pk and el_pk together and clears the ISR's", cpu_snap.seq == seq + 1u && cpu_snap.pk == 700u && cpu_snap.el_pk == 900u && cpu_pk == 0u && cpu_el_pk == 0u);
    cpu_win_take(CPU_W_CON, &pk, &el);
    cpu_win_take(CPU_W_HEAD, &pk2, &el2);
    expect("owner: both readers still get the 700 / 900 a poll consumed for them", pk == 700u && el == 900u && pk2 == 700u && el2 == 900u);
    cpu_win_take(CPU_W_CON, &pk, &el);
    expect("owner: and a second take of the same window is empty", pk == 0u && el == 0u);
    /* LOST */
    cpu_win_take(CPU_W_MENU, 0, 0);
    cpu_win_take(CPU_W_HEAD, 0, 0);
    cpu_pk = 300u; cpu_el_pk = 400u;
    cpu_win_take(CPU_W_HEAD, &pk, &el);                 /* (another reader's take polls: it must not take the console's) */
    cpu_pk = 200u; cpu_el_pk = 250u;                    /* (a smaller one after it) */
    cpu_win_take(CPU_W_CON, &pk2, &el2);
    expect("lost: the head's take sees 400; the console's, after it, still sees the 400, the later 250 does not lower it", el == 400u && pk == 300u && el2 == 400u && pk2 == 300u);
    host_irq_hook = isr_hook;
    late_el = 3100u;
    cpu_win_take(CPU_W_HEAD, &pk, &el);
    expect("lost: a peak published as the interrupts go off is in that poll, not lost", el == 3100u && cpu_el_pk == 0u);
    host_irq_hook = 0;
    cpu_win_take(CPU_W_CON, &pk, &el);
    expect("lost: and in the console's window too", el == 3100u);
    /* WINDOWS */
    cpu_win_take(CPU_W_MENU, 0, 0); cpu_win_take(CPU_W_HEAD, 0, 0);
    cpu_pk = 500u; cpu_el_pk = 600u; cpu_snap_poll();
    cpu_pk = 800u; cpu_el_pk = 900u; cpu_snap_poll();
    cpu_pk = 100u; cpu_el_pk = 150u;
    cpu_win_take(CPU_W_HEAD, &pk, &el);
    expect("windows: the longest of three polls", pk == 800u && el == 900u);
    cpu_pk = 50u; cpu_el_pk = 60u;
    cpu_win_take(CPU_W_HEAD, &pk, &el);
    cpu_win_take(CPU_W_MENU, &pk2, &el2);
    expect("windows: the head starts again after its take, TEST / ABOUT's window kept the whole 900", pk == 50u && el == 60u && pk2 == 800u && el2 == 900u);
    /* METER */
    cpu_n = cpu_sum = cpu_pk = cpu_el_pk = cpu_miss = 0;
    memset(&cpu_snap, 0, sizeof cpu_snap);
    cpu_meter_open();
    fm1_ms += CPU_WIN_MS;
    cpu_n += 344u; cpu_sum += 344u * (CPU_BUDGET_US / 2u); cpu_pk = CPU_BUDGET_US * 40u / 100u; cpu_el_pk = CPU_BUDGET_US * 60u / 100u;
    cpu_meter_poll();
    expect("meter: the peak is the ELAPSED 60 %, the average the render's 50 %", cpum.peak >= 59u && cpum.peak <= 60u && cpum.avg >= 49u && cpum.avg <= 50u);
#if DUELL_TIDE
    {   /* HEAD: cpu_head reads (the window of the last 4 s, the count since the clear) and gives the two texts and the colour */
        uint32_t g;
        char l[8], r[8];
        uint16_t c;
        cpu_n = cpu_sum = cpu_pk = cpu_el_pk = cpu_miss = 0;
        memset(&cpu_snap, 0, sizeof cpu_snap);
        memset(&cpuh, 0, sizeof cpuh);
        ux.dim = 0x2104u; ux.rec = 0xF800u;              /* (distinct colours: the stub's are all 0) */
        cpum.base = 0;
        cpu_meter_open();
        fm1_ms += 10000u;
        cpu_el_pk = CPU_BUDGET_US * 92u / 100u + 5u;
        g = cpu_head(l, r, &c);
        expect("head: the first look has no history: it starts the window (0, 0)", g && !strcmp(l, "0") && !strcmp(r, "0") && c == T_DIM);
        fm1_ms += CPU_HOLD_MS;
        cpu_el_pk = CPU_BUDGET_US * 92u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: a window later the longest elapsed half shows: 92 %, no miss", !strcmp(l, "92") && !strcmp(r, "0"));
        fm1_ms += CPU_WIN_MS;
        cpu_el_pk = CPU_BUDGET_US * 40u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: it stays for the 4 s window (read every 4 s, not every frame)", !strcmp(l, "92"));
        fm1_ms += CPU_HOLD_MS;
        cpu_el_pk = CPU_BUDGET_US * 40u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: and goes to the next window's: 40 % (the 92 % was taken)", !strcmp(l, "40"));
        cpu_miss += 2u; cpu_el_pk = CPU_BUDGET_US * 130u / 100u + 5u;
        fm1_ms += CPU_HOLD_MS;
        g = cpu_head(l, r, &c);
        expect("head: 2 periods missed: the count 2 and the 130 peak; what a redraw depends on moved", !strcmp(r, "2") && !strcmp(l, "130") && c == T_REC && g != 0u);
        fm1_ms += 60u * CPU_WIN_MS;
        cpu_head(l, r, &c);
        expect("head: a minute later the count stays until the clear; the old peak is gone", !strcmp(r, "2") && !strcmp(l, "0") && c == T_REC);
        cpum.base = cpu_snap.miss;                      /* TEST's RESET */
        cpu_head(l, r, &c);
        expect("head: after TEST > RESET the count is 0", !strcmp(r, "0") && c == T_DIM);
    }
#endif
    printf("cpusnap: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
