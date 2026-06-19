# Changelog

All notable changes to this project will be documented in this file.

Versioning scheme:
- Major (X.0.0): Milestone feature sets complete
- Minor (0.X.0): Individual unit/feature complete
- Micro (0.0.X): Intermediate commits, bug fixes, side work

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
