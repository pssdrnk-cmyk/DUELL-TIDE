/* SPDX-License-Identifier: GPL-3.0-only */
/* FM6 live operator level (fm6_core.c fm6_live_level): an OL the morph moves under a sounding note is a gain over the
 * note-on envelope. From JAM's jam_ready_test.c test_fm (branch jam, 71654ba), split out for the 1.1 port. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("fm6liveol: FAIL %s\n", s); bad++; } } while (0)
static void fm_compute(fm6_note_t *n, const uint8_t *p, int32_t *out)
{
    int32_t dt[6] = {0};
    memset(out, 0, 3 * FM6_N * sizeof *out);
    fm6_note_compute(n, p, out, 0, 0, fm6_note_logfreq(60 * 16, 0), 31, 0, dt, 0, 0);
}
int main(void)
{
    static const uint32_t notes[] = {24, 60, 108}, velocities[] = {1, 32, 127};
    _Static_assert(sizeof(fm6_env_t) == 24u, "the key and velocity reuse the envelope's padding");
    ui_power_on();
    frame();
    for (uint32_t a = 0; a < 3; a++)
        for (uint32_t b = 0; b < 3; b++) {
            uint8_t p[FP_SIZE];
            fm6_note_t normal, walked;
            int32_t x[3 * FM6_N], y[3 * FM6_N];
            int restored = 1, intact = 1;
            memcpy(p, fm6_patch[0], sizeof p);
            p[FP_ALG] = 31;
            for (uint32_t k = 0; k < 6; k++) {
                uint8_t *o = p + k * FP_OP;
                o[FP_R1] = o[FP_R1 + 1] = o[FP_R1 + 2] = 99;
                o[FP_L1] = o[FP_L1 + 1] = o[FP_L1 + 2] = 99;
                o[FP_L1 + 3] = 0;
                o[FP_OL] = 99;
                o[FP_RS] = 0;
                o[FP_KVS] = 7;
            }
            fm6_note_init(&normal, p, notes[a], velocities[b], 1);
            walked = normal;
            for (uint32_t i = 0; i < 100; i++) {         /* operator 1's OL toggles 99 / 0 every five blocks */
                p[FP_OL] = 99;
                fm_compute(&normal, p, x);
                p[FP_OL] = (i % 10 < 5) ? 0 : 99;
                fm_compute(&walked, p, y);
                intact &= !memcmp(normal.env, walked.env, sizeof normal.env);
                if (i % 10 >= 7)
                    restored &= !memcmp(x, y, FM6_N * sizeof *x);
            }
            CHECK("live OL does not mutate EG attenuation", intact);
            CHECK("OL returns to the same audio after repeated mute/unmute", restored);
            p[FP_OL] = 0;
            fm6_note_init(&walked, p, notes[a], velocities[b], 1);
            for (uint32_t i = 0; i < 100; i++)
                fm_compute(&walked, p, y);
            p[FP_OL] = 99;
            for (uint32_t i = 0; i < 4; i++)
                fm_compute(&walked, p, y);
            CHECK("initially silent operator keeps keyboard/velocity scaling", walked.op[0].level_in == normal.op[0].level_in);
            for (uint32_t j = 0; j < 4; j++)
                p[FP_L1 + j] = 0;
            p[FP_OL] = 0;
            fm6_note_init(&walked, p, notes[a], velocities[b], 1);
            p[FP_OL] = 99;
            for (uint32_t i = 0; i < 20; i++)
                fm_compute(&walked, p, y);
            CHECK("an all-zero EG stays inaudible when OL rises", walked.op[0].gain_out < FM6_THRESH);
        }
    printf("fm6liveol: %d checks, %d failed\n", checks, bad);
    return bad != 0;
}
