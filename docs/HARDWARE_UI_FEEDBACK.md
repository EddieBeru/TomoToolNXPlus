# Hardware feedback — 2026-11-10

User confirmed that startup native profile selection appears and works on a physical
Switch. This evidence covers that flow, not all browser or loader-return acceptance.

Requested corrections:
- Remove the old account list dialog and texture-local account action. The root header
  must expose the native selector as a global, focusable/tappable profile control.
- Show the selected profile's nickname and bounded avatar in the header, including child
  details/settings. Missing/failed avatars use a neutral fallback, never another profile.
  Cancelled selection preserves account, images and navigation; confirmed account changes
  invalidate old reads and previews. Profile images remain memory-only.
- Use a full-width standard sidebar Save/Exit button with sufficient height and no clipped
  label in the included locales. Keep Plus and the existing draft/error exit rules.
- Pause/resume browser work across child activities; make settings entry/return immediate
  rather than blending both full screens. Restore the same entry control and scroll.
- Hide the default battery/time widgets in these app frames so profile identity is clear.

Verification: native adapter avatar success/failure/oversize tests, browser/session/config
regressions and cross-build. Real input, layout, native re-selection/cancellation, focus
return and transitions still require a follow-up physical run of the new NRO.

## Implementation evidence

- Native selection/avatar adapter checks passed, including missing/failed/oversized avatar,
  invalid returned length and account/profile cleanup. No personal fixtures were added.
- All eight host suites passed; sanitizer runs and config preservation/failure regressions
  passed. New profile-header text keys match across English, Spanish and es-419.
- make -j4 borealis produced the updated TomoToolNXPlusBorealis.nro with the existing toolchain.
- No new hardware run was performed by the agent. Retest the global native change/cancel,
  avatar and long nickname, full sidebar label, controller/touch focus and immediate
  settings entry/return (including dirty Save/Discard/Keep editing).


Follow-up: the battery widget remained in dialogs/dropdowns that create their own footer.
Application overlay factories now hide battery/time throughout those view trees, including
Dropdown's external BottomBar. Plus now offers Save separately from Exit. In Settings,
Save persists the draft and keeps the screen open; elsewhere it reports no pending changes.
No new physical verification is claimed for this follow-up.

## Central navigation and footer policy — 2026-11-12

The Borealis navigation module now owns B/Plus/ZL/ZR bindings, root Back
suppression, child push/pop transitions, and the application footer factory.
Root bindings live on the frame, including the profile header's focus scope.
Screens supply contextual callbacks; Settings retains its existing dirty-draft,
save-error and exit checks. Focusable/touch controls retain the same callbacks.
Child transitions are immediate, with lifecycle forwarding and Borealis's parent
focus stack; modal dialogs retain library-owned isolation and animations.

Every XML-created BottomBar, including the dropdown's separate footer and any
library-created overlay, now uses one application subclass. It keeps Borealis's
standard hints, layout and separator while hiding clock/battery/wireless once at
construction and skipping clock updates. Per-screen recursive hiding was removed;
no vendored Borealis changes are needed. This applies to the Borealis app only.

Host verification: `make -j4 borealis` passed and produced the updated NRO;
`tests/run_texture_browser.sh` passed all eight suites. These checks cover build
integration and browser/model/session/profile regressions, not rendered footer
appearance or actual input. The configuration regression command in
`tests/README.md` also passed its preservation, persistence and failure checks.

Physical follow-up remains pending: root/sidebar/profile-header hints; Plus from
each screen; settings dropdowns; Save/Exit, dirty-draft and error dialogs; absence
of clock/battery/wireless in every footer; B focus restoration and retained scroll;
held buttons and input switching; controller-only and touch-only operation; all
three locales and both themes at handheld and docked resolutions. Confirm Plus
and ZL/ZR cannot bypass a modal or discard a settings draft. No new hardware or
emulator validation is claimed.

## Texture detail focus — 2026-11-12

Physical feedback: opening a texture left the apparent focus on the underlying
browser and prevented controller navigation. The detail had no explicit initial
control and disabled Retry during loading. Its stable entry control is now Back,
placed before the preview with the other actions. Centered controller scrolling
keeps focused detail actions visible. Retrying moves focus off Retry before it is
disabled; preview completion does not move focus.

The shared navigation guard permits focus changes only within the top activity
and validates the resolved default target as well. Browser rendering and deferred
layout restoration use this guard; restoration stays pending while another
activity owns the screen. The shared push operation retries focus assignment if
Borealis leaves it outside the entered screen. Parent focus/scroll restoration
continues through the activity stack and browser context.

