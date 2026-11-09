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
BOREALIS_PROTOTYPE.md. The normal deliverable does not contain that workaround.
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

Native 720p captures: [settings](images/borealis-prototype/settings-v02-es.png)
and [invalid configuration](images/borealis-prototype/settings-error-v02-es.png).
