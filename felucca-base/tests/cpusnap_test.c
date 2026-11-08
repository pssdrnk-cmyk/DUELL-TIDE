/* SPDX-License-Identifier: GPL-3.0-only */
/* K2 (CPU audit): the CPU meter's one owner. ui_menu.c cpu_snap_poll alone reads and clears the audio ISR's peaks (cpu_pk, cpu_el_pk) and folds the elapsed peak into
 * every reader's own window (cpu_win_take: the header's, the console's, the bench's, TEST / ABOUT's), so no reader can take another's peak away.
 *   SAME     two readers see the same peak for the same ISR peak (the first one's take leaves the second its own)
 *   OWNER    after a poll the ISR's counters are 0 (cleared by the owner only); the snapshot (sum, n, miss) changes only with a poll
 *   LOST     a peak the ISR publishes between a reader's take and the next is in the next take, and a second peak in the same poll is not dropped; the peak
 *            published at the moment the interrupts go off (host_irq_hook) is in that very poll
 *   WINDOWS  a window keeps the longest of the polls since ITS last take and starts again after it
 *   METER    TEST / ABOUT's PEAK is the elapsed peak, the average the render's
 *   HEAD     TIDE (and, built with DUELL_TIDE=0, that none of it is there): when the header's readout shows, its window and count, the baseline 8 s after power-on, the
 *            clear by TEST > RESET, a fresh peak after the header was hidden, no overflow; the header's picture follows the readout only on a TIDE page
 * The writers of cpu_pk / cpu_el_pk are checked by host_tests.sh (group cpusnap: only audio.c and cpu_snap_poll write them).
 * Built and run by DUELL/host_tests.sh (name: cpusnap, and again with DUELL_TIDE=0 in tideoff): the UI sources against ui_test.c's stubs. */
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
    uint32_t el, el2;
    cpu_pk = cpu_el_pk = cpu_sum = cpu_n = cpu_miss = 0;
    memset(&cpu_snap, 0, sizeof cpu_snap);
    memset(cpu_wel, 0, sizeof cpu_wel);
    /* SAME */
    cpu_pk = 2100u; cpu_el_pk = 2500u; cpu_n = 40u; cpu_sum = 40u * 1500u; cpu_miss = 3u;
    el = cpu_win_take(CPU_W_CON);
    el2 = cpu_win_take(CPU_W_HEAD);
    expect("same: the console's take sees the ISR's elapsed peak (2500)", el == 2500u);
    expect("same: the header's take sees the same peak after the console's", el2 == 2500u);
    expect("same: so does a third reader", cpu_win_take(CPU_W_BENCH) == 2500u);
    /* OWNER */
    expect("owner: the ISR's peaks are cleared by the poll", cpu_pk == 0u && cpu_el_pk == 0u);
    expect("owner: the snapshot has the counters and the misses", cpu_snap.n == 40u && cpu_snap.sum == 60000u && cpu_snap.miss == 3u);
    cpu_pk = 700u; cpu_el_pk = 900u; cpu_miss = 4u;
    expect("owner: the snapshot does not move without a poll, the ISR's counters are untouched", cpu_snap.miss == 3u && cpu_pk == 700u);
    cpu_snap_poll();
    expect("owner: a poll fills the snapshot and clears the ISR's peaks", cpu_snap.miss == 4u && cpu_pk == 0u && cpu_el_pk == 0u);
    expect("owner: both readers still get the 900 a poll consumed for them", cpu_win_take(CPU_W_CON) == 900u && cpu_win_take(CPU_W_HEAD) == 900u);
    expect("owner: and a second take of the same window is empty", cpu_win_take(CPU_W_CON) == 0u);
    /* LOST */
    (void)cpu_win_take(CPU_W_MENU);
    cpu_el_pk = 400u;
    el = cpu_win_take(CPU_W_HEAD);                       /* (another reader's take polls: it must not take the console's) */
    cpu_el_pk = 250u;                                    /* (a smaller one after it) */
    el2 = cpu_win_take(CPU_W_CON);
    expect("lost: the head's take sees 400; the console's, after it, still sees the 400, the later 250 does not lower it", el == 400u && el2 == 400u);
    host_irq_hook = isr_hook;
    late_el = 3100u;
    el = cpu_win_take(CPU_W_HEAD);
    expect("lost: a peak published as the interrupts go off is in that poll, not lost", el == 3100u && cpu_el_pk == 0u);
    host_irq_hook = 0;
    expect("lost: and in the console's window too", cpu_win_take(CPU_W_CON) == 3100u);
    /* WINDOWS */
    (void)cpu_win_take(CPU_W_MENU); (void)cpu_win_take(CPU_W_HEAD);
    cpu_el_pk = 600u; cpu_snap_poll();
    cpu_el_pk = 900u; cpu_snap_poll();
    cpu_el_pk = 150u;
    expect("windows: the longest of three polls", cpu_win_take(CPU_W_HEAD) == 900u);
    cpu_el_pk = 60u;
    el = cpu_win_take(CPU_W_HEAD);
    el2 = cpu_win_take(CPU_W_MENU);
    expect("windows: the head starts again after its take, TEST / ABOUT's window kept the whole 900", el == 60u && el2 == 900u);
    /* METER */
    cpu_n = cpu_sum = cpu_pk = cpu_el_pk = cpu_miss = 0;
    memset(&cpu_snap, 0, sizeof cpu_snap);
    cpu_meter_open();
    fm1_ms += CPU_WIN_MS;
    cpu_n += 344u; cpu_sum += 344u * (CPU_BUDGET_US / 2u); cpu_pk = CPU_BUDGET_US * 40u / 100u; cpu_el_pk = CPU_BUDGET_US * 60u / 100u;
    cpu_meter_poll();
    expect("meter: the peak is the ELAPSED 60 %, the average the render's 50 %", cpum.peak >= 59u && cpum.peak <= 60u && cpum.avg >= 49u && cpum.avg <= 50u);