Host focus ownership regression and all eight browser/profile suites passed.
The Borealis cross-build passed. These checks do not reproduce the physical
renderer. Pending Switch retest: A and touch entry; visible Back during loading,
success and error; Retry; Plus modal isolation; B and touch return to the original
texture and scroll position; repeated entry/return and input switching.

Additional physical feedback identified the green block immediately to the left
of the footer clock, including on the main screen. Borealis's BatteryWidget
colors its level green while charging. The footer override had been installed
before `Application::createWindow()`, whose built-in XML registration then
replaced the custom BottomBar factory. Startup now installs navigation after
window creation and before creating any app views. This fixes the registration
order for root, child and overlay footers without modifying the library. The
previous build had not actually activated the shared footer policy.

The combined focus/footer build passed `make -j4 borealis`. The footer diagnosis
is based on source inspection, not a new physical run. Retest root/detail/settings,
Save/Exit dialogs and dropdowns: battery/wireless/clock absent, contextual hints
present, and controller focus restricted to the active screen.

## Preview bounds and controller reachability — 2026-11-12

User confirmed the previous focus and footer corrections on Switch. New feedback:
texture previews extended across the view by repeating their edge columns, and
controller navigation could not reach the bottom of the preview.

Borealis Image draws its image pattern across the entire view when clipsToBounds
is enabled, including the fitted image's letterbox area. TexturePreviewView now
draws the RGBA image and checkerboard only inside one shared, centered FIT
rectangle. Its height is fixed at 320 without flex growth/shrink. No edge pixels
are painted into the surrounding space. This changes rendering only, not decoded
pixels or save data.

A successfully loaded preview is now a focus target after the detail actions.
D-pad/left-stick Down can reach it; the existing centered scroller brings the
bounded preview fully into view. Clearing/loading/failure removes that focus
target. Touch retains the scrolling frame's drag behavior. The preview keeps a
focus outline but suppresses the highlight background over the image.

Host aspect-ratio/bounds and navigation-scope regressions passed; the Borealis
cross-build passed. Pending physical check: square/book/TV layouts and transparency,
no replicated edge columns, focus the entire preview with controller Down, return
Up to actions, touch drag, loading/error/retry, and B return preserving browser
position. No new physical or emulator rendering validation is claimed.

## Plus / Save–Exit shortcut registration — 2026-11-12

Physical feedback: Plus stopped opening the Save/Exit menu. The shared helper
was named `bind`; argument-dependent lookup selected `std::bind` for some calls,
notably the temporary label/Action in Save/Exit and lambdas in root/tab bindings.
The discarded bind object registered no action. The helper is now `bindAction`,
removing that overload collision. Existing menu workflows and draft handling are
unchanged; Plus opens the menu rather than immediately exiting.

A host test compiles the actual navigation implementation and checks installed
actions and callbacks from root content, header and child content, B suppression,
ZL/ZR and modal isolation. It reproduced the missing registrations using the old
helper name and passes with the rename. The focus-scope regression also passes;
`make -j4 borealis` passed. The updated NRO also includes the pending preview
bounds/controller-reachability fix. Physical retest remains pending: Plus from
root/header/detail/settings, Cancel/Save/Exit choices, dirty settings/error paths,
no shortcut reach-through during dialogs, and ZL/ZR tab switching.

## Confirmed physical retest and closure — 2026-11-12

After installing the combined build, the user reported “probado y funciona bien”.
This confirms Plus opening the Save/Exit menu, proportional preview rendering
without extended edge columns, and controller reachability/scrolling of the
preview. Earlier feedback confirmed the focus and footer corrections. The final
installed NRO was downloaded directly from DBI and matched its expected SHA-256:
`cf01950123728387854aa2847424049a724eb94a90efadc4ee6f9fef5a68169e`.
Verified previous-NRO recovery copies remain local and gitignored.

This closes the reported regressions; it does not establish every acceptance
matrix row. Exhaustive repeated cycles, all locale/theme/docked combinations,
profile cancellation/error cases and performance measurements remain pending.
The feature task checklist is not marked complete on the basis of this feedback.

KDE deployment is now part of the repository workflow rather than depending on
personal absolute paths or temporary helpers. See docs/DEPLOY_SWITCH.md for the
explicit undated-target option, safeguards and evidence boundaries.
