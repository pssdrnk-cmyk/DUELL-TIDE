# TIDE next proposals: syncopation, call and response, the arc

> Design record. The feature is built; what ships is described in [TIDE_GUIDE.md](../TIDE_GUIDE.md) (SYNC, TURNS LEAD / ECHO, ARC and STYLE). Names, numbers and estimates below are as of the date of writing. Kept at this path because source comments cite it.

Status: design proposal, 3 November 2026; owner decisions recorded the same day (see the end). **Built since (branch `integ`, 4 November 2026): step 1 SYNC (`1344f53`), step 2 ANSWER, i.e. TURNS LEAD / ECHO (`e691df2`), step 3 ARC (`5f2ff83`) and step 4 STYLES (`9e3b716`), with TURNS and POLY; not yet on the unit. The text below is the design as proposed, with the names and values of that time; the TIDE guide and README describe what the code does.** It is based on fork/jam11 `30212a1`. It also uses the uncommitted M8a TURNS and generative POLY work on branch `tide-turns` (`seq_poly.c`, `sqm_turns`, `sqm_auto`, `tide_store_load_min`), so recheck those names once that branch lands. Background and earlier reasoning are in [TIDE_MUSICAL_DESIGN.md](TIDE_MUSICAL_DESIGN.md) (M7, M8, M9). The delivery order is in the roadmap's "Musical delivery order".

This document covers four steps, in this order (step 4, STYLES, was added on 3 November). Each one is listened to and accepted before the next is built:

| Step | What the owner hears | New TIDE SETTINGS row | Flash (est.) | RAM (est.) |
|---|---|---|---:|---:|
| 1. M7a/b SYNC | Movable chord hits drift towards the "and" and the occasional push into the beat | **SYNC** OFF / SOFT / PUSH | 300–450 B | 0 B |
| 2. M8b/c ANSWER | One part leads for two bars while the other thins out, then they swap; later the answer echoes the call's rhythm | none: the **TURNS** row gains two values | 450–750 B | 16–20 B |
| 3. M4/M9 ARC | 30–60-bar shapes: build, peak, optional 2-bar drop (automatic or by hand), sparse breakdown, recovery | **ARC** OFF / RISE / DROP | 1.1–1.7 KB | 9–13 B |
| 4. STYLES | One row picks a genre: DUB / DEEP / MINIMAL / AMBIENT / PEAK set the other rows together | **STYLE** (top row) | 250–370 B | 1 B |

All estimates are for the linked application image and have not been built. A fresh TIDE build of jam11 + the GLO fix (with boot-lz `ecd2918`), measured 3 November 2026: application 556,548 of the 581,564 B slot (about 25.0 KB free); RAM .data+.bss 67,936 of 73,728 B (5.8 KB free); pool 323,840 of 368,640 B. Pending branches (MULTIS preview, TURNS/POLY, U1, F05–F07, M3) take roughly 3–5 KB of that flash. All three steps together (about 2–3 KB flash, under 40 B RAM) fit without S0c/S0e or a drop.

## Common ground

### One metric-position helper (used by all three)

The current code decides "beat or not" with the local index `i % 4` in `rhythm_mutate()`, `phrase_source()`, `sqm_build()` (DENS/ACC weights) and the VARY thresholds `idx / 4u` in `sqm_step()`. That is only true for a 16-step loop at 1/16. Proposed: one main-loop function, `tide_metric(t, idx)` in `seq_morph.c`. It returns where *stored* step `idx` sits in the beat: BEAT, E, AND, A, OFFGRID or NONE (grid not supported).

- **Division** (`P_SDIV`; `P_ARATE` for ARP SEQ, as `phrase_duration()` already does). Count position in 32nds: 1/8 (id 1) = 4 per step, 1/16 (id 2) = 2, 1/32 (id 3) = 1. Within the beat, `p32 % 8`: 0 BEAT, 4 AND, 6 A (push), 2 E, odd OFFGRID. Triplets (ids 4, 5), slow rates (6–9) and the 1/48 family (10+) return NONE, so those grids behave exactly as today. This is the "unsupported grids unchanged" rule from the roadmap.
- **Phase**: the stored loop must last a whole number of beats (`len × 32nds per step` divisible by 8). PLAY starts every loop on a beat (`TIDE_SEQ_START`), so step positions are then fixed against the beat. Otherwise the result is NONE. With POLY, the helper always uses the **full stored length** (`tide_poly_full()`), never `tide_slen()`. Phrase edits are made on the stored steps and heard against the full loop. During a POLY stretch the rotation already produces the syncopation, and the full loop locks back to the bar afterwards (`tide_poly_wrap()`).
- **Swing**: `step_samples()` makes odd 16ths late by `sw × period / 250` (track + global swing, at most 100). AND sits on even 16ths and is never swung. A (the push) is odd, so heavy swing pulls it towards the next beat, where it sounds like a flam with the kick. Proposed rule: when the summed swing is ≥ 60 (about 65 %), A is scored like E. A TR-6S's own swing is not visible to the FM-1. The listening test asks the owner to match swing on both machines.
- A real beat is never a move *destination*, even where `i % 4 != 0`. For example, at 1/8 every second step is a beat. The original anchors (`i % 4 == 0`) still never move. Their rule is unchanged, so a motif is not forced onto global downbeats.

