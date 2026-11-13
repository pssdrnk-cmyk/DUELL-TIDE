/* SPDX-License-Identifier: GPL-3.0-only */
/* emu_run: a scripted session on the emulator's device (web/emu/felucca_web.c built natively, as native_check.c does: the same sources and the same
 * calls as the WebAssembly build, whose samples CI checks equal to the native ones, so the screens and the audio are what the browser shows and plays).
 *   build:  cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o build/emu/emu_run web/emu/emu_run.c -lm     (tools/soundlab: `sh web/emu/emu_run.sh` does it)
 *   run:    build/emu/emu_run SCRIPT.txt --out DIR
 * Script: one command a line, # starts a comment.
 *   wait MS                  run the device for MS milliseconds of audio time (it draws, scans the panel, plays)
 *   button NAME [tap|down|up]   NAME: FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCTDN OCTUP (tap: press, 80 ms, release, 120 ms; the default)
 *   keys NOTE...             hold keys: names F3..G5 (the 27 keys; # for sharps: F#3), or "off"
 *   enc ROLE N               turn an encoder N steps (negative: left): SELECT ALGO PRESETS K1 K2 K3 K4  (each step: 40 ms)
 *   master V                 the MASTER pot 0..1023
 *   midi STATUS D1 D2        a channel message (decimal or 0x..)
 *   shot NAME                DIR/NAME.png (240 x 240, the panel) after 40 ms to draw; "NAME crc32-of-the-RGB565-frame" is added to DIR/screens.txt
 *   trace NAME [N]           the next N screen transfers (default 300), one picture after each: DIR/NAME_000.png .. and DIR/NAME.txt (n x y w h fill|blit),
 *                            so a page change's in-between states show (what the panel holds while the next transfer is sent)
 *   rec NAME | rec off       DIR/NAME.wav (stereo 16-bit 44100 Hz) and DIR/NAME.json (for `soundlab measure DIR/NAME`) while recording
 * A fresh flash boots to the first-start GUIDE: the script starts with `boot` (2.2 s, then OCTUP to leave the GUIDE as emu_test.mjs does), or `boot keep` (no OCTUP). */
static void trace_xfer(unsigned x, unsigned y, unsigned w, unsigned h, int blit);
#define WEB_LCD_HOOK(x, y, w, h, b) trace_xfer(x, y, w, h, b)
#include "felucca_web.c"
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(d) _mkdir(d)
#else
#define MKDIR(d) mkdir(d, 0777)
#endif

static const char *OUT = ".";
static FILE *wavf, *scrf;
static char wavname[512];
static uint32_t wav_frames;
static uint32_t btn_state;
static uint32_t crc_t[256];

