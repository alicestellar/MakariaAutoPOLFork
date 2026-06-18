# User Stories — Multi-Profile Alliance Launcher

Stories are ordered by dependency and testability. Each story represents a working increment that can be built, tested, and verified before moving to the next.

---

## Story 0: Baseline Build Verification

**As a** developer,
**I want to** confirm the existing project builds and runs correctly,
**So that** I have a known-good baseline before making changes.

**Acceptance Criteria:**

- [ ] Project compiles with MSVC v143 (x64 Release) with no errors
- [ ] Existing `config.json` loading works (no regressions)
- [ ] Single-account launch still functions (manual test)
- [ ] Multi-account interactive selection still functions (manual test)

---

## Story 1: POL Path Discovery and Storage

**As a** multiboxer,
**I want** autoPOL to find my POL installation path automatically,
**So that** I don't have to hardcode paths or maintain batch scripts with paths.

**Acceptance Criteria:**

- [ ] Application checks Windows Registry for POL install location
- [ ] Falls back to scanning common paths (`C:\Program Files (x86)\PlayOnline\SquareEnix\PlayOnlineViewer`)
- [ ] If neither works, prompts the user to enter the path manually
- [ ] Discovered path is stored in `config.json` under `polPath`
- [ ] On subsequent runs, stored path is used without re-discovery
- [ ] If stored path is invalid (directory missing), re-runs discovery

---

## Story 2: Extended Account Config Schema

**As a** multiboxer,
**I want** each account to have a profile group and slot assignment,
**So that** the application knows which `login_w.bin` file to use for each character.

**Acceptance Criteria:**

- [ ] `config.json` accounts now include `profileGroup` (1-5) and `slot` (1-4) fields
- [ ] Existing configs without these fields still load (backward compatibility)
- [ ] When legacy config detected, offer migration (assign all to group 1, sequential slots)
- [ ] Config writing preserves new fields
- [ ] Validation: reject duplicate profileGroup+slot combinations

---

## Story 3: Profile Group File Swap

**As a** multiboxer,
**I want** autoPOL to automatically swap the correct `login_w.bin` backup into place,
**So that** I don't have to manually copy files between launches.

**Acceptance Criteria:**

- [ ] Before launching an account, copy `login_w.{profileGroup}.bin` → `login_w.bin` in the POL directory
- [ ] If the correct profile group is already active, skip the copy
- [ ] Verify source backup file exists and is non-zero before copying
- [ ] Report error and abort launch if copy fails
- [ ] Track which profile group is currently active to avoid unnecessary copies

---

## Story 4: Archive Management

**As a** multiboxer,
**I want** to archive my current `login_w.bin` to a numbered backup slot,
**So that** I can set up multiple profile groups without losing my existing account data.

**Acceptance Criteria:**

- [ ] Menu option `[B] Backup/Archive login_w.bin` available from main menu
- [ ] When selected, ask: save to new slot or overwrite existing?
- [ ] If overwriting, display list of existing archives with associated character names
- [ ] User can select which slot (1-5) to write to
- [ ] User can choose which POL slots (1-4) their accounts occupy (with gentle advice to use sequential slots)
- [ ] Archives are persistent — never auto-deleted
- [ ] Confirm before overwriting an existing archive

---

## Story 5: Unified Alliance Selection UI

**As a** multiboxer,
**I want** a single unified UI that shows my accounts grouped by party,
**So that** I can quickly select one character, a party, or a custom set to launch.

**Acceptance Criteria:**

- [ ] New UI is always active (even for 1-4 accounts)
- [ ] Accounts displayed grouped as Party 1 (1-6), Party 2 (7-12), Party 3 (13-18)
- [ ] Only configured accounts shown (no empty slots displayed as selectable)
- [ ] Options include: pick single character by number, select multiple characters
- [ ] Edit config menu remains accessible
- [ ] Replaces the old simple selection menu
- [ ] Works correctly with 1 account, 4 accounts, 6 accounts, 18 accounts
- [ ] Selection produces a launch queue (list of accounts in order)

---

## Story 6: Sequential Multi-Launch with Pause

**As a** multiboxer,
**I want** autoPOL to launch queued accounts one at a time and wait for me between each,
**So that** I can ensure each character is fully logged in before starting the next.

**Acceptance Criteria:**

- [ ] After launching an account, display checkpoint message
- [ ] Wait for user to press Enter (default) before proceeding to next
- [ ] Continue key is configurable in `config.json` (default: Enter)
- [ ] After last account in queue, display completion message and exit
- [ ] Single-account selection launches immediately (no pause needed)
- [ ] Profile group file swap (Story 3) executes before each launch as needed

---

## Story 7: Profile Swap with Warning and Non-Skippable Delay

**As a** multiboxer,
**I want** a warning and mandatory delay when the app swaps profile groups between launches,
**So that** the file swap completes before the next POL instance reads it.

**Acceptance Criteria:**

- [ ] When next account requires a different profile group, display warning
- [ ] File swap executes immediately when user presses the continue key
- [ ] Non-skippable countdown (default 4 seconds) runs after the swap
- [ ] User cannot bypass the countdown
- [ ] After countdown, proceed to launch the next account
- [ ] If swap is not needed (same group), no delay — just the normal Enter pause

---

## Story 8: Party and Alliance Launch Presets

**As a** multiboxer,
**I want** preset launch options for "Party 1", "Party 2", "Party 3", "Full Alliance",
**So that** I can launch common groupings with a single menu choice.

**Acceptance Criteria:**

- [ ] Menu shows preset options: Party 1, Party 2, Party 3, Two Parties, Full Alliance
- [ ] Presets only appear if the user has enough accounts configured to fill them
- [ ] Selecting a preset queues all accounts in that group for sequential launch

---

## Story 9: Ad-Hoc Custom Queue Builder

**As a** multiboxer,
**I want** to build a one-time custom launch queue by picking accounts individually,
**So that** I can launch any combination without creating a saved preset.

**Acceptance Criteria:**

- [ ] Menu option to build a custom queue for this session
- [ ] User can select accounts one at a time (by number)
- [ ] Current queue displayed as accounts are added
- [ ] User confirms when done building the queue
- [ ] Queue is not saved — it's one-time use

---

## Story 10: Saved Custom Presets

**As a** multiboxer,
**I want** to save named presets (e.g., "Farming team") that I can reuse,
**So that** I don't have to rebuild the same custom queue every time.

**Acceptance Criteria:**

- [ ] Menu option to create a new saved preset
- [ ] User names the preset and selects accounts to include
- [ ] Presets stored in `config.json`
- [ ] Saved presets appear as selectable options in the main launch menu
- [ ] Can edit and delete existing presets from menu

---

## Dependency Order

```text
Story 0 (baseline build)
    └── Story 1 (POL path discovery)
        └── Story 2 (config schema)
            ├── Story 3 (file swap)
            └── Story 4 (archive mgmt)
                └── Story 5 (unified UI → produces launch queue)
                    └── Story 6 (sequential launch → consumes queue)
                        └── Story 7 (swap delay)
                            ├── Story 8 (party presets)
                            ├── Story 9 (ad-hoc queue builder)
                            └── Story 10 (saved presets)
```

After Story 6, you have a fully functional end-to-end multi-account launcher.
Stories 7-10 layer on polish and convenience.
