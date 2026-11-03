/* M3: JOURNEY's destination memory (tide_ui.c jh_*): the last four targets of a part are avoided where the group has others,
 * relaxed oldest first where it has not; RAM only; cleared when a session starts and on HOME. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

#define STEPS 48u

/* part 0 on the first sound of group (e, cat) as its Original; returns the group's size */
static uint32_t setup(uint32_t e, uint32_t cat)
{
    uint32_t j, n = 0, first = 0;
    for (j = 0; j < ds_count(e); j++)
        if (ds_cat_of(e, j) == cat) {
            if (!n++)
                first = j;
        }
    ui_power_on(); frame();
    song.sel = 0; morph.target = MT_SELECTED;
    set_engine_of(&trk[0], e);
    apply_preset_to(&trk[0], first);
    frame();
    jh_clear(0); jh_clear(1);
    return n;
}

/* STEPS arrivals of a JOURNEY on part 0, alternating the end; the sound each reached. hist 0: the memory emptied before every pick
 * (the code as it was). Returns the steps run before something failed (STEPS: all) */
static uint32_t walk(uint32_t seed, int hist, int *seq, uint32_t *same_as_current)
{
    uint32_t i, x = 1u;
    morph.gen = seed; fm1_ms = 0;
    for (i = 0; i < STEPS; i++, x ^= 1u) {
        int32_t cur = morph_to_of(0, x ^ 1u);
        if (!hist)
            jh_clear(0);
        tide_journey(1u, x, 1u, 0u);
        seq[i] = morph_to_of(0, x);
        if (seq[i] < 0)
            return i;
        if (seq[i] == cur)
            ++*same_as_current;
    }
    return STEPS;
}
static uint32_t revisits(const int *seq)   /* a pick equal to one of the 4 before it */
{
    uint32_t i, j, r = 0;
    for (i = 1; i < STEPS; i++)
        for (j = i > 4u ? i - 4u : 0u; j < i; j++)
            if (seq[j] == seq[i]) {
                r++;
                break;
            }
    return r;
}

int main(void)
{
    uint32_t e, cat, groups = 0, big = 0, small = 0, stalls = 0, cur = 0, rv_h = 0, rv_n = 0, big_rv = 0, picks = 0;
    uint32_t seen[NENGINES][32] = {{0}}, size_hist[8] = {0};
    int a[STEPS], b[STEPS], c[STEPS];
    uint32_t one_same = 0, one_n = 0, det = 1;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e) || ds_count(e) < 2u)
            continue;
        for (cat = 0; cat < 32u; cat++) {
            uint32_t n, i, ra, rb, dist;
            uint32_t j;
            for (n = j = 0; j < ds_count(e); j++)
                n += ds_cat_of(e, j) == cat;
            if (n < 1u || seen[e][cat]++)
                continue;
            groups++;
            
            setup(e, cat);
            stalls += walk(0x1234u + e * 32u + cat, 1, a, &cur) != STEPS;
            setup(e, cat);
            walk(0x1234u + e * 32u + cat, 1, c, &cur);
            for (i = 0; i < STEPS; i++)
                det &= a[i] == c[i];
            setup(e, cat);
            stalls += walk(0x1234u + e * 32u + cat, 0, b, &cur) != STEPS;
            for (i = dist = 0; i < 2u * STEPS; i++) {   /* the sounds the walks really reached: the eligible ones (the peer part holds some) */
                int v = i < STEPS ? a[i] : b[i - STEPS];
                uint32_t q;
                for (q = 0; q < i && (q < STEPS ? a[q] : b[q - STEPS]) != v; q++)
                    ;
                dist += q == i;
            }
            size_hist[n > 7u ? 7u : n]++;
            ra = revisits(a); rb = revisits(b);
            if (getenv("JM_DUMP")) {
                printf("e%u c%u n%u rv %u/%u:", e, cat, n, ra, rb);
                for (i = 0; i < 16u; i++)
                    printf(" %d", a[i]);
                printf("\n");
            }
            rv_h += ra; rv_n += rb;
            picks += STEPS - 1u;
            if (n >= 5u && dist >= 5u) {
                big++;
                big_rv += ra;
            } else
                small++;
            if (n == 2u) {                        /* one candidate: the same picks with and without the memory */
                one_n++;
                for (i = 0, j = 1; i < STEPS; i++)
                    j &= a[i] == b[i];
                one_same += j;
            }
        }
    }
    printf("groups %u (5+ eligible sounds: %u, fewer: %u); category sizes 1:%u 2:%u 3:%u 4:%u 5:%u 6:%u 7+:%u\n", groups, big, small,
           size_hist[1], size_hist[2], size_hist[3], size_hist[4], size_hist[5], size_hist[6], size_hist[7]);
    printf("revisits within 4 picks over %u picks: without the memory %u, with it %u (groups of 5+: %u)\n",
           picks, rv_n, rv_h, big_rv);
    CHECK("jrny memory: every group walks STEPS arrivals without a stall or a roll", !stalls);
    CHECK("jrny memory: seeded runs repeat", det);
    CHECK("jrny memory: groups of five or more never revisit within four picks", big && !big_rv);
    CHECK("jrny memory: fewer revisits than before overall", rv_h < rv_n);
    CHECK("jrny memory: a group of two (one candidate) picks exactly as before", one_n && one_same == one_n);
    CHECK("jrny memory: never the sound just reached", !cur);

    /* a session start clears it, and so does HOME */
    for (e = 0; e < NENGINES && !(eng_ok(e) && ds_count(e) > 6u); e++)
        ;
    setup(e, ds_cat_of(e, 0));
    tide_journey(1u, 1u, 1u, 0u);
    CHECK("jrny memory: a pick is remembered", jh[0][0] != 0 && jh_eng[0] == e);
    gl.mode = GM_JOURNEY; gl.on = gl.armed = 0; gl.target = GT_SEQ;
    glide_start(0u, 1u, 0u);
    CHECK("jrny memory: a new session starts it empty", !jh[0][0] && !jh[0][3]);
    gl.on = gl.armed = 0;
    tide_journey(1u, 1u, 1u, 0u);
    tide_home(1u);
    CHECK("jrny memory: HOME clears it", !jh[0][0]);
    printf("%u checks, %u failed\n", checks, failures);
    return failures != 0;
}
