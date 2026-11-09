# Borealis Switch port

This is an independent Switch UI migration target. Browser tabs use synthetic
data; Texture Settings v0.2.0 edits the real application preferences. It does not
replace the current application. WebUI is outside its scope.

## Build

Use the existing devkitPro/libnx installation, Switch SDL2/Mesa portlibs, CMake
(3.13 or newer) and GNU Make:

```sh
make                      # existing TomoToolNXPlus.nro
make -j4 borealis # separate TomoToolNXPlusBorealis.nro
make borealis-clean
```

The optional target uses CMake internally for Borealis's existing build system;
no dependencies are downloaded or upgraded. Objects, ELF, NACP, map and staged
ROMFS live in `build/borealis/`. The NRO is also copied to the project
root. A failed build does not overwrite the last root Borealis artifact.
`make clean` removes both sets of build intermediates; use the Borealis clean
target to remove its root NRO as well.

Borealis is vendored at `5f08b286f3df737f3321d2247a6fe633fcead03c` from
https://github.com/xfangfang/borealis. See `libs/borealis/VENDOR.md`, `LICENSE`,
`NOTICE`, `AUTHORS` and the bundled dependency notices. License texts are also
included in the Borealis ROMFS under `legal/`; the Material Icons license is
under `material/`. The single SDL context-selection patch is documented in `VENDOR.md`. SDL2
comes from devkitPro.

The port uses SDL2/OpenGL ES 2. Borealis owns its window, graphics context,
input and event loop. RTTI and exceptions are enabled only in this separate
build. The existing SDL renderer and production compiler flags are unchanged.
Deko3D is not used: the earlier evaluation could not find `glm/vec2.hpp`.

## Scope and isolation

The root destinations remain WebUI, Textures, Mii, Player and Map. Textures opens
the real Texture Settings as a child. The other pages and texture slots remain
samples. The current NRO version is 0.2.0, authored by EddieBeru.

The target links the shared texture-settings model, flat-config serialization,
SD preference store and focus utilities, plus Borealis/platform dependencies.
It does not link or call SaveMount, SaveEditor, SaveWorkflow or HTTP. It never
mounts game saves. It reads/writes `/switch/TomoToolNX/config.ini` only through
the preference store, which preserves unrelated fields and legacy theme/color
keys. Borealis continues to use its system theme. No theme picker is migrated.

See [migration boundaries and recovery](BOREALIS_MIGRATION.md) for persistence
semantics and how SDL drawing, previews and canvases will be ported.

## Controls

ZL/ZR switch top-level tabs. D-pad/left stick move focus; A enters or activates;
B closes the innermost interaction or returns to the parent. Settings has five
standard selector rows, Save preferences, Reload preferences and Back. Changes
remain drafts until saved. Back with changes offers Save and return, Discard
changes or Keep editing. Disabled rows are skipped and retain their values.
Touch provides the same controls. Plus/Minus/X/Y remain unbound in this target.

The activity stack restores entry focus and isolates settings/dialogs from tab
switching. Root tabs retain their cached focus and scroll. Exit remains a
focusable/tappable WebUI row. English, Spanish and Latin American Spanish are
included; other system locales fall back to English.

## Validation boundaries

A successful host cross-build establishes that an NRO can be produced, not
hardware usability or game-save compatibility. This port contains no save
workflow and cannot establish save compatibility. Physical Switch testing is
required before a production migration.

## Naming and direction

Borealis is the approved visual direction and primary Switch UI migration target.
Sources live in `apps/borealis/`; build with `make borealis` and clean with
`make borealis-clean`. The old `borealis-prototype` commands remain compatibility
aliases. The NRO filename remains `TomoToolNXPlusBorealis.nro`, with launcher title
`TomoToolNXPlus Borealis` and author EddieBeru. Sample pages still indicate
unfinished functionality; approving the appearance does not mark migration complete.

Host verification: `make borealis`, its compatibility alias and the default SDL
build succeeded. The final NRO metadata contains the new title and EddieBeru
author; locale keys and documentation links were checked. Runtime checks were
not repeated for this terminology-only change.

The records below retain the terminology and commands used at the time.

## Verification record (2026-11-08)

Host: devkitPro/libnx cross-build on Linux (GCC 16.1.0, CMake 3.30.5). Both `make -j4` and
`make -j4 borealis-prototype` succeeded. The existing ELF was also repackaged
through the default Makefile with
`make -j4 --assume-new="$PWD/TomoToolNX.elf"`, producing `TomoToolNX.nro`.
The default goal remains `all`. Production RTTI/exceptions flags remain
`-fno-rtti -fno-exceptions`; the pilot uses `-frtti -fexceptions`.

