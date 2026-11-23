# Switch UI migration boundaries

The first functional slice is Texture Settings in the separate Borealis NRO.
The stable SDL application remains available for comparison. WebUI's transport
and presentation are outside this migration.

## What happens to existing drawing code?

| Existing responsibility | Borealis destination | Reusable portion |
| --- | --- | --- |
| Text, buttons, selection pills, lists, dialogs, focus outlines | Labels, DetailCells, Dropdowns, ScrollingFrames and Dialogs | Setting values, validation and application operations |
| Texture/Mii image previews | Image views backed by decoded RGBA pixels | Decoders, image data and preview-selection rules |
| Map terrain, actor overlays, grids, selection and zoom | A dedicated View implementing NanoVG drawing in its own bounds | Map data and extracted selection/coordinate rules |
| Bespoke diagrams or small procedural illustrations | A dedicated NanoVG View, where a standard component is insufficient | Geometry calculations that do not depend on SDL |
| Touch hit rectangles and per-screen controller switches | Borealis focus/gestures plus shared navigation policy | Domain commands and validation, rather than old button bindings |

SDL_Texture objects belong to the old SDL_Renderer and cannot be handed to
Borealis. For images, reuse CPU-side decoded data (such as RgbaImage), then let
the Borealis view create and own its image resource in Borealis's graphics
context. Upload and release graphics resources on that context's thread.
Workers may decode data; they must not manipulate the rendering context.

For a canvas such as DrawMap, extract the map/selection state from main.cpp,
then implement a focused View's draw callback with NanoVG paths, rectangles
and images. Convert input from the view's bounds into model coordinates, taking
zoom, scrolling and Borealis scaling into account. Keep model coordinates
independent of screen pixels. Expose zoom/tools as focusable controls with touch
equivalents and preserve selection/viewport when returning from details.
Do not recreate the old immediate-mode hit-zone globals inside the new view.

Borealis owns the window, context and event loop. Port each visual at this
boundary instead of running a second SDL renderer over the Borealis surface.
A low-level GL drawing path would need a demonstrated requirement and explicit
state/resource management; it is not needed for the settings slice.

## Texture Settings v0.2.0

The screen uses the existing TextureSettings model, including its row order,
values and dependencies. SelectValue is the shared validated mutation operation;
the original SDL command handler also uses it. Borealis handles focus and
selector navigation. The five settings are canvas shape, encoder, BC1 mode,
image fit and matte/background. Disabled preferences retain their old values.
An existing custom matte is retained until the user explicitly chooses a preset;
the selector includes a keep-current-color choice.

The screen owns a draft. Selecting values changes only that draft. Save
preferences persists it; Back with changes offers Save and return, Discard
changes, or Keep editing. B cancels selectors and dialogs first. Tab shortcuts
remain scoped to the root. No Plus/Minus/X/Y bindings are introduced in this
slice; all actions are focusable and tappable.

ConfigStore reads the existing `/switch/TomoToolNX/config.ini` so these settings
also apply to the stable app's future imports. It updates only changed texture
keys. Comments, unknown keys, unrelated settings, legacy theme/color keys,
inactive values and untouched line endings remain intact. It does not set the
Borealis theme from legacy keys. Invalid known texture values or a config over
64 KiB block editing until corrected/reloaded. ConfigStore has no game-save,
user-account, SDL or Borealis dependency.

Before overwriting an existing config, the store creates an exclusive backup
named `config.ini.borealis-backup.N`, flushes and closes it, and verifies its
bytes by reading it back. It retains earlier backups and tries up to 1000 names;
if none is available, saving fails without overwriting the config. It checks for
external edits against the loaded bytes before backup and again before writing.
This is conflict detection, not a cross-process lock or an atomic transaction.

The config write checks short writes, flush, close and readback. Success is
reported only after verification. No rename/rollback or journal atomicity is
assumed for SD. A partial failure leaves the verified original backup available;
the screen reports its path and asks for restoration. In the same session,
reload/retry remains blocked until the original file is restored (or a failed
new file is removed). Across a restart, restoration is manual; this does not
claim power-loss atomicity. No personal config or save fixture is checked in.

The stable app's legacy general config writer is unchanged. This slice improves
the migrated path without claiming the entire legacy configuration workflow has
been migrated.

## Next slices

Move the texture browser to real read-only data, then connect import/export and
editing through shared workflows. Extract bounded map/preview responsibilities
when those screens migrate, keeping game-data mutation out of drawing classes.
Every game-save write still requires the normal successful backup, validation,
commit/error handling and recovery path.

## Verification of v0.2.0

Host regression checks passed for the existing focus/setting model and the new
configuration store, including AddressSanitizer/UndefinedBehaviorSanitizer.
The store tests use synthetic files and inject read, backup, short-write, flush,
close and verification failures. Both NROs cross-built with the established
Makefile. The prototype's symbol list contains no SaveMount, SaveEditor,
SaveWorkflow or HTTP classes; its NACP reads back as EddieBeru / v0.2.0.

