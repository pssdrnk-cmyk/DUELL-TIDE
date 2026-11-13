## changelog
### 1.0.0-b10
- **Fixed (stability, from DUELL 1.2 by OSCILL/R):** while it waits for work, the second core no longer polls in a tight loop. It pauses about 400 cycles between two looks. On DUELL 1.2's bench, the tight loop made the first core lose register writes and froze WHEEL with 8 voices (4 crashes in 5 long runs, 0 in 10 with the pause). Core 1 now sees a job at most a few microseconds later.
