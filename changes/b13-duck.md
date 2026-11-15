## changelog
### 1.0.0-b14
- **Fixed (MORPH TO and DUCK SRC):** a saved or reloaded patch or multi whose End B is a MORPH TO sound that ducks from another source now plays that source at B, as the live morph did; End B stores DUCK SRC (`morph.c` mbv_never; `tools/endb.py`, the editor's `patchrecord.js` follow). A patch saved before keeps B on the part's own source, as it played. A build before this one refuses a block that carries it (End B then loads as the Original). Test: `tidekeep`.