#if DUELL_TIDE
    {   /* HEAD */
        char l[8], r[8];
        uint16_t c;
        uint32_t g;
        cpu_n = cpu_sum = cpu_pk = cpu_el_pk = cpu_miss = 0;
        memset(&cpu_snap, 0, sizeof cpu_snap);
        memset(&cpuh, 0, sizeof cpuh);
        cpum.base = 0;
        cpu_meter_open();
        /* where it shows */
        ui_power_on();
        ux.dim = 0x2104u; ux.rec = 0xF800u;              /* (distinct colours: the stub's are all 0) */
        go_title("MORPH");
        expect("head on: MORPH, no message, layer, menu, guide or dialog", cpu_head_on(0, 0));
        expect("head off: a global page", !cpu_head_on(1, 0));
        expect("head off: a message in the header", !cpu_head_on(0, 1));
        ui.menu = 1; expect("head off: the menu", !cpu_head_on(0, 0)); ui.menu = 0;
        ui.guide = 1; expect("head off: the guide", !cpu_head_on(0, 0)); ui.guide = 0;
        ui.confirm = 1; expect("head off: a dialog", !cpu_head_on(0, 0)); ui.confirm = 0;
        ui.layer = 1; expect("head off: a layer", !cpu_head_on(0, 0)); ui.layer = 0;
        go_title("TIDE SETTINGS"); expect("head on: TIDE SETTINGS", cpu_head_on(0, 0));
        go_title("AUTO TIDE"); expect("head on: AUTO TIDE", cpu_head_on(0, 0));
        go_title("ENGINE"); expect("head off: a page that is not HOME's (tide_foot_parts is false)", !cpu_head_on(0, 0));
        /* what is drawn: the header's picture (its signature) follows the readout on a TIDE page and does not on another */
        {
            uint32_t sa, sb;
            go_title("MORPH"); cpuh.t = fm1_ms; cpuh.hold = 10u; ui.force = 1; frame(); sa = ui.head_sig;
            cpuh.t = fm1_ms; cpuh.hold = 20u; ui.force = 1; frame(); sb = ui.head_sig;
            expect("head drawn: the header's picture follows the readout on MORPH", sa != sb);
            go_title("ENGINE");
            cpuh.t = fm1_ms; cpuh.hold = 10u; ui.force = 1; frame(); sa = ui.head_sig;
            cpuh.t = fm1_ms; cpuh.hold = 20u; ui.force = 1; frame(); sb = ui.head_sig;
            expect("head drawn: and does not on a page that is not HOME's", sa == sb);
            go_title("MORPH");
        }
        /* the numbers */
        memset(&cpuh, 0, sizeof cpuh);
        fm1_ms = 10000u;
        cpu_el_pk = CPU_BUDGET_US * 92u / 100u + 5u;
        g = cpu_head(l, r, &c);
        expect("head: the first look has no history: it starts again (0, 0), dim", g && !strcmp(l, "0") && !strcmp(r, "0") && c == T_DIM);
        fm1_ms += CPU_WIN_MS;
        cpu_el_pk = CPU_BUDGET_US * 50u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: a second after a hidden spell it shows a fresh peak (50), not 0", !strcmp(l, "50"));
        fm1_ms += CPU_HOLD_MS;
        cpu_el_pk = CPU_BUDGET_US * 92u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: a window later the longest elapsed half shows: 92, no % sign", !strcmp(l, "92") && !strcmp(r, "0"));
        fm1_ms += CPU_WIN_MS;
        cpu_el_pk = CPU_BUDGET_US * 40u / 100u + 5u;
        cpu_head(l, r, &c);
        expect("head: it stays for the 4 s window", !strcmp(l, "92"));
        fm1_ms += CPU_HOLD_MS;
        cpu_el_pk = 0xFFFFFFF0u;
        cpu_head(l, r, &c);
        expect("head: a huge peak does not overflow: 999", !strcmp(l, "999"));
        /* the baseline */
        memset(&cpuh, 0, sizeof cpuh);
        cpu_miss = 5u; fm1_ms = 3000u;
        cpu_head_arm();
        cpu_miss = 6u; cpu_head(l, r, &c);
        expect("baseline: misses of the start (before 8 s) are not counted: the header says 0", !cpuh.armed && !strcmp(r, "0") && c == T_DIM);
        fm1_ms = 9000u;
        cpu_head_arm();
        expect("baseline: set once the start is over, the raw count (6) is its zero", cpuh.armed && cpuh.base == 6u);
        cpu_miss = 8u; fm1_ms += CPU_HOLD_MS; cpu_snap_poll();
        cpu_head(l, r, &c);
        expect("baseline: a miss later still counts (8 - 6 = 2), red", !strcmp(r, "2") && c == T_REC);
        expect("baseline: ABOUT / TEST keep the raw count (8)", cpu_snap.miss == 8u);
        cpum.base = cpu_snap.miss; cpuh.base = cpu_snap.miss;   /* (TEST > RESET: test_reset) */
        cpu_head(l, r, &c);
        expect("baseline: after TEST > RESET the count is 0 and dim again", !strcmp(r, "0") && c == T_DIM);
    }
#else
    {   /* DUELL_TIDE=0: the plain DUELL header has no readout: the code (cpu_head, cpuh, the draw) is not there, the meter's owner and windows are (above) */
        ui_power_on();
        go_title("MORPH");
        ui.force = 1; frame(); frame();
        expect("plain: the page draws with the meter's polling in place", cpu_snap.n == cpu_n);
    }
#endif
    printf("cpusnap: %d checks, %d failed\n", ncheck, bad);
    return bad ? 1 : 0;
}
