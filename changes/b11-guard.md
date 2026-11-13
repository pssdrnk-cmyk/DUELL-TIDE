## changelog
### 1.0.0-b10
- **Fixed (clicks at a chord's start, from DUELL 1.2 by OSCILL/R):** the audio now checks itself inside every half-buffer. After each block it projects when the half will end. If that is past 96 % of the time, the two oldest released notes (their tails) fade at once, so the half finishes in time instead of missing its deadline and clicking. Held notes are never touched, and nothing changes in a half that would have been in time. The ease ladder still acts only on whole halves.
