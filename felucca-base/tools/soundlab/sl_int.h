/* SPDX-License-Identifier: GPL-3.0-only */
/* soundlab lane B: types shared by sl_metrics.c (signal) and sl_notes.c (voice log). */
#ifndef SL_INT_H
#define SL_INT_H
#include "sl_lab.h"
#include <math.h>

typedef struct { int part, note, vel; long s0, s1, se; } voice;   /* note-on, note-off (release start), voice end; samples relative to the range */

typedef struct {
    int sr; long n;                   /* range length in samples */
    double *l, *r, *m;                /* the range; m = (l + r) / 2 */
    int nbar; long *bs;               /* starts of the bars inside the range, relative; nbar entries */
    voice *v; int nv;                 /* voices that start inside the range */
    double bpm;
    int has_end;                      /* the json gave t_end */
} sig;

typedef struct { long t; int first, cnt; } group;   /* an onset: notes of one part starting within tol samples of the first */

int sl_part_voices(const sig *s, int part, voice **out);            /* sorted by onset; caller frees */
int sl_make_groups(const voice *a, int k, long tol, group *g);      /* g must hold k entries */
int sl_lowest(const voice *a, const group *g);

typedef struct {
    int nnotes, ngroups;
    double onsets_bar, notes_onset, share_16th_off, ioi1_share, swing_pct;
    double vel_sd, vel_min, vel_max, vel_distinct;
    double gate_med_steps, sound_med_steps, blip_pct, gapfill_med, jump_oct_pct, jump_5th_pct, jump_mean;
    double note_len_med_ms, low, high, note_med, note_p90;
    double h30, oddeven_db, odd_share_pct, h1_share_db, hcentroid, move_pct, env_move_pct, held_cv_pct, attack_ms;
    int timbre_clean, timbre_n;       /* 1 = every onset used had the other part silent around it */
} partm;
void sl_part_metrics(const sig *s, int part, partm *o);

typedef struct { double mean[SL_NP], maxc[SL_NP], sil[SL_NP], both_mean, both_max, both_sil; int fullest[SL_NP][16], nfull[SL_NP]; } bedm;
void sl_bed(const sig *s, bedm *b);

typedef struct {
    double rub12_pct, rub_oct_pct, both_pct, within_pct[SL_NP];    /* share of the time both parts sound / of the time a part sounds with 2+ notes */
    int sets[SL_NP][128], nset[SL_NP];                              /* distinct notes per part over the range */
} rubm;
void sl_rubs(const sig *s, int all_mode, rubm *r);                  /* all_mode 0 = REL (note-on to note-off), 1 = ALL (to voice end) */

#endif
