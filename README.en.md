# Keyboard Mouse

[Türkçe dokümantasyon](README.md)

Keyboard Mouse is a C++17 application that lets you control the mouse pointer
from a keyboard on Linux. It automatically detects USB, Bluetooth, 2.4 GHz
wireless, and built-in laptop keyboards from their Linux input capabilities.

It does not modify keyboard firmware, BIOS, or hardware. It reads physical
keyboard events through `evdev` and creates separate virtual keyboard and mouse
devices through `uinput`.

## Features

- Hides mouse-control keystrokes from applications in the default exclusive mode
- Accelerating arrow-key movement with normalized diagonal speed
- Left click with `+`, right click with `-`, and middle click with `0`
- Drag and drop by holding a click key
- Automatic Caps Lock fallback when Fn cannot be detected
- Multiple keyboard profiles and Bluetooth reconnection support
- X11 and Wayland support
- Safe shutdown through `Ctrl+C`, `Ctrl+Alt+Esc`, or `--stop`
- Session-scoped udev access without membership in the `input` group

## Requirements

- Linux kernel 4.5 or newer
- A desktop distribution using udev, `evdev`, and `uinput`
- CMake 3.16 or newer
- GCC or Clang with C++17 support

Current releases of Debian, Ubuntu, Linux Mint, Fedora, Arch/Manjaro, and
openSUSE are targeted. WSL, Android, ChromeOS, containers, and minimal systems
without udev are not directly supported.

## Installation

### 1. Install build dependencies

Debian, Ubuntu, or Linux Mint:

```bash
sudo apt update
sudo apt install build-essential cmake udev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake systemd-udev
```

Arch Linux or Manjaro:

```bash
sudo pacman -S base-devel cmake systemd
```

