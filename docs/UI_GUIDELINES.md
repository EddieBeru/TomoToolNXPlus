# UI guidelines

This guide defines the direction for interface changes in this fork. Apply it
incrementally to affected screens. It does not claim that existing screens
already meet these requirements. Save-integrity rules in `AGENTS.md` take priority.

## Controller and Joy-Con navigation

The target is one global navigation system, replacing separate button bindings
for individual actions. Its implementation is a planned major feature; the
following rules guide new work and incremental migration of existing screens.

| Input | Global meaning |
| --- | --- |
| ZL / ZR | Previous / next top-level tab. |
| D-pad / left stick | Move focus within the current navigation scope. |
| A | Activate the focused control or enter its child scope/editor. |
| B | Cancel the current interaction, close the current layer, or return focus to the parent scope. |
| Minus (-) | Open the context menu for the focused element, when available. |
| Plus (+) | Open the global Save/Exit menu. |
| X / Y | Optional contextual interactions; specific roles remain unassigned. |

- Reserve ZL/ZR for top-level tabs across the app. Do not repurpose them for
  subtabs, field editing, or individual actions. Modal interactions and unsafe
  write phases may temporarily block tab switching; do not bypass a dialog,
  silently discard edits, or interrupt a write to switch tabs.
- Keep one active controller focus in the current scope. Make it unmistakable
  with a consistent outline, highlight, or equivalent treatment that does not
  depend on color alone. Distinguish focus from selection, toggled state, and
  disabled state.
- Use predictable spatial navigation for layouts and sequential navigation for
  lists. Every essential control must be reachable; avoid focus traps and
  unexpected jumps between unrelated regions. D-pad and left stick must expose
  the same navigation paths, with controlled repeat behavior.
- Present actions as focusable buttons, menu entries, or controls activated with
  A. Do not add one-off X/Y/L/R or stick-click bindings as the required route to
  an action. Existing shortcuts may remain during migration, but must not define
  the architecture or replace the focus-based route in migrated screens.
- Minus opens secondary actions for the focused element without executing them.
  Use normal focus navigation and A inside the menu; B closes it and restores
  focus to its owner. Indicate context-menu availability and provide a focusable,
  touch-accessible way to open it. If no context actions exist, Minus must not
  trigger an unrelated operation.
- Plus opens a consistent Save/Exit menu rather than immediately writing or
  exiting. Present the applicable choices explicitly, such as Save, Save and
  exit, Exit, and Cancel. Save and exit must exit only after backup, validation,
  writes, and commits succeed; failures keep the app open with recovery guidance.
  Explain pending changes before exiting without saving. Only offer discarding
  changes when they are actually uncommitted; do not imply that exiting undoes
  already committed writes. Keep this menu reachable through a focusable control
  and touch as well.
- Context and Save/Exit shortcuts must respect active dialogs, field editors,
  and write phases. They must not bypass an unresolved interaction or interrupt
  a write. Use the shared navigation system to control their availability.
- X/Y may accelerate a special interaction for the current focus or screen.
  Their specific roles will be decided when the feature is designed. Label the
  actual action when available, keep recurring meanings consistent, and provide
  the same action through a focusable control or context menu and through touch.
  Do not add shortcuts simply because buttons are unused, or let them bypass
  validation, confirmation, or save-integrity requirements.
- A enters the focused element's interaction or child scope. Within a field
  editor, directional input may edit the value, but distinguish editing from
  navigation visually and show how to confirm or cancel. Focusing or entering a
  control must not implicitly commit save changes.
- B acts on the innermost active scope first: cancel an editor, close a dialog,
  or return to the parent. Returning from a child restores focus to its entry
  control where possible. At the root, do not assign B an unrelated action.
  Handle unsaved edits explicitly; do not silently discard or commit them as a
  side effect of backing out or changing tabs.
- Do not introduce new subtabs. Prefer focusable sections, lists, or child
  screens entered with A and left with B. Existing subtabs may be retained
  temporarily, but when adapting them, expose their selectors through focus
  and A rather than introducing another shoulder-button navigation layer.
