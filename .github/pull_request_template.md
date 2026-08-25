## Summary

Describe the behavior and motivation.

## Verification

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
- [ ] `cmake --build build --parallel`
- [ ] `ctest --test-dir build --output-on-failure`
- [ ] Turkish and English documentation updated when behavior changed
- [ ] No raw keyboard logs, generated build output, or personal config included

## Hardware testing

List the tested Linux distribution, desktop/session type, and keyboard models,
or state that this change does not require hardware testing.
