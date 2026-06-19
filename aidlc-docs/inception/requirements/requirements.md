# Requirements: Multi-Profile Alliance Launcher

## Intent Analysis

- **User Request**: Add multi-profile login_w.bin management to autoPOL, supporting up to 18 accounts across 5 profile groups (4 slots each), with an alliance-style selection UI, sequential launch with pause, profile swap automation, and saved custom presets.
- **Request Type**: New Feature (major enhancement to existing application)
- **Scope Estimate**: Single Component (all changes within FFXI-Launcher.cpp and config.json)
- **Complexity Estimate**: Moderate (multiple interacting subsystems: file management, UI, config schema, sequential orchestration)

---

## Functional Requirements

### FR-01: POL Path Discovery and Storage

- The application MUST attempt to discover the POL installation path automatically using:
  1. Windows Registry lookup (standard Square Enix/PlayOnline install keys)
  2. Scanning common install paths as fallback
  3. Prompting the user if neither method succeeds
- The discovered path MUST be stored in `config.json` under a `polPath` field for future use.
- If the stored path becomes invalid (directory missing), re-run discovery on next launch.

### FR-02: Profile Group File Management

- The application MUST support up to 5 profile group files (`login_w.1.bin` through `login_w.5.bin`), stored in the POL `usr\all\` directory.
- Each profile group file holds up to 4 POL account slots.
- The application MUST be able to copy the correct profile group file into `login_w.bin` before launching an account that belongs to that group.
- If the correct profile group is already active (same file), skip the copy.

### FR-03: Archive Management

- The application MUST provide a menu option to archive the current `login_w.bin` to a numbered backup slot (`login_w.{N}.bin`).
- When the user selects the archive option, the application MUST:
  1. Ask whether to save to a new slot or overwrite an existing slot.
  2. If overwriting, display a list of existing archives showing: archive number, and the character names associated with accounts in that archive group.
  3. Allow the user to select which archive slot to write to.
- The user MUST be able to choose which account slots (1-4) within each profile group their current accounts represent. The application SHOULD gently advise that sequential slots are preferred but MUST allow non-sequential selection (e.g., slots 1, 2, 5, 6).
- Archives are persistent — they are NOT automatically deleted.

### FR-04: Account Configuration (Extended)

- Each account in `config.json` MUST have:
  - `name` (string, unique identifier)
  - `password` (string)
  - `totpSecret` (string, optional)
  - `profileGroup` (integer, 1-5)
  - `slot` (integer, 1-4, the slot within the profile group's login_w.bin)
  - `args` (string, optional Windower arguments)
- The application MUST support up to 18 accounts total.
- The existing `delay`, `clientRegion`, and `POLProxy` config fields MUST be preserved.

### FR-05: Unified Alliance Selection UI (New Default)

- The application MUST always use the new alliance-style UI, even for 1-4 accounts.
- The selection menu MUST present accounts grouped by party (alliance structure):
  - Party 1: Accounts 1-6
  - Party 2: Accounts 7-12
  - Party 3: Accounts 13-18
- The mapping from "party position" to actual profile group + slot is internal; the UI shows party structure regardless of storage layout.
- Selection options MUST include:
  - Pick a single character by number
  - Pick a set of characters for this session only (ad-hoc custom queue)
  - Launch a specific party (Party 1, Party 2, or Party 3)
  - Launch two specific parties
  - Launch the full alliance (all configured accounts)
  - Launch a saved custom preset
- The edit configuration option MUST remain accessible from the selection menu.

### FR-06: Saved Custom Presets

- The application MUST allow users to define and save named presets (e.g., "Farming team" = accounts 1, 3, 7).
- Presets MUST be stored in `config.json`.
- Users MUST be able to create, edit, and delete presets from the menu.
- Presets MUST appear as selectable options in the main launch menu.

### FR-07: Sequential Multi-Launch with Pause

- When multiple accounts are queued for launch, the application MUST:
  1. Launch the first account.
  2. After the login automation completes, display a checkpoint message.
  3. Wait for the user to press Enter (default) or a user-configured key.
  4. Proceed to the next account in queue.
- The configurable "continue" key MUST default to Enter.
- The key MUST be stored in `config.json` if the user changes it from the default.

### FR-08: Profile Swap with Warning and Delay

- When the next account in a sequential launch queue requires a different profile group than the currently active one:
  1. Display a warning that a profile swap is about to occur.
  2. Execute the file swap (copy `login_w.{N}.bin` → `login_w.bin`) immediately upon user pressing the continue key.
  3. Begin a non-skippable countdown delay (configurable, default TBD based on file copy speed — suggest 4 seconds).
  4. After the countdown completes, proceed to launch the next account.
- The user MUST NOT be able to skip the countdown during a profile swap.

### FR-09: Backward Compatibility

- Existing `config.json` files without `profileGroup` and `slot` fields MUST continue to work.
- If legacy config is detected, the application SHOULD offer to migrate (assign all accounts to profile group 1 with sequential slots).

### FR-10: Exit Option

- The main menu MUST include an exit option to cleanly close the application.
- Selecting exit MUST clean up any resources (hosts file entries, etc.) before terminating.

### FR-11: Return to Main Menu After Launch Queue

- Once all selected accounts in a launch queue have been launched and the user has confirmed the last one, the application MUST return to the main menu instead of exiting.
- This allows the user to launch additional characters or perform other actions without restarting the application.

### FR-12: POL Window Closure Detection and Retry

- During the login automation sequence, if the PlayOnline Viewer window closes unexpectedly before FFXI loads, the application MUST detect the closed window.
- Upon detection, the application MUST ask the user if they would like to retry launching that account.
- If the user confirms retry, re-launch that account (including any necessary profile swap).
- If the user declines, skip that account and continue to the next in the queue (or return to the main menu if it was the last/only account).

---

## Non-Functional Requirements

### NFR-01: Security — Credential Protection

- Passwords and TOTP secrets MUST NOT be logged to console or file at any time (including in debug mode).
- The `config.json` file SHOULD be readable only by the current user (recommend setting file permissions on creation).
- The application MUST NOT transmit credentials over the network (they are used locally via simulated keystrokes/clipboard only).
- Clipboard content MUST be restored after pasting passwords (existing behavior — must be preserved).

### NFR-02: Security — File Integrity

- Before overwriting `login_w.bin`, the application MUST verify the source backup file exists and is non-zero size.
- If the copy fails, the application MUST report the error and NOT proceed with launch.
- The application MUST NOT delete any backup files without explicit user confirmation.

### NFR-03: Reliability

- The application MUST handle all file I/O errors gracefully (missing files, permission denied, disk full).
- If POL path discovery fails, the application MUST inform the user and provide instructions to set the path manually.
- The countdown timer MUST be non-interruptible to prevent race conditions with file operations.

### NFR-04: Usability

- All menu options MUST have clear labels.
- The party/alliance display MUST only show configured accounts (don't show empty slots as selectable).
- Error messages MUST be user-friendly and suggest corrective actions.

### NFR-05: Performance

- Profile group file swap MUST complete in under 2 seconds for typical login_w.bin sizes (~1KB).
- The 4-second default countdown provides margin for slow I/O.

---

## Extension Configuration

| Extension              | Enabled | Decided At            |
| ---------------------- | ------- | --------------------- |
| Security Baseline      | Yes     | Requirements Analysis |
| Property-Based Testing | No      | Requirements Analysis |

---

## Constraints

- Windows-only (Win32 API, MSVC toolchain)
- POL hard limit: 4 account slots per `login_w.bin` file
- Maximum 18 accounts (5 profile groups × 4 slots, with 2 unused in the last group)
- Must run as Administrator (for hosts file modification and BlockInput)
- Single-threaded UI (console application)