- Show persistent, concise ZL/ZR tab hints on top-level screens and contextual
  A/B hints for the focused element or active scope. Show Plus for Save/Exit where
  available, Minus when context actions exist, and X/Y only for available special
  interactions. During a modal or blocked operation, indicate unavailable
  navigation appropriately. Avoid a growing legend of per-action button assignments.
- Preserve the selected item, filters, and scroll position when returning from
  details, closing a modal, or revisiting a tab. After removing an item, move
  focus predictably to a remaining item or a meaningful empty-state control.
- Keep focused controls visible when navigating scrollable content. Moving
  focus must not activate an action or start a destructive operation.
- When a dialog opens, move focus into it and restore focus to a meaningful
  control when it closes. Do not allow input to activate controls behind it or
  let the button press that opened it also activate its default action.
- Centralize input-to-navigation mapping, focus ownership, and parent/child
  transitions in shared navigation code. Screens should describe their controls
  and actions rather than reimplement the global button scheme. Keep navigation
  logic separate from rendering and save operations.

## Touch

- Use comfortably sized, separated targets and validate them on the Switch
  display. Do not copy desktop control density without checking usability.
- Give clear pressed feedback and distinguish taps, drags, and scrolling.
  Scrolling or releasing a drag must not accidentally trigger a destructive action.
- Provide an alternative to precision gestures such as dragging tiny map objects.
  Essential actions must also be available through controller navigation.
- Switching between touch and controller must leave focus and selection coherent.
- Do not depend on hover, tooltips, or color alone to communicate essential state.

## Visual system and information hierarchy

- Establish shared style values for typography, spacing, colors, radii, and
  component states before adding new screen-specific visual variants. Reuse
  existing shared values where available; extract them as needed.
- Give each screen a clear purpose and primary action. Group related settings,
  use progressive disclosure for advanced controls, and keep labels consistent.
- Use readable text and clear contrast at the Switch's actual display size.
  Distinguish normal, focused, selected, pressed, disabled, and error states.
- Keep high-frequency actions easy to reach. Visually separate destructive
  actions and describe their consequences before execution.
- Use confirmations for consequential actions. Avoid repetitive dialogs for
  harmless navigation or reversible local edits.
- Keep motion purposeful and restrained; decoration must not obscure focus,
  delay navigation, or compete with progress and error feedback.
- Keep translated text and user-generated names readable without overlapping
  controls. Provide a way to inspect essential text that must be truncated.

## Operations and recovery

- Represent loading, empty, success, and error states explicitly. Include a
  useful next step for empty and error states.
- Give immediate feedback when an operation starts. Show measured progress when
  available; otherwise communicate that work is ongoing without invented percentages.
- Avoid blocking the interface during expensive work where practical. Prevent
  duplicate submissions and conflicting edits while a save operation is active.
- Clearly distinguish editable changes, writes in progress, and committed results.
  Never display a successful save before the required commit succeeds.
- Before a destructive action, identify the affected data and user. Offer
  cancellation before writing and describe the available backup/recovery path.
- Offer undo or cancellation only where the operation can actually support it.
  Explain partial failure and recovery options without claiming an automatic
  rollback occurred unless it was verified.

## Review and verification

- Check the affected flow with controller navigation and touch, including
  opening/closing dialogs, returning from details, and switching input methods.
- For migrated flows, verify every essential action is reachable using only
  ZL/ZR, D-pad/left stick, A, and B. Check parent focus restoration, tab context
  preservation, modal isolation, held-input behavior, and visible focus while
  scrolling. Verify ZL/ZR never operate a secondary navigation layer.
- Check Minus menu ownership and focus restoration, Plus save/exit error handling,
  and any X/Y shortcuts. Verify their focus-based and touch equivalents and that
  shortcuts cannot bypass dialogs, confirmations, or active writes.
- Check relevant empty lists, long lists, long names, translated text, invalid
  input, loading, disabled controls, and error recovery.
- Inspect affected screens at the Switch's native 1280 x 720 handheld resolution;
  check docked output when the change affects scaling or layout there.
- Capture before/after screenshots when practical for visual changes. Screenshots
  help review aesthetics but cannot establish input usability.
- Separate host/emulator checks from physical Switch checks in the completion
  report. Identify unverified touch, Joy-Con, or hardware-specific behavior.
