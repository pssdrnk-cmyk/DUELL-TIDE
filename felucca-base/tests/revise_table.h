/* SPDX-License-Identifier: GPL-3.0-only */
/* tests/revise_multis.c's table (DUELL 1.1 U2 option B): the sequenced arrangements of the factory multis whose stored patterns were empty, and the
 * line of M10 .. M12. The tonal centre is C, the SEQUENCER arp's stand-in root (a PLAY with no key held plays the arp from C4, tide_seq.c): the
 * plain-pattern parts (chords, pads) are written in absolute notes, the SEQ parts as offsets from that root, so a held key moves them and the plain
 * parts stay (a live key plays on top). A part left out ({ .trans = KEEP }) stays as the record has it: M05 WHISPER PAD, M08 GEARBOX and
 * M13 SINGING BOWL are classic arps played live on purpose. */
#define KP { .trans = KEEP }
static const rv_t RV[] = {
    /* M03 PAD SPLIT, 97 BPM: the pad turns a four-chord loop (Cm9 Abmaj7 Fm9 Gm7) a bar each; EMBER GLOW glints on it with a 12-step line on 1/8 (a
     * bar and a half, so it phases against the four bars and returns after twelve) */
    { 3, { { .trans = KEEP, .sdiv = "1/1", .sgate = 120,
             .steps = "C3+G3+Bb3+D4 Ab2+Eb3+G3+C4 F2+C3+Eb3+G3 G2+D3+F3+Bb3" },
           { .amode = "SEQ", .arate = "1/8", .agate = 45, .trans = 0,
             .steps = "+12 . . +7 . +15 . . +10 . +14 ." } } },
    /* M05 WHISPER TIDE, 77 BPM: the digital pad breathes through four chords, two bars each (Fm9 Dbmaj9 Bbm7 Ebsus), a tide of eight bars; the random
     * WHISPER arp stays live over held keys */
    { 5, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
             .steps = "F2+Eb3+G3+C4 - Db3+Ab3+Eb4+F4 - Bb2+F3+Ab3+Db4 - Eb3+Bb3+Db4+F4 -" },
           KP } },
    /* M06 TINE STRINGS, 77 BPM: the strings hold Cm9 and Bbmaj9 two bars each; the tines play a two-bar Dorian lick on 1/8 under the dotted-quarter echo */
    { 6, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
             .steps = "C3+Bb3+D4+G4 - Bb2+A3+C4+F4 -" },
           { .amode = "SEQ", .arate = "1/8", .agate = 50, .trans = 0,
             .steps = "+12A . +7 . . +10 . . +3 . +7 . +10 . . ." } } },
    /* M07 FINAL REEL, 71 BPM: the brass pad in open fifths, C Am7 Fmaj7 Gsus4, two bars each; the strings shimmer a high C arpeggio on 1/4, the
     * accents on the downbeats bring them forward through the velocity crossfade. Their TRN +12 moved into the steps */
    { 7, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
             .steps = "C2+G2+C3+G3 - A2+E3+A3+C4 - F2+C3+F3+E4 - G2+D3+G3+C4 -" },
           { .amode = "SEQ", .arate = "1/4", .agate = 110, .trans = 0,
             .steps = "+12A +16 +19 +24 +19 +16 . ." } } },
    /* M08 HAZY TENSOR, 117 BPM: the ladder bass rolls a syncopated C line (3 + 3 + 2 + 3 + 3 + 2); GEARBOX stays a live random triplet arp. TRN -12
     * moved into the steps */
    { 8, { { .amode = "SEQ", .arate = "1/16", .agate = 70, .trans = 0,
             .steps = "-24A . . -24 . . -12 . -24 . . -21 . . -19 ." },
           KP } },
    /* M13 COLD CHAPEL, 71 BPM: the pad drones four open chords, two bars each (Dsus2 Bbmaj7#11 Gm9 Asus4); the bowl walks up the held chord, live */
    { 13, { { .trans = KEEP, .sdiv = "1/1", .sgate = 124,
              .steps = "D2+A2+E3+A3 - Bb2+F3+A3+E4 - G2+D3+F3+A3 - A2+E3+A3+D4 -" },
            KP } },
    /* M16 STRESS TEST, 137 BPM: still a stress test. PLAY now sounds the whistle alone as a steady 1/16 minor-pentatonic run (one voice at a time,
     * no key needed: the load of one held key, the unit's calibration); keys bring the pad's arp and chords on top as before */
    { 16, { { .trans = KEEP, .sdiv = "1/16", .sgate = 110,
              .steps = "C5 Eb5 G5 Bb5 G5 Eb5 C5 Bb4 G4 Bb4 C5 Eb5 G5 Bb5 C6 Bb5 G5 Eb5 C5 Eb5 G5 Bb5 G5 Eb5 C5 Bb4 G4 Bb4 C5 Eb5 G5 Bb5" },
            KP } },
    /* M10 / M11 / M12: the shared chromatic run C4..D#5 is replaced by a line of each one's own, under its arp */
    { 10, { KP, { .trans = KEEP, .steps = "C5A . . Eb5 . G5 . . Bb5A . . G5 . Eb5 . ." } } },
    { 11, { KP, { .trans = KEEP, .steps = "C4A . . . Eb4 . . Gb4 . . A4 . . . Gb4 ." } } },
    { 12, { KP, { .trans = KEEP, .steps = "C4A . C4 . . C4 . . G3 . . C4 . . Db4 ." } } },
};
