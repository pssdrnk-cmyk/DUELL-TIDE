/* SPDX-License-Identifier: GPL-3.0-only */
/* TIDE page STEPS against the sound (tide_ui.c tide_views / tide_steps; owner, 2026-11-04: "the UI display or notes seems to be
 * disconnected from the sounds coming from the machine"). The sequencer runs block by block (events_block, a UI frame every 16 ms) in
 * automatic WNDR and JRNY, TARGET BOTH, ROLL MUSIC, across TURNS (3), POLY (3), SYNC (3), ARC (3), VARY BARS, with factory multis and
 * built patterns (chance steps, ties after them, chords, long patterns, an ARP SEQ part whose rate is not the steps'). For every step the
 * sequencer starts, the notes it triggers (the MIDI out ring: what arp_note_on sent) are compared with
 *   pred  the step as the page showed it one block before (the page's model, tide_views, the whole page of the part sampled as drawn)
 *   post  the step as the page shows it right after (a chance step has rolled by then: not undecided any more)
 * and, as a report, with the page of before the fix (legacy: P_SLEN steps, all 16 in the bar now, the seq_idx playhead, ties as
 * stored). A chance step drawn undecided (striped) is right either way. Mismatches are counted by cause and kind:
 *   phantom  drawn as a hit, silent        missed  drawn as nothing, sounds        tie  a tie drawn that did not sound, or the reverse
 *   chord    a hit with the wrong number of notes        head  the playhead is not the step that sounded (ARP SEQ: the arp's step)
 *   past     hits drawn past the loop's end while POLY shortens it
 * The new page must have none. Built and run by host_tests.sh (name tidesteps). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, checks;
#define CHECK(s, x) do { checks++; if (!(x)) { printf("tidesteps: FAIL %s\n", s); bad++; } } while (0)

enum { M_OLD, M_NEW, M_N };
enum { C_BAR, C_CHANCE, C_ARP, C_TIE, C_POLY, C_OTHER, C_N };
static const char *const CN[C_N] = {"bar boundary", "chance", "arp seq", "ties", "poly", "other"};
enum { E_PHANTOM, E_MISSED, E_TIE, E_CHORD, E_HEAD, E_PAST, E_N };
static const char *const EN[E_N] = {"phantom", "missed", "tie", "chord", "head", "past"};
static uint32_t err[M_N][C_N][E_N], npred, nsteps, nmaybe, nvaried, nchance, ntie, narp, npoly, ndrop, nsig;

/* ---- the page before the fix: the bar now for the whole page, a tie as stored ---- */
static void old_view(const track_t *t, uint32_t idx, uint32_t bar, tv_t *e)
{
    const step_t *o = &t->step[idx], *v = seq_play_step_at(t, idx, bar);
    e->maybe = 0;
    e->n = v->n;
    e->k = tv_hit(v) ? (v == o || !memcmp(v, o, sizeof *v) ? TV_HIT : tv_hit(o) ? TV_MOVE : TV_ADD) : v->time == ST_TIE ? TV_TIE : tv_hit(o) ? TV_DROP : TV_REST;
}

/* ---- the scenarios ---- */
typedef struct {
    int multi;                  /* a factory multi 0..15, or -1: built (kind) */
    int kind;
    uint8_t mode, turns, poly, sync, arc, vary, time, bars;
} scn_t;

