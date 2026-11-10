# Exit to the homebrew loader (Borealis)

## Behavior

Plus opens the shared Save/Exit menu; it does not terminate immediately. A persistent,
focusable/tappable Save/Exit control is available in the root sidebar. Texture details
and Texture Settings expose equivalent controls. Dialogs/selectors retain their input scope.

Read-only screens offer Cancel and Exit to loader. Dirty preferences offer Keep editing,
Save preferences and exit, or Discard preferences and exit. Save failure keeps the app
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