Cost: about 120–180 B flash, no RAM. M7 uses it for weights, M8 for its thinning ranks and M9 for its density choices.

### The TIDE SETTINGS record

`tide_ui.c` stores the fields `TS_MODE … TS_WASH` (7 bytes) in object 48 (`tide_store.c`, `TIDE_STORE_MAX` 32). `tide-turns` appends `TS_TURNS` and `TS_POLY`, giving 9 bytes. It also adds `tide_store_load_min(b, TS_N, TS_OLD)`, which reads an older, shorter record and leaves the defaults the caller has already filled in for the missing fields. This proposal appends **two** bytes, `TS_SYNC` and `TS_ARC`, giving 11 of 32 bytes and leaving 21 spare. TURNS reuses its byte with more values.

- `TIDE_STORE_VER` stays 1: fields are only appended. Keep `TS_OLD` = 7 as the minimum. A 7-byte (installed), 9-byte (`tide-turns`) or 11-byte record must all load.
- Each new field is checked on load against its value count. A value out of range takes the default instead of being masked (`& 1u` would turn a future TURNS value 3 into 1). Change the TURNS check from `& 1u` to `< 4u ? v : default`.
- Going back to older firmware is safe. The installed loader needs ≥ 7 bytes and reads only those 7. The `tide-turns` loader reads 9 of 11 and masks TURNS with `& 1u`, so a stored LEAD (2) loads as TOGETHER (0). That is harmless and is noted in the changelog.
- Tests: extend the existing TIDE settings round trip in `tide_controls_test.c` to load 7-, 9- and 11-byte records, a short (< 7) record, a wrong version, and out-of-range new bytes. Each expects the stored fields or the defaults.
- The editor backup did not include object 48 when this was written. Since `fix/prefs-backup` (D11) the fields live in the settings record's reserved tail and every backup holds them.

Page: the TIDE SETTINGS list already scrolls (`JL_VIS` in `graph_tidelist()`). It goes from 8 rows to 10, with no new page. Each new row also needs `jl_n`, `jl_get`, `jl_vname`, `jl_put` and a `ui_message`, and must appear in the web editor's `layout.json`/`layout.js` if those mirror the rows.

### What M8a TURNS and POLY already give (on `tide-turns`)

- `sqm_auto` holds one bit per part in automatic MUSIC WNDR/JRNY, set every pass in `glide_poll()`. Everything below runs only for those parts, and the two-part features only when `sqm_auto == 3`.
- TURNS = TAKE TURNS shifts part 2's VARY BARS 4 + 8 bar by 2 inside `seq_play_step()`. Part 1 varies on bars 4 and 8, part 2 on bars 2 and 6. All bar numbers come from `sqm_bar + sqm_bar_turn`, counted in `seq.c` (H3). This is the **shared musical-bar clock**, and every step below uses it. None adds a counter that runs separately from it.
- POLY: one part at a time plays a shorter loop for 4–8 full loops. Changes happen only where the full loop wraps. During a stretch `phrase_peer_attack()` returns 0 (no prediction).

## 1. M7a/b: SYNC, the soft off-beat and the push

### Musically

The kick owns the four beats. A dub chord sounds relaxed on the "and" (the 8th between beats: steps 3, 7, 11, 15). It sounds urgent one 16th before a beat (the push: steps 4, 8, 12, 16, strongest on 16 into the next bar). TIDE already moves one movable hit by a 16th every two loops, but it picks the direction without regard to the beat, so syncopation happens only by chance. SYNC makes those moves *lean* towards the "and", allows a few pushes, and keeps a hit that already sits on the "and" there more often. The beats stay put, so the groove still has its reference. An anchor-only pattern (all hits on beats) cannot change, by design. Only DENS-added hits (End B) can bring an off-beat there.

- **OFF**: today's behaviour, byte for byte.
- **SOFT** (recommended default): the "and" is preferred, and at most one push per bar per part.
- **PUSH**: stronger preference, up to two pushes per bar. Meant for driving stretches. M9's build raises SOFT to PUSH levels by itself.

### Hook points (all main loop; nothing in the ISR)