static step_t hit(uint32_t n0, uint32_t chance)
{
    step_t s = {{(uint8_t)n0}, 1, ST_NOTE, 0, 100, 0};
    step_set_chance(&s, chance);
    return s;
}
/* a pattern of len steps: hits (some on a chance, some chords), ties after some, rests */
static void fill(track_t *t, uint32_t len, uint32_t seedv, uint32_t chancey)
{
    uint32_t i, r = seedv;
    memset(t->step, 0, sizeof t->step);
    t->p[P_SLEN] = (int16_t)len;
    for (i = 0; i < len; i++) {
        r = r * 1103515245u + 12345u;
        switch ((r >> 16) % 6u) {
        case 0: case 1:
            t->step[i] = hit(48u + (r >> 20) % 24u, chancey && ((r >> 12) & 1u) ? 40u + (r >> 24) % 50u : 100u);
            break;
        case 2:
            t->step[i] = hit(55u + (r >> 20) % 12u, 100u);
            t->step[i].n = 3, t->step[i].note[1] = (uint8_t)(t->step[i].note[0] + 4u), t->step[i].note[2] = (uint8_t)(t->step[i].note[0] + 7u);
            break;
        case 3:
            t->step[i].time = i ? ST_TIE : ST_REST;
            break;
        default:
            t->step[i].time = ST_REST;
            break;
        }
    }
    t->step[0] = hit(52, 100);
}
static void build(int kind)
{
    track_t *a = &trk[0], *b = &trk[1];
    switch (kind) {
    case 0:                                                  /* 16 and 16, chance steps and the ties after them */
        fill(a, 16, 11, 1), fill(b, 16, 29, 1);
        break;
    case 1:                                                  /* longer patterns: 32 (two pages) and 24 */
        fill(a, 32, 5, 1), fill(b, 24, 71, 1);
        break;
    case 2:                                                  /* part 1 an ARP SEQ at another rate than its steps (arp DIV 3), PROB 90 */
        fill(a, 16, 3, 1), fill(b, 16, 41, 1);
        a->p[P_AMODE] = AM_SEQ, a->p[P_ARATE] = 3, a->p[P_APROB] = 90;
        break;
    default:                                                 /* an ARP SEQ part of 20 steps, always playing, and a 12-step part */
        fill(a, 20, 9, 0), fill(b, 12, 17, 1);
        a->p[P_AMODE] = AM_SEQ, a->p[P_ARATE] = 2, a->p[P_APROB] = 127;
        break;
    }
}

static void drain_on(uint32_t *nons)
{
    while (mo_r != mo_w) {
        uint32_t e = midi_out_q[mo_r % MQ];
        mo_r++;
        if (((e >> 8) & 0xF0u) == 0x90u)
            nons[(e >> 8) & 1u]++;                           /* (the parts' channels 1 and 2) */
    }
}

/* the pages of a part a step may start on, sampled before the block */
/* what the steps of part p are made of: the stored steps, both ends as played, the end each step is on at the fader, the thinning masks and the ARC / POLY levers. The page can only be compared with a later step while it stands; a change (the glide moves the fader, a new End B, a POLY stretch begins) is the page being redrawn */
static uint32_t made_of(uint32_t p)
{
    uint32_t h = 2166136261u, i;
    const uint8_t *b[3] = {(const uint8_t *)trk[p].step, (const uint8_t *)sqm_pat[p][0], (const uint8_t *)sqm_pat[p][1]};
    for (i = 0; i < 3u * NSTEP * sizeof(step_t); i++)
        h = (h ^ b[i / (NSTEP * sizeof(step_t))][i % (NSTEP * sizeof(step_t))]) * 16777619u;
    for (i = 0; i < NSTEP; i++)
        h = (h ^ (sqm_step(&trk[p], i, 1u) == &sqm_pat[p][1][i])) * 16777619u;   /* (the fader: which end each step is on) */
    h = (h ^ sqm_thin[p][0] ^ sqm_thin[p][1] * 3u ^ (uint32_t)tarc_thin << 8 ^ (uint32_t)tarc_dropphr << 16 ^ (uint32_t)tarc_fast << 24 ^ (uint32_t)poly_len[p] << 5 ^ (uint32_t)sqm_auto << 12 ^ (uint32_t)sqm_vary_all << 14 ^ (uint32_t)song.playing << 15) * 16777619u;
    return h;
}
typedef struct { tv_t v[3][16]; uint8_t page[3]; uint32_t n, made; } snap_t;
static void take(const track_t *t, snap_t *s, uint32_t ph, uint32_t L)
{
    uint32_t nx = ph == TV_NOPH ? 0u : (ph + 1u) % L, k, j, want[3];
    want[0] = 0u, want[1] = ph == TV_NOPH ? 0u : ph / 16u, want[2] = nx / 16u;
    s->n = 0;
    s->made = made_of(trk_index(t));
    for (k = 0; k < 3u; k++) {
        for (j = 0; j < s->n && s->page[j] != want[k]; j++)
            ;
        if (j == s->n) {
            s->page[s->n] = (uint8_t)want[k];
            tide_views(t, want[k], s->v[s->n]);
            s->n++;
        }
    }
}
static const tv_t *snap_get(const snap_t *s, uint32_t idx)
{
    uint32_t j;
    for (j = 0; j < s->n; j++)
        if (s->page[j] == idx / 16u)
            return &s->v[j][idx % 16u];
    return 0;
}

