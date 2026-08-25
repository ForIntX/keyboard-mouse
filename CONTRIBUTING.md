# Contributing

Contributions are welcome through GitHub issues and pull requests.

## Development setup

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The unit tests do not require access to `/dev/input` or `/dev/uinput`. Test real
hardware only on a local Linux desktop after reviewing the udev rule.

## Pull requests

1. Keep changes focused and explain the user-visible behavior.
2. Add or update controller tests for input-state changes.
3. Run the full CMake build and CTest suite.
4. Update both `README.md` and `README.en.md` when commands or behavior change.
5. Update `CHANGELOG.md` under `Unreleased` for user-visible changes.
6. Do not include raw keyboard-event logs, credentials, generated build output,
   or personal calibration files.

## Coding style

- Use C++17 and the existing RAII ownership patterns.
- Compile cleanly with `-Wall -Wextra -Wpedantic`.
- Preserve physical-keyboard recovery on every exit and error path.
- Avoid adding runtime dependencies unless the benefit is substantial.
- Keep shell scripts POSIX-compatible (`/bin/sh`).

## Reporting bugs

Include the Linux distribution, kernel, desktop environment, X11/Wayland
session type, keyboard model, application version, and sanitized
`keyboard-mouse --status` output. Never attach raw key-event logs.
