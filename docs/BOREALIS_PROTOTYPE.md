# Borealis Switch pilot

This is an independent UI experiment with synthetic data, built to evaluate
Borealis before considering a migration of the Switch interface. It does not
replace the current application. WebUI is outside its scope.

## Build

Use the existing devkitPro/libnx installation, Switch SDL2/Mesa portlibs, CMake
(3.13 or newer) and GNU Make:

```sh
make                      # existing TomoToolNX.nro
make -j4 borealis-prototype # separate TomoToolNXBorealis.nro
make borealis-prototype-clean
```

The optional target uses CMake internally for Borealis's existing build system;
no dependencies are downloaded or upgraded. Objects, ELF, NACP, map and staged
ROMFS live in `build/borealis-prototype/`. The NRO is also copied to the project
root. A failed build does not overwrite the last root prototype artifact.
`make clean` removes both sets of build intermediates; use the prototype clean
target to remove its root NRO as well.

Borealis is vendored at `5f08b286f3df737f3321d2247a6fe633fcead03c` from
https://github.com/xfangfang/borealis. See `libs/borealis/VENDOR.md`, `LICENSE`,
`NOTICE`, `AUTHORS` and the bundled dependency notices. License texts are also
included in the prototype ROMFS under `legal/`; the Material Icons license is
under `material/`. The single SDL context-selection patch is documented in `VENDOR.md`. SDL2
comes from devkitPro.

The pilot uses SDL2/OpenGL ES 2. Borealis owns its window, graphics context,
input and event loop. RTTI and exceptions are enabled only in this separate
build. The existing SDL renderer and production compiler flags are unchanged.
Deko3D is not used: the earlier evaluation could not find `glm/vec2.hpp`.

## Isolation

The executable links only the prototype and Borealis/platform dependencies.
It does not link or call the application's SaveMount, SaveEditor, SaveWorkflow,
configuration, HTTP server or any other production module. It does not mount
user saves or persist settings. Its resources are separate from production
ROMFS and contain no personal data. Borealis initializes platform services,
reads the system language/theme/fonts and loads only its own ROMFS resources.

The system's light/dark theme is used without overrides. The pilot adds no
custom theme, palette or theme picker. If a future migration is approved,
remove those controls from migrated screens while preserving old configuration
keys without rewriting them. That migration is a separate task.

## Sample and controls

The top-level destinations retain the application's names: WebUI, Textures,
Mii, Player and Map, presented with the standard Borealis Switch sidebar.
The Textures page contains fictional slots and a focusable Texture Settings
entry. Settings opens a separate activity with standard boolean and selector
cells and a Back row. Values live only in RAM, including when the child is
reopened, and reset on exit. Other sample rows open an informational dialog.

- ZL/ZR: previous/next top-level tab, wrapping at the ends and restoring the
  last content focus. They are unavailable in the child screen and dialogs.
- D-pad or left stick: standard Borealis focus navigation. The sidebar also
  supports choosing a destination and A enters that destination's content.
- A: activate a row, enter settings, toggle the demo switch, or open a selector.
- B: close the innermost dialog/selector, return from settings to its entry row,
  or return from tab content to the sidebar. B at the sidebar does nothing.
- Touch: tap sidebar destinations, rows, selector choices, the Back row/footer
  hint or Return to tab list. Each essential action has both routes.
- Exit pilot: focusable/tappable row in WebUI. Plus, Minus, X and Y have no
  pilot actions. There is no Save action because the pilot has no user data.

`Navigation` in `prototype/borealis/source/pilot.cpp` owns tab selection and
shared bindings. Cached pages keep focus, scroll and local state. Borealis's
activity stack handles child/modal isolation and return focus. This deliberately
does not use `TabFrame`, which destroys/recreates its page when changing tabs.
There are no new subtabs, renderer customizations, or global theme/style edits.

All pilot strings use Borealis i18n resources. English, Spanish and Latin
American Spanish are included; other system locales fall back to English.
Production language files and configuration are not read or rewritten.

## Validation boundaries

A successful host cross-build establishes that an NRO can be produced, not
hardware usability or game-save compatibility. This pilot contains no save
workflow and cannot establish save compatibility. Physical Switch testing is
required before a production migration.
