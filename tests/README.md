# Host regression checks

Run from the repository root with a host C++17 compiler:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
    source/focus_navigation.cpp source/texture_settings.cpp tests/texture_settings_test.cpp \
    -o /tmp/tomotoolnx-texture-settings-test
/tmp/tomotoolnx-texture-settings-test
```

The texture-settings checks exercise navigation boundaries, dependent controls,
touch previews, confirm/cancel, parent focus restoration, and per-frame input consumption.
They require no Switch libraries, SDL, filesystem access, or personal saves.

These checks do not validate rendering, touch targets, physical controller input,
config persistence, or game-save compatibility. Build the app with `make` and
verify affected interface flows on a physical Switch separately.

## Save safety

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Wno-implicit-fallthrough \
    -fno-exceptions -fno-rtti -Iinclude \
    source/save_editor.cpp source/save_workflow.cpp source/backup_snapshot.cpp \
    tests/save_safety_test.cpp \
    -Wl,--wrap=fwrite,--wrap=fread,--wrap=fclose,--wrap=mkdir \
    -o /tmp/tomotoolnx-save-safety-test
/tmp/tomotoolnx-save-safety-test
```

Requires GNU ld wrappers and `/dev/full` on the host. Fixtures are synthetic and
isolated in temporary directories. Add `-fsanitize=address,undefined
-fno-omit-frame-pointer -g` for sanitizer checks.

Covers all pending-file combinations, backup/preflight/write/commit failures,
retry after partial success, serializer error reporting, unknown-field and version
preservation, backup readback, incomplete snapshots, and destination collisions.
Commit failures are injected callbacks; these tests do not model libnx journal
behavior or validate the recovery UI. See [manual checks](../docs/SAVE_RECOVERY.md).

## Texture preference configuration

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
    source/focus_navigation.cpp source/texture_settings.cpp \
    source/texture_settings_config.cpp source/texture_settings_store.cpp \
    tests/texture_settings_config_test.cpp \
    -Wl,--wrap=open,--wrap=read,--wrap=write,--wrap=close,--wrap=fsync \
    -o /tmp/tomotool-config-test
/tmp/tomotool-config-test
```

Uses synthetic temporary configs. Checks preservation of comments, unknown keys,
legacy theme/color keys, line endings, inactive preferences and custom matte
colors; invalid tokens, duplicates, bounds and external edits; verified backups,
exclusive creation, short writes, read/write/flush/close/verification failures
and reopen persistence. This models POSIX file failures on the host, not SD card
power loss or libnx behavior. The backup remains available after partial writes;
retry is blocked until configuration is reloaded.
