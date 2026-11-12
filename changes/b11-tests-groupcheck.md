## changelog
### 1.0.0-b10
- **Added (developer tooling, b11 tests lane):** `jam/check_groups.py` (in the `jam-tools` CI job, test `jam/test_check_groups.py`) fails when a `host_tests.sh` case is neither in the default list nor marked `# not default: LABEL - reason`, so a group like `take` can't silently stop running again. The six groups outside the list (`soundlab`, `twocore_tsan`, `physcost`, `tidecost`, `budgetcal`, `polycalib`) carry that marker; `emu` is in the list as `$EMU`.
