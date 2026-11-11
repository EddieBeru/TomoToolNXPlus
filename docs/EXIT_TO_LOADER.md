# Exit to the homebrew loader (Borealis)

## Behavior

Plus opens the shared Save/Exit menu; it does not terminate immediately. A persistent,
focusable/tappable Save/Exit control is available in the root sidebar. Texture details
and Texture Settings expose equivalent controls. Dialogs/selectors retain their input scope.

The shared menu offers Cancel, Save and Exit to loader. Save persists dirty preferences
and keeps Settings open; on read-only screens it reports that no changes are pending.
Choosing Exit with dirty preferences then offers Keep editing, Save preferences and exit,
or Discard preferences and exit. Save failure keeps the app
open with its existing error/recovery feedback; exit happens only after successful saving
or explicit discard. No game-save mutation/commit is introduced.

After confirmation, reject duplicate exit requests, cancel/drain the read worker while
keeping the event loop alive, then quit Borealis and return from main. Use libnx's normal
loader return with exit-to-HOME mode disabled. Do not hard-code hbmenu paths, chain-load
another NRO, call exit from a callback, or bypass cleanup.

## Verification

Build Borealis; run host browser/session and preference failure regressions. Physical
Switch checks remain required: Plus/Cancel and focus restoration, touch sidebar exit,
details exit, dirty preferences Save/Discard/Keep editing, failed Save staying open,
exit during loading and return to the originating loader in supported launch modes.


Host browser/session/profile suites and config preservation/failure regressions passed.
The Borealis Makefile target builds successfully. No physical loader-return or input
validation was performed for this change; those acceptance checks remain pending.


## Save-only and overlay status widgets — 2026-11-10

Save from Plus reuses the existing verified preference store and success/error feedback;
it neither pops the Settings activity nor requests application shutdown. An invalid or
partially failed config cannot be saved until recovery/reload, and gives guidance instead.
No Save option enables writes to game saves in the read-only browser.

Application dialog/dropdown factories now apply the same status-widget policy as root
frames. Dropdown owns a separate BottomBar outside its AppletFrame; hiding only the
frame footer missed that battery indicator. Both footer instances are now covered without
removing controller hints or modifying vendored Borealis.

Cross-build and host config preservation/failure regressions passed. Physical confirmation
of overlay appearance, Save staying in Settings and the error/exit routes remains pending.
