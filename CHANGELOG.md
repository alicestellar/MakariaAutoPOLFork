# Changelog

All notable changes to this project will be documented in this file.

Versioning scheme:
- Major (X.0.0): Milestone feature sets complete
- Minor (0.X.0): Individual unit/feature complete
- Micro (0.0.X): Intermediate commits, bug fixes, side work

---

## [1.1.0] - 2026-06-19

### Added

- New unified Alliance Selection UI (replaces old character selection menu)
- Party-grouped display (P1/P2/P3 labels, separators between parties)
- Shows profile group and slot info for each character
- `[Q]` Exit option to cleanly close the application
- Returns to main menu after launching (no longer exits after single launch)
- `--character` CLI flag still bypasses the menu for scripted use

### Changed

- Main menu is now always displayed regardless of account count (even 1 account)
- Launch flow now loops back to menu instead of exiting

---

## [1.0.0] - 2026-06-19

### Milestone: Profile Management Complete

First major release of the Makaria fork. All profile management features are functional and tested.

### Features (since 0.0.23 upstream)

- POL path auto-discovery (registry, common paths, user prompt)
- Multi-profile group system (up to 5 groups of 4 accounts = 20 slots)
- Automatic profile group file swap before launch
- Active profile group persisted across restarts
- Archive management (save, import from MultiPOL)
- Account defaults (shared password, TOTP, Windower args)
- Config backup on every write (never destroys data)
- Backup restore on JSON parse failure
- Auto-elevation to admin on startup
- Default Windower profile option during account setup
- Import existing MultiPOL archives from main menu

---

## [0.4.2] - 2026-06-19

### Added

- Config backup: `config.json.bak` created before every write (never destroys data)
- Backup restore prompt on parse failure (recovers from accidental corruption)
- Account defaults system: shared password, TOTP, and Windower args across accounts
- `defaults` object in `config.json` for cross-account shared values
- Accounts with empty fields automatically fall back to defaults at runtime
- `[F] Set account defaults` option in edit configuration menu
- Defaults setup offered during initial configuration

### Changed

- All account field reads now use `.value()` for safer JSON parsing (no exceptions on missing keys)
- Setup prompts now say "leave empty to use default" for password/TOTP
- `promptWindowerArgs()` helper used consistently in all account creation/edit flows

---

## [0.4.1] - 2026-06-19

### Added

- Import existing archives option (`[I]` in archive menu)
- Scans for existing `login_w.{N}.bin` files (e.g., from MultiPOL)
- Lets user assign account names, passwords, TOTP, slots to discovered archives
- Skips archives that already have accounts assigned
- Updates existing accounts if name matches

---

## [0.4.0] - 2026-06-19

### Added

- Archive management menu (`[B]` from main selection screen)
- Shows all 5 archive slots with associated account names
- Save current `login_w.bin` to any slot (1-5) with overwrite confirmation
- Assign accounts to archive group during save (new or existing accounts)
- Duplicate slot validation within the same group

---

## [0.3.1] - 2026-06-19

### Changed

- Active profile group now persisted in `config.json` as `activeProfileGroup`
- Removed global variable tracking; swap state survives app restarts
- Skip unnecessary file copies on startup when config already knows which group is loaded

---

## [0.3.0] - 2026-06-19

### Added

- Profile group file swap: automatically copies correct `login_w.{N}.bin` into `login_w.bin` before launch
- Tracks active profile group to skip unnecessary copies
- Validates source file exists and is non-zero before copying
- Aborts launch with error message if swap fails

---

## [0.2.1] - 2026-06-19

### Added

- Auto-elevation: if not running as admin, prompts UAC and re-launches elevated
- FR-13 requirement added to requirements doc

---

## [0.2.0] - 2026-06-19

### Added

- `profileGroup` field (1-5) on each account in `config.json`
- Profile group + slot duplicate validation during account setup
- Legacy config migration: auto-assign or manual assignment when old config detected
- Profile group prompt during new account creation

### Changed

- Slot range restricted to 1-4 (within a profile group) instead of 1-20
- Setup flow now asks for profile group before slot

---

## [0.1.0] - 2026-06-19

### Added

- POL path auto-discovery (registry → common paths → user prompt)
- `polPath` field in `config.json` for persistent storage of discovered path
- Re-discovery triggers automatically if stored path becomes invalid
- `autoPOL` as output executable name for x64 builds
- Fork attribution (Makaria) alongside original author (jaku)
- AIDLC planning artifacts (`aidlc-docs/`)
- CHANGELOG.md

### Changed

- `.gitignore` updated to exclude build outputs, `.bin` files, and `config.json`
- Version string updated from 0.0.23 to 0.1.0

---

## [0.0.23] - (upstream)

- Last version from jaku's original FFXI-autoPOL repository
- Single/multi-account login with TOTP support
- POL proxy server for fast login redirect
- Interactive character selection and config editing
