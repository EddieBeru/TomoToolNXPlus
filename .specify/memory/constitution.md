# TomoToolNXPlus Constitution

## Core Principles

### I. Save Integrity Before All Other Goals

Before modifying existing game-save data, every workflow MUST obtain a successful,
recoverable backup for the affected user and data. Background-job completion alone MUST
NOT establish success; its error result MUST be checked. Backup failure MUST abort the
write. Inputs, bounds, field types and relevant cross-file relationships MUST be validated
before mutation. Unknown format behavior MUST be resolved before dependent writes are enabled.

Focused edits MUST preserve unrelated fields, unknown fields and format metadata. Read,
write, close and commit failures MUST propagate; success MUST follow all required writes
and commits. Multi-file operations MUST define recovery for partial failure and respect
platform journal constraints. Rename, rollback and transaction atomicity MUST NOT be
assumed without evidence. Backups and restore targets MUST remain isolated by user;
no incomplete or unverified result may replace the only recoverable copy. Personal saves
and identifying data MUST stay out of source control, logs and fixtures. These safeguards
have priority over aesthetics, performance and convenience because lost data may be irreplaceable.

### II. Functional Borealis Adaptation With Explicit Parity

Borealis is the approved Switch UI direction. Migration MUST deliver complete workflows
using standard components and system themes, with dedicated views where standard controls
cannot express the interaction. Visual approval, sample data and successful builds MUST
NOT count as functional completion.

Each slice MUST identify its existing capabilities, real data source, inputs, outcomes,
failure behavior and remaining gaps. A maintained capability inventory MUST account for
legacy Switch flows, including Textures, Mii, Player, Map and WebUI access/lifecycle
controls. Before claiming full replacement, every capability MUST be migrated and verified
or explicitly dispositioned with maintainer approval and documented rationale. Unsafe
legacy behavior MUST be corrected or withheld with an explanation rather than copied
for parity. Functional completeness includes validation, persistence and recovery.

### III. Cohesive Modules and Shared Application Rules

Presentation, application workflows, domain rules, serialization and platform I/O MUST
have explicit responsibilities. UI code MUST call application/domain operations rather
than manipulate binary save fields or own backup and commit policy. Switch UI and WebUI
MUST share validation and save rules, maintaining consistent capabilities and outcomes
as flows migrate while adapting presentation to each input method.

Domain logic MUST remain independent of Borealis, SDL, HTTP and libnx wherever practical;
unavoidable platform dependencies MUST have explicit boundaries. Dependencies, ownership
and lifetimes MUST be clear. New shared mutable globals, speculative frameworks and
wrappers without meaningful responsibility MUST NOT be added. Existing dependencies MUST
be reused where suitable; new dependencies require a documented need before adoption.

### IV. Incremental Improvement of Existing Code

New behavior MUST live in its responsible module instead of enlarging `source/main.cpp`.
That file's target responsibility is startup, lifecycle, composition and delegation.
When touching coupled behavior, contributors MUST assess a bounded cohesive extraction
and perform it when practical, or explain why it would expand the slice unsafely. Moving
arbitrary blocks while retaining global coupling MUST NOT count as an extraction.

Behavior-preserving extractions MUST be reviewable separately from redesigns and behavior
changes, with verification of affected paths. Broad rewrites, unrelated formatting and
unrelated toolchain changes MUST NOT accompany migration slices. Existing debt does not
require complete cleanup before a safe independent improvement ships. Comments MUST
explain non-obvious intent, constraints or contracts concisely; essential format, safety,
platform and licensing information MUST remain intact.

### V. One Predictable Controller and Touch Model

Every essential action MUST be reachable through controller focus and through touch.
ZL/ZR MUST switch top-level tabs only; D-pad/left stick MUST move focus; A MUST activate
or enter; B MUST cancel the innermost interaction or return to its parent. Minus MUST
be reserved for the focused element's context menu and Plus for a Save/Exit menu with
explicit choices. X/Y MAY provide labeled contextual shortcuts with focusable and touch
equivalents. Shortcuts MUST NOT bypass dialogs, validation or active writes.

Shared navigation code MUST own common input mapping, focus scopes and parent transitions.
Screens MUST expose focusable actions instead of inventing button schemes. New subtabs
MUST NOT be introduced; redesigned flows MUST use focusable sections or child screens.
Focus MUST be visible, distinct from selection and predictably restored. Returning to a
screen MUST preserve meaningful selection, filters and scroll context. Switching input
methods MUST retain coherent state. Modals MUST isolate input from underlying controls.

Interface work MUST follow `docs/UI_GUIDELINES.md`, reuse shared visual values and keep
text localizable with room for longer translations. Loading, empty, disabled, success
and error states MUST give accurate feedback and actionable next steps. Long operations
MUST remain responsive and MUST NOT display invented progress. Cancellation and undo MUST
be offered only when safe and supported. Drafts, active writes and committed results
MUST be distinguishable; navigation MUST NOT silently save or discard edits.

### VI. Specifications and Evidence Define Completion

Behavior changes MUST specify inputs, state transitions, validation, failure/recovery
behavior and acceptance checks before implementation is considered complete. Tasks MUST
trace to those requirements. Save parsing, serialization, mutation, backup and restore
changes MUST have relevant regression checks, including failure paths and preservation
of unaffected data, using synthetic or sanitized fixtures. Round trips MUST preserve
semantics; byte identity is required only where the operation or format promises it.

