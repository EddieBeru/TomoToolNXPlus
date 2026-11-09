# Texture-settings focus pilot

Shared scope navigation and input consumption live in `focus_navigation`.
Texture settings uses parent row focus and a child option scope. Preferences
remain unchanged until confirmation; only confirmed changes persist config.

- D-pad/left stick Up/Down focuses enabled rows; A enters their options.
- Left/Right chooses a candidate; A applies it; B cancels and restores row focus.
- B at row level returns to Textures, with the gear action focused.
- Tap a row or option to enter editing; Apply/Cancel have touch buttons.
- The row/option outline and arrow indicate focus. Filled options with a check
  mark indicate committed values. Custom matte colors select no preset.
- A/B prompts and the navigation footer use the bundled controller icons.
- Editing blocks other rows, tab touches, and legacy L/R/Plus actions. One frame
  cannot both enter and confirm, including combined touch/controller input.

The full global ZL/ZR remap, Minus context menus, and Plus Save/Exit menu remain
separate migrations. Existing L/R tab shortcuts remain available at row level.

Host checks cover scopes, disabled-row traversal, preview/cancel/confirm, preserved
inactive values, and input consumption. Emulator and physical Switch checks for
this pilot remain pending. Test at Textures → select an existing texture → gear:
enter each row, move its option focus, cancel, then repeat and apply. Check touch,
held directions, returning to Textures, and that dragging never activates a pill.
