# Units of Work

## Architecture Context

This is a single-binary monolithic C++ console application (`FFXI-Launcher.cpp`). There are no separate services or deployables. Each unit of work represents a logical feature slice implemented within the same source file and built as part of the same binary.

**Deployment model**: Single `.exe` binary
**Code organization**: Single `.cpp` file (existing pattern maintained)
**Build system**: Visual Studio 2022, MSVC v143, vcxproj

---

## Unit 0: Baseline Build Verification

- **Scope**: No code changes. Verify existing project compiles and runs.
- **Deliverable**: Confirmed working build + manual test pass
- **Estimated effort**: Minimal (build + run)

## Unit 1: POL Path Discovery and Storage

- **Scope**: Add `discoverPolPath()` function. Registry lookup → common path scan → user prompt. Store in config.json.
- **Deliverable**: New function, config schema addition (`polPath` field), integration into startup flow
- **Key functions**: `discoverPolPath()`, modified `loadConfig()`, modified `writeConfigFile()`
- **Estimated effort**: Small

## Unit 2: Extended Account Config Schema

- **Scope**: Add `profileGroup` and `slot` fields to AccountConfig struct and JSON serialization. Backward-compatible loading. Migration prompt for legacy configs.
- **Deliverable**: Updated struct, updated `loadConfig()`/`writeConfigFile()`, migration logic
- **Key functions**: Modified `AccountConfig` struct, modified `loadConfig()`, modified `writeConfigFile()`, new `migrateConfig()`
- **Estimated effort**: Small

## Unit 3: Profile Group File Swap

- **Scope**: Add function to copy `login_w.{N}.bin` → `login_w.bin` before launch. Track active group. Validate source file.
- **Deliverable**: New `swapProfileGroup()` function, integration into launch flow
- **Key functions**: `swapProfileGroup()`, modified `launchAccount()`
- **Estimated effort**: Small

## Unit 4: Archive Management

- **Scope**: Add interactive menu for archiving current `login_w.bin` to a numbered backup. Show existing archives with character associations. Allow slot selection.
- **Deliverable**: New `archiveMenu()` function, `listArchives()`, integration into main menu
- **Key functions**: `archiveMenu()`, `listArchives()`, `archiveCurrentConfig()`
- **Estimated effort**: Medium

## Unit 5: Unified Alliance Selection UI

- **Scope**: Replace existing character selection with party-grouped display. Support single-select and multi-select. Produce a launch queue (vector of accounts).
- **Deliverable**: New `allianceSelectionUI()` function replacing old selection logic
- **Key functions**: `allianceSelectionUI()`, `displayPartyGroups()`, replaces current selection in `main()`
- **Estimated effort**: Medium

## Unit 6: Sequential Multi-Launch with Pause

- **Scope**: Consume the launch queue produced by the UI. Launch accounts one at a time, pause between each (Enter key by default, configurable).
- **Deliverable**: New `executeLaunchQueue()` function, config addition for continue key
- **Key functions**: `executeLaunchQueue()`, modified `main()` flow
- **Estimated effort**: Medium

## Unit 7: Profile Swap with Warning and Non-Skippable Delay

- **Scope**: Enhance sequential launch to detect profile group boundaries. Display warning, execute swap, enforce non-skippable countdown.
- **Deliverable**: Enhanced `executeLaunchQueue()` with swap detection and countdown
- **Key functions**: Modified `executeLaunchQueue()`, new `swapCountdown()`
- **Estimated effort**: Small

## Unit 8: Party and Alliance Launch Presets

- **Scope**: Add built-in presets (Party 1, Party 2, Party 3, Two Parties, Full Alliance) to the selection UI. Presets only shown if enough accounts configured.
- **Deliverable**: Preset options added to `allianceSelectionUI()`
- **Key functions**: Modified `allianceSelectionUI()`
- **Estimated effort**: Small

## Unit 9: Ad-Hoc Custom Queue Builder

- **Scope**: Interactive mode to pick accounts one at a time into a queue for this session. Display running queue. Confirm when done.
- **Deliverable**: New `buildCustomQueue()` function accessible from selection UI
- **Key functions**: `buildCustomQueue()`, integrated into `allianceSelectionUI()`
- **Estimated effort**: Small

## Unit 10: Saved Custom Presets

- **Scope**: Named presets stored in config.json. CRUD operations from menu. Appear as selectable options in main UI.
- **Deliverable**: Config schema addition (`presets` array), menu for create/edit/delete, display in selection UI
- **Key functions**: `managePresets()`, `createPreset()`, `deletePreset()`, modified `loadConfig()`/`writeConfigFile()`, modified `allianceSelectionUI()`
- **Estimated effort**: Medium