1. `phrase_direction()` (`seq_dialogue.c`): today it returns `dir` unless the peer attacks the other neighbour. New: score each neighbour `a`, `b` as `4 × (no peer attack) + W[tide_metric(dest)]` with W = {AND 3, A 2, E 1, OFFGRID 1, BEAT excluded}. When the scores differ, take the better one with probability 2/3 (SOFT) or 7/8 (PUSH), drawn from `sqm_hash(seed, cur, 0x5Cu)`. Otherwise keep the momentum direction. Peer avoidance stays the stronger term, and a NONE grid returns today's result. `phrase_direction()` needs the mutate seed passed in. It is static with one caller, so this is a signature change only.
2. `rhythm_mutate()` (`seq_rhythm.c`), destination check: skip a destination where `tide_metric()` is BEAT, in both attempts. This adds the real-beat rule for 1/8 and 1/32 grids. Source loop: a source whose *reached* position is AND is skipped with probability 1/2 (SOFT) or 3/4 (PUSH), so off-beats tend to stay.
3. **M7b push cap**, same place: before accepting a destination that is A, count the part's reached attacks on A in that bar (stored hits + `rhythm_offset()` + RHYTHM_REPEAT copies, which is the same walk the collision check already does). At the cap (1 SOFT, 2 PUSH), A scores like E. A push right before the part's own hit on that beat also scores like E, because it would only double the beat. Held sources (tie, slide, ratchet) are already refused by `sqm_held()`, and a destination followed by a tie by `sqm_target()`.
4. Optional, same slice: `phrase_gap()`'s second pass leaves hits on AND until last, so thinning removes the weak 16ths first. `sqm_build()`'s DENS offbeat weight (`i % 4u == 2u ? 3u`) switches to `tide_metric() == AND` only when SYNC ≠ OFF, so non-MUSIC modes do not change.
5. Row: `JL_SYNC` in `tide_ui.c`, `static volatile uint8_t sqm_sync = 1` beside `sqm_turns`, and `TS_SYNC` in the record.

Cost: helper 120–180 B + scoring/cap 150–220 B + row 40–60 B, about **300–450 B flash**. No new RAM: the cap is counted on the fly from `rhythm_map`.

### With TURNS and POLY

- The cumulative overlay (`rhythm_apply()`) applies to both ends, so a syncopated move is heard on every bar, not only on the varied bars. TURNS keeps choosing *which bars* carry the End B variation. The two do not interact.
- During a POLY stretch the peer term is 0 (prediction is blind), so the metric term decides alone. That is acceptable, because the move is still judged against the full loop that comes back.
- Both parts may land on the same "and". M1 peer avoidance keeps that rarer, and the M8b thinning removes the rest in follower bars.

### KEEP / UNDO

Unchanged. SYNC only chooses among moves the phrase lifecycle already makes. KEEP stores the reached steps (`seqm_keep()` → `sqm_step(t, i, 1)`) and clears the map. UNDO restores KEEP's copy. SYNC itself is a setting and is not part of UNDO.

### Tests (host)

New `tide_sync_test.c`, or a section of `tide_dialogue_test.c`, which already has the M1 fixture:

- With a fixed seed and SYNC OFF, the result equals today's `rhythm_mutate()`/`phrase_direction()` choices on the M1 fixture (bit-exact regression).
- A four-on-the-floor chord pattern with E-position hits: over 64 mutations the AND+A share rises (SOFT less than PUSH) and no attack lands on a real beat.
- An already-off-beat pattern keeps ≥ 75 % of its AND hits after 64 mutations.
- Push cap: never more than 1 (SOFT) or 2 (PUSH) A-hits in any bar of the reached pattern. A push never moves a held hit.
- DIV 1/8, 1/32 and triplets: real beats are never destinations, and triplets/NONE behave as SYNC OFF.
- Swing ≥ 60: the A share stays at E level.
- POLY 15/16 active: `tide_metric()` uses the full length, and the results equal those with POLY off.
- The M1 collision count does not get worse than today.

### Listening test with the TR-6S

Use M04 DUB CHORD, AUTO TIDE JRNY, TARGET BOTH, VARY 4 + 8, POLY OFF, and the TR-6S on a straight four-on-the-floor kick with closed hats on the 16ths, both at 124 BPM on one clock. Play SYNC OFF for 64 bars and note how often the chords sit on the kick. Restart with the same multi and SYNC SOFT for 64 bars, then PUSH. Listen for the chords leaving the kick room and leaning forward, and for single pushes into the bar line. Reject the setting if it sounds as if the pattern has slipped a 16th or the groove has turned ragged. Repeat SOFT with 60 % swing on both machines.

### Open decisions

1. **A row, or folded into a later ENERGY?** *Owner decision (3 November): SYNC gets its own row.* It costs one byte and lets the owner compare OFF and SOFT in the set. If ENERGY is accepted later, it can take over the same byte.
2. **Default.** OFF (today) / SOFT. *Recommended: SOFT after the listening test passes, OFF until then.*
3. **Push cap.** 1 per bar / 1 per 2 bars / 2 per bar. *Recommended: 1 per bar for SOFT, 2 for PUSH.*
4. **Re-rank DENS-added hits with the real metric?** *Recommended: yes, only with SYNC ≠ OFF.* It is the only way an anchor-heavy pattern gains an off-beat.

## 2. M8b/c: ANSWER, call and response

### Musically

M8a already staggers the turnarounds: part 2 varies on bar 2, part 1 on bar 4. M8b turns that into a conversation inside each 4-bar phrase. Part 2 **leads** bars 1–2 (its question ends with its bar-2 turnaround). Part 1 **leads** bars 3–4 (its answer ends with its bar-4 turnaround). While one leads, the other plays a **thinned** version of its own pattern: beats, held notes and long notes stay, and about half of the optional hits drop out. Nobody goes silent. M8c then adds the **echo**. Over the next development edits, the answering part slowly takes hits into the places the leader leaves empty when it thins, so the reply reuses the question's rhythm in the space the question left.

The TURNS row grows from 2 to 4 values. Each value includes the one before it, so the owner can step through them in one set:

