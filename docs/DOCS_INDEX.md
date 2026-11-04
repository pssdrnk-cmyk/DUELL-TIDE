# Documents

Every document in the repository, who it is for, whether it is current, and what it is for. **Status:** *current* means it describes the code as it is and is kept up to date; *reference* means it is accurate for what it covers and is not edited with every release; *design record* means the feature is built and the page keeps the reasoning; *historical* means it is superseded and kept as a record (the page says by what).

Audiences: **player** (uses the unit), **installer** (flashes it), **contributor** (changes the code or the documents), **maintainer** (plans releases).

## Start here

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [README.md](../README.md) | player, installer | current | What DUELL with TIDE is, the stable and beta builds, how TIDE works, install, and DUELL 1.1's reference. |
| [docs/QUICKSTART.md](QUICKSTART.md) | player | current | A short tour of the controls, saving, effects and MIDI. |
| [docs/TIDE_GUIDE.md](TIDE_GUIDE.md) | player | current | A five-minute guide to AUTO TIDE and TIDE SETTINGS. |
| [docs/INSTALL.md](INSTALL.md) | installer | current | Installing with the web installer (Stable or Beta), erasing data, going back to M-VAVE's firmware. |
| [docs/TROUBLESHOOTING.md](TROUBLESHOOTING.md) | player, installer | current | Symptom and fix for the installer, the emulator, the unit and TIDE. |
| [docs/RECOVERY.md](RECOVERY.md) | installer | current | What to try if a flash stops or fails. |
| [docs/KNOWN_ISSUES.md](KNOWN_ISSUES.md) | all | current | Bugs and untested ground. |
| [docs/KNOWN_LIMITS.md](KNOWN_LIMITS.md) | all | current | Design limits of TIDE and of DUELL 1.1. |
| [docs/TESTING.md](TESTING.md) | player (testers) | current | What to try and how to report it. |

## Project

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [CHANGELOG.md](../CHANGELOG.md) | all | current | What changed, and the builds flashed to the fork owner's unit. |
| [RELEASE_NOTES.md](../RELEASE_NOTES.md) | player | reference | OSCILL/R's release notes for DUELL 1.1, the base of this fork. |
| [docs/ROADMAP.md](ROADMAP.md) | maintainer, contributor | current | Delivery status, memory budget, engineering and musical plan. |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | contributor | current | Where things go, how to build and test, the check_off rule, commit style. |
| [SECURITY.md](../SECURITY.md) | all | current | The flashing risk and how to report a security problem. |
| [CREDITS.md](../CREDITS.md), [NOTICE.md](../NOTICE.md) | all | current | Who made what, licences and the notice for a modified version. |
| [UPSTREAM_COMMIT.txt](../UPSTREAM_COMMIT.txt) | contributor | reference | The Felucca commit DUELL was taken from. |
| [.github/ISSUE_TEMPLATE/](../.github/ISSUE_TEMPLATE/) (`bug_report.yml`, `idea.yml`, `config.yml`) | all | current | The issue forms and the links shown when you open an issue. |
| [.github/pull_request_template.md](../.github/pull_request_template.md) | contributor | current | The checklist of a pull request. |

## Specification and sounds

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [SPEC.md](../SPEC.md) | contributor, player | reference | OSCILL/R's full specification of DUELL, with a note at the top for what this fork changes. |
| [docs/PATCH_NUMBERS.md](PATCH_NUMBERS.md) | player | current | The 160 factory sounds in PATCHES order (a host test reads this table). |
| [docs/MIDI_MAP_DEFAULTS.csv](MIDI_MAP_DEFAULTS.csv) | player, contributor | current | The shipped MIDI CC map (the README table is generated from it). |
| [docs/VOICE_CONTROLS.md](VOICE_CONTROLS.md) | player | reference | When each VOICE control is heard. |
| [docs/MOD_INERT_TABLE.md](MOD_INERT_TABLE.md) | player, contributor | reference | Which modulation depths do nothing on which engine. |
| [SOUND_RECIPES.md](../SOUND_RECIPES.md) | contributor | reference | Research notes for the factory sounds (DUELL 1.0 era). |

## The web editor and emulator

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [docs/EDITOR_LIBRARIAN.md](EDITOR_LIBRARIAN.md) | player | current | The patch library, import and export, DX7 voices, and what the editor shows for USER slots and sounds 129 to 160. |
| [docs/EDITOR_ROLL.md](EDITOR_ROLL.md) | player | current | The piano roll for the step pattern. |
| [felucca-base/web/EDITOR_PROTOCOL.md](../felucca-base/web/EDITOR_PROTOCOL.md) | contributor | reference | The SysEx protocol between the unit and the editor. |

