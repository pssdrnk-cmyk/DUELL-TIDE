/* SPDX-License-Identifier: GPL-3.0-only */
/* MUSIC's SHAPE already travels slowly in WNDR. Use its existing decay coordinate
 * for FM6 too: slower carrier attacks/releases, a held body, faster modulation
 * decay. No algorithm, ratios, output levels or additional synthesis state. */
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
    int32_t pos = shape > 0 ? shape : 0, neg = shape < 0 ? -shape : 0;
    for (uint32_t k = 0; k < 6u; k++) {
        uint32_t o = k * 9u;
        if ((carriers >> k) & 1u) {
            int32_t floor = base[o] < 35 ? base[o] : 35;
            int32_t release = base[o + 3u] < 55 ? base[o + 3u] : 55;
            int32_t body = base[o + 4u] > base[o + 5u] ? base[o + 4u] : base[o + 5u];
            if (body > 70) body = 70;
            if (body < base[o + 6u]) body = base[o + 6u];
            end[o] = (uint8_t)clamp(base[o] - pos * 60 / 400 + neg * 20 / 400, floor, 99);
            end[o + 1u] = (uint8_t)clamp(base[o + 1u] - pos * 12 / 400 + neg * 12 / 400, 0, 99);
            end[o + 2u] = (uint8_t)clamp(base[o + 2u] - pos * 8 / 400 + neg * 12 / 400, 0, 99);
            end[o + 3u] = (uint8_t)clamp(base[o + 3u] - pos * 15 / 400 + neg * 15 / 400, release, 99);
            end[o + 6u] = (uint8_t)clamp(base[o + 6u] + (body - base[o + 6u]) * pos / 400 - neg * 20 / 400, 0, 99);
        } else {
            end[o + 1u] = (uint8_t)clamp(base[o + 1u] + pos * 10 / 400, 0, 99);
        }
    }
}
