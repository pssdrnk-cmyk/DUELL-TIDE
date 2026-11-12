# changes/: changelog and roadmap fragments

A branch that adds lines to `CHANGELOG-TIDE.md` or `docs/ROADMAP.md` writes them here instead, in `changes/<branch>.md`, so two branches never edit the same lines.
`python3 jam/assemble_changes.py` merges every fragment into the two files at RC time and deletes them (details: its header, and `jam/README.md`).

```
## changelog
### 1.0.0-b10
- **New (...):** one bullet; a continuation line starts with a space.
## roadmap
### Repository and CI
- one bullet
```

`### <name>` is a `## <name>` section of the file or, in the roadmap, a bold label `**<name>**`. Bullets only add lines; rewording or striking an existing row is still an edit of the roadmap itself.
