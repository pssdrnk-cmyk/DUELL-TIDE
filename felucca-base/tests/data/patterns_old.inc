/* SP3 reference: the factory pattern table as it was before the notes were trimmed to LEN (b8). tests/patpack_test.c compares engines.c with it. */
#define ROOT 60
#define O(x) (ROOT + (x))
#define T_ 4
#define A_ 1
#define S_ 2
#define OLD_NPAT_STEPS 32u
static const struct {
    const char *name;
    uint32_t fl[3];                              /* the flags as bit masks, bit i step i (1.0.0-b7, SP3: 52 B a pattern, was 72): fl[0] accent, fl[1] slide, fl[2] tie; pat_fl */
    uint8_t len, div;
    uint8_t swing, gate;                         /* 0 = the default (SWG 50 %, GATE 64) */
    uint8_t note[OLD_NPAT_STEPS];
} OLD_PATTERNS[] = {
    /* 1 dub techno chord: stabs on the off-beat 8ths, two of them held on ties for the echo to carry, the last a minor 3rd up */
    {"DUB CHORD", {1028u, 0u, 6168u}, 16, 2, 0, 0, {0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(3), 0}},
    /* 2 deep house shuffle: the off-beat 8th and a swung 16th three steps on (the e of 2), the last hit a whole tone down (SWING 60) */
    {"DEEP SHUFFLE", {1028u, 0u, 0u}, 16, 2, 60, 36, {0, 0, O(0), 0, 0, O(0), 0, 0, 0, 0, O(0), 0, 0, O(-2), 0, 0}},
    /* 3 dub offbeat stabs: the off-beat 8ths only, accents on the 2nd and 4th (all hits on even steps, so no SWING) */
    {"DUB OFFBEAT", {16448u, 0u, 0u}, 16, 2, 0, 42, {0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0}},
    /* 4 house stabs: off-beat 8ths with a double hit before the 2nd beat and the 4th, accents on 7 and 15 (SWING 56) */
    {"HOUSE STAB", {16448u, 0u, 0u}, 16, 2, 56, 30, {0, 0, O(0), O(0), 0, 0, O(0), 0, 0, 0, O(0), O(0), 0, 0, O(0), 0}},
    /* 5 dotted delay figure: hits 3-4-3-4 sixteenths apart, root C then down a major 3rd, a 5th and a 4th (SWING 52) */
    {"DOT DELAY", {129u, 0u, 0u}, 16, 2, 52, 36, {O(0), 0, 0, O(0), 0, 0, 0, O(-4), 0, 0, O(-7), 0, 0, 0, O(-5), 0}},
    /* 6 straight 8th-note pulse (new wave / driving rock bass), the last 8th on the 5th */
    {"8TH PULSE", {4369u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(0), 0, O(7), 0}},
    /* 7 minimal: a repeating cell where one upper note changes each bar */
    {"GLASS LOOP", {1u, 0u, 0u}, 16, 2, 0, 0, {O(0), O(7), O(3), O(7), O(0), O(7), O(3), O(7), O(0), O(8), O(3), O(8), O(0), O(8), O(3), O(8)}},
    /* 8 Berlin school run: the root as a pedal on every other 16th, a figure climbing and falling between */
    {"BERLIN RUN", {4369u, 0u, 0u}, 16, 2, 0, 0, {O(0), O(7), O(0), O(10), O(0), O(12), O(0), O(10), O(0), O(7), O(0), O(3), O(0), O(5), O(0), O(3)}},
    /* 9 Detroit / house stab rhythm: syncopated chord hits (3-3-4-3-3 feel), root, minor 3rd, 5th */
    {"DETROIT STAB", {65u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, 0, O(3), 0, 0, O(7), 0, 0, 0, O(0), 0, 0, O(3), 0, 0}},
    /* 10 minimal 12: a three-beat loop for polymeter against a 16-step part, a drop of a whole tone held on a tie */
    {"MINIMAL 12", {1u, 0u, 1024u}, 12, 2, 0, 40, {O(0), 0, 0, O(0), 0, 0, O(0), 0, 0, O(-2), 0, 0}},
    /* 11 dub half-time: few long notes, the root, a 5th below after the space */
    {"DUB HALFTIME", {1u, 0u, 24702u}, 16, 2, 0, 0, {O(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, O(-5), 0, 0, 0}},
    /* 12 reggae skank: the off-beat of beats 2 and 4, a bubbling 5th in between */
    {"SKANK", {16448u, 0u, 0u}, 16, 2, 0, 0, {0, 0, 0, 0, 0, 0, O(0), 0, 0, 0, 0, O(7), 0, 0, O(0), 0}},
    /* 13 house 3-3-5-3-2: the comp, stabs on 1, a, & of 2, a of 3, & of 4 (SWING 54) */
    {"DEEP 332", {65u, 0u, 0u}, 16, 2, 54, 40, {O(0), 0, 0, O(0), 0, 0, O(0), 0, 0, 0, 0, O(3), 0, 0, O(0), 0}},
    /* 14 minor pentatonic riff: root, b3, 4, 5, b7 (and the b7 below), a slide into the 4th */
    {"PENTA RIFF", {1057u, 8u, 0u}, 16, 2, 0, 0, {O(0), 0, O(3), O(5), 0, O(7), 0, O(5), O(3), 0, O(0), 0, O(-2), 0, O(0), 0}},
    /* 15 pad breath: two long tied chords a bar, the second a major 3rd down, a step of air before each (GATE 100) */
    {"PAD BREATH", {257u, 0u, 32382u}, 16, 2, 0, 100, {O(0), 0, 0, 0, 0, 0, 0, 0, O(-4), 0, 0, 0, 0, 0, 0, 0}},
    /* 16 synthpop hook on dotted rhythms */
    {"DOTTED HOOK", {513u, 0u, 0u}, 16, 2, 0, 0, {O(12), 0, 0, O(10), 0, 0, O(7), 0, 0, O(5), 0, 0, O(7), 0, 0, 0}},
    /* 17 half-time pad: one chord a bar held on ties, a step of air, then a whole tone down (two bars, GATE 100) */
    {"HALF PAD", {65537u, 0u, 2147385342u}, 32, 2, 0, 100, {O(0), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, O(-2), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    /* 18 Latin: the anticipated bass of salsa and son */
    {"TUMBAO", {16448u, 0u, 0u}, 16, 2, 0, 0, {0, 0, 0, 0, 0, 0, O(0), 0, 0, 0, 0, 0, O(7), 0, O(12), O(7)}},
    /* 19 krautrock: driving 8ths between root and octave */
    {"MOTORIK", {4369u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, O(0), 0, O(12), 0, O(0), 0, O(0), 0, O(0), 0, O(12), 0, O(0), 0}},
    /* 20 jazz: a walking bass in eight 8ths up and down a minor scale */
    {"WALK LINE", {17u, 0u, 0u}, 8, 1, 0, 0, {O(0), O(3), O(5), O(6), O(7), O(5), O(3), O(2)}},
    /* 21 UK garage: the skipping sub bass of 2-step */
    {"2-STEP SUB", {1089u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, 0, O(0), 0, 0, O(7), 0, 0, 0, O(0), 0, 0, O(3), 0, O(5)}},
    /* 22 neo-soul: sparse low ghost notes with one accent */
    {"GHOST FUNK", {1u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, 0, O(12), 0, 0, 0, O(0), 0, 0, O(10), 0, 0, O(0), 0, 0}},
    /* 23 blues 12/8: a rolling triplet groove */
    {"TRIPLET SWAY", {585u, 0u, 0u}, 12, 4, 0, 0, {O(0), 0, O(7), O(3), 0, O(7), O(0), 0, O(7), O(5), 0, O(7)}},
    /* 24 dub progression: off-beat stabs over two bars, the root, then a major 3rd down, then a 4th (all hits on even steps, so no SWING) */
    {"DUB PROG", {67372036u, 0u, 0u}, 32, 2, 0, 42, {0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(0), 0, 0, 0, O(-4), 0, 0, 0, O(-4), 0, 0, 0, O(-5), 0, 0, 0, O(-5), 0}},
    /* 25 90s rave: stabs on the one, the a of one and the and of two */
    {"RAVE STABS", {36993u, 0u, 0u}, 16, 2, 0, 0, {O(7), 0, 0, O(7), 0, 0, 0, O(10), 0, 0, 0, 0, O(7), 0, 0, O(12)}},
    /* 26 electric-piano comping: short chords on a 3-3-2 rhythm with ties */
    {"EP COMP", {257u, 0u, 8226u}, 16, 2, 0, 0, {O(0), 0, 0, 0, O(7), 0, 0, 0, O(3), 0, 0, 0, O(10), 0, 0, 0}},
    /* 27 synthwave lead hook */
    {"NEON HOOK", {257u, 0u, 1536u}, 16, 2, 0, 0, {O(12), 0, O(10), 0, O(7), 0, O(10), 0, O(12), 0, 0, 0, O(15), 0, O(12), 0}},
    /* 28 pop / minor: a call and an answer (32 steps) */
    {"NIGHT CALL", {16843009u, 0u, 234881536u}, 32, 2, 0, 0, {O(7), 0, O(7), O(10), 0, O(12), 0, O(10), O(7), 0, 0, 0, 0, 0, 0, 0, O(5), 0, O(5), O(8), 0, O(10), 0, O(8), O(3), 0, 0, 0, 0, 0, 0, 0}},
    /* 29 ambient: two long tied chord roots a bar, a root and its 5th */
    {"TIED ROOTS", {257u, 0u, 65278u}, 16, 2, 0, 0, {O(0), 0, 0, 0, 0, 0, 0, 0, O(7), 0, 0, 0, 0, 0, 0, 0}},
    /* 30 FM marimba ostinato: the root alternating with the octave and a moving third */
    {"DX OSTINATO", {0u, 0u, 0u}, 16, 2, 0, 0, {O(0), 0, O(12), 0, O(7), 0, O(12), 0, O(3), 0, O(12), 0, O(5), 0, O(12), 0}},
    /* 31 FM bells and tines: sparse tones in 5ths and octaves with ties */
    {"DX BELLS", {257u, 0u, 8192u}, 16, 2, 0, 0, {O(12), 0, 0, 0, O(19), 0, 0, 0, O(16), 0, 0, 0, O(24), 0, 0, 0}},
    /* 32 prog: a riff in 7/8 (14 steps) */
    {"SEVEN EIGHT", {65u, 0u, 0u}, 14, 2, 0, 0, {O(0), 0, O(0), 0, O(7), 0, O(3), 0, O(0), 0, O(10), 0, O(7), 0}},
};
#undef S_
#undef A_
#undef T_
#undef O
#undef ROOT
