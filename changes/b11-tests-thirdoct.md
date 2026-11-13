## changelog
### 1.0.0-b10
- **Added (developer tooling, b11 tests lane):** `tools/render_third_octave.py` (numpy): compares two sets of render dumps (`REGRESS_DUMP` / `MULTI_DUMP`) by long-term third-octave spectrum and level, for bit-level DSP changes where the waveforms drift apart in phase; recipe 10 of `tools/soundlab/README.md`.