static uint32_t crc32_upd(uint32_t c, const unsigned char *p, size_t n)
{
    size_t i;
    if (!crc_t[1]) {
        uint32_t k, j, x;
        for (k = 0; k < 256; k++) {
            x = k;
            for (j = 0; j < 8; j++)
                x = x & 1 ? 0xEDB88320u ^ (x >> 1) : x >> 1;
            crc_t[k] = x;
        }
    }
    c = ~c;
    for (i = 0; i < n; i++)
        c = crc_t[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void be32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v; }
static void chunk(FILE *f, const char *type, const unsigned char *d, uint32_t n)
{
    unsigned char h[8], t[4];
    uint32_t c;
    be32(h, n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n)
        fwrite(d, 1, n, f);
    c = crc32_upd(0, (const unsigned char *)type, 4);
    c = n ? crc32_upd(c, d, n) : c;
    be32(t, c);
    fwrite(t, 1, 4, f);
}
static void write_png(const char *fn, const unsigned char *rgb, uint32_t w, uint32_t h)   /* uncompressed (stored) deflate: no zlib needed */
{
    FILE *f = fopen(fn, "wb");
    uint32_t rowb = w * 3u + 1u, rawn = rowb * h, y, i, pos = 0, a = 1, b = 0, nb;
    unsigned char *raw = (unsigned char *)malloc(rawn), *z, ihdr[13];
    if (!f)
        return;
    for (y = 0; y < h; y++) {
        raw[y * rowb] = 0;
        memcpy(raw + y * rowb + 1, rgb + (size_t)y * w * 3u, w * 3u);
    }
    for (i = 0; i < rawn; i++) {
        a = (a + raw[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    nb = (rawn + 65534u) / 65535u;
    z = (unsigned char *)malloc(2u + rawn + nb * 5u + 4u);
    z[pos++] = 0x78;
    z[pos++] = 0x01;
    for (i = 0; i < nb; i++) {
        uint32_t n = rawn - i * 65535u > 65535u ? 65535u : rawn - i * 65535u;
        z[pos++] = i + 1 == nb;
        z[pos++] = (unsigned char)n;
        z[pos++] = (unsigned char)(n >> 8);
        z[pos++] = (unsigned char)~n;
        z[pos++] = (unsigned char)(~n >> 8);
        memcpy(z + pos, raw + i * 65535u, n);
        pos += n;
    }
    be32(z + pos, (b << 16) | a);
    pos += 4;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    be32(ihdr, w);
    be32(ihdr + 4, h);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, pos);
    chunk(f, "IEND", 0, 0);
    fclose(f);
    free(raw);
    free(z);
}
static void le32(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); fputc(v >> 16 & 255, f); fputc(v >> 24 & 255, f); }
static void le16(FILE *f, uint32_t v) { fputc(v & 255, f); fputc(v >> 8 & 255, f); }

static void wav_open(const char *name)
{
    snprintf(wavname, sizeof wavname, "%s/%s", OUT, name);
    {
        char fn[600];
        snprintf(fn, sizeof fn, "%s.wav", wavname);
        wavf = fopen(fn, "wb");
    }
    if (!wavf)
        return;
    fwrite("RIFF", 1, 4, wavf);
    le32(wavf, 0);
    fwrite("WAVEfmt ", 1, 8, wavf);
    le32(wavf, 16);
    le16(wavf, 1);
    le16(wavf, 2);
    le32(wavf, 44100);
    le32(wavf, 44100 * 4);
    le16(wavf, 4);
    le16(wavf, 16);
    fwrite("data", 1, 4, wavf);
    le32(wavf, 0);
    wav_frames = 0;
}
static void wav_close(void)
{
    char fn[600];
    FILE *j;
    if (!wavf)
        return;
    fseek(wavf, 4, SEEK_SET);
    le32(wavf, 36 + wav_frames * 4u);
    fseek(wavf, 40, SEEK_SET);
    le32(wavf, wav_frames * 4u);
    fclose(wavf);
    wavf = 0;
    snprintf(fn, sizeof fn, "%s.json", wavname);
    j = fopen(fn, "wb");
    if (j) {                                           /* what `soundlab measure` reads: no voice log (the device plays it), 120 bpm bars by time */
        fprintf(j, "{\"tool\":\"emu_run\",\"commit\":\"\",\"sample_rate\":44100,\"bars\":%u,\"bpm\":120,\"source\":{\"multi\":0,\"name\":\"emulator\",\"sound\":null},"
                   "\"settings\":{},\"voices\":[],\"events\":[],\"blocks\":{}}\n", (wav_frames / 88200u) ? (wav_frames / 88200u) : 1u);
        fclose(j);
    }
}
static void render(double ms)
{
    uint32_t n = (uint32_t)(ms * 44.1 + 0.5), k, i;
    for (k = 0; k < n; k += 128u) {
        uint32_t m = n - k < 128u ? n - k : 128u;
        web_render(m);
        for (i = 0; wavf && i < m; i++) {
            float l = out_l[i], r = out_r[i];
            int a = (int)(l * 32767.0f + (l < 0 ? -0.5f : 0.5f)), b = (int)(r * 32767.0f + (r < 0 ? -0.5f : 0.5f));
            a = a > 32767 ? 32767 : a < -32768 ? -32768 : a;
            b = b > 32767 ? 32767 : b < -32768 ? -32768 : b;
            le16(wavf, (uint32_t)(a & 0xFFFF));
            le16(wavf, (uint32_t)(b & 0xFFFF));
            wav_frames++;
        }
    }
}
static void shot(const char *name)
{
    static unsigned char rgb[240 * 240 * 3];
    uint32_t i, crc = 0;
    char fn[600];
    render(40);
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t p = web_fb[i];
        uint16_t v = (uint16_t)((p >> 8) | (p << 8));                     /* the panel gets the high byte first */
        rgb[i * 3] = (unsigned char)(((v >> 11) & 31u) * 255u / 31u);
        rgb[i * 3 + 1] = (unsigned char)(((v >> 5) & 63u) * 255u / 63u);
        rgb[i * 3 + 2] = (unsigned char)((v & 31u) * 255u / 31u);
        crc = crc32_upd(crc, (const unsigned char *)&web_fb[i], 2);
    }
    snprintf(fn, sizeof fn, "%s/%s.png", OUT, name);
    write_png(fn, rgb, 240, 240);
    if (scrf)
        fprintf(scrf, "%s %08x\n", name, crc);
}

static char trace_name[256];
static int trace_left, trace_n;
static FILE *trace_f;
static void fb_rgb(unsigned char *rgb)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t p = web_fb[i];
        uint16_t v = (uint16_t)((p >> 8) | (p << 8));
        rgb[i * 3] = (unsigned char)(((v >> 11) & 31u) * 255u / 31u);
        rgb[i * 3 + 1] = (unsigned char)(((v >> 5) & 63u) * 255u / 63u);
        rgb[i * 3 + 2] = (unsigned char)((v & 31u) * 255u / 31u);
    }
}
static void trace_xfer(unsigned x, unsigned y, unsigned w, unsigned h, int blit)
{
    static unsigned char rgb[240 * 240 * 3];
    char fn[600];
    if (trace_left <= 0)
        return;
    fb_rgb(rgb);
    snprintf(fn, sizeof fn, "%s/%s_%03d.png", OUT, trace_name, trace_n);
    write_png(fn, rgb, 240, 240);
    if (trace_f)
        fprintf(trace_f, "%d %u %u %u %u %s\n", trace_n, x, y, w, h, blit ? "blit" : "fill");
    trace_n++;
    if (!--trace_left && trace_f) {
        fclose(trace_f);
        trace_f = 0;
    }
}

