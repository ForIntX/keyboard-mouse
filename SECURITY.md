# Security Policy

## Supported version

Security fixes are applied to the latest release only.

## Input-device access

Keyboard Mouse must read raw keyboard events and create uinput devices. The
installed udev rule grants those permissions to the active local desktop user;
it does not add the user to the system-wide `input` group. Other processes
running under the same user account may nevertheless use those device
permissions, so only run trusted software in that session.

Exclusive mode temporarily grabs selected physical keyboards. Normal shutdown,
`keyboard-mouse --stop`, and `Ctrl+Alt+Esc` release virtual keys and grabs. The
kernel also releases grabs when the process exits unexpectedly.

## Reporting a vulnerability

Do not open a public issue for a vulnerability that could expose keystrokes,
cross user/session boundaries, or signal an unrelated process. Use GitHub's
private security advisory feature for the repository. Include reproduction
steps without real passwords, tokens, or raw personal keystrokes.
