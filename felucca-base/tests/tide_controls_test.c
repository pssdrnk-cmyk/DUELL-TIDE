/* Exercise assignable CCs through the actual map and queued main-loop dispatch. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static unsigned checks, failures;
#define CHECK(label, cond) do { checks++; if (!(cond)) { failures++; printf("FAIL: %s\n", label); } } while (0)

int main(void)
{
    uint32_t q[MQ_N], n;
    char name[40];
    map_info_t info;
    ui_power_on(); frame();
    song.playing = song.rec = 0;
    morph.target = MT_SELECTED;
    gl.mode = GM_ONCE; gl.target = GT_SOUND; gl.on = gl.armed = 0;
    mq_r = mq_w = 0;
    memset(midi_tide_fired, 0, sizeof midi_tide_fired);
    CHECK("new targets append to stock IDs", MAP_T_GLIDE == MAP_T_RES_SMART + 1u && MAP_T_SEQ == MAP_T_GLIDE + 1u);
    CHECK("GLIDE can be assigned to part 2", map_set(80, MAP_T_GLIDE, MAP_R_PART2, MAP_W_TYPED, 1, 0) == MAP_OK);
    CHECK("SEQ can be assigned to part 2", map_set(81, MAP_T_SEQ, MAP_R_PART2, MAP_W_TYPED, 1, 0) == MAP_OK);
    CHECK("GLIDE rejects a global rule", !map_rule_ok(MAP_T_GLIDE, MAP_R_GLOBAL));
    map_name(MAP_LINE(MAP_T_GLIDE, MAP_R_PART2, MAP_W_TYPED), name);
    CHECK("GLIDE has a map label", !strcmp(name, "GLIDE"));
    map_target_info(MAP_T_GLIDE, &trk[1], &info);
    CHECK("GLIDE is advertised as an action", info.kind == MAP_K_ACTION);
    map_target_info(MAP_T_SEQ, &trk[1], &info);
    CHECK("SEQ is advertised as continuous", info.kind == MAP_K_CONTINUOUS);

    midi_map_cc(3, &trk[0], 80, 0);
    midi_map_cc(3, &trk[0], 80, 20);
    n = morph_pop(q);
    CHECK("staying in A queues one launch", n == 1 && (q[0] & 255u) == MQ_GLIDE && (q[0] >> 16) == 0);
    CHECK("the queued part follows the map", n == 1 && ((q[0] >> 8) & 255u) == 1u);
    midi_map_cc(3, &trk[0], 80, 64);
    CHECK("neutral queues nothing", morph_pop(q) == 0);
    midi_map_cc(3, &trk[0], 80, 100);
    morph_poll();
    CHECK("B dispatch starts part 2's glide", gl.to == 1 && gl.mask == 2 && gl.pi == 1 && gl.on);
    midi_map_cc(3, &trk[0], 80, 127);
    CHECK("staying in B queues nothing", morph_pop(q) == 0);
    midi_map_cc(3, &trk[0], 80, 42);
    n = morph_pop(q);
    CHECK("crossing directly back to A retriggers", n == 1 && (q[0] >> 16) == 0);
    midi_map_cc(4, &trk[0], 80, 42);
    CHECK("zone latches are independent per channel", morph_pop(q) == 1);
    midi_map_cc(3, &trk[0], 81, 87);
    midi_map_cc(3, &trk[0], 81, 93);
    n = morph_pop(q);
    CHECK("a queued SEQ stream coalesces to its latest value", n == 1 && (q[0] & 255u) == MQ_SEQF && (q[0] >> 16) == 93);
    midi_map_cc(3, &trk[0], 81, 89);
    morph_poll();
    CHECK("SEQ dispatch moves part 2's sequence fader", seqm_fader_of(1) == 89);

    CHECK("GLIDE can also be assigned to part 1", map_set(82, MAP_T_GLIDE, MAP_R_PART1, MAP_W_TYPED, 1, 0) == MAP_OK);
    midi_map_cc(3, &trk[0], 82, 42);
    n = morph_pop(q);
    CHECK("same-channel part controllers have independent latches", n == 1 && ((q[0] >> 8) & 255u) == 0);
    midi_tide(3, NSHOWN, MAP_T_GLIDE, 0);
    CHECK("hidden tracks cannot launch a shown part", morph_pop(q) == 0);

    song.sel = 0; gl.on = gl.armed = 0;
    perf_enter();
    CHECK("PERFORM labels match its actions", !strcmp(PF_NAME[PF_ROLLA], "GLIDE A") && !strcmp(PF_NAME[PF_ROLLB], "GLIDE B"));
    perf_roll_n[1]++; perf_input(0, fm1_ms, 0);
    CHECK("PERFORM B launches a glide", gl.on && gl.to == 1 && gl.mask == 1);
    perf_roll_n[0]++; perf_input(0, fm1_ms, 0);
    CHECK("PERFORM A launches a glide", gl.on && gl.to == 0 && gl.mask == 1);
    frame();
    const char *shot = getenv("DUELL_TEST_SCREENSHOT");
    if (shot) {
        FILE *f=fopen(shot,"wb");
        if (f) {
            fprintf(f,"P6\n240 240\n255\n");
            for (uint32_t i=0;i<240u*240u;i++) {
                uint16_t v=swap16(host_screen[i]);
                uint8_t rgb[3]={(uint8_t)(((v>>11)&31u)*255u/31u),(uint8_t)(((v>>5)&63u)*255u/63u),(uint8_t)((v&31u)*255u/31u)};
                fwrite(rgb,1,3,f);
            }
            fclose(f);
        }
    }
    printf("tidecontrols: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
