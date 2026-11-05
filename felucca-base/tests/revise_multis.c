/* SPDX-License-Identifier: GPL-3.0-only */
/* DUELL 1.1 U2 option B: revised factory copies of the multis whose stored patterns are empty (M03 M05 M06 M07 M08 M13) and of M10 M11 M12 (one
 * shared chromatic line). A record of tools/factory_multis/original/Mnn.bin (the record as it was, kept) is loaded as the firmware loads a multi
 * (multi.c mu_unpack, project_restore_runtime), the parts' arp, gate, hold, chord and steps are set as the table below says, and it is saved
 * again by multi.c's packer: everything else (sounds, End B, mix, effects, globals, MODE, XFADE, SPLIT) comes back byte for byte (checked: the
 * differing bytes lie in the parts' arp / chord / transpose values and the step maps only). Not the sounds' work: no sound value is touched.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -DFELUCCA_VERSION='"x"' -o build/host/revise_multis tests/revise_multis.c -lm
 *   build/host/revise_multis dump IN.bin [..]                 what a record holds (parts, arp, steps)
 *   build/host/revise_multis build ORIGDIR OUTDIR             every table entry: ORIGDIR/Mnn.bin -> OUTDIR/Mnn.bin
 * Steps (as tools/factory_multis.py's): space separated, "." rest, a signed number = semitones from the held key (the SEQUENCER arp, AM_SEQ) or
 * from C4 (60, a plain pattern), suffix A an accent. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define RV_MAX 2048u
#define S(id) ((id) + 1)                      /* (a P_ id in a set list: 0 ends it) */

typedef struct {
    const char *amode, *arate, *hold, *chrd, *voic, *sdiv;   /* names of the lists (0: as the record has it) */
    int16_t trans, agate, sgate;                     /* TRN (KEEP: as it is), the arp's GATE and the pattern's GATE (0: as they are) */
    const char *steps;                               /* 0: the part's steps as they are */
    int16_t set[16];                                 /* more (S(P_ id), value) pairs, 0-terminated */
} rv_part_t;
typedef struct { uint8_t slot; rv_part_t p[2]; } rv_t;
#define KEEP 0x7FFF
#include "revise_table.h"

static int find(const char *const *l, uint32_t n, const char *s)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (str_eq(l[i], s))
            return (int)i;
    fprintf(stderr, "revise_multis: no list entry %s\n", s);
    exit(1);
}

static int load_rec(const char *path, uint8_t *buf, uint32_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return perror(path), 1;
    *len = (uint32_t)fread(buf, 1, RV_MAX, f);
    fclose(f);
    return 0;
}
static int restore(const uint8_t *in, uint32_t len)
{
    static project_t q;
    uint8_t amt[NSHOWN], set[6];
    uint32_t pi;
    ui_power_on();
    project_capture(&q);
    memset(amt, 0, sizeof amt); memset(set, 0, sizeof set);
    if (!mu_unpack(&q, amt, set, in, len))
        return fprintf(stderr, "revise_multis: not a multi record\n"), 1;
    proj_bound(&q);
    proj_morph_skip = 1;                              /* (project_load's path: End B below, from the record's own block and AMOUNT) */
    if (project_restore_runtime(&q))
        return fprintf(stderr, "revise_multis: the record does not restore\n"), 1;
    proj_morph_skip = 0;
    morph.cfg = set[2];
    for (pi = 0; pi < NSHOWN; pi++)
        trk[pi].engine = trk[pi].eng_req;             /* (no engine-change fade pending: the morph is not stale) */
    for (pi = 0; pi < NSHOWN; pi++)
        if (q.bvn[pi]) {
            morph_load_part_v(pi, q.bv[pi]);
            mph[pi].amount = (uint8_t)(amt[pi] > 100u ? 100u : amt[pi]);
        } else
            morph_load_part(pi, 0, amt[pi], 0, 0);
    return 0;
}