## Building and testing

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [felucca-base/BUILDING.md](../felucca-base/BUILDING.md) | contributor | current | Toolchain, build options, the package, the emulator, tests. Felucca's guide with DUELL's changes. |
| [docs/JAM11_TEST_BASELINE.md](JAM11_TEST_BASELINE.md) | contributor, maintainer | reference | The host-test results at named checkpoints. |
| [docs/JAM11_SOUND_REFERENCES.md](JAM11_SOUND_REFERENCES.md) | contributor | reference | How the sound reference hashes were re-based for DUELL 1.1. |
| [docs/MIDI_MAP_ENGINE.md](MIDI_MAP_ENGINE.md) | contributor | reference | How the MIDI map table and MIDI LEARN work in the code. |
| [docs/VELO_CSV.md](VELO_CSV.md) | contributor | reference | The key-recording format of the velocity tools. |
| felucca-base/tests/*.txt, felucca-base/tools/bench_suites/*.txt | contributor | reference | Test data (golden hashes, baselines) and bench suites. Not prose. |

## Developer references (historical, kept in place)

Source and test comments cite these paths, so they stay where they are. Each says so at the top.

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [docs/TWOCORE_STAGE1.md](TWOCORE_STAGE1.md), [docs/TWOCORE_REACH_REAL.md](TWOCORE_REACH_REAL.md) | contributor | historical | The two-core work: the hand-over protocol, the safety net, what core 1 reaches. |
| [docs/MIXSPLIT_NOTES.md](MIXSPLIT_NOTES.md) | contributor | historical | The audio block split into stages. |
| [docs/BENCH_GUIDE.md](BENCH_GUIDE.md) | contributor | historical | The developer bench builds (not distributed). |
| [docs/CONVERT_TWIN_REPORT.md](CONVERT_TWIN_REPORT.md) | contributor | historical | Sound comparison of the conversion to the twin filter. |
| [docs/OWNER_REFINE.md](OWNER_REFINE.md) | contributor | historical | OSCILL/R's way of turning edited sounds into the factory set. |

## Design notes (docs/design)

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [TIDE_MUSICAL_DESIGN.md](design/TIDE_MUSICAL_DESIGN.md) | maintainer | current | The musical proposals for TIDE and their open decisions. |
| [LIVE_GRAPHS.md](design/LIVE_GRAPHS.md) | contributor | design record | How the live graph markers work. |
| [TIDE_NEXT_PROPOSALS.md](design/TIDE_NEXT_PROPOSALS.md) | maintainer | design record | SYNC, call and response, ARC and STYLES as proposed; all built. |
| [MIGRATION_1_1_PLAN.md](design/MIGRATION_1_1_PLAN.md) | maintainer | design record | The move of TIDE onto DUELL 1.1 and its receipts. |
| [MIDI_MAP_FINAL.md](design/MIDI_MAP_FINAL.md), [MIDI_MAP_DESIGN_2.md](design/MIDI_MAP_DESIGN_2.md) | contributor | design record | The MIDI map design (the first is the final one). |
| [TWIN_FILTER_DESIGN_2.md](design/TWIN_FILTER_DESIGN_2.md), [TWIN_FILTER_DESIGN.md](design/TWIN_FILTER_DESIGN.md) | contributor | design record | The twin filter design (the first is the one built). |
| [SUB_AND_CYCLE_DESIGN.md](design/SUB_AND_CYCLE_DESIGN.md) | contributor | design record | The SUB layer, CYCLE envelope and WAVE STEPS. |
| [EDITABLE_B_DESIGN.md](design/EDITABLE_B_DESIGN.md) | contributor | design record | Setting End B by hand. |

## History (docs/history)

Superseded documents, each with a one-line header saying by what.

| Document | Status | Superseded by |
|---|---|---|
| [TIDE_ROADMAP_2026-11-02.md](history/TIDE_ROADMAP_2026-11-02.md) | historical | [ROADMAP.md](ROADMAP.md) |
| [ACTION_PLAN.md](history/ACTION_PLAN.md) | historical | [ROADMAP.md](ROADMAP.md) |
| [UI_COMPACTION_HANDOFF.md](history/UI_COMPACTION_HANDOFF.md) | historical | [ROADMAP.md](ROADMAP.md) |
| [FEATURE_LIST.md](history/FEATURE_LIST.md) | historical | [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](../CHANGELOG.md) |
| [BETA_CALL.md](history/BETA_CALL.md) | historical | [INSTALL.md](INSTALL.md) |
| [TWOCORE_DIAG.md](history/TWOCORE_DIAG.md), [TWOCORE_JOB5_REACH.txt](history/TWOCORE_JOB5_REACH.txt), [RAMPLACE_NOTES.md](history/RAMPLACE_NOTES.md) | historical | [TWOCORE_STAGE1.md](TWOCORE_STAGE1.md) |
| [SUBCYC_BUILD_NOTES.md](history/SUBCYC_BUILD_NOTES.md) | historical | [SPEC.md](../SPEC.md) |

## Inherited from Felucca (lightly annotated)

| Document | Audience | Status | Purpose |
|---|---|---|---|
| [felucca-base/README.md](../felucca-base/README.md) | contributor | reference | Felucca's README, with a DUELL note at the top. |
| [felucca-base/CONTRIBUTING.md](../felucca-base/CONTRIBUTING.md) | contributor | reference | OSCILL/R's contributing rules for plain DUELL; the fork's are in [CONTRIBUTING.md](../CONTRIBUTING.md). |
| [felucca-base/LICENSING.md](../felucca-base/LICENSING.md), [felucca-base/LICENSES/](../felucca-base/LICENSES/) | all | current | Licences of the third-party parts. |
| felucca-base/.github/ | none | inactive | Felucca's own issue forms. GitHub reads only the `.github` folder at the top, so these are not used. |
| [history/ROADMAP_BASELINE_2026-11-03.md](history/ROADMAP_BASELINE_2026-11-03.md) | developers | historical | The roadmap's baseline and evidence tables before the 4 November beta |