enum { A_NONE, A_NOTE, A_TIE };
/* does the view e agree with what sounded (actual; nons note-ons)? else the kind of mismatch in *et */
static int judge(const tv_t *e, uint32_t actual, uint32_t nons, int arp, uint32_t *et)
{
    uint32_t want = e->k >= TV_HIT ? A_NOTE : e->k == TV_TIE ? A_TIE : A_NONE;
    *et = E_N;
    if (want == actual) {
        if (want == A_NOTE && !arp && nons != e->n) {
            *et = E_CHORD;
            return 0;
        }
        return 1;
    }
    if (e->maybe && (actual == A_NONE || want == A_NONE))
        return 1;
    *et = want == A_NOTE && actual == A_NONE ? E_PHANTOM : want == A_NONE && actual == A_NOTE ? E_MISSED : E_TIE;
    return 0;
}

/* the whole loop of a part as the pages (new, old) showed it when step number `no` had just started (its playhead `ph`): what a step further on looked like h steps ahead */
typedef struct { tv_t nw[NSTEP], ol[NSTEP]; uint32_t ph, no, valid, made; } hist_t;
static hist_t hist[2][16];
static uint32_t stepno[2], nfar;

static void run_scn(const scn_t *c, uint32_t id)
{
    uint32_t blk, nblk, p, k, nons[2], bar_before, tsig_prev = 0, ssig_prev = 0;
    memset(hist, 0, sizeof hist);
    stepno[0] = stepno[1] = 0;
    uint32_t prev_idx[2], prev_pos[2], prev_arp[2], ph[2], L[2], full[2], legacy_bar[2];
    static snap_t snap[2];
    static tv_t old_snap[2][NSTEP];
    ui_power_on();
    frame();
    gl.on = gl.armed = 0;
    transport_req = 2u;
    frame();
    frame();
    if (c->multi >= 0)
        project_load((uint32_t)c->multi);
    else
        build(c->kind);
    song.g[G_BPM] = 240;
    song.g[G_CLOCK] = 0;
    song.g[G_SWING] = 50;
    midi_pch[0] = 0;
    midi_pch[1] = 1;
    usb.config = 1;
    for (p = 0; p < 2u; p++) {
        trk[p].p[P_SQNT] = 1;                                /* (NUDGE plays a step off its start: not the page's business) */
        trk[p].p[P_MUTE] = 0;
        trk[p].nheld = 0;
        trk[p].seq_active = 1;
        seqm_set_amount(p, 80);
        seqm_roll(p);
    }
    SEL_BOTH = 1;
    morph_musical = 1;
    gl.mode = c->mode, gl.target = GT_BOTH, gl.time = c->time, gl.wait = 0;
    sqm_vary_all = c->vary, sqm_turns = c->turns, sqm_poly = c->poly, sqm_sync = c->sync, sqm_arc = c->arc;
    mo_w = mo_r = 0;
    glide_launch(1u, 3u, 0);
    nblk = c->bars * 4u * (uint32_t)FS * 60u / 240u / CTL + 40u;
    for (blk = 0; blk < nblk; blk++) {
        if (c->arc && song.playing && sqm_bar >= 6 && (sqm_bar - 6) % 24 == 0 && !tarc.hand && tarc.phase != AP_DROP && (blk % 8u) == 0)
            tarc_drop_hand();                                 /* (OCT- + K2: a drop at the next phrase, in either ARC mode) */
        bar_before = sqm_bar;
        for (p = 0; p < 2u; p++) {
            const track_t *t = &trk[p];
            uint32_t arp = arp_mode(t) == AM_SEQ_ID;
            full[p] = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
            L[p] = song.playing && poly_len[p] && poly_len[p] < full[p] ? poly_len[p] : full[p];
            ph[p] = !song.playing ? TV_NOPH : arp ? (t->arp_idx == 0xFFFFFFFFu ? TV_NOPH : tide_arp_idx(t, t->arp_idx, full[p])) : t->seq_idx;
            if (ph[p] != TV_NOPH && ph[p] >= L[p])
                ph[p] = TV_NOPH;
            prev_idx[p] = t->seq_idx, prev_pos[p] = t->seq_pos, prev_arp[p] = t->arp_idx;
            if (song.playing) {
                take(t, &snap[p], ph[p], L[p]);
                legacy_bar[p] = (uint32_t)sqm_bar + sqm_bar_turn;
                for (k = 0; k < full[p]; k++)
                    old_view(t, k, legacy_bar[p], &old_snap[p][k]);
            }
        }
        events_block(CTL);
        nons[0] = nons[1] = 0;
        drain_on(nons);
        for (p = 0; p < 2u && song.playing; p++) {
            const track_t *t = &trk[p];
            uint32_t arp = arp_mode(t) == AM_SEQ_ID, started, idx, actual, et, m, chance, still, cs, isr_bar = bar_before + sqm_bar_turn;
            const step_t *vt;
            const tv_t *pr;
            tv_t post[16], oi;
            if (arp)
                started = t->arp_idx != prev_arp[p] && t->arp_idx != 0xFFFFFFFFu, idx = tide_arp_idx(t, t->arp_idx, full[p]);
            else
                started = (t->seq_idx != prev_idx[p] || t->seq_pos < prev_pos[p]) && t->seq_pos < 0x7FFFFFFu, idx = t->seq_idx;
            if (!started || idx >= full[p] || !(pr = snap_get(&snap[p], idx)))
                continue;
            if (!arp && idx == 0 && poly_len[p] && poly_len[p] < full[p])   /* steps drawn past a shortened loop's end */
                for (m = poly_len[p]; m < full[p]; m++) {
                    tv_t pz[16];
                    err[M_OLD][C_POLY][E_PAST] += old_snap[p][m].k >= TV_HIT;
                    tide_views(t, m / 16u, pz);
                    err[M_NEW][C_POLY][E_PAST] += pz[m % 16u].k != TV_END;
                }
            vt = seq_play_step_at(t, idx, isr_bar);
            actual = nons[p] ? A_NOTE : !arp && t->seq_n && vt->time == ST_TIE ? A_TIE : A_NONE;
            chance = arp ? t->p[P_APROB] < 127 : step_chance(vt) < 100u;
            for (still = 1, m = 1; m <= 3u; m++)                             /* the pattern stood for the last steps (a tie holds a note from the hit: it needs the same steps at both) */
                still &= stepno[p] >= m && hist[p][(stepno[p] - m) & 15u].valid && hist[p][(stepno[p] - m) & 15u].made == made_of(p);
            nvaried += memcmp(vt, &t->step[idx], sizeof *vt) != 0;
            nchance += chance, ntie += vt->time == ST_TIE, narp += arp, npoly += poly_len[p] != 0, ndrop += TARC_DROP_BAR(isr_bar) && ((sqm_auto >> p) & 1u) && (tv_hit(&t->step[idx]) || tv_hit(vt));
            nsteps++;
            nmaybe += pr->maybe;
            npred += snap[p].made == made_of(p) && (ph[p] == TV_NOPH || idx > ph[p]);
            cs = arp ? C_ARP : chance ? C_CHANCE : t->step[idx].time == ST_TIE || vt->time == ST_TIE ? C_TIE : C_OTHER;
            if (snap[p].made == made_of(p) && (ph[p] == TV_NOPH || idx > ph[p]) && !judge(pr, actual, nons[p], arp, &et) && (still || et != E_TIE)) {   /* the new page, as drawn before the step */
                if (++err[M_NEW][cs][et] <= 2u)
                    printf("tidesteps:   scenario %u part %u step %u: before it k=%u n=%u maybe=%u, but %u (%u note-ons)\n", id, p + 1u, idx, pr->k, pr->n, pr->maybe, actual, nons[p]);
            }
            tide_views(t, idx / 16u, post);                                  /* .. and right after: it has rolled, nothing is undecided */
            if ((!judge(&post[idx % 16u], actual, nons[p], arp, &et) || (post[idx % 16u].maybe && !arp)) && (still || et != E_TIE)) {
                et = et == E_N ? E_MISSED : et;
                if (++err[M_NEW][cs][et] <= 2u)
                    printf("tidesteps:   scenario %u part %u step %u: after it k=%u maybe=%u, but %u\n", id, p + 1u, idx, post[idx % 16u].k, post[idx % 16u].maybe, actual);
            }
            old_view(t, idx, isr_bar, &oi);                                  /* the old page: wrong in the bar it used? else wrong in the bar it showed? */
            if (!judge(&oi, actual, nons[p], arp, &et))
                err[M_OLD][arp ? C_ARP : chance ? C_CHANCE : t->step[idx].time == ST_TIE || oi.k == TV_TIE ? C_TIE : poly_len[p] ? C_POLY : C_OTHER][et]++;
            else if (!judge(&old_snap[p][idx], actual, nons[p], arp, &et))
                err[M_OLD][C_BAR][et]++;
            err[M_OLD][C_ARP][E_HEAD] += arp && t->seq_idx != idx;           /* its playhead is seq_idx, the arp's step is arp_idx */
            {   /* the steps further on in the page, as they were shown 1, 3, 7 and 15 steps before: in the bar they were drawn for, not the one they play in */
                static const uint8_t HZ[4] = {1, 3, 7, 15};
                uint32_t z;
                for (z = 0; z < 4u; z++) {
                    const hist_t *h = &hist[p][(stepno[p] - HZ[z]) & 15u];
                    if (stepno[p] < HZ[z] || !h->valid || h->no != stepno[p] - HZ[z] || h->ph >= idx || idx - h->ph != HZ[z] || h->ph / 16u != idx / 16u || h->made != made_of(p))
                        continue;
                    nfar++;
                    if (!judge(&h->nw[idx], actual, nons[p], arp, &et) && (still || et != E_TIE)) {
                        if (++err[M_NEW][cs][et] <= 3u)
                            printf("tidesteps:   scenario %u part %u step %u: %u steps ahead it was k=%u maybe=%u, but %u\n", id, p + 1u, idx, HZ[z], h->nw[idx].k, h->nw[idx].maybe, actual);
                    }
                    if (judge(&oi, actual, nons[p], arp, &et) && !judge(&h->ol[idx], actual, nons[p], arp, &et))
                        err[M_OLD][C_BAR][et]++;
                }
                {   /* this step is now the playhead: the pages as they show it */
                    hist_t *h = &hist[p][stepno[p] & 15u];
                    h->valid = 1, h->no = stepno[p], h->ph = idx, h->made = made_of(p);
                    for (m = 0; m < full[p]; m += 16u)
                        tide_views(t, m / 16u, h->nw + m);
                    for (m = 0; m < full[p]; m++)
                        old_view(t, m, (uint32_t)sqm_bar + sqm_bar_turn, h->ol + m);
                    stepno[p]++;
                }
            }
        }
        if ((blk % 22u) == 21u) {                                            /* a UI frame: the page's playhead and its hash */
            uint32_t ts, ss;
            frame();
            for (p = 0; p < 2u && song.playing; p++) {
                tv_t v[16];
                const track_t *t = &trk[p];
                uint32_t arp = arp_mode(t) == AM_SEQ_ID, h = tide_views(t, 0xFFu, v), flen = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                uint32_t want = arp ? (t->arp_idx == 0xFFFFFFFFu ? TV_NOPH : tide_arp_idx(t, t->arp_idx, flen)) : t->seq_idx;
                err[M_NEW][arp ? C_ARP : C_OTHER][E_HEAD] += h != want && !(h == TV_NOPH && want >= flen);
            }
            ts = tide_sig();
            ss = tide_steps_sig(&trk[song.sel]) ^ tide_steps_sig(&trk[song.sel ^ 1u]);
            nsig += song.playing && ss != ssig_prev && ts == tsig_prev;      /* the steps changed, the page's hash did not */
            ssig_prev = ss, tsig_prev = ts;
        }
    }
    CHECK("the sequencer ran (PLAY, automatic WNDR / JRNY on both parts)", song.playing && sqm_auto == 3u);
    transport_req = 2u;
    frame();
    frame();
}

