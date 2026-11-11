/* SPDX-License-Identifier: GPL-3.0-only */
/* SP3 (1.0.0-b9): the factory pattern table keeps only the steps a pattern has (engines.c PATTERNS + PAT_NOTES, pat_note). This test keeps the
 * old table (tests/data/patterns_old.inc, 32 notes a pattern) as a reference and checks every one of the 32 patterns: name, len, div, swing,
 * gate, and for all 32 steps the note (0 past LEN) and the flags (pat_fl). build/host/patpack_test (host_tests.sh patpack) */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "data/patterns_old.inc"

int main(void)
{
    uint32_t k, i, bad = 0, n = sizeof OLD_PATTERNS / sizeof OLD_PATTERNS[0];
    if (n != NPATTERNS) { printf("patpack: FAIL count %u vs %u\n", (unsigned)n, (unsigned)NPATTERNS); return 1; }
    for (k = 0; k < n; k++) {
        const char *a = OLD_PATTERNS[k].name, *b = PATTERNS[k].name;
        uint32_t ok = OLD_PATTERNS[k].len == PATTERNS[k].len && OLD_PATTERNS[k].div == PATTERNS[k].div &&
                      OLD_PATTERNS[k].swing == PATTERNS[k].swing && OLD_PATTERNS[k].gate == PATTERNS[k].gate;
        while (*a && *a == *b) { a++; b++; }
        ok &= *a == *b;
        for (i = 0; i < 32u; i++) {
            uint32_t of = (OLD_PATTERNS[k].fl[0] >> i & 1u) | (OLD_PATTERNS[k].fl[1] >> i & 1u) << 1 | (OLD_PATTERNS[k].fl[2] >> i & 1u) << 2;
            ok &= pat_note(k, i) == OLD_PATTERNS[k].note[i];
            ok &= pat_fl(k, i) == of;
        }
        if (!ok) { printf("patpack: pattern %u %s FAIL\n", (unsigned)k + 1u, OLD_PATTERNS[k].name); bad++; }
    }
    printf("patpack: %u patterns, %s\n", (unsigned)n, bad ? "FAIL" : "all ok");
    return bad != 0;
}