The emulator checks used the same temporary libgcc GCS workaround documented in
BOREALIS_PORT.md. The normal deliverable does not contain that workaround.
Using a synthetic SD config and keyboard-mapped controller / emulated touch,
checks passed for loading real values and a custom matte, draft-only editing,
BC1 disabled-state focus skipping, selector cancellation, dirty Back prompts,
Save and return with entry-focus restoration, touch selection/discard, and an
invalid config that remains untouched and can be reloaded after correction.
The successful save changed only the encoder token and retained an exact
original backup. An explicit Save preferences also changed only canvas shape,
retained its original backup and displayed the success notification. Spanish was inspected at native 1280×720; help text spacing
was adjusted using existing Borealis metrics. Translation key sets match in
English, Spanish and Latin American Spanish.

Physical Switch validation of this new configuration workflow, real SD failure
behavior, dark theme and docked scaling remain pending. Prior user-reported
hardware success applies to the earlier pilot builds. This slice cannot verify
game-save compatibility because it does not access game saves.

## Read-only texture browser — implementation of feature 001

Textures now lists real UGC files for a selected account, with optional names from
Player.sav and a detail preview. Other browser tabs remain samples. Texture Settings
keeps its existing draft/store behavior. This slice adds no texture mutation or export.

The new Switch source owns a private filesystem opened with fsOpen_SaveDataReadOnly,
not SaveMount or a writable devoptab mount. Files use FsOpenMode_Read; metadata enumeration
separates a missing save from an access failure. Read, size and enumeration errors are
reported; Player name failures degrade to identifiers. No source operation exposes
create/write/delete/rename/commit. Handles close on the worker before its session ends.

A serial session owns one active and one replaceable pending request. Delivery uses
weak lifetime tokens and a generation guard. Leaving a loading catalog suspends it;
returning reloads once with a new generation and retains navigation context. Entering
settings or dialogs cannot publish UI changes into an inactive page. Exit cancels work
and keeps the event loop running until the worker has drained, before quitting.

CPU decoding is extracted from TextureProcessor into texture_decode.cpp; existing
encoder/import entry points remain in the legacy module. The legacy Scan wrapper retains
its old behavior; the new pure Associate operation adds deterministic order, ambiguity
warnings and bounded catalogs for this browser. Names use the real typed index, bounded
heap/count spans and validated UTF-16, without linking MiiManager or save writers.
Preview chooses a valid rectangular canvas, otherwise the main texture; canvas failure
falls back with a warning. NanoVG images are uploaded/released on the UI/context thread,
FIT over a checkerboard, with no temporary image files or cross-renderer SDL textures.

Operational limits: Player 8 MiB, individual compressed/output image 4 MiB, zstd window
8 MiB, image axes 1024, 2048 primary textures and 8192 directory records. Exceeding limits
is an explicit error, never a successful truncated catalog. These are application limits,
not claimed game-format maxima. Paths/config/backups and WebUI remain unchanged.

### Verification and remaining acceptance

