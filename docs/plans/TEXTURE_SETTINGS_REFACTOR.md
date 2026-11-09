# Complete the texture-settings refactor

Status: implemented. Host regression checks, the Switch build, and user-reported
emulator checks passed. Physical Switch testing remains pending. The findings
below describe the starting state; completion details are recorded at the end.

## Goal and scope

Finish the existing texture-settings row refactor before starting the global
navigation feature. Keep the user's row-action table approach, remove duplicated
availability rules, and establish a small responsibility that can leave
`main.cpp`. Do not expand this task into a rewrite of the editor.

This screen changes application preferences used by later texture operations.
Completing the refactor must not itself import textures, mutate game saves, alter
backups, or change save/exit semantics. The previously identified save/exit error
handling issue remains separate, high-priority work and must be resolved before
building a new Save/Exit flow on top of it.

## Findings

- The pending diff adds `TexRow`, a row table with action/availability callbacks,
  and controller navigation that tries to skip disabled rows.
- The five rows are Shape, Encoder, BC1 Mode, Fit, and Background. BC1 Mode is
  available only with the Custom encoder; Background only with Contain fit.
- Moving Down from Fit when Background is disabled still selects Background:
  the loop reaches index 4 and stops without finding an enabled destination.
- Rendering registers touch selection for disabled rows. Tapping an enabled
  pill changes its setting without consistently updating controller focus.
- Controller and touch code duplicate availability checks and setting changes.
  A dependency changed through touch can leave focus on a now-disabled row.
- Row actions use numeric names and a `-99` no-input sentinel. A advances Shape
  and Encoder, but does nothing for BC1 Mode, Fit, and Background. This legacy
  difference must be explicit rather than accidentally changed during extraction.
- Several comments still describe two or four settings rather than the five
  actual rows. Row counts and indices are repeated as literals.

## Reviewable implementation steps

1. **Extract the existing behavior.** Introduce a small texture-settings model or
   controller in `include/` and `source/`. Use named row identifiers and explicit
   state/dependencies rather than callbacks reaching into `main.cpp` globals.
   Keep SDL drawing, libnx input decoding, and config persistence at the boundary.
   Initially preserve current setting transitions, including legacy A behavior;
   do not combine this structural step with interaction changes.
2. **Finish row navigation and availability.** Use one source of truth for row
   order, count, and availability across controller navigation, drawing, and touch.
   Search for an enabled destination before changing focus. At either boundary,
   retain the current valid row instead of landing on a disabled one. If a
   dependency disables the focused row, choose the next enabled row, falling back
   to the previous one when necessary. Disabled rows remain visible with their
   explanation but cannot receive interactive focus or activate.
3. **Unify input paths and make actions explicit.** Represent navigation,
   previous/next value, activation, and touch selection explicitly; an idle frame
   must dispatch no setting action. Replace `-99` and numeric helper names. Route
   touch and controller changes through the same validated transitions. A valid
   pill tap updates focus to its owning row. Preserve inactive setting values
   when their controls become unavailable. Persist only actual preference changes
   through the existing config mechanism, not on focus movement or idle frames.
4. **Verify and prepare the completion commit.** Correct stale comments and use
   the row definition's count rather than repeating five/last-index literals.
   Preserve the file's existing line endings and avoid unrelated formatting.
   Build using `make`, run focused host checks for the extracted logic, and
   report separately any emulator or physical Switch verification. Include only
   changes belonging to this refactor in the commit.

## Acceptance checks

- All five rows and existing setting values remain available in their valid
  configurations; texture-processing behavior is unchanged.
- With PCA selected, movement skips BC1 Mode in both directions. With Custom
  selected, BC1 Mode is reachable.
- With Cover or Fill selected, Down from Fit stays on Fit; Background is never
  focused or activated. With Contain selected, Background is reachable.
- Changing Encoder or Fit through touch repairs focus if the current row becomes
  disabled. Disabled row taps cannot change focus or values. Enabled pill taps
  update the owning row's focus and the requested value.
- Focus movement and idle input do not change preferences or persist config.
  Existing left/right wrap behavior and the documented legacy A behavior remain
  unchanged during this refactor. B continues returning to the texture screen.
- Pure logic checks cover disabled boundaries, dependency changes, value
  transitions, and idle input. Test invariants and outcomes rather than mirroring
  the callback table's implementation.
- A successful build is recorded separately from checking visible focus, touch
  target behavior, and held-input repeat on a physical Switch.

## Follow-up: global navigation pilot

This completion is not the unified navigation feature. Once it is verified, reuse
the explicit model and actions when introducing shared focus ownership and
parent/child navigation. Replace the legacy A behavior with entering an editor or
choice scope, use B to cancel/return, and reserve ZL/ZR for top-level tabs through
the shared system. Do not add another screen-specific button scheme here.

The current L/R navigation and Plus exit behavior are legacy behavior, not the
target design. Minus context menus, Plus Save/Exit, full touch/controller focus
integration, and visual redesign belong to that separately reviewed feature under
[the UI guidelines](../UI_GUIDELINES.md).

## Implementation and verification

- Added `include/texture_settings.h` and `source/texture_settings.cpp`. The model
  owns named rows, availability, value transitions, and focus repair, with no SDL,
  libnx, filesystem, or config-persistence calls.
- Consolidated the five preference globals and row selection into one model
  state. The screen decodes input and persists actual preference changes through
  the existing config mechanism. Rendering and touch registration use the same
  availability rules as controller navigation.
- Disabled boundary rows can no longer capture focus. Disabled touch targets are
  not registered and model operations reject them. Enabled pill taps also focus
  their owner. Inactive BC1 and matte values are retained.
- Removed numeric row-step helpers, the no-input sentinel, duplicated matte
  indexing, stale comments, and literal row bounds from the event handler.
  Preserved legacy A, L/R, B, and Plus behavior for the later navigation feature.
- Host regression checks passed with C++17, warnings treated as errors,
  exceptions/RTTI disabled, and again with AddressSanitizer/UndefinedBehaviorSanitizer.
  Run instructions are in [the tests README](../../tests/README.md).
- `make -j2` built `TomoToolNX.nro` successfully. Existing compiler warnings in
  `main.cpp` remain. Import, config loading/writing, blank-texture creation, and
  background-removal entry points were checked against the starting version:
  only preference-field access changed in those functions. CRLF line endings in
  `main.cpp` were preserved.
- User-reported emulator checks passed on 2026-11-08: navigation skips BC1 Mode
  with PCA, Down stays on Fit when Background is disabled, Contain enables
  Background, enabled option taps update value and row focus, disabled row taps
  have no effect, and B returns to Textures. Emulator/version was not specified.
- Automated checks used no personal saves. The host checks do not establish
  physical input, rendering, config I/O, or game-save compatibility. On-device
  review still needs to check disabled-row visuals, touch hit areas, switching
  between touch and controller, repeat behavior, and B returning to Textures.