static void show(const char *tag)
{
    uint32_t pi, i;
    char nm[16];
    printf("%s  BPM %d  MODE %s  XF %u SPLIT %u\n", tag, song.g[G_BPM], DM_NAME[duell_mode_req], mix_xf, mix_split);
    for (pi = 0; pi < NSHOWN; pi++) {
        const track_t *t = &trk[pi];
        uint32_t used = 0;
        sound_name(t, nm);
        printf("  part %u %-12s TRN %d VCE %s arp %s %s hold %s oct %d gate %d chord %s %s LEN %d SDIV %s LVL %d\n    steps:", pi + 1u, nm, t->p[P_TRANS],
               N_VOICE[t->p[P_VOICE] % 4], N_AMODE[t->p[P_AMODE] % 16], N_DIV[t->p[P_ARATE] % 17], N_HOLD[t->p[P_AHOLD] % 3], t->p[P_AOCT], t->p[P_AGATE],
               N_CHRD[t->p[P_CHRD] % 16], N_VOIC[t->p[P_VOIC] % 5], t->p[P_SLEN], N_DIV[t->p[P_SDIV] % 17], t->p[P_LEVEL]);
        for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) {
            const step_t *s = &t->step[i];
            if (s->time == ST_REST || (!s->n && s->time != ST_TIE)) { printf(" ."); continue; }
            used++;
            printf(" %s%d%s", s->time == ST_TIE ? "-" : "", s->time == ST_TIE ? 0 : s->note[0], s->flags & SF_ACCENT ? "A" : "");
        }
        printf("%s\n", used ? "" : " (none)");
    }
}

/* a pattern step: "." rest, "-" tie, a signed number (the SEQUENCER arp: semitones from the held key, one note), or notes joined by "+" (a plain pattern:
 * C3 Eb2 F#4, C4 = 60, up to four to a step), suffix A an accent */
