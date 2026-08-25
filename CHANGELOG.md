# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.5.0] - 2026-08-25

### Added

- Interactive `--devices` keyboard and saved-profile selection.
- Preservation of disconnected Bluetooth keyboard profiles.
- Automatic reattachment of selected keyboards after reconnection.
- Connection state in `--status` and startup output.

## [1.4.1] - 2026-08-25

### Added

- Verified process shutdown through `keyboard-mouse --stop`.
- Physical `Ctrl+Alt+Esc` emergency shutdown shortcut.
- Compatibility shutdown path for older processes without a PID record.

## [1.4.0] - 2026-08-25

### Changed

- Exclusive mode is the default again.
- Virtual keyboard and virtual mouse are separate uinput devices.
- Multiple concurrent instances are rejected.

## [1.3.1] - 2026-08-25

### Fixed

- Moved the udev rule before systemd seat/uaccess processing so built-in
  keyboards receive the active-session ACL.

## [1.3.0] - 2026-08-25

### Added

- Non-exclusive safe recovery mode.

## [1.2.0] - 2026-08-25

### Changed

- `+` is left click, `-` is right click, and `0` is middle click.
- Number-row and numpad aliases work without recalibration.

## [1.0.0] - 2026-08-25

### Added

- Initial C++17 evdev/uinput implementation.
- Fn detection with Caps Lock fallback, acceleration, clicking, and drag/drop.
