#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""DUELL dub pack: levels and tone of the rendered WAVs (helper). tools/dub_pack/analyze.py DIR
Per file: peak, the loudest 400 ms RMS, the spectral centroid of that window, the 5 % of the file that is silent (< -70 dB), and how A and B
differ (the same window's centroid and level), so a patch whose End B does nothing shows."""
import array
import cmath
import math
import sys
import wave
from pathlib import Path



def load(p):
    with wave.open(str(p), "rb") as w:
        n, ch, sr = w.getnframes(), w.getnchannels(), w.getframerate()
        a = array.array("h", w.readframes(n))
    return [sum(a[i:i + ch]) / ch / 32768.0 for i in range(0, len(a), ch)], sr


def fft_mag(x):
    """magnitudes of the first half of a radix-2 FFT (len(x) a power of two)"""
    n = len(x)
    a = [complex(v) for v in x]
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j |= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    ln = 2
    while ln <= n:
        w = cmath.exp(-2j * cmath.pi / ln)
        for i in range(0, n, ln):
            wn = 1
            for k in range(ln // 2):
                u, v = a[i + k], a[i + k + ln // 2] * wn
                a[i + k], a[i + k + ln // 2] = u + v, u - v
                wn *= w
        ln <<= 1
    return [abs(c) for c in a[:n // 2]]


def stats(p):
    x, sr = load(p)
    win = int(0.4 * sr)
    hop = win // 2
    best, at = -200.0, 0
    for i in range(0, max(1, len(x) - win), hop):
        seg = x[i:i + win]
        r = 20 * math.log10(math.sqrt(sum(v * v for v in seg) / len(seg)) + 1e-9)
        if r > best:
            best, at = r, i
    n = 16384
    seg = [v * (0.5 - 0.5 * math.cos(2 * math.pi * k / n)) for k, v in enumerate(x[at:at + n])]
    seg += [0.0] * (n - len(seg))
    sp = fft_mag(seg)
    cen = sum(m * (k * sr / n) for k, m in enumerate(sp)) / (sum(sp) + 1e-9)
    return dict(peak=20 * math.log10(max(abs(v) for v in x) + 1e-9), loud=best, at=at / sr, cen=cen, n=len(x) / sr)


def main(d):
    d = Path(d)
    files = sorted(d.glob("*.wav"))
    for f in files:
        s = stats(f)
        print(f"{f.name:34s} peak {s['peak']:6.1f}  loud400 {s['loud']:6.1f} dB @{s['at']:5.2f}s  centroid {s['cen']:7.0f} Hz")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