Affected C++ targets MUST build through the established devkitPro/libnx Makefile: `make`
for the legacy target and `make borealis` for Borealis. Shared changes affecting both
MUST build both targets. Relevant host checks MUST follow `tests/README.md`.
`tools/check_save.py` MAY contribute schema checks when schemas exist, but MUST NOT
establish game compatibility by itself.

UI verification MUST cover controller and touch routes, dialogs, focus restoration,
tab context, held input, translations, relevant empty/loading/error states and native
1280 x 720 layout. Theme and docked scaling checks MUST accompany affected changes.
Reports MUST distinguish host, emulator and physical Switch evidence and identify
unavailable checks. Builds and adapted emulator runs MUST NOT be presented as hardware
validation or proof of save compatibility.

### VII. Responsible Fork Maintenance

Upstream attribution, copyright notices and license obligations MUST be preserved.
Significant architectural or behavioral departures MUST document their purpose. Changes
MUST remain scoped so upstream fixes remain understandable and practical to port.
Existing user work MUST be preserved.

GitHub reads and writes MUST use only local terminal `git` and `gh`, never GitHub
connectors, plugins or MCP tools. Before authenticated remote work, the effective identity
for the actual host and credentials MUST be verified as `EddieBeru`, for example using
`gh api user --jq .login`; Git author configuration is insufficient. Authentication or
network failures MUST be reported with commands prepared for the user. Connections MUST
NOT be changed, reconnected or enabled automatically; connectors MUST NOT be fallback.

## Migration Constraints and Completion

The baseline in `README.md`, `docs/BOREALIS_PORT.md` and `docs/BOREALIS_MIGRATION.md`
is two independent NRO targets: the broader SDL editor and the incremental Borealis app.
Texture Settings uses real preferences; browser pages still use sample data. This MUST
NOT be described as a complete editor migration. A slice MUST NOT add game-save access
until its shared workflow, backup, validation, commit handling and recovery are specified
and verified.

Migration MUST proceed through independently reviewable functional slices: real read-only
data before dependent editing, then shared operations and remaining screens. The legacy
target MUST remain available for comparison until replacement criteria are met. WebUI
transport and presentation are outside the Switch UI rewrite; shared backend improvements
MUST preserve its behavior and apply the same safety rules.

Borealis MUST own its window, graphics context and event loop. Standard controls MUST use
Borealis components. Image previews MUST reuse decoded CPU data and create resources in
the owning renderer; SDL textures MUST NOT cross renderer boundaries. Maps and custom
canvases MUST use dedicated NanoVG views with model coordinates and selection rules
separated from drawing. Graphics resources MUST be created and released on the owning
context thread; workers MAY decode data but MUST NOT manipulate the graphics context.

Existing `/switch/TomoToolNX/` configuration and backup paths MUST retain continuity
unless an explicit migration and recovery plan authorizes changes. Preference writes
MUST preserve unrelated settings and unknown keys, verify a recoverable backup before
overwriting existing data, detect conflicting external edits, check write completion
and provide recovery guidance after partial failure. SD atomicity MUST NOT be assumed.
Use pinned Borealis sources and the established toolchain; dependency changes MUST be
justified, scoped and preserve notices.

Full replacement requires a reviewed capability inventory with no unexplained gaps,
real data throughout supported flows, relevant regression checks and target builds,
and physical Switch evidence for migrated controller, touch, SD persistence and game-save
workflows. Remaining unsupported operations and hardware uncertainty MUST be disclosed,
not hidden behind sample content or a general completion claim.

## Development Workflow and Quality Gates

1. Define a bounded specification with user/data scope, acceptance checks, navigation,
   persistence boundaries and failure/recovery behavior.
2. Plan application/domain boundaries, renderer ownership, affected targets and bounded
   extractions. Resolve format uncertainty before enabling dependent writes.
3. Create dependency-ordered tasks tied to acceptance checks. Separate structural changes
   from behavior changes and identify required host and hardware checks.
4. Implement the smallest complete workflow, preserving unrelated data and user changes.
   Sample screens, unreachable actions and unhandled failures MUST NOT count as complete.
5. Run risk-appropriate regressions and affected Makefile targets. Verify UI flows against
   the UI guide and explicitly record unavailable hardware checks.
6. Review constitution compliance and report behavior changes, evidence, recovery limits
   and remaining migration gaps before claiming completion.

Documentation-only changes require consistency and reference/placeholder checks instead
of unrelated application builds. Requirements, implementation and evidence MUST agree;
a checked task alone does not establish acceptance.

## Governance

This constitution is the governance baseline. `AGENTS.md` supplies operational development
rules and `docs/UI_GUIDELINES.md` supplies detailed interface criteria; specifications,
plans, tasks and reviews MUST comply with all three. Save integrity has priority when
goals compete. Existing code and undocumented practices MUST NOT override requirements;
a feature plan MUST NOT silently waive them.

Amendments MUST identify affected principles, rationale, compatibility impact and any
migration work, and require explicit maintainer approval before adoption. Amendments MUST
update the version and last-amended date, preserve the original ratification date and
review dependent guidance for consistency. MAJOR versions remove or incompatibly redefine
governance; MINOR versions add principles or materially expand obligations; PATCH versions
clarify wording without changing obligations. Initial adoption is 1.0.0.

Every specification and implementation review MUST check applicable principles and record
compliance gaps. Legacy debt MAY remain outside a bounded change, but new work MUST NOT
extend unsafe patterns or claim unverified compliance. Proposed exceptions MUST be
explicit to the maintainer; changing obligations requires the amendment procedure.

**Version**: 1.0.0 | **Ratified**: 2026-11-09 | **Last Amended**: 2026-11-09
