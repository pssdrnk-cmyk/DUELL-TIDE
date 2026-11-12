/* SPDX-License-Identifier: GPL-3.0-only */
/* MUSIC's SHAPE already travels slowly in WNDR. Use its existing decay coordinate
 * for FM6 too: a longer shape decays the modulators faster. The carriers' envelopes
 * (the voice's level) move once, through the roll's ATTACK / DECAY / RELEASE on FM6's
 * AMP ENV knobs (eng_fm6.c fm6_env_apply), as on every engine. b10 (ROADMAP Later 19,
 * tide-morph audit finding 7): this used to move the carriers' rates and L3 as well, on
 * top of the knobs and past the roll's own carrier caps (morph.c fm_tag: L1..L3 +-1);
 * FM6 rolls at AMOUNT 40 moved the level -12 .. +8 dB (69.7 % within +-4.5 dB). Without it
 * 99.7 %, -4.3 .. +4.5 dB, as the other engines. No algorithm, ratios, output levels or
 * additional synthesis state. */
static int32_t fm_shape_value(const int8_t *base, int32_t decay)
{
    const int8_t (*b)[2] = MM_BAND[mm_role(base)];
    int32_t d = decay - base[P_DEC];
    int32_t reach = d > 0 ? b[MB_DEC][1] - base[P_DEC] : base[P_DEC] - b[MB_DEC][0];
    if (reach < MM_MIN) reach = MM_MIN;
    return clamp(d * 1000 / reach, -400, 400);
}
static void morph_fm_shape(const uint8_t *base, uint8_t *end, uint32_t carriers, int32_t shape)
{
    int32_t pos = shape > 0 ? shape : 0;
    for (uint32_t k = 0; k < 6u; k++) {
        uint32_t o = k * 9u + 1u;                        /* (R2) */
        if (!((carriers >> k) & 1u))
            end[o] = (uint8_t)clamp(base[o] + pos * 10 / 400, 0, 99);
    }
}