- **TOGETHER**: today without M8a.
- **TAKE TURNS** (M8a, the `tide-turns` default): staggered turnarounds only.
- **LEAD** (M8b): plus the follower thinning.
- **ECHO** (M8c): plus the echo bias.

### Hook points

**Leader (ISR, cheap):** in `seq_play_step()` (`seq_morph.c`), with `bar = sqm_bar + sqm_bar_turn` as M8a already computes. Leader = part 2 when `(bar & 3) < 2`, otherwise part 1. This is the same phrase grid and the same bar M8a uses, so the leader and the turnaround always agree. The step a follower plays is:

```c
if (sqm_turns >= 2 && sqm_auto == 3u && pi != leader && (sqm_thin[pi] >> idx & 1u))
    return &SQM_REST;          /* a const rest step; ties and the next_tie lookup see a rest */
```

That is one shift, one compare and one 64-bit test per step lookup. Note that `seq_step()` also looks up `idx + 1` for `next_tie`. Run the ISR cost check (hot code has 1,236 B after reserve) and keep the code in the hot section only if `seq_play_step()` is there already.

**Thinning mask (main loop):** `static uint64_t sqm_thin[NSHOWN]` (16 B), rebuilt at the end of `sqm_refresh()` whenever the ends change. Built from End A as played (`sqm_pat[pi][0]`, which already includes the phrase overlay):

- Never drop: a BEAT or anchor step, a held hit (`sqm_held()`), a step followed by a tie, or a hit lasting ≥ 2 steps. Also never drop when the pattern has ≤ 2 hits.
- Candidates: the other hits, ranked with `sqm_score()` with preference E > A > AND, so the follower keeps its off-beats and loses the busy 16ths. With ECHO, hits on the part's own echo targets rank first (see below). Drop `⌈candidates / 2⌉`. NONE grids rank by seed only.
- Only onsets are masked (`time == ST_NOTE`). A tie step stays. A tie after a masked onset sounds nothing, as `sqm_shape_hold()` already notes, so the mask never cuts a sounding note. Rests release naturally and effects tails ring.

**Echo (M8c, main loop, cumulative):** the echo targets of part *p* are the other part's thinned-away positions, mapped through `tide_metric()` when the two grids match (same DIV family and whole-beat lengths). Otherwise ECHO acts as LEAD. These are the places that are free in *p*'s lead bars and where the other part was just heard. Two biases:

- `phrase_repeat()`'s restore/repeat loop and `rhythm_mutate()`'s destination score: +2 for an echo target, ranked below peer avoidance. Since the target is thinned out of the peer during *p*'s lead bars, the M1 prediction (which reads the peer's `seq_play_step()` at the current bar) normally reports it free anyway.
- The same targets rank first in *p*'s own thinning mask, so *p*'s echo hits drop out while the other part leads and sound only when *p* answers.

No new RAM beyond the thin masks, because the targets are read from the peer's `sqm_thin`. Open/closed pitch endings (root/5th cadences) stay deferred, as in the roadmap: there is no chord context.

Cost: leader + mask in ISR 60–100 B, mask builder 250–400 B, echo scoring 120–200 B, TURNS values 30 B. About **450–750 B flash, 16 B RAM** (plus a 9-byte const rest step).

### With TURNS, POLY and SYNC

- LEAD and ECHO include TAKE TURNS: part 2's End B variation lands on bar 2, inside its own lead, and part 1's on bar 4. A follower is never also varying.
- POLY: a shortened loop wraps at `tide_slen()`, so its stored index no longer matches the bar. That does not matter, because the mask is indexed by stored step and tests one bit per lookup. POLY already alternates parts per phrase (`(lc >> 2) & 1`). *Recommended:* while a part is in a POLY stretch it is never thinned. The rotation is its "question", and thinning it as well makes it disappear.
- SYNC: thinning keeps a follower's off-beats, so the follower keeps the off-beat chord while the leader plays its busier figure. This is the classic dub layout.
- A single selected part, TURNS < LEAD, a recording, or the glide stopped (`sqm_auto != 3`): no mask, exactly as today.

### KEEP / UNDO

The mask is playback-only. KEEP stores `sqm_step(t, i, 1)`, which never sees the mask, so it stores the full motif, not the thinned one. UNDO is unchanged. The echo bias works through ordinary phrase edits, so it is kept by KEEP and cleared by HOME like any other reached position.

### Tests

New `tide_answer_test.c`, beside `tide_turns_test.c`:

- Leader per bar: bars 0–1 part 2, 2–3 part 1, across `sqm_bar` wrap at 256 and with `sqm_bar_turn` set on the last block of a bar.
- The mask never removes a beat, an anchor, a held/tied/slid/ratcheted hit or a step before a tie. It keeps ≥ 2 hits and drops ⌈n/2⌉ of the candidates.
- Masked onsets produce no note-on, and a tie after one stays silent. A held note across the follower boundary releases at its own gate, not earlier (host render: compare gate-off times with LEAD and TAKE TURNS).
- KEEP during a follower bar stores the unmasked pattern, and UNDO restores the previous one.
- One selected part, `sqm_auto != 3`, STOP/PLAY (`sqm_bar` reset): the leader comes from the bar count again. ARP SEQ stand-in chords are masked the same way.
- ECHO: with fixed seeds, over 64 edits, part 1's hits on part 2's thinned positions rise, and peer collisions do not.
- A POLY stretch is never thinned.

