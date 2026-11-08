# Host regression checks

Run from the repository root with a host C++17 compiler:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
    source/texture_settings.cpp tests/texture_settings_test.cpp \
    -o /tmp/tomotoolnx-texture-settings-test
/tmp/tomotoolnx-texture-settings-test
```

The texture-settings checks exercise navigation boundaries, dependent controls,
touch-equivalent selections, value cycling, legacy A activation, and idle input.
They require no Switch libraries, SDL, filesystem access, or personal saves.

These checks do not validate rendering, touch targets, physical controller input,
config persistence, or game-save compatibility. Build the app with `make` and
verify affected interface flows on a physical Switch separately.
