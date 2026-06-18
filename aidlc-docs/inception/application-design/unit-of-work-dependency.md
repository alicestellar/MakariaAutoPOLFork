# Unit of Work — Dependency Matrix

## Dependency Graph

```text
Unit 0 (baseline)
  └── Unit 1 (POL path)
      └── Unit 2 (config schema)
          ├── Unit 3 (file swap)
          └── Unit 4 (archive mgmt)
              └── Unit 5 (unified UI)
                  └── Unit 6 (sequential launch)
                      └── Unit 7 (swap delay)
                          ├── Unit 8 (party presets)
                          ├── Unit 9 (ad-hoc queue)
                          └── Unit 10 (saved presets)
```

## Dependency Matrix

| Unit | Depends On       | Required By            | Reason                                              |
| ---- | ---------------- | ---------------------- | --------------------------------------------------- |
| 0    | (none)           | 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 | Baseline must build before any changes     |
| 1    | 0                | 3, 4                   | File swap and archive need to know POL path         |
| 2    | 0                | 3, 4, 5, 6, 7, 8, 9, 10 | All subsequent units need profileGroup/slot fields |
| 3    | 1, 2             | 6, 7                   | Sequential launch needs swap capability             |
| 4    | 1, 2             | 5                      | UI should show archive option; archive uses POL path |
| 5    | 2, 4             | 6, 8, 9, 10           | All launch/queue features need the selection UI      |
| 6    | 3, 5             | 7, 8, 9, 10           | All queue consumers need the launch engine           |
| 7    | 6                | 8, 9, 10              | Presets and queues may cross profile group boundaries |
| 8    | 7                | (none)                 | Standalone convenience feature                      |
| 9    | 7                | (none)                 | Standalone convenience feature                      |
| 10   | 7                | (none)                 | Standalone convenience feature                      |

## Execution Order (Strict Sequential)

Units MUST be implemented in order 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10.

No parallelization is possible — this is a single source file with each unit building on the previous.

## Critical Path

The critical path to a functional multi-account launcher is:

**Unit 0 → 1 → 2 → 3 → 5 → 6** (6 units to first end-to-end working flow)

Unit 4 (archive) and Unit 7 (swap delay) are important but not blocking for basic functionality.

## Build Checkpoints

| After Unit | What You Can Test                                                |
| ---------- | ---------------------------------------------------------------- |
| 0          | Existing behavior works unchanged                                |
| 1          | POL path auto-detected and stored in config                      |
| 2          | Config with new fields loads/saves correctly                     |
| 3          | Correct login_w.bin copied before launch                         |
| 4          | Can archive current login_w.bin to numbered slot                 |
| 5          | New party-grouped UI displays, can select accounts               |
| 6          | Selected accounts launch sequentially with Enter pause           |
| 7          | Profile swaps show warning + enforced countdown                  |
| 8          | Can launch full party/alliance from preset menu option           |
| 9          | Can build a one-time custom queue interactively                  |
| 10         | Can save/load/delete named presets                               |