### Listening test with the TR-6S

Same setup as M7, both parts in JRNY, TARGET BOTH, the TR-6S playing kick + hats + a clap on 2 and 4. Play TURNS TAKE TURNS, LEAD and ECHO for 64 bars each from the same start. Listen for the parts audibly taking turns every two bars and the follower sounding *calmer*, not broken. With ECHO, after a few minutes, listen for the answer starting to sound like the call. Reject LEAD if the follower sounds like it is dropping notes by mistake, or if the drive suffers against the TR-6S. Reject ECHO if the two parts converge into one rhythm.

### Open decisions

1. **Lead order.** Fixed (part 2 asks, part 1 answers, matching M8a) / swap every 8 bars. *Recommended: fixed first.* Swapping also requires swapping M8a's offset in the same expression, which is easy to add after the listening test.
2. **How thin.** Drop half / a third / all optional hits. *Recommended: half.* "All" sounds like a dropout.
3. **ECHO as its own value or part of LEAD?** *Owner decision (3 November): TURNS gains LEAD and ECHO as separate values*, then merge if it is always preferred. **Decided 2026-11-04: ECHO folds into LEAD.** Planned, not built: TURNS becomes TOGETHER / TAKE TURNS / LEAD, the PEAK style uses LEAD, and a stored ECHO reads as LEAD.
4. **Default TURNS.** *Recommended: TAKE TURNS stays the default until LEAD passes the listening test; then LEAD.*

## 3. M4/M9: ARC, build, peak, drop, breakdown

### Musically

One shape shared by both parts over 30–60 bars. **Rise** over 16–32 bars: more syncopation, more repeats, faster turns, and JRNY prefers brighter targets. **Peak** for one 4-bar phrase. An optional **drop**: the first two bars of the next phrase, when both TIDE parts stop starting notes while tails and the TR-6S carry on. **Breakdown** for 8–16 bars: both parts play their thinned patterns and JRNY prefers softer, darker sounds. **Recovery**: the thinning lifts phrase by phrase into the next rise. The motif survives the whole arc because breakdowns are playback masks, not edits. Lengths vary from the seed, and not every arc drops.

- **OFF**: today's even evolution.
- **RISE** (recommended default once accepted): rise, peak, ease back, and breakdown without the drop.
- **DROP**: the full arc. A drop happens by itself on about half of the arcs, and the owner can also fire one by hand (the drop gesture below).

### One clock, one planner

The arc does not add a scheduler. Its single input is the same shared bar `sqm_bar` that TURNS and LEAD read:

- `static struct { uint16_t phr; uint8_t last, phase, left, t; } tarc;` (about 8 B) lives in a new `seq_arc.c` included from `seq_morph.c`. It is polled from `glide_poll()` next to `rhythm_poll()`, in the main loop.
- Each pass: when `sqm_bar` crosses a 4-bar boundary (`(sqm_bar >> 2) != tarc.last`), advance one **phrase**. Read the boundary from the same bits M8b uses, so an arc phase can only start or end where a call-and-response phrase starts. If `sqm_bar` went down (PLAY reset it), resynchronise without advancing. STOP freezes everything, because `sqm_bar` stands still. A new WNDR/JRNY session (`glide_start()` with `!gl.on`, where `gl.wbars` is already reset) restarts the arc at RISE. WAIT bars are played bars, so they count.
- Phase lengths in phrases, drawn from the seed at each phase start: RISE 4–8, PEAK 1, DROP ½ (2 bars, then the rest of that phrase is BREAK), BREAK 2–4, RECOVER 1–2. Tension `t` (0–255) is linear inside RISE, 255 at PEAK, 0 in DROP/BREAK, and rises again in RECOVER.
- The glide's trip clock (`gl.wbars`, `mm_tier`) is left alone. Trips and JRNY arrivals keep their TIME/WAIT. They only *read* `tarc`.

### What tension drives (existing machinery only)

| Lever | Hook | Rise → Peak | Break |
|---|---|---|---|
| Syncopation | M7 scoring in `phrase_direction()`/`rhythm_mutate()` | SOFT acts as PUSH above t ≥ 160 | SOFT |
| Density bound | `phrase_repeat()` cap `base + base/4` | up to `base + base/2` at peak | not raised; repeats removed first by `phrase_gap()` |
| Turn speed | M8b leader expression | above t ≥ 200 the leader swaps every bar | normal |
| Thinning | M8b `sqm_thin` (both parts, all bars) | off | on for both parts, ½ of optional hits; RECOVER lifts it for one part, then the other |
| Drop | `seq_play_step()`: `tarc_rest` flag → all onsets masked, ties kept | — | the 2 drop bars |
| Sound | JRNY: `sound_candidate_score()` gets a ± brightness term (the existing `morph_complement.c` quality) from `t`; WNDR: `mm_walk()` brightness direction bias | brighter | softer, darker |

