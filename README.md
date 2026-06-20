# FFXI-AutoLogin (Makaria Fork)

**Original by: jaku | https://github.com/jaku/FFXI-autoPOL**
**Fork by: Makaria | https://github.com/alicestellar/MakariaAutoPOLFork**

## What is this?

A tool to automate logging into Final Fantasy XI using Windower or POL directly. This fork adds multi-profile management for up to 18 accounts (a full alliance), sequential launching with pause, and saved presets.

---

## Quick Start

### 1. Place the EXE

Put `autoPOL.exe` in the same folder as your `pol.exe` or `Windower.exe`.

- For Windower users, this is usually your Windower install folder.
- For vanilla POL users, it's usually `C:\Program Files (x86)\PlayOnline\SquareEnix\PlayOnlineViewer`.

### 2. First Run

Double-click `autoPOL.exe`. It will:

1. **Request admin privileges** (UAC prompt) — needed for hosts file and input blocking.
2. **Auto-detect your POL path** via registry or common locations. If it can't find it, you'll be asked to enter the path manually.
3. **Walk you through setup** if no `config.json` exists:
   - Set account defaults (shared password, TOTP, Windower args)
   - Add characters with name, profile group, and slot

### 3. Main Menu

After setup, you'll see the Alliance Launcher menu:

```
===================================================
           FFXI autoPOL - Alliance Launcher
===================================================

  Characters:
  [1] P1 | Makaria (group 1 slot 1)
  [2] P1 | Mule1 (group 1 slot 2)
  [3] P1 | Healer (group 1 slot 3)
  [4] P1 | Tank (group 1 slot 4)
  ---
  [5] P2 | Support1 (group 2 slot 1)
  [6] P2 | Support2 (group 2 slot 2)

  Options:
  [M] Launch multiple (enter numbers separated by commas)
  [P1] Launch Party 1 (chars 1-6)
  [A]  Launch Full Alliance (all characters)
  --- Saved Presets ---
  [1p] Farming Team (Makaria, Mule1, Healer)
  [S] Manage saved presets
  [E] Edit configuration
  [B] Backup/Archive login_w.bin
  [I] Import existing archives
  [Q] Exit
===================================================
```

---

## Features

### Single Launch

Type a character number (e.g., `3`) and press Enter. The app swaps the correct profile group into place and launches that character. After launch, you return to the menu.

### Multi-Launch

Press `M`, then enter comma-separated numbers (e.g., `1,2,3,5`). Characters launch one at a time. Between each, the app pauses and waits for you to press Enter before continuing. Profile swaps happen automatically.

### Party Presets

- `P1` — launches characters 1-6 (Party 1)
- `P2` — launches characters 7-12 (Party 2)
- `P3` — launches characters 13-18 (Party 3)
- `A` — launches all configured characters

Presets only appear if you have enough accounts configured.

### Saved Custom Presets

Press `S` to create named presets (e.g., "Farming Team" = characters 1, 3, 7). Presets are stored in `config.json` and appear in the main menu. Launch them by typing `1p`, `2p`, etc.

### Profile Groups

POL only supports 4 accounts per `login_w.bin` file. This fork manages up to 5 profile groups (files `login_w.1.bin` through `login_w.5.bin`), giving you 20 account slots. The app swaps the correct file into place before each launch — you never have to do it manually.

### Archive Management

Press `B` from the main menu to:

- **Save** your current `login_w.bin` to a numbered archive slot (1-5)
- **Import** existing archives (e.g., from MultiPOL) using `I` from the main menu

### Account Defaults

If all your accounts share the same password, TOTP secret, or Windower args, set them once as defaults. Individual accounts with empty fields automatically use the defaults. Edit defaults anytime with `E` > `F`.

---

## Config File

The `config.json` file stores everything. A backup (`config.json.bak`) is created automatically before every write, so you can never lose your TOTP secrets to a bad edit.

Example structure:

```json
{
    "delay": 3000,
    "clientRegion": "US",
    "polPath": "C:\\Program Files (x86)\\PlayOnline\\SquareEnix\\PlayOnlineViewer\\usr\\all",
    "activeProfileGroup": 1,
    "defaults": {
        "password": "sharedpass",
        "totpSecret": "BASE32SECRET",
        "args": "-p=\"Default Profile\""
    },
    "accounts": [
        {
            "name": "Makaria",
            "password": "",
            "totpSecret": "",
            "profileGroup": 1,
            "slot": 1,
            "args": ""
        }
    ],
    "presets": [
        {
            "name": "Farming Team",
            "accounts": [1, 3, 7]
        }
    ]
}
```

- Empty `password`, `totpSecret`, or `args` on an account = uses the value from `defaults`
- `profileGroup` (1-5): which `login_w.{N}.bin` file this account lives in
- `slot` (1-4): which slot within that file

---

## Command Line Options

| Flag | Description |
|------|-------------|
| `--character NAME` | Skip menu, launch a specific character by name |
| `--no-proxy` | Disable the POL redirect proxy for this run |
| `--setup` | Force re-run of the setup wizard |
| `--edit` | Open the edit configuration menu |
| `--debug-keys` | Log simulated keystrokes (for troubleshooting) |

---

## How to Get Your TOTP Secret

If you use a one-time password (TOTP) for your FFXI account:

1. **Remove your current authenticator** from your Square Enix account.
2. **Register a new authenticator**. When you get to the QR code, select "Can't scan this image."
3. Copy the secret provided on that page — use this as your TOTP secret.
4. Done! Also register the QR code in your authenticator app so you still have a backup.

If you use a TOTP generator that stores the secret (like Bitwarden), you can just copy it from there without removing your authenticator.

**Note:** If you use any TOTP generator that remembers the secret (such as Bitwarden), you can just copy that secret and do not need to forget the old one.

---

## Migrating from MultiPOL or Batch Scripts

If you already have `login_w.1.bin`, `login_w.2.bin`, etc. from MultiPOL:

1. Place `autoPOL.exe` in your Windower/POL folder
2. Run it — it will auto-detect the POL path
3. Press `I` (Import) from the main menu
4. Walk through assigning character names and credentials to each archive

Your existing archive files are compatible — no conversion needed.

---

## Troubleshooting

### Login Issues

- **Partial Character Entry**: Increase the delay in your config (`E` > `C`). Start by adding 1-2 seconds.
- **Controller Interference**: Try disabling controller support if key entry is unreliable.
- **Locked keyboard/mouse**: Press Ctrl+Alt+Del to regain control if the lock persists.

### Build from Source

1. Install Visual Studio 2022 with **Desktop development with C++** workload
2. Clone the repository
3. Run `download_deps.ps1` to fetch nlohmann/json and httplib headers
4. Build with MSBuild: `MSBuild.exe FFXI-Launcher.vcxproj /p:Configuration=Release /p:Platform=x64`
5. Output: `x64\Release\autoPOL.exe`

---

## Security Notes

- Passwords and TOTP secrets are stored in `config.json` on your local machine
- They are never transmitted over the network
- `config.json.bak` is created before every write to prevent data loss
- The app never logs credentials to console (even in debug mode)
- Consider keeping `config.json` in a location only your user account can read

---

## Credits

- **jaku** — original autoPOL (https://github.com/jaku/FFXI-autoPOL)
- **Makaria** — this fork with multi-profile and alliance features

If you're on the Phoenix server, say hi to jaku sometime!
And if you're on the Bahamut server, you can say hi to Makaria!