static int note_of(const char **ps)
{
    static const int8_t L[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    const char *s = *ps;
    int n = L[*s++ - 'A'], o;
    if (*s == '#') { n++; s++; }
    else if (*s == 'b') { n--; s++; }
    o = *s++ - '0';
    *ps = s;
    return 12 * (o + 1) + n;
}
static int parse_steps(track_t *t, const char *s)
{
    uint32_t i = 0;
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
    for (i = 0; *s && i < NSTEP; i++) {
        step_t *st = &t->step[i];
        while (*s == ' ') s++;
        if (*s == '.') { st->time = ST_REST; s++; }
        else if (*s == '-' && (s[1] == ' ' || !s[1])) { st->time = ST_TIE; s++; }
        else {
            st->time = ST_NOTE;
            st->vel = 100;
            if ((*s >= 'A' && *s <= 'G')) {
                for (;;) {
                    if (st->n >= 4u) { fprintf(stderr, "more than four notes in a step\n"); exit(1); }
                    st->note[st->n++] = (uint8_t)note_of(&s);
                    if (*s != '+') break;
                    s++;
                }
            } else {
                int32_t sg = 1, v = 0;
                if (*s == '+' || *s == '-')
                    sg = *s++ == '-' ? -1 : 1;
                if (*s < '0' || *s > '9') { fprintf(stderr, "bad step '%s'\n", s); exit(1); }
                while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
                st->n = 1;
                st->note[0] = (uint8_t)(60 + sg * v);
            }
            for (; *s == 'A'; s++)
                st->flags |= SF_ACCENT;
        }
        while (*s == ' ') s++;
    }
    if (*s) { fprintf(stderr, "more than %u steps\n", NSTEP); exit(1); }
    return (int)i;
}

static void apply(const rv_t *r)
{
    uint32_t k, i;
    for (k = 0; k < 2u; k++) {
        const rv_part_t *c = &r->p[k];
        track_t *t = &trk[k];
        if (c->amode) t->p[P_AMODE] = (int16_t)find(N_AMODE, 16, c->amode);
        if (c->arate) t->p[P_ARATE] = (int16_t)find(N_DIV, 17, c->arate);
        if (c->hold) t->p[P_AHOLD] = (int16_t)find(N_HOLD, 3, c->hold);
        if (c->chrd) t->p[P_CHRD] = (int16_t)find(N_CHRD, 16, c->chrd);
        if (c->voic) t->p[P_VOIC] = (int16_t)find(N_VOIC, 5, c->voic);
        if (c->sdiv) t->p[P_SDIV] = (int16_t)find(N_DIV, 17, c->sdiv);
        if (c->trans != KEEP) t->p[P_TRANS] = c->trans;
        if (c->agate) t->p[P_AGATE] = c->agate;
        if (c->sgate) t->p[P_SGATE] = c->sgate;
        for (i = 0; c->set[i]; i += 2u)
            t->p[c->set[i] - 1] = c->set[i + 1u];
        if (c->steps) {
            t->p[P_SLEN] = (int16_t)parse_steps(t, c->steps);
        }
    }
}

int main(int argc, char **argv)
{
    static uint8_t in[RV_MAX];
    uint32_t len, i;
    if (argc > 2 && str_eq(argv[1], "dump")) {
        for (i = 2; i < (uint32_t)argc; i++) {
            if (load_rec(argv[i], in, &len) || restore(in, len))
                return 1;
            show(argv[i]);
        }
        return 0;
    }
    if (argc > 3 && str_eq(argv[1], "diff")) {          /* diff A.bin B.bin: every part value, global, End B and mix byte that differs, as ids (not the steps) */
        static int16_t pa[NSHOWN][P_COUNT], ga[G_COUNT];
        static uint8_t ma[NSHOWN][MB_N], mb[NSHOWN][MB_N], ea[2048];
        uint32_t k, n = 0;
        if (load_rec(argv[2], in, &len) || restore(in, len))
            return 1;
        for (k = 0; k < NSHOWN; k++) {
            memcpy(pa[k], trk[k].p, sizeof pa[k]);
            morph_blk_make(k, ma[k]);
        }
        memcpy(ga, song.g, sizeof ga);
        memcpy(ea, in, len);
        if (load_rec(argv[3], in, &len) || restore(in, len))
            return 1;
        for (k = 0; k < NSHOWN; k++) {
            for (i = 0; i < P_COUNT; i++)
                if (pa[k][i] != trk[k].p[i]) { printf(" part %u P_%u %d>%d", k + 1u, i, pa[k][i], trk[k].p[i]); n++; }
            morph_blk_make(k, mb[k]);
            if (memcmp(ma[k], mb[k], MB_N)) { printf(" part %u End B differs", k + 1u); n++; }
        }
        for (i = 0; i < G_COUNT; i++)
            if (ga[i] != song.g[i]) { printf(" G_%u %d>%d", i, ga[i], song.g[i]); n++; }
        printf("%s\n", n ? "" : " same values");
        return 0;
    }
    if (argc > 2 && str_eq(argv[1], "rt")) {            /* rt IN.bin ..: loaded and saved again unchanged: which bytes differ */
        for (i = 2; i < (uint32_t)argc; i++) {
            uint32_t n, j, d = 0;
            const uint8_t *rec;
            if (load_rec(argv[i], in, &len) || restore(in, len))
                return 1;
            memcpy(proj_name, in + MU_NAME_AT, PROJ_NAME_LEN);
            proj_name[PROJ_NAME_LEN] = 0;
            duell_mode_req = in[4];
            mix_xf = in[len - 2u];
            mix_split = in[len - 1u];
            if (project_save_as(0, proj_name)) return 1;
            mu_obj_read(0);
            rec = mu_rec(0, &n);
            printf("%s: %u -> %u bytes;", argv[i], len, n);
            for (j = 0; j < len && j < n; j++)
                if (rec[j] != in[j] && d++ < 12u)
                    printf(" @%u %02x>%02x", j, in[j], rec[j]);
            printf("%s\n", d ? "" : " identical");
        }
        return 0;
    }
    if (argc > 3 && str_eq(argv[1], "build")) {
        for (i = 0; i < sizeof RV / sizeof *RV; i++) {
            char path[512];
            uint32_t slot = RV[i].slot, n;
            const uint8_t *rec;
            FILE *f;
            snprintf(path, sizeof path, "%s/M%02u.bin", argv[2], slot);
            if (load_rec(path, in, &len) || restore(in, len))
                return 1;
            memcpy(proj_name, in + MU_NAME_AT, PROJ_NAME_LEN);
            proj_name[PROJ_NAME_LEN] = 0;
            duell_mode_req = in[4];                  /* (MODE, XFADE and SPLIT are the record's own: the last two bytes) */
            mix_xf = in[len - 2u];
            mix_split = in[len - 1u];
            apply(&RV[i]);
            if (project_save_as(slot - 1u, proj_name)) { printf("save failed\n"); return 1; }
            mu_obj_read((slot - 1u) / 2u);
            rec = mu_rec((slot - 1u) & 1u, &n);
            snprintf(path, sizeof path, "%s/M%02u.bin", argv[3], slot);
            f = fopen(path, "wb");
            if (!f) return perror(path), 1;
            fwrite(rec, 1, n, f);
            fclose(f);
            printf("M%02u %u -> %u bytes\n", slot, len, n);
        }
        return 0;
    }
    return fprintf(stderr, "usage: revise_multis dump IN.bin .. | build ORIGDIR OUTDIR\n"), 2;
}
