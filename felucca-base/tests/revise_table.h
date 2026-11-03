/* SPDX-License-Identifier: GPL-3.0-only */
/* tests/revise_multis.c's table (DUELL 1.1 U2 option B): the sequenced arrangements of the factory multis whose stored patterns were empty, and the
 * line of M10 .. M12. The tonal centre is C, the SEQUENCER arp's stand-in root (a PLAY with no key held plays the arp from C4, tide_seq.c): the
 * plain-pattern parts (chords, pads) are written in absolute notes, the SEQ parts as offsets from that root, so a held key moves them and the plain
 * parts stay (a live key plays on top). A part left out ({ .trans = KEEP }) stays as the record has it: M03 EVOLVING PAD, M05 WHISPER PAD, M08 GEARBOX and
 * M13 SINGING BOWL are played live on purpose. Cost: PLAY alone is kept to a worst half of about 85 % or less (tools/cpu_budget.sh, SLOT= PLAY=1): the pads are
 * heavy sounds, so they hold one or two notes; chords are what the keys play. */
#define KP { .trans = KEEP }
static const rv_t RV[] = {
    /* M03 PAD SPLIT, 97 BPM: EVOLVING PAD stays live (one pad voice alone costs about the unit's budget); EMBER GLOW glints a sparse C minor figure on 1/4, 12 steps
     * (three bars) */
    { 3, { KP,
           { .amode = "SEQ", .arate = "1/4", .agate = 40, .trans = 0,
             .steps = "+12 . . +7 . . +15 . . . +14 ." } } },
    /* M05 WHISPER TIDE, 77 BPM: the digital pad breathes through four fifths-and-sevenths dyads (Fm, Db, Bbm, Eb), two bars each, a tide of eight bars; the random
     * WHISPER arp stays live over held keys */
    { 5, { { .trans = KEEP, .sdiv = "1/1", .sgate = 100,
             .steps = "F3+C4 - Db3+Ab3 - Bb2+F3 - Eb3+Bb3 -" },
           KP } },
    /* M06 TINE STRINGS, 77 BPM: the strings hold one line, Bb3 then A3, two bars each; the tines play three Dorian notes in two bars on 1/8 under the dotted-quarter echo */
    { 6, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
             .steps = "Bb3 - A3 -" },
           { .amode = "SEQ", .arate = "1/8", .agate = 40, .trans = 0,
             .steps = "+12A . . . +7 . . . . +10 . . . +3 . ." } } },
    /* M07 FINAL REEL, 71 BPM: the brass pad holds the fifth of C, Am, F, G, two bars each; the strings shimmer a high C and G on 1/2, the accent on the downbeat
     * brings them forward through the velocity crossfade. Their TRN +12 moved into the steps */
    { 7, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
             .steps = "G2 - E3 - C3 - D3 -" },
           { .amode = "SEQ", .arate = "1/2", .agate = 70, .trans = 0,
             .steps = "+12A . . . +19 . . ." } } },
    /* M08 HAZY TENSOR, 117 BPM: the ladder bass rolls a syncopated C line (3 + 3 + 2 + 3 + 3 + 2); GEARBOX stays a live random triplet arp. TRN -12
     * moved into the steps */
    { 8, { { .amode = "SEQ", .arate = "1/16", .agate = 70, .trans = 0,
             .steps = "-24A . . -24 . . -12 . -24 . . -21 . . -19 ." },
           KP } },
    /* M13 COLD CHAPEL, 71 BPM: the pad holds four dyads, two bars each (Dsus2, Bb, Gm, Asus4); the bowl walks up the held chord, live */
    { 13, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
              .steps = "A2+E3 - F3+A3 - D3+F3 - E3+A3 -" },
            KP } },
    /* M16 STRESS TEST, 137 BPM: still a stress test for keys. PLAY alone sounds the whistle as a 16-step 1/8 minor-pentatonic run, one voice (about 74 % worst half on the
     * cpu_budget proxy); keys bring the pad's arp and chords on top as before */
    { 16, { { .trans = KEEP, .sdiv = "1/8", .sgate = 70,
              .steps = "C5 Eb5 G5 Bb5 G5 Eb5 C5 Bb4 G4 Bb4 C5 Eb5 G5 Bb5 C6 Bb5" },
            KP } },
    /* M10 / M11 / M12: the shared chromatic run C4..D#5 is replaced by a line of each one's own, under its arp */
    { 10, { KP, { .trans = KEEP, .steps = "C5A . . Eb5 . G5 . . Bb5A . . G5 . Eb5 . ." } } },
    { 11, { KP, { .trans = KEEP, .steps = "C4A . . . Eb4 . . Gb4 . . A4 . . . Gb4 ." } } },
    { 12, { KP, { .trans = KEEP, .steps = "C4A . C4 . . C4 . . G3 . . C4 . . Db4 ." } } },
};