int main(void)
{
    static const scn_t S[] = {
        /* multi kind mode        turns poly sync arc vary time bars */
        {3, 0, GM_JOURNEY, 1, 1, 1, 0, 0, 0, 24},
        {-1, 0, GM_WANDER, 1, 1, 1, 0, 0, 0, 24},
        {-1, 0, GM_JOURNEY, 2, 2, 2, 1, 0, 1, 24},
        {-1, 0, GM_WANDER, 2, 1, 0, 2, 1, 0, 24},
        {-1, 0, GM_JOURNEY, 0, 2, 1, 0, 0, 2, 24},
        {-1, 1, GM_WANDER, 1, 1, 1, 1, 0, 0, 32},
        {-1, 1, GM_JOURNEY, 2, 2, 2, 2, 0, 1, 32},
        {-1, 1, GM_WANDER, 2, 0, 1, 0, 1, 0, 32},
        {-1, 2, GM_JOURNEY, 1, 1, 1, 0, 0, 0, 24},
        {-1, 2, GM_WANDER, 2, 2, 2, 1, 0, 1, 24},
        {-1, 3, GM_JOURNEY, 2, 2, 1, 2, 0, 0, 32},
        {-1, 3, GM_WANDER, 0, 1, 0, 0, 1, 1, 24},
        {0, 0, GM_WANDER, 1, 1, 1, 0, 0, 0, 16},
        {5, 0, GM_JOURNEY, 2, 2, 2, 1, 0, 1, 16},
        {8, 0, GM_WANDER, 2, 1, 1, 2, 0, 0, 16},
        {11, 0, GM_JOURNEY, 1, 2, 0, 0, 1, 1, 16},
        {13, 0, GM_WANDER, 2, 1, 2, 1, 0, 0, 16},
        {15, 0, GM_JOURNEY, 2, 2, 1, 2, 0, 1, 16},
        {-1, 0, GM_WANDER, 1, 1, 1, 2, 0, 0, 72},       /* (ARC needs its 5+ phrases before a drop) */
        {-1, 1, GM_JOURNEY, 2, 2, 2, 2, 0, 0, 72},
        {-1, 3, GM_WANDER, 2, 1, 1, 1, 0, 0, 72},
    };
    uint32_t i, m, c, e, tot[M_N] = {0, 0};
    for (i = 0; i < sizeof S / sizeof S[0]; i++)
        run_scn(&S[i], i);
    printf("tidesteps: %u steps started: %u varied from the stored step, %u on a chance, %u ties, %u of an ARP SEQ part, %u in a POLY stretch, %u in an ARC drop (nothing starts); %u shown undecided; checked against the page 1, 3, 7, 15 steps before %u, a block before %u\n",
           nsteps, nvaried, nchance, ntie, narp, npoly, ndrop, nmaybe, nfar, npred);
    for (m = 0; m < M_N; m++) {
        printf("tidesteps: %s page, mismatches by cause:", m ? "new" : "old");
        for (c = 0; c < C_N; c++) {
            uint32_t n = 0;
            for (e = 0; e < E_N; e++)
                n += err[m][c][e];
            tot[m] += n;
            printf("  %s %u", CN[c], n);
        }
        printf("  = %u\n", tot[m]);
        for (c = 0; c < C_N; c++)
            for (e = 0; e < E_N; e++)
                if (err[m][c][e])
                    printf("tidesteps:     %s: %s / %s: %u\n", m ? "new" : "old", CN[c], EN[e], err[m][c][e]);
    }
    CHECK("the runs cover varied steps, chance steps, ties, an ARP SEQ part and POLY stretches", nvaried > 50u && nchance > 50u && ntie > 20u && narp > 50u && npoly > 20u);
    CHECK("the harness sees the old page's mismatches (it can fail)", tot[M_OLD] > 0u);
    CHECK("the page's hash changes whenever its steps do", nsig == 0u);
    CHECK("the page shows what plays: no phantom hit, missed note, wrong tie, chord size, playhead or step past the loop's end", tot[M_NEW] == 0u);
    printf("tidesteps: %d of %d checks failed\n", bad, checks);
    return bad != 0;
}