openSUSE:

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install cmake systemd
```

### 2. Download the project

Replace the URL with your repository address:

```bash
git clone https://github.com/ForIntX/keyboard-mouse.git
cd keyboard-mouse
```

If you downloaded a ZIP archive, extract it and open a terminal in the project
directory.

### 3. Stop an older process

If a previous release is running:

```bash
pkill -TERM keyboard-mouse 2>/dev/null || true
```

This command continues without an error when no process is running.

### 4. Run the installer

```bash
chmod +x install.sh uninstall.sh
./install.sh
```

`chmod +x` makes the scripts executable. `./install.sh` then:

1. Configures and builds a Release binary with CMake.
2. Runs the automated tests and stops if a test fails.
3. Installs the binary as `/usr/local/bin/keyboard-mouse`.
4. Removes the obsolete `99-keyboard-mouse.rules` file.
5. Installs the `70-keyboard-mouse.rules` udev rule.
6. Attempts to load the `uinput` kernel module.
7. Reloads udev rules and retriggers input devices.
8. Verifies that the installed binary matches the freshly built file.

Your sudo password is required for system-wide changes. The `70-` rule ordering
is intentional: it runs before systemd seat and uaccess processing so the active
desktop user receives access to built-in and external keyboards.

### 5. Refresh session permissions

If keyboards are reported as `permission required`, sign out of the desktop and
sign back in. A reboot is normally unnecessary.

### 6. Verify the installation

```bash
keyboard-mouse --version
keyboard-mouse --status
```

Expected version output:

```text
keyboard-mouse 1.5.0 (+ sol tik, - sag tik, 0 orta tik)
```

`--status` shows the configuration file, selected keyboards, connection state,
click codes, and `/dev/uinput` access.

### 7. Select and calibrate keyboards

For the initial setup:

```bash
keyboard-mouse --calibrate
```

The command first lists connected keyboards and saved offline profiles. After
you choose which keyboards to use, it calibrates Fn and click keys on selected
keyboards that are currently connected.

When Fn produces a real Linux event, the trigger is `Fn + Ctrl`. Most laptop
keyboards handle Fn in hardware and expose no event; the trigger then falls back
to **holding Caps Lock**.

## Quick start

```bash
keyboard-mouse --status
keyboard-mouse --calibrate
keyboard-mouse
```

The final command starts the recommended exclusive mode. The application does
not start automatically and runs while its terminal session remains open.

## Key mappings

While holding the configured trigger:

| Key | Action |
| --- | --- |
| Arrow keys | Move the pointer |
| `+` | Left click |
| `-` | Right click |
| `0` | Middle click |

Both the number row and numpad variants are supported, regardless of Num Lock.
If your layout requires Shift for `+`, hold Shift together with the trigger.
Holding a click key enables drag and drop.

Movement begins at 180 px/s and accelerates to 1080 px/s over 1.2 seconds.
Diagonal movement is normalized so that it is not faster than straight movement.

## Command reference

| Command | Purpose |
| --- | --- |
| `keyboard-mouse` | Starts the recommended exclusive mode and hides mouse commands from applications. |
| `keyboard-mouse --safe` | Runs without grabbing physical keyboards; command keys may also reach applications. |
| `keyboard-mouse --devices` | Selects connected keyboards and saved offline profiles. |
| `keyboard-mouse --calibrate` | Opens device selection and recalibrates selected connected keyboards. |
| `keyboard-mouse --status` | Shows profiles, connections, mappings, and permission status. |
| `keyboard-mouse --stop` | Verifies the PID and safely sends `SIGTERM` to the running process. |
| `keyboard-mouse --version` | Shows the installed version and primary click layout. |
| `keyboard-mouse --help` | Shows command-line help. |
| `./install.sh` | Builds, tests, and installs the binary and udev rule. |
| `./uninstall.sh` | Removes the binary and udev rule while preserving calibration. |
| `./uninstall.sh --purge-config` | Also removes the user's calibration file. |

## Keyboard selection and Bluetooth profiles

To change the selected keyboards without recalibrating existing profiles:

```bash
keyboard-mouse --devices
```

- A newly selected connected keyboard is calibrated immediately.
- A powered-off Bluetooth keyboard remains visible when it has a saved profile.
- Keeping an offline profile selected preserves its settings.
- Deselecting a profile removes it from the active configuration.
- Other selected keyboards continue working when one disconnects.
- A selected keyboard is re-enabled within two seconds after reconnection.

Configuration is stored at:

```text
${XDG_CONFIG_HOME:-$HOME/.config}/keyboard-mouse/config.conf
```

The `start_speed`, `max_speed`, and `acceleration_seconds` values may be edited
manually. Restart the application after changing them.

## Exclusive and safe modes

Plain `keyboard-mouse` uses exclusive mode. It grabs physical keyboard events,
forwards normal keys through the virtual keyboard, and consumes only mouse
commands. The virtual keyboard and virtual mouse are separate devices. A second
instance cannot run under the same user account.

If a desktop does not accept the virtual keyboard, use the recovery mode:

```bash
keyboard-mouse --safe
```

Safe mode never grabs physical keyboards. Consequently, arrow and click keys
may also reach the focused application.

## Stopping and emergency recovery

Normal terminal shutdown:

```text
Ctrl+C
```

Physical emergency shortcut when the terminal does not deliver Ctrl+C:

```text
Ctrl+Alt+Esc
```

From another terminal:

```bash
keyboard-mouse --stop
```

For an older release without `--stop`:

```bash
pkill -TERM keyboard-mouse
```

These paths release virtual keys, mouse buttons, and physical keyboard grabs.
If the process crashes, the kernel closes its file descriptors and releases
`EVIOCGRAB` automatically. Signing out or rebooting is the final recovery path;
no permanent hardware or firmware damage occurs.

## Troubleshooting

### The built-in keyboard is missing

Run `./install.sh`, sign out and back in, then check
`keyboard-mouse --status`. The installer removes the obsolete late-running
`99-` rule and installs the correctly ordered `70-` rule.

### Old behavior remains after installation

```bash
keyboard-mouse --stop
./install.sh
keyboard-mouse --version
```

An already-running process is not replaced in memory during installation. Make
sure the reported version is `1.5.0`, then start the program again.

### `/dev/uinput: permission required`

Sign out and back in. If the problem remains:

```bash
sudo modprobe uinput
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=input --action=change
sudo udevadm trigger --subsystem-match=misc --action=change
```

### The keyboard fails in exclusive mode

Exit with `Ctrl+Alt+Esc` or `keyboard-mouse --stop`, then test safe mode:

```bash
keyboard-mouse --safe
```

When filing a bug, include the distribution, desktop environment,
`keyboard-mouse --status` output, and keyboard model. Do not share raw key event
logs because they may contain sensitive input.

## Moving to another Linux computer

Transfer the source through Git, ZIP, or USB and run `./install.sh` on the target
computer. Building on the target provides compatibility with its CPU and system
libraries. Repeat `--status`, `--devices`, and `--calibrate` on every computer.

## Security

Reading raw keyboard events is required for this application. The udev rule
grants access to the active local desktop user; other processes running as that
user may also read permitted keyboard events. See [SECURITY.md](SECURITY.md).

## Uninstalling

Remove the binary and udev rule while preserving calibration:

```bash
./uninstall.sh
```

Also remove calibration data:

```bash
./uninstall.sh --purge-config
```

No autostart service is installed.

## Development and testing

Build and test without system installation:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The binary is written to `build/keyboard-mouse`. Hardware integration requires
udev access and `/dev/uinput`; CI only builds and runs unit tests.

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines and
[CHANGELOG.md](CHANGELOG.md) for release history.

## License

This project is distributed under the [MIT License](LICENSE).