The pilot's link command contains only its main, sample and upstream Switch
lifecycle wrapper objects plus Borealis/platform libraries. Symbol inspection
found no production SaveMount, SaveEditor, SaveWorkflow, Config or HTTP classes.
All locale JSON parses successfully, and pilot key sets match in all three
locales. The vendored source matches the pinned Git tree except for the
explicit SDL context-selection patch; no source files from the snapshot are
missing, apart from the unused SDL/GLFW gitlinks.

**Emulator limitation:** Ryujinx Canary 1.3.338 fails on the unmodified NRO
before displaying the UI: `Unknown MRS 0xD53B2521` (`GCSPR_EL0`) inside
libgcc's exception unwinder. This is not a successful test of the deliverable
NRO in that emulator. Neither the installed toolchain nor emulator was upgraded.

For UI checks only, a temporary copy of the pilot ELF/NRO under `/tmp` was
adapted to skip the libgcc guarded-control-stack paths: five `CBNZ x16`
instructions following `MOV x16,#1; CHKFEAT x16` were replaced with unconditional
branches to their existing targets. No application instructions were changed.
This workaround is absent from the source, Make/CMake targets and deliverable
`TomoToolNXBorealis.nro`. It limits the runtime evidence to that adapted copy;
it does not validate the deliverable's unwinder or hardware behavior.

With that copy, using an isolated portable emulator profile, synthetic data,
keyboard-mapped controller input and mouse-generated touchscreen input:

| Check | Result |
| --- | --- |
| All five destinations; ZL/ZR forward/back and end wrapping | Passed |
| D-pad and left-stick focus movement, A activation | Passed |
| Held direction repeats focus; held ZR changes only one tab | Passed |
| Settings child and B return to its entry row | Passed |
| Return to tab restores previous content focus | Passed |
| Boolean and selector controls; values survive reopening the child | Passed |
| B closes selector/dialog; ZL/ZR does not switch tabs behind a child | Passed |
| Touch selects tabs, opens settings, edits controls and returns using Back | Passed |
| Focusable Exit pilot returns from the homebrew session | Passed |
| English and Latin American Spanish; visible focus; light theme at 720p | Passed |

The longer Spanish hints initially truncated the footer and wrapped the clock;
shorter localized hint labels fixed that layout. The longer descriptions wrap,
and the native scrolling frame keeps focused texture rows visible. The demo
empty slot is explicitly labeled Empty; no real asynchronous/loading/save-error
states exist in this sample.

The system's light theme was observed. Dark-theme appearance, automatic theme
selection on real hardware, physical Joy-Con/touch usability, docked scaling
and the unmodified NRO's runtime remain unverified on a physical Switch.
Plus/Minus/X/Y have no registered pilot actions (source review). No save-format
or save-write compatibility claim follows from any of these checks.

The emulator's system-font rendering shows uneven spacing around some accented
glyphs (for example Sí and Vacío). Shared-font rendering needs inspection on
hardware before adopting the framework.

## Fork branding

Builds now use `TomoToolNXPlus` as the main artifact and display name, and
`TomoToolNXPlusBorealis` as the prototype artifact name. Both NROs identify
`EddieBeru` as the fork author in their embedded NACP metadata. The prototype's
launcher title is `TomoToolNXPlus Borealis Pilot`; its English/Spanish screen
titles include Plus as well. At that branding update the versions were 1.3.10 and 0.1.0 respectively;
the functional settings milestone raises the prototype to 0.2.0.
Upstream and dependency credits are retained.

The numeric application Title ID remains unspecified, preserving the emulator
compatibility decision in the main Makefile. The game Title ID, existing SD
paths, configuration, backups and `.ltd`/`.ltdx` file formats are unaffected by
this packaging change. The NRO name/author fields carry the fork identity.

The verification record and captures above predate the Plus branding update.

Branding verification: `make -j4` and `make -j4 borealis-prototype` succeeded.
The embedded NACP was read back from each final NRO: all 12 language entries
populated by nacptool contain the expected Plus name and author EddieBeru;
versions match their build definitions and the presence group ID is zero.
English/Spanish pilot title resources include Plus and retain matching key sets.
The main build still reports existing warnings in unrelated code. These checks
were performed on the host; the renamed builds have not been rerun on hardware
or in the emulator.

## Functional settings milestone (2026-11-09)

The user reports that both Plus builds preceding v0.2.0 run well on a physical
Switch. This records user-reported hardware startup/runtime evidence for those
builds, without attributing specific controller, touch or theme checks to it.
The new v0.2.0 preference workflow requires its own hardware validation.
