# Unit of Work — Story Map

Each unit maps 1:1 to a user story. This is intentional — the stories were already designed as independently testable increments.

| Unit | Story | Requirements Covered | Config Changes | UI Changes |
| ---- | ----- | -------------------- | -------------- | ---------- |
| 0    | Story 0: Baseline Build | (none — verification only) | None | None |
| 1    | Story 1: POL Path Discovery | FR-01 | Add `polPath` | Prompt if auto-detect fails |
| 2    | Story 2: Extended Config Schema | FR-04, FR-09 | Add `profileGroup`, `slot` per account | Migration prompt for legacy |
| 3    | Story 3: Profile Group File Swap | FR-02, NFR-02 | None | None (transparent to user) |
| 4    | Story 4: Archive Management | FR-03 | None (archives are .bin files, not config) | New archive menu |
| 5    | Story 5: Unified Alliance UI | FR-05, FR-10 | None | Replace selection menu entirely |
| 6    | Story 6: Sequential Launch | FR-07, FR-11, FR-12 | Add `continueKey` (optional) | Checkpoint messages, retry prompt |
| 7    | Story 7: Swap Delay | FR-08 | Add `swapDelay` (optional, default 4s) | Warning + countdown display |
| 8    | Story 8: Party Presets | FR-05 (preset portion) | None | Preset menu options |
| 9    | Story 9: Ad-Hoc Queue | FR-05 (custom queue portion) | None | Queue builder interaction |
| 10   | Story 10: Saved Presets | FR-06 | Add `presets` array | Preset CRUD menu |

## Security Baseline Touchpoints

| Unit | SECURITY Rules Applicable | Notes |
| ---- | ------------------------- | ----- |
| 2    | SECURITY-12 (no hardcoded creds) | Credentials stay in config, never in source |
| 3    | SECURITY-15 (exception handling) | File I/O must handle errors gracefully |
| 4    | SECURITY-15 (exception handling) | Archive copy must validate before overwrite |
| 5    | SECURITY-03 (no sensitive data in logs) | Never display passwords in UI |
| 6    | SECURITY-03, SECURITY-15 | Launch errors handled safely, no cred leaks |
| All  | SECURITY-09 (no default creds) | Config file permissions, no secrets in source |

## NFR Coverage

| NFR | Covered By Units |
| --- | ---------------- |
| NFR-01: Credential Protection | 2, 5, 6 (all units touching creds) |
| NFR-02: File Integrity | 3, 4 (file swap and archive operations) |
| NFR-03: Reliability | 1, 3, 4, 6 (all file I/O and launch operations) |
| NFR-04: Usability | 4, 5, 8, 9, 10 (all UI-facing units) |
| NFR-05: Performance | 7 (swap delay timing) |
