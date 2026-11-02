/* SPDX-License-Identifier: GPL-3.0-only */
/* MU_VER 7 -> 8 (multi.c): End B of a version 7 multi as values. Host tool, uses the firmware's own code (like tests/gen_sound_endb.c); compiled and run by
 * tools/multi_v7_to_v8.py, which does the rest of the conversion. Reads lines from stdin:
 *   <v8 record, End B left out (length byte 0)> <part 1: the v7 End B block (MB_N bytes)> <part 1: the hand-set B edits, (wide id, value + 64) pairs> <part 2: the same two>
 * all hex, "-" for none. For each: the record is stored in M01 and loaded as the MULTI page does (MODE RECALL ON), then each part's End B is made as 1.0 made it
 * (morph_load_part with the v7 block and the edits: the same roll from its seed, the values set by hand on top) and written as the record would hold it
 * (morph_bv_make against the part's values, project_capture's reference). Prints one line: part 1's MBV block, part 2's (hex, "-" for none: a v7 part without a
 * valid block and without edits rolled a fresh B at each load and still does). */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_FLASH 1
#include "ui_test.c"

static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{ const uint8_t *s = src; uint32_t i; if ((off & 0xFFu) + n > 256u) return -1; for (i = 0; i < n; i++) nor[off + i] &= s[i]; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }
static void fl_plain_window_init(void) {}

static uint32_t unhex(const char *s, uint8_t *b, uint32_t max)
{
    uint32_t n = 0, v;
    if (!strcmp(s, "-"))
        return 0;
    while (s[0] && s[1] && n < max && sscanf(s, "%2x", &v) == 1) {
        b[n++] = (uint8_t)v;
        s += 2;
    }
    return n;
}
static void hex(const uint8_t *b, uint32_t n)
{
    uint32_t i;
    if (!n)
        printf("-");
    for (i = 0; i < n; i++)
        printf("%02x", b[i]);
}

int main(void)
{
    static char line[16384], w[5][8192];
    static uint8_t rec[2048], blk[NSHOWN][MB_N], ed[NSHOWN][2u * XB_MAX], out[MBV_MAX];
    static xb_edit_t xe[NSHOWN][XB_MAX];
    uint32_t len, rl, cut, pi, i, nb[NSHOWN], ne[NSHOWN], amt[NSHOWN];
    while (fgets(line, sizeof line, stdin)) {
        if (sscanf(line, "%8191s %8191s %8191s %8191s %8191s", w[0], w[1], w[2], w[3], w[4]) != 5)
            continue;
        rl = unhex(w[0], rec, sizeof rec);
        for (pi = 0; pi < NSHOWN; pi++) {
            memset(blk[pi], 0, MB_N);
            nb[pi] = unhex(w[1 + 2 * pi], blk[pi], MB_N);
            ne[pi] = unhex(w[2 + 2 * pi], ed[pi], sizeof ed[pi]) / 2u;
            for (i = 0; i < ne[pi]; i++) {
                xe[pi][i].wid = ed[pi][2 * i];
                xe[pi][i].val = (int16_t)(ed[pi][2 * i + 1] - 64);
            }
        }
        ui_power_on();
        flash_ok = 1;
        pt_boot();
        frame();
        song.playing = 0;
        mu_obj_read(0);
        len = mu_put_rec(0, 1, &cut, rec, rl);
        if (mu_obj_write(0, len)) {
            fprintf(stderr, "multi_v7_endb: record not stored\n");
            return 2;
        }
        mu_note(0);
        ui_prefs2 |= PREF_MODE_RECALL >> 16;
        project_load(0);
        if (proj_cur != 0u) {
            uint8_t a[NSHOWN], s[6];
            fprintf(stderr, "multi_v7_endb: the v8 record does not load (%s)\n",
                    mu_unpack(&proj_scratch, a, s, rec, rl) ? "it unpacks: the load refused it" : "multi.c mu_unpack refuses it");
            return 2;
        }
        for (pi = 0; pi < NSHOWN; pi++) {
            int f;
            for (f = 0; f < 200 && trk[pi].engine != trk[pi].eng_req; f++)   /* (an engine change fades first: settled, as gen_sound_endb.c does; a morph of */
                frame();                                                     /* a sound still fading in is stale, morph_bv_make would give no block) */
            trk[pi].engine = trk[pi].eng_req;
            amt[pi] = mph[pi].amount;
        }
        for (pi = 0; pi < NSHOWN; pi++) {                /* 1.0's multi_load: morph_load_part(pi, blk, amt, the edits) */
            int16_t ref[EXT_P_LEN];
            uint32_t n = 0;
            if ((nb[pi] == MB_N && morph_blk_ok(blk[pi])) || ne[pi]) {   /* (edits without a roll: End B is the Original with them on top) */
                morph_load_part(pi, blk[pi], amt[pi], xe[pi], ne[pi]);
                memset(ref, 0, sizeof ref);
                for (i = 0; i < P_COUNT; i++)
                    ref[i] = motion_base_value(&trk[pi], i);
                for (i = 0; i < NX; i++)
                    ref[XW_BASE + i] = (int16_t)clamp(XV(&trk[pi], i), -64, 127);
                n = morph_bv_make(pi, ref, out);
            }
            if (pi)
                printf(" ");
            hex(out, n);
        }
        printf("\n");
        fflush(stdout);
    }
    return 0;
}
