# TomoToolNXPlus

**A spec-driven rebuild of the TomoToolNX experience, maintained by EddieBeru.**

TomoToolNXPlus is a fork of [TomoToolNX](https://github.com/miiazertyy/TomoToolNX),
a Nintendo Switch editor for Tomodachi Life saves. The goal is to turn a useful
but difficult-to-maintain program into a coherent, predictable tool, with save
integrity taking priority over everything else.

## Why this fork exists

My assessment of the original program is blunt: useful functionality was buried
under bad UI and UX, inconsistent controls, and tightly coupled code. It felt
like **AI slop**: features accumulated faster than the interaction design and
architecture could support them. That describes the result I want to fix; it
isn't a claim that I know how every upstream contribution was produced.

I'm using AI to help rebuild it, too. The difference I want to establish is a
process that makes the work accountable: **spec-driven development**. Requirements,
interaction rules, failure behavior and acceptance checks come before an
implementation is considered finished.

**AI beats bad-AI development when the specs lead and the results are checked.**
That is the direction of this fork, not a claim that the cleanup is already done.

## What spec-driven means here

- **Define behavior first.** Describe the screen, its inputs, state transitions,
  validation and failure paths before implementing it.
- **Use one interaction model.** ZL/ZR switch top-level tabs; D-pad or left stick
  move focus; A enters or activates; B cancels or returns to the parent. Essential
  actions also need touch equivalents.
- **Separate responsibilities.** Screens present state and call application
  operations. They should not manipulate binary save fields or own backup rules.
- **Protect data.** Require successful recoverable backups before save mutation,
  preserve unrelated fields, and propagate read/write/commit failures.
- **Ship reviewable slices.** Extract cohesive responsibilities from main.cpp,
  reuse shared rules, and keep structural changes separate from redesigns.
- **Verify claims.** Use synthetic regression fixtures and report host, emulator
  and physical Switch results separately. A successful build is not proof of
  save compatibility.

The concrete development contract lives in [AGENTS.md](AGENTS.md), with the
interaction and visual requirements in [UI_GUIDELINES.md](docs/UI_GUIDELINES.md).
AI-generated work must meet those requirements; generating code is only one
part of the job.

## Current state

There are two independent build targets during the migration:

| Build | Purpose |
| --- | --- |
| `TomoToolNXPlus.nro` | Existing SDL editor, including its broader editing features and WebUI |
| `TomoToolNXPlusBorealis.nro` | Incremental Switch UI migration using Borealis's standard components and system themes |

The Borealis target is **not yet a replacement for the full editor**. Its browser
tabs still contain sample data. Its first functional screen is Texture Settings:
canvas shape, encoder, BC1 mode, image fit and background. It reuses the existing
settings model, keeps edits as drafts, and offers explicit saving and discard
choices.

Those preferences use the existing `/switch/TomoToolNX/config.ini`. The migrated
writer preserves unrelated settings and legacy theme/color keys, verifies a
backup before overwriting an existing config, and checks the final write. The
Borealis target does not mount or edit game saves. WebUI remains outside this
Switch UI migration.

Earlier Plus builds have user-reported success on real Switch hardware. The new
Borealis v0.2.0 preference workflow has host regression and adapted-emulator
checks; its physical SD/configuration behavior still needs hardware validation.
Existing legacy code has not all been brought up to the fork's development rules.

See [the prototype record](docs/BOREALIS_PROTOTYPE.md),
[migration boundaries and recovery](docs/BOREALIS_MIGRATION.md), and
[host regression checks](tests/README.md) for scope and evidence.

## Where it is going

1. Complete functional screens in small, independently reviewable steps.
2. Move the texture browser to real read-only data.
3. Connect texture operations through shared validation, backup and save workflows.
4. Migrate the remaining Switch screens while reducing main.cpp's responsibilities.

Standard controls become Borealis components. Image previews reuse decoded
pixels with resources owned by the new renderer. Maps and custom canvases need
dedicated NanoVG views, with their data and interaction rules extracted from SDL
rendering. The migration adopts Borealis's visual language rather than copying
the old interface pixel for pixel.

## Build and run

Use the established devkitPro/devkitA64 + libnx toolchain and the project's Switch
portlibs (SDL2, SDL2_image, SDL2_ttf, SDL2_mixer, zstd, curl and their dependencies).
The optional Borealis build also requires CMake; its pinned framework source is
included in this repository.

```sh
make setup                 # first checkout: fetch libhaze and apply local overrides
make -j4                   # TomoToolNXPlus.nro
make -j4 borealis-prototype # TomoToolNXPlusBorealis.nro
```

Copy the desired NRO into a directory under `/switch/` on the SD card and launch
it through the homebrew menu on a homebrew-capable Switch. Both targets identify
EddieBeru as the fork author. Existing SD paths retain the TomoToolNX name for
configuration and backup continuity.

This fork's builds and status are separate from upstream releases. The legacy
editor's update checker currently points to upstream; it does not track this
fork's releases.

## Contributing

For a behavior change, explain the problem, intended interaction, acceptance
checks and data risks. Keep changes scoped and include what was actually tested.
Report issues and propose changes in
[this fork](https://github.com/EddieBeru/TomoToolNXPlus).

Legacy Switch translations live in `romfs/lang/`; WebUI translations live in
`data/webui.html`. Borealis translations live in
`prototype/borealis/resources/i18n/`. Keep user-facing text localizable and check
longer translations as part of UI work.

## Credits and license

TomoToolNXPlus is EddieBeru's fork of TomoToolNX. The original NRO author is
Imprimante. Critiquing and redesigning the program does not erase the upstream
work or the contributions that made its editing features possible.

| Project / asset | Attribution |
| --- | --- |
| [LivinTheDreamToolkit](https://gamebanana.com/tools/22435) | by the UGC editor contributors, the tool this follows from |
| [ShareMii](https://github.com/Star-F0rce/ShareMii) | by Star-F0rce, what Mii sharing is based on |
| [alexislours/ltd-save-editor](https://github.com/alexislours/ltd-save-editor) | the save parser, the generated data tables (wishes, habits, clothes, coordinates, treasures), the `Map.sav` schema hashes, the UGC kind table, the housing edit safety rules and the BC1/BC3 encoder. AGPL-3.0-or-later |
| [tlmodding/ltd-gamedata](https://github.com/tlmodding/ltd-gamedata) | early reference for the save key hashes |
| [ITotalJustice/libhaze](https://github.com/ITotalJustice/libhaze) | MTP over USB |
| [Switch Button Icons and Controls](https://zacksly.itch.io/switch-button-icons-and-controls) | Button prompt icons in `romfs/ui/controller/`, by Zacksly, CC BY 3.0. The bundled PNGs are unmodified. |

| [Borealis](https://github.com/xfangfang/borealis) | Switch UI framework and bundled dependencies; pinned source, Apache 2.0 license and notices in `libs/borealis/` |

The project's [GPL-3.0 license](LICENSE) and third-party attribution obligations
remain in place. See the bundled dependency licenses and `romfs/ASSET_CREDITS.txt`.
