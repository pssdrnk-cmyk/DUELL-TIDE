## changelog
### 1.0.0-b10
- **Added (developer tooling, b11 tests lane):** the store's cross-version test (`storexver`) now holds records written by b8 rc1 / rc2 (made on `fork/b8-acid-integ` ebc63ca by `tests/store_xver8_gen.c`): the ones without ANALOG WAVE load to b8's values and stay the bytes they were; the ones with WAVE 3 / 7 / 8 (4 bits, no width flag) are the known B9-STORE ambiguity and are pinned (4 of 5 accepted, the WAVE 7 one refused, none loads to b8's values).