static const char *BN[] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC", "OCTDN", "OCTUP"};
static const char *EN[] = {"SELECT", "ALGO", "PRESETS", "K1", "K2", "K3", "K4"};
static int idx_of(const char **t, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!strcmp(t[i], s))
            return i;
    return -1;
}
static int key_of(const char *s)                                          /* F3 = 0 ... G5 = 26 (MIDI 53..79) */
{
    static const int base[7] = {9, 11, 0, 2, 4, 5, 7};                    /* A B C D E F G */
    int n, o;
    char c = (char)toupper((unsigned char)s[0]);
    const char *q = s + 1;
    if (c < 'A' || c > 'G')
        return -1;
    n = base[c - 'A'];
    if (*q == '#')
        n++, q++;
    else if (*q == 'b')
        n--, q++;
    if (!isdigit((unsigned char)*q))
        return -1;
    o = atoi(q);
    n = 12 * (o + 1) + n - 53;
    return n >= 0 && n < 27 ? n : -1;
}

int main(int argc, char **argv)
{
    FILE *sf;
    char line[512], fn[600];
    int ln = 0, i, rc = 0;
    if (argc < 2) {
        fprintf(stderr, "usage: emu_run SCRIPT.txt [--out DIR]\n");
        return 2;
    }
    for (i = 2; i + 1 < argc; i++)
        if (!strcmp(argv[i], "--out"))
            OUT = argv[i + 1];
    sf = fopen(argv[1], "r");
    if (!sf) {
        fprintf(stderr, "emu_run: cannot read %s\n", argv[1]);
        return 2;
    }
    MKDIR(OUT);
    snprintf(fn, sizeof fn, "%s/screens.txt", OUT);
    scrf = fopen(fn, "wb");
    web_nor_erase();
    web_boot();
    while (fgets(line, sizeof line, sf)) {
        char *t[8], *p = line;
        int nt = 0;
        ln++;
        for (p = strtok(line, " \t\r\n"); p && nt < 8; p = strtok(0, " \t\r\n"))
            t[nt++] = p;
        if (!nt || t[0][0] == '#')
            continue;
        if (!strcmp(t[0], "boot")) {
            render(2200);
            if (nt < 2 || strcmp(t[1], "keep")) {                          /* leave the first-start GUIDE: OCT+ (emu_test.mjs) */
                web_buttons(1u << 13);
                render(60);
                web_buttons(0);
                render(300);
            }
        } else if (!strcmp(t[0], "wait") && nt > 1) {
            render(atof(t[1]));
        } else if (!strcmp(t[0], "button") && nt > 1) {
            int b = idx_of(BN, 14, t[1]);
            const char *m = nt > 2 ? t[2] : "tap";
            if (b < 0) {
                fprintf(stderr, "emu_run: line %d: button %s\n", ln, t[1]);
                rc = 1;
                continue;
            }
            if (!strcmp(m, "down") || !strcmp(m, "tap")) {
                btn_state |= 1u << b;
                web_buttons(btn_state);
                render(80);
            }
            if (!strcmp(m, "up") || !strcmp(m, "tap")) {
                btn_state &= ~(1u << b);
                web_buttons(btn_state);
                render(120);
            }
        } else if (!strcmp(t[0], "keys")) {
            uint32_t mask = 0;
            for (i = 1; i < nt; i++) {
                int k = strcmp(t[i], "off") ? key_of(t[i]) : -2;
                if (k == -1) {
                    fprintf(stderr, "emu_run: line %d: key %s\n", ln, t[i]);
                    rc = 1;
                } else if (k >= 0) {
                    mask |= 1u << k;
                }
            }
            web_keys(mask);
            render(60);
        } else if (!strcmp(t[0], "enc") && nt > 2) {
            int r = idx_of(EN, 7, t[1]), n = atoi(t[2]), s = n < 0 ? -1 : 1;
            if (r < 0) {
                fprintf(stderr, "emu_run: line %d: encoder %s\n", ln, t[1]);
                rc = 1;
                continue;
            }
            for (i = 0; i < (n < 0 ? -n : n); i++) {
                web_enc((uint32_t)r, s);
                render(40);
            }
            render(60);
        } else if (!strcmp(t[0], "master") && nt > 1) {
            web_master((uint32_t)atoi(t[1]));
            render(40);
        } else if (!strcmp(t[0], "midi") && nt > 3) {
            web_midi((uint32_t)strtoul(t[1], 0, 0), (uint32_t)strtoul(t[2], 0, 0), (uint32_t)strtoul(t[3], 0, 0));
            render(20);
        } else if (!strcmp(t[0], "shot") && nt > 1) {
            shot(t[1]);
        } else if (!strcmp(t[0], "trace") && nt > 1) {
            char fn[600];
            snprintf(trace_name, sizeof trace_name, "%s", t[1]);
            trace_left = nt > 2 ? atoi(t[2]) : 300;
            trace_n = 0;
            if (trace_f)
                fclose(trace_f);
            snprintf(fn, sizeof fn, "%s/%s.txt", OUT, t[1]);
            trace_f = fopen(fn, "w");
        } else if (!strcmp(t[0], "rec") && nt > 1) {
            wav_close();
            if (strcmp(t[1], "off"))
                wav_open(t[1]);
        } else {
            fprintf(stderr, "emu_run: line %d: cannot read \"%s\"\n", ln, t[0]);
            rc = 1;
        }
    }
    wav_close();
    if (scrf)
        fclose(scrf);
    fclose(sf);
    return rc;
}
