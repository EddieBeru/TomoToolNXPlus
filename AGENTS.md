# Development rules

These rules apply throughout this fork. Save integrity has the highest priority
when project requirements compete. Existing code is not evidence that a pattern
is acceptable: these are requirements for ongoing development, not a claim that
the entire codebase already complies.

## 1. Save integrity comes first

- Before modifying existing save data, require a successful, recoverable backup
  for the affected user and data. A background backup being finished does not
  establish success; check its error result. Abort the write if backup fails.
- Validate inputs, bounds, field types, and relevant cross-file relationships
  before mutation. Do not guess unknown save-format behavior. Resolve the
  uncertainty before enabling writes that depend on it.
- Preserve unrelated fields, unknown fields, and format metadata. A focused edit
  must not silently normalize or discard other data.
- Check and propagate read, write, close, and commit failures. Report success
  only after the required writes and commits succeed.
- Plan recovery for partial failure in operations spanning multiple files.
  Respect the platform's journal and commit constraints; do not assume a rename,
  rollback, or multi-file transaction is supported or atomic without evidence.
- Keep backups and restore targets isolated by user. Do not replace the only
  recoverable copy with an incomplete backup or unverified result.
- Keep personal saves and identifying data out of source control, logs, and
  test fixtures. Use synthetic or sanitized fixtures for validation.
- Never trade these safeguards for aesthetics, performance, or convenience.

## 2. Low coupling, high cohesion, clear abstractions

- Keep presentation, application workflows, domain rules, binary serialization,
  and platform I/O separated by explicit responsibilities.
- UI code should call domain/application operations rather than manipulate
  binary save fields directly. Share validation and save rules between the
  on-Switch interface and WebUI.
- Keep domain logic independent of SDL rendering, HTTP transport, and libnx
  wherever practical. Pass dependencies explicitly and make ownership and
  lifetimes clear; avoid adding shared mutable globals.
- Introduce abstractions around meaningful responsibilities. Avoid speculative
  frameworks, unnecessary inheritance, and wrappers that add no useful boundary.
- Reuse existing dependencies and platform constraints. Explain why a new
  dependency is needed before introducing it; avoid unrelated toolchain changes.

## 3. Reduce main.cpp incrementally

- Move toward `source/main.cpp` handling startup, lifecycle, composition, and
  delegation to application and screen components.
- Extract cohesive responsibilities, not arbitrary blocks chosen to meet a line
  limit. Splitting files while retaining the same global coupling is insufficient.
- Put new behavior in its responsible module instead of growing `main.cpp`.
  When touching existing behavior, extract a bounded responsibility when practical.
- Keep behavior-preserving extractions separate from redesigns or behavioral
  changes in reviewable steps. Avoid broad rewrites and unrelated formatting.
- Preserve existing behavior during extraction and verify the affected paths.
  Existing debt does not require cleaning up the entire system before shipping
  an independently reviewable improvement.

## 4. UX and visual quality

- Treat Joy-Con/controller and touch interaction as first-class input methods.
  Every essential action must be reachable with either method.
- Use the global controller model: ZL/ZR switch top-level tabs; D-pad/left stick
  move focus; A activates or enters the focused element; B cancels the current
  interaction or returns focus to its parent. Keep a clear visual focus indicator.
- Reserve Minus for the focused element's context menu and Plus for a Save/Exit
  menu with explicit choices. X/Y may provide optional, clearly labeled contextual
  interactions; their specific roles are not assigned yet. Keep every essential
  action available through focus and A, with touch equivalents for these shortcuts.
- Expose actions as focusable controls instead of assigning a separate controller
  button to each action. Do not introduce new subtabs; migrate existing ones
  toward focusable sections or hierarchical screens as their flows are redesigned.
- Treat the unified navigation system as a planned major feature. New work must
  move toward this model; existing screens may migrate incrementally. Centralize
  shared navigation behavior instead of adding screen-specific button schemes.
- Provide predictable focus, visible selection, consistent confirm/back behavior,
  and clear feedback. Preserve context when returning from a detail screen.
- Maintain consistent capabilities and validation across Switch UI and WebUI;
  adapt the interaction to each input method.
- Use a shared visual system for typography, spacing, color, controls, and states.
  Follow [the UI guide](docs/UI_GUIDELINES.md) for interface work.
- Keep long operations responsive and communicate progress, completion, and
  failure accurately. Never offer cancellation that cannot be performed safely.
- Keep user-facing text localizable, allow for longer translations, and explain
  errors with an actionable next step. Avoid exposing implementation details
  unless they help the user recover.

## 5. Verification and completion

- Scale verification to risk. Save parsing, serialization, mutations, backups,
  and restore changes require relevant regression checks, including failure
  paths. Use sanitized fixtures to check preservation of unaffected data.
- For save round trips, verify semantic preservation; require byte identity only
  where the format or operation promises it.
- Build affected C++ changes with the established devkitPro/libnx Makefile
  (`make`). Do not install or upgrade the toolchain as an unrelated change.
- `tools/check_save.py` can contribute schema validation when its required
  schemas are available; it does not by itself establish game compatibility.
- For UI changes, check navigation, touch behavior, translations, and relevant
  empty/loading/error states. Follow the UI guide's verification criteria.
- State exactly what was verified on the host, emulator, or physical Switch.
  A successful build is not hardware validation or proof of save compatibility.
  Report unavailable checks and remaining uncertainty explicitly.
- Keep changes focused and explain behavior changes, verification, and material
  limitations. Preserve existing user work in the working tree.

## 6. Fork maintenance and GitHub

- Preserve upstream attribution, copyright notices, and license obligations.
  Document significant architectural or behavioral departures and their purpose.
- Keep changes scoped so upstream fixes remain practical to understand and port.
- Use only local terminal `git` and `gh` for GitHub operations. Never use the
  GitHub connector, plugin, or MCP server for reads or writes, including any
  `mcp__codex_apps__github_*` tools.
- Before authenticated remote work, verify the effective authenticated identity
  is `EddieBeru` using local authentication tooling (for example,
  `gh api user --jq .login`). Do not infer identity from Git author configuration;
  ensure the check applies to the host and credentials used for the operation.
- If local authentication or networking fails, report the issue and prepare
  commands for the user. Do not fall back to the connector, or automatically
  change, reconnect, or enable GitHub connections.

## 7. Code comments

- Write concise comments for experienced developers, only where needed to explain
  non-obvious intent, behavior, constraints, or API contracts.
- Prefer clear names and structure. Do not narrate obvious code, teach basic
  programming, or add redundant function summaries or development history.
- Preserve essential save-format, safety, platform, and licensing information.
