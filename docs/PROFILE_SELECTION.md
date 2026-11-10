# Startup profile selection (Borealis)

## Behavior and acceptance

- Show Switch's native user selector once when the Borealis application starts.
- Require no Nintendo Account/network login and do not permit profile creation.
- Confirmed selection becomes the texture browser's initial account. Open Textures and
  load that account through the existing read-only workflow, without asking again.
- Skipping/cancelling does not choose an arbitrary account, mount a save or close the app.
  Keep the existing Choose account control available in Textures.
- If the applet cannot launch, keep the app usable and explain that the user can select
  an account from Textures. A failed nickname lookup keeps the selected UID with a
  localized fallback name. Never use a nickname as identity or display/log the UID.
- Existing Change account, preferences, controller/touch routes and read-only boundaries
  remain available. No save write, commit or backup is introduced.
- The legacy SDL target is outside this change.

## Verification

Host: compile the native adapter against the libnx test shim. Check settings, selected
UID, skip, applet setup/show failure and failed profile lookup with balanced account/profile
cleanup. Build Borealis against the installed real libnx headers and run existing browser
regressions. Physical Switch acceptance is still required for native applet presentation,
B cancellation, controller/touch handoff and startup loading of the chosen account.


Host adapter tests and the existing browser suites passed. The Borealis target builds
against the installed libnx API. No physical Switch or native applet run was performed;
B cancellation/result behavior and input/focus restoration still require console checks.
The startup adapter passes only the confirmed account identity to the browser, with a
bounded optional nickname. It never opens a game-save filesystem itself.
