## changelog
### Next (in progress)
- **Fix (owner 16 November, "the cpu is skipping hard" on M07 HSE CHICAGO, AUTO TIDE TRAVEL 32):** TRAVEL now keeps each part's own envelope, SAT, sends, SPREAD,
 INSERT and EQ when it moves the part to a factory sound; the sound's instrument still travels. On FM6 no carrier of the destination rings longer
 than its loudest. The note budget now charges SUB, CYCLE, the matrix slots, their LFOs and KEY TRK from what the morph's ends play, not only from the part's own values, so
 TRAVEL thins its notes where the destinations really cost more. M07 TRAVEL 32 on the unit: 94-119 missed blocks in 3 minutes on 1.0.0, 45-61 with the fix (work in progress; the reverb on core 2 is the next step);
 EVOLVE unchanged (0 missed, 5 notes). Costs about 172 B of flash (800 B free after it).
