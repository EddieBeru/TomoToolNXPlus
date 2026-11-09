# Borealis snapshot

Source: https://github.com/xfangfang/borealis
Commit: `5f08b286f3df737f3321d2247a6fe633fcead03c` (2026-11-08 integration).

`library/`, `LICENSE`, `NOTICE`, `AUTHORS` and `README.md` start from a
`git archive` of that commit. The local change listed below is kept explicit. The library includes its bundled dependencies
and their license notices. SDL and GLFW git submodules are intentionally absent;
the port uses SDL2 from the existing devkitPro installation. No network fetch
or toolchain installation occurs during a build.

The port adds Borealis because its standard Switch components, system themes,
focus navigation and touch handling are the subjects of this evaluation.
Only `make borealis` uses it. See `docs/BOREALIS_PORT.md`.

The Material Icons font and its Apache 2.0 license in the port resources are
also copied unchanged from this commit. Nintendo fonts, demo artwork and other
demo resources are not distributed; Switch fonts come from the system service.

## Local patch

`library/lib/platforms/sdl/sdl_video.cpp`: the Switch-specific OpenGL 4.3 branch
now applies only when GLES2/GLES3 is not selected. This lets the port actually
request an ES 2.0 context with the existing GLES2 branch, matching its NanoVG
renderer. Upstream otherwise ignores USE_GLES2 for Switch context creation.
No other library sources are changed.