The harshness score (M9a: resonance, drive, FM6 modulator levels, ratio simplicity) is **not** in this slice. Brightness is already measured and cheap. Harshness can replace it once listening shows brightness is not enough, which is the roadmap's "bounded, scene-calibrated bias". The JRNY palette stays within its engine and category (M3 rule), so a breakdown sound comes from the same family.

The drop mask is one more term in the same ISR test as LEAD: `if ((tarc_rest || (follower && thin bit)) && s->time == ST_NOTE) return &SQM_REST;`. It needs one volatile byte. A drop bar keeps tied notes and effects tails. POLY: `tide_poly_wrap()` does not start a stretch during PEAK/DROP (one extra condition), so the drop and the bar line after it are clean.

**Drop gesture (owner decision: ships with ARC):** OCT− held + a K2 turn on the TIDE page. This is the free knob slot of the existing OCT− reset path (`tide_reset()` in `tide_ui.c`: K1 HOME, K3 TIME, K4 WAIT), so no new input handling is needed. It sets `tarc.req`. At the next 4-bar phrase boundary the planner goes to DROP from any phase except DROP itself, then BREAK as usual. From RISE it skips straight to the drop. The message is `DROP NEXT PHRASE`, and a second turn before the boundary cancels it (`DROP CANCELLED`). With ARC RISE the gesture still drops (the hand overrides the no-drop setting). With ARC OFF it shows `ARC OFF` and does nothing. Automatic drops in DROP mode stay. In addition, PEAK holds up to 2 phrases waiting for a hand drop before the automatic choice applies. Cost: 60–100 B, 1 B RAM.

Status line: the TIDE page shows `RISE 3/6`, `PEAK`, `DROP`, `BREAK 2/3` in the place of the "WNDR TO B 3/8" text, which lets the owner see a drop coming.

Cost: planner 300–450 B, levers 400–700 B, JRNY/WNDR bias 150–250 B, status text and row 100–150 B, drop gesture 60–100 B. About **1.1–1.7 KB flash, 9–13 B RAM**. This is the largest piece. It needs a CPU check only for the one extra ISR byte test.

### With TURNS, POLY and SYNC

The arc's phrase is the M8 4-bar phrase, so phases always change at a call-and-response boundary. A drop is the first two bars of a phrase, which with LEAD are part 2's question, and the answer then enters in bar 3. Build quickens turns, and breakdown thins both parts. With TURNS TOGETHER or TAKE TURNS the arc still works: the thinning lever then applies to both parts in BREAK only. With one selected part, the arc runs for that part and skips the turn lever. POLY rotation is allowed in RISE and BREAK and blocked at PEAK/DROP.

### KEEP / UNDO

The arc is performance state, like the glide. KEEP during any phase stores the unmasked, reached pattern (masks are playback-only) and does not reset the arc. UNDO is unchanged. HOME restores End A's Original and leaves the arc running (recommended; see decisions). Nothing in `tarc` is saved, and a power cycle starts at RISE.

### Tests

New `tide_arc_test.c`:

- Phase sequence and lengths from fixed seeds. Boundaries only at `(sqm_bar & 3) == 0`. Resync after a `sqm_bar` reset (PLAY) without a skipped or double phrase. STOP freezes, resume continues, and a new session restarts.
- WAIT and TIME of 1–64 bars do not change the arc timing, because the arc reads only bars.
- A drop masks only onsets for exactly 2 bars, keeps ties and starts on a phrase boundary. With RISE and no gesture, no bar is ever fully masked.
- Gesture: OCT− + K2 arms a drop at the next phrase boundary from RISE, PEAK, BREAK and RECOVER. A second turn cancels it. It works with ARC RISE, does nothing with ARC OFF, and does nothing on the TIDE SETTINGS list (as `tide_reset()` already returns 0 there). K1/K3/K4 resets are unchanged.
- Density bounds: across 512 bars, the hit count stays within `[base/2, base + base/2]`. There is no progressive thinning, and brightness bias does not saturate: record the dial range per phase and check that it returns.
- KEEP in each phase stores the unmasked pattern. HOME leaves `tarc` running.
- OFF equals today bit for bit (same seeds, same edits).

### Listening test with the TR-6S

A 10-minute session per setting (OFF, RISE, DROP) with M04 DUB CHORD in JRNY, TURNS LEAD, SYNC SOFT, and the TR-6S on a steady kick/hat/clap groove left alone. Listen for: buildups you notice without watching the screen, peaks that feel like peaks, drops that sound like a musical rest rather than a dropout (the TR-6S stays in front, the tails ring), breakdowns that breathe, and the motif still recognisable after each breakdown. Fire at least two drops by hand (OCT− + K2) and check that each lands on the next phrase start. Reject automatic drops if one lands where the owner did not want it more than once in the session; the hand gesture then stays and automatic drops can be turned down. Reject the arc as a whole if it sounds scripted.

### Open decisions