Host checks cover catalogs 0/1/100, names, layouts/golden pixels, zstd limits, source
failures, read-only API flags, cancellation and stale deliveries. Run
[the browser host suites](../tests/README.md#read-only-borealis-texture-browser).
Both Makefile targets cross-build; source/link-map inspection complements the source tests.
No personal saves were accessed or modified. Builds are not proof of compatibility.

The initial implementation run had no emulator or physical Switch available. Subsequent
user retests confirmed specific fixes (see below). Full controller/touch,
modal transitions, focus/scroll restoration on the renderer, three-language text layout,
light/dark themes, handheld/docked rendering, real account-save permissions and the
latency target still require the [acceptance guide](../specs/001-browse-real-textures/quickstart.md).
Its SC matrix distinguishes host evidence from pending physical acceptance. Do not call
this a fully verified editor replacement or infer success from the task checkboxes.


### Global identity and child-screen lifecycle — 2026-11-10

Physical feedback confirmed native startup selection. The texture-local account dialog
has been removed; the root header now owns the native change-profile action and shows
the selected nickname/avatar. Detail/settings headers retain the same visible identity.
The profile presentation replaces the default battery/time indicators in these frames.
Save/Exit uses a full-width standard Button, rather than a DetailCell with a narrow title.

ScreenActivity now forwards activity pause/resume to its view, because the pinned
Borealis Activity implementation leaves those hooks empty. This suspends browser work
behind child activities and prevents late root renders/focus changes during settings.
Settings opens and returns without a fade, preventing two full screens from being
blended. The browser's catalog/selection/scroll are retained through the pause.
New portrait limits/failures are covered by the native adapter host tests. Physical
profile changes, portrait/button layout and the revised transition still require retesting.


### Convergence evidence — 2026-11-12

The [physical feedback record](HARDWARE_UI_FEEDBACK.md#confirmed-physical-retest-and-closure)
confirms startup native selection, focus/footer corrections, Plus opening Save/Exit,
proportional previews without edge repetition and controller reachability of the preview.
These are user-reported physical results for the previously installed build, not new
hardware tests of this convergence change. Earlier “pending retest” notes above describe
historical checkpoints; the confirmation supersedes them only for those reported fixes.

The [acceptance matrix](../specs/001-browse-real-textures/quickstart.md#matriz-de-aceptación-detallada--2026-11-12)
records all ten host suites passing (including `tests/borealis_ui_screens_test.cpp` for
Borealis screen controller/touch/alternating-input/20-cycle/10-tab/settings/exit-menu
flows against the host UI shim) while keeping untested physical account/error paths,
input methods, repeated cycles, preferences, locale/theme/dock combinations and latency
explicitly pending on physical hardware. The new ambiguous-image notice also requires
visual/touch verification. No emulator run or new physical run is claimed.

On 2026-11-19, after deploying via `make -j4 deploy-borealis`, user physical feedback
confirmed the M0 flows look good aside from wide lateral spacing around `á` in
“Automático” caused by Borealis defaulting Switch labels to
`PlSharedFontType_ChineseSimplified`. `navigation::initialize()` now promotes
`PlSharedFontType_Standard` as the primary font with CJK/icon fallbacks without modifying
vendored Borealis sources; after redeploying with verified MTP SHA-256 readback, the
user confirmed the fix on physical Switch hardware (“ahora sí”).


### Milestone M1 — Read-only Player tab (Player.sav) — 2026-11-19

- Replaced the fictional sample entries in the `Player` (`Jugador`) tab (`apps/borealis/source/app.cpp`) with `PlayerBrowserScreen` (`apps/borealis/source/player_browser_screen.{hpp,cpp}`), backed by the pure read-only parser and formatter `PlayerProfile` (`include/player_profile.h`, `source/player_profile.cpp`).
- Reuses `TextureBrowser::ReadSource` (`fsOpen_SaveDataReadOnly`) and `TextureBrowser::Session` without linking `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager`, or `SaveEditor::Save`, and without modifying `source/main.cpp`.
- Empirical `seed_Player.bin` check confirmed `Player.HowToCallName` and `Player.HowToCallIslandName` use `DT_WStr64` (128 bytes) and `DT_Bool64Key` entries may carry `slot == 0`; the new reader supports both `DT_WStr64` and `DT_WStr32` with full UTF-16LE surrogate pairs.
- Renders all 15 attributes inline in the scrollable list (each `DetailCell` followed by an indented multiline `Label` for secondary metadata such as pronunciation/language, wish liberation progress over 268 entries, and donation elapsed days, plus read-only description), preserving `focusTarget` and scroll offset across tab switches and global dialogs.
- **Verification**:
  - Host: all 12 suites in `tests/run_texture_browser.sh` (plain and `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0`), `en-US`/`es`/`es-419` i18n key parity (`162` keys each), and standalone host regressions passed.
  - Builds & ELF audit: `make -j4` and `make -j4 borealis` succeeded; `aarch64-none-elf-nm -C build/borealis/TomoToolNXPlusBorealis.elf` reported 0 references to `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager`, or `SaveEditor::Save`.
  - Emulator: not executed. Physical Switch hardware verification remains pending per `specs/002-browse-player-readonly/quickstart.md`.


### Milestone M2 — Read-only Mii tab (Mii.sav + optional Map.sav) — 2026-11-22

- Replaced the fictional sample entries in the `Mii` tab (index `2` in `apps/borealis/source/app.cpp`) with a 3-level hierarchical read-only browser without subtabs: `MiiBrowserScreen` → `MiiDetailScreen` → `MiiSectionScreen` (`apps/borealis/source/mii_browser_screen.{hpp,cpp}`), backed by the pure read-only module `MiiRoster` (`include/mii_roster.h`, `source/mii_roster.cpp`).
- Extended `TextureBrowser::ReadSource` (`include/texture_browser.h`, `apps/borealis/source/texture_read_source.cpp`) strictly under `fsOpen_SaveDataReadOnly` (`FsOpenMode_Read`) to allow `"/Mii.sav"` (`MaxMiiBytes = 8 MiB`) and optional `"/Map.sav"` (`MaxMapBytes = 4 MiB`), without linking `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager`, or `SaveEditor::Save`, and without modifying `source/main.cpp`.
- Covers up to 70 residents (including empty islands with `0` residents distinguished from errors) and all 7 resident sections (`Stats`, `Words`, `Relations`, `Belongings`, `Habits`, `Housing`, `Social Summary`), reusing `include/belongings_data.h`, `include/habits_data.h`, and `include/rel_tier_labels.h` with local degradation when `Map.sav` is absent or unknown enum hashes appear.
- **Verification**:
  - Host: all 14 suites in `tests/run_texture_browser.sh` (plain and `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0`, including `mii-roster`, `mii-context`, and expanded `ui-screens`) and `en-US`/`es`/`es-419` i18n key parity (`290` keys each) passed.
  - Builds & ELF audit: `make -j4` and `make -j4 borealis` succeeded; `aarch64-none-elf-nm -C build/borealis/TomoToolNXPlusBorealis.elf` reported `0` references to `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager`, or `SaveEditor::Save`.
  - Emulator: not executed. Physical Switch hardware verification remains pending per `specs/003-browse-mii-readonly/quickstart.md`.
