# Borealis snapshot

Source: https://github.com/xfangfang/borealis
Commit: `5f08b286f3df737f3321d2247a6fe633fcead03c` (2026-11-08 integration).

`library/`, `LICENSE`, `NOTICE`, `AUTHORS` and `README.md` are an unmodified
`git archive` of that commit. The library includes its bundled dependencies
and their license notices. SDL and GLFW git submodules are intentionally absent;
the pilot uses SDL2 from the existing devkitPro installation. No network fetch
or toolchain installation occurs during a build.

The pilot adds Borealis because its standard Switch components, system themes,
focus navigation and touch handling are the subjects of this evaluation.
Only `make borealis-prototype` uses it. See `docs/BOREALIS_PROTOTYPE.md`.

The Material Icons font and its Apache 2.0 license in the pilot resources are
also copied unchanged from this commit. Nintendo fonts, demo artwork and other
demo resources are not distributed; Switch fonts come from the system service.
