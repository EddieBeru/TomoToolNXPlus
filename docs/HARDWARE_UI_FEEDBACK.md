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
