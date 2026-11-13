## changelog
### 1.0.0-b10
- **Changed (developer tooling, b11 tests lane):** `check.sh --changed` leaves out the slow tier, the groups whose last recorded run (`build/host/grouptimes.json`) took over 60 s, and prints them by name as NOT RUN at the end; `--with-slow` brings them back, `--slow-s N` moves the limit (or `JAM_SLOW_S`). Groups named on the command line, full runs and CI are not affected. Test: `jam/test_slow_tier.py`.
## roadmap
### Later, in order
- **Firmware objects for the host tests** (stage 1 of the HOSTOBJ design note, kept in the workspace notes): compile the firmware once per flag set to an object (statics exported by a build-time transform and a generated declarations header, no firmware file edited), link every group against it. Model: a firmware edit's `--changed` about 4 min to about 2 min (with the slow tier left out); M effort (about 4 tools sessions). Per-module objects (stage 2) not worth it. Needs the measurement below first: the model overestimated stage 0 (see `jam/README.md`, slow tier).
- **Fixed (developer tooling):** `jam/wslmirror.sh` sets `core.fileMode false` in the mirror's repository on every run (also on a mirror made earlier): rsync from `/mnt/c` gave every file mode 755 against the index's 644, so `git diff BASE` listed every file and `--changed` selected every group in every lane's mirror.