1. **Who owns the drop.** *Owner decision (3 November): the hand gesture ships now, alongside the automatic drops.* Gesture: OCT− held + K2 on the TIDE page (see above). Still open: whether a hand drop should also start a new RISE immediately after BREAK (default: no, the planner's lengths apply).
2. **ARC values.** OFF / RISE / DROP (recommended) or OFF / ON. The roadmap's earlier name BUILD/FULL is the same idea. RISE and DROP say what is heard.
3. **Brightness now, harshness later?** *Owner decision (3 November): brightness only in this slice.* M9a becomes a separate decision after listening.
4. **Breakdown sound.** JRNY bias only / also slow the glide TIME in breakdown. *Recommended: bias only.* Changing TIME makes a second clock.
5. **HOME during an arc.** Leaves it running (recommended) / restarts at RISE.
6. **Arc lengths.** *Owner decision (3 November): 30–60-bar shapes.* One constant table; tune by ear.
7. **ARC on STOP and PLAY.** **Decided 2026-11-04:** ARC resumes where it was and does not restart on STOP and PLAY. It restarts only on a load or a K1 stop. Planned, not built; the "STOP freezes, resume continues" test above is the intended behaviour.
8. **Velocity.** **Decided 2026-11-04:** an automatic shape with no setting. RISE is louder; BREAK and the followers are softer. Planned, not built.

## 4. TIDE STYLES: one knob for the genre

Owner request, 3 November 2026. A **STYLE** row at the top of TIDE SETTINGS sets the other rows together, so a style is chosen in one move. It is a preset of existing rows only. It adds no generator behaviour, and every row stays editable afterwards.

### Values

STYLE: DUB / DEEP / MINIMAL / AMBIENT / PEAK / CUSTOM. PEAK is the one added style. It is the only style that uses ARC DROP and ECHO, so the build-and-drop work has a one-move setting for the moments the owner wants it.

"GLIDE" in the request is read as the glide's MODE column (the GLIDE column itself is the launch fader, not a setting). TIME is the trip length and WAIT the hold. TARGET, ROLL, ARP and PITCH are not set: they choose *what* TIDE may change, not the genre.

| STYLE | MODE | TIME | WAIT | VARY BARS | TURNS | POLY | SYNC | ARC | WASH | Musical reason |
|---|---|---|---|---|---|---|---|---|---|---|
| DUB | JRNY | 16 bars | 2 bars | 4 + 8 | LEAD | GENTLE | SOFT | RISE | OFF | Off-beat chords that hold and change at the turnaround, two parts trading phrases, sounds wandering slowly through the palette: the showcase. |
| DEEP | WNDR | 32 bars | 4 bars | 4 + 8 | TAKE TURNS | OFF | SOFT | RISE | LOW | Deep house keeps one warm chord sound and lets it breathe: slow walks around the same sound, steady grid, a little wash, gentle rises. |
| MINIMAL | WNDR | 8 bars | 0 | ALL | LEAD | WIDE | PUSH | OFF | OFF | Minimal lives on small changes every bar and loops shifting against the kick; no big arcs, so the groove never stops. |
| AMBIENT | JRNY | 64 bars | 8 bars | 4 + 8 | TOGETHER | GENTLE | OFF | OFF | HIGH | A bed rather than a groove: very long sound journeys, straight placement, slow phasing, the most wash. |
| PEAK | JRNY | 8 bars | 0 | ALL | ECHO | GENTLE | PUSH | DROP | OFF | Peak-time sets: busy, pushed, call and echo, buildups and drops (automatic and by hand). |
| CUSTOM | — | — | — | — | — | — | — | — | — | Whatever the rows hold. Picking CUSTOM changes nothing. |

MINIMAL note: VARY ALL + SYNC PUSH + POLY WIDE is the busiest combination after PEAK. The listening test must confirm it still reads as "minimal". If it does not, the fallback is SYNC SOFT.

Picking a style writes all its rows at once, through the same setters a hand edit uses (`jl_put()`, `gl.*`), so a running glide reacts exactly as if each value had been turned by hand. Values for rows not built yet (SYNC, ARC, LEAD/ECHO before steps 1–3) are ignored until those rows exist. For that reason STYLE ships after step 3.

### Default at first start

With no TIDE record (a new unit, ERASE DATA, the emulator), STYLE is **DUB**. DUB's values on the existing rows are exactly today's no-record defaults (`gl` initialiser in `morph_glide.c`: JRNY, 16 bars, WAIT 2 bars; VARY 4 + 8; WASH OFF; TARGET BOTH stays as it is). The first-start showcase (M04 DUB CHORD, `TIDE_FIRST_MULTI`) therefore starts as it does today, plus the accepted new rows. If a later default of an existing row changes, DUB's table changes with it.

### Hand edit → CUSTOM

One check in `tide_settings()` (`tide_ui.c`), which already compares the settings with `tide_seen` every frame. When STYLE ≠ CUSTOM and any of the nine owned values differs from that style's table entry, STYLE becomes CUSTOM and the values stay. This one place catches every path: TIDE page knobs (MODE, TIME, WAIT), TIDE SETTINGS rows, OCT− + K3/K4 resets and MIDI. It never switches back to a named style by itself, even if the values happen to match one again.

### Record byte

`TS_STYLE`, index 11: one byte appended after `TS_ARC`, giving 12 of 32 bytes. The rule from "The TIDE SETTINGS record" applies: `TIDE_STORE_VER` stays 1 and `TS_OLD` stays 7.

- An old record (7, 9 or 11 bytes) has no STYLE byte, so it loads as **CUSTOM** with every stored value kept. An update never changes the owner's settings.
- No record at all: DUB, which is today's behaviour.
- An out-of-range byte loads as CUSTOM.
- Downgrade: older firmware ignores byte 11.

The row is `JL_STYLE`, inserted first in the `JL_*` enum. The record order is independent of the row order, so nothing else moves.

### Cost

A const table of 5 styles × 9 bytes (45 B), apply and compare code 150–250 B, and the row 50–70 B. About **250–370 B flash, 1 B RAM.**

### Tests (extend `tide_controls_test.c`)

- Each style sets exactly its nine rows and leaves TARGET, ROLL, ARP and PITCH unchanged. Picking CUSTOM changes nothing.
- Every manual path (MODE/TIME/WAIT knobs, each owned TIDE SETTINGS row, OCT− + K3/K4, MIDI where mapped) switches STYLE to CUSTOM and keeps the edited value and all others. Edits to TARGET/ROLL/ARP/PITCH leave STYLE alone. Setting the values back by hand stays CUSTOM.
- Round trip: no record → DUB, with the existing rows equal to today's defaults. 7-, 9- and 11-byte records → CUSTOM with their stored values. A 12-byte record → its style and values. An out-of-range style byte → CUSTOM. A style chosen, then a power cycle → the same style and values, and the record is written once per change, not every frame.
- A style picked while a glide runs: the same result as turning the rows by hand (existing glide tests with the new setter path).

### Listening test with the TR-6S

Step through DUB, DEEP, MINIMAL, AMBIENT and PEAK for 32–64 bars each on M04 next to a steady TR-6S groove. Each should be recognisable within a minute without looking. Merge or drop a style that sounds like its neighbour (for example DEEP and DUB).

### Open decisions

1. **PEAK as a sixth style.** *Recommended: yes.* Without it, ARC DROP and ECHO are never reached from a style.
2. **WASH in DUB.** OFF (today's showcase, recommended) / LOW (more dub space, but it changes the first start).
3. **MODE in DEEP and MINIMAL.** WNDR (keeps the sound family, recommended) / JRNY.

## Settings summary

| Row | Values | Default | Record byte | Old record |
|---|---|---|---|---|
| STYLE (new, top row) | DUB / DEEP / MINIMAL / AMBIENT / PEAK / CUSTOM | DUB (no record) | `TS_STYLE` (index 11) | absent → CUSTOM, stored values kept |
| TURNS (exists on `tide-turns`) | TOGETHER / TAKE TURNS / LEAD / ECHO | TAKE TURNS, LEAD once accepted | `TS_TURNS` (index 7) | absent → TAKE TURNS |
| SYNC (new) | OFF / SOFT / PUSH | SOFT once accepted | `TS_SYNC` (index 9) | absent → default |
| ARC (new) | OFF / RISE / DROP | OFF until accepted, then RISE | `TS_ARC` (index 10) | absent → default |

Total: 12 of 32 record bytes. No new pages, modes or saved pattern data.

Planned change (decided 4 November 2026): ECHO folds into LEAD, so the PEAK style would use LEAD where the table shows ECHO, and the TURNS row has three values.

## Budget and order

Build each step on its own, measure the image after each, and do not bundle them. Suggested order: the metric helper + SYNC (smallest, needed by the others), then LEAD, then ECHO, then ARC, then STYLES. Space is not expected to be the limit (see the figures at the top). Fallback only, if a later measurement shows flash running out: the cuts in order of least loss are the ECHO value (≈ 150–200 B), the ARC status text, and the ARC sound bias (keeping rhythm-only arcs). ARC DROP's mask costs little once LEAD's mask exists.

## Owner decisions, 3 November 2026

1. SYNC gets its own row (OFF / SOFT / PUSH).
2. TURNS gains LEAD and ECHO (TOGETHER / TAKE TURNS / LEAD / ECHO); no new row.
3. ARC ships the hand drop gesture (OCT− held + K2 on the TIDE page) now, alongside the automatic drops; not automatic-only.
4. ARC uses 30–60-bar shapes with a brightness-only sound bias first; harshness (M9a) is a later decision.
5. The budget fits: about 25 KB flash free before the pending branches' 3–5 KB. Build in the order SYNC, LEAD, ECHO, ARC, STYLES, each measured on its own; the cut order is a fallback only.

Still open, with the recommended defaults above: SYNC/TURNS/ARC defaults after listening, the push cap, lead order, thinning amount, HOME during an arc, and whether a hand drop restarts RISE.

## Owner decisions, 4 November 2026

Planned, not built; the plan is in [the roadmap](../ROADMAP.md#planned-work-after-the-4-november-2026-decisions).

1. ECHO folds into LEAD (TURNS: TOGETHER / TAKE TURNS / LEAD). LEAD's call and response applies to lead and pluck roles only, never to chord skanks.
2. ARC resumes on STOP and PLAY and restarts only on a load or a K1 stop.
3. A velocity shape comes with ARC, with no setting: RISE louder, BREAK and the followers softer.
4. A skank hit may drift at most two 16ths from its origin, and the bass never walks under PITCH ON.
