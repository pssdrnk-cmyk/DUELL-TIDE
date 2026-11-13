#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Needs numpy (outside the soundlab, which needs none). Do two sets of renders (int32 L/R pairs, 44100 Hz; regress REGRESS_DUMP / multi_render MULTI_DUMP) sound the same? Per differing render:
(a) the long-term third-octave spectrum (base-10 bands 40 Hz .. 16 kHz, Hann window, whole render, L+R), the worst band difference in dB over the bands within 40 dB of the render's loudest band;
(b) the integrated level (RMS of L+R) difference in dB.   usage: render_third_octave.py OLD_BUILD_LSB NEW_BUILD_LSB   (each has reg/ and mr/)
How the dumps are made: REGRESS_DUMP=DIR build/host/regress GOLDEN CPU, MULTI_DUMP=DIR build/host/multi_render_test GOLDEN, once on each tree. Pass used for the FM6 exp2 table (b11): worst band under 0.5 dB."""
import os, sys
import numpy as np
FS = 44100.0
old, new = sys.argv[1:3]
centers = [1000.0 * 10 ** (0.1 * k) for k in range(-14, 13)]          # 39.8 Hz .. 15.8 kHz
def bands(x):
    n = len(x)
    w = np.hanning(n)
    sp = np.abs(np.fft.rfft(x * w)) ** 2
    f = np.fft.rfftfreq(n, 1.0 / FS)
    out = []
    for c in centers:
        lo, hi = c * 10 ** -0.05, c * 10 ** 0.05
        m = (f >= lo) & (f < hi)
        out.append(sp[m].sum())
    return np.array(out)
rows = []
for kind in ("reg", "mr"):
    for n in sorted(os.listdir(os.path.join(new, kind))):
        a = np.fromfile(os.path.join(old, kind, n), dtype="<i4").astype(np.float64)
        b = np.fromfile(os.path.join(new, kind, n), dtype="<i4").astype(np.float64)
        if np.array_equal(a, b):
            continue
        am = a[0::2] + a[1::2]
        bm = b[0::2] + b[1::2]
        A, B = bands(am), bands(bm)
        dba = 10 * np.log10(A + 1e-30)
        dbb = 10 * np.log10(B + 1e-30)
        keep = dba >= dba.max() - 40.0
        d = np.abs(dbb - dba)[keep]
        worst_i = int(np.argmax(np.where(keep, np.abs(dbb - dba), -1)))
        lvl = abs(10 * np.log10((np.mean(bm ** 2) + 1e-9) / (np.mean(am ** 2) + 1e-9)))
        rows.append((float(d.max()), centers[worst_i], int(keep.sum()), lvl, kind, n))
rows.sort(reverse=True)
print("differing renders: %d; bands compared per render: %d..%d" % (len(rows), min(r[2] for r in rows), max(r[2] for r in rows)))
print("(a) worst third-octave band difference over all differing renders: %.3f dB (%s %s, band %.0f Hz)" % (rows[0][0], rows[0][4], rows[0][5], rows[0][1]))
print("(b) worst integrated level difference: %.4f dB" % max(r[3] for r in rows))
for r in rows[:8]:
    print("  band diff %.3f dB at %.0f Hz (level diff %.4f dB, %d bands)  %s %s" % (r[0], r[1], r[3], r[2], r[4], r[5]))
print("renders over 0.5 dB:", [(r[4], r[5], round(r[0], 2), round(r[1])) for r in rows if r[0] >= 0.5])
