# Implementation Plan: Consulta de datos reales del jugador e isla en solo lectura (M1)

**Branch**: `main` (`.specify/feature.json` → `002-browse-player-readonly`) | **Date**: 2026-11-19 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/002-browse-player-readonly/spec.md`

## Summary

Reemplazar los datos ficticios de la pestaña **Jugador** (`Player`, índice `3` en [apps/borealis/source/app.cpp](../../apps/borealis/source/app.cpp)) por la consulta real y estrictamente en modo **solo lectura** de los 15 atributos de `/Player.sav` y sus metadatos secundarios para la cuenta activa en Switch. Un módulo de dominio puro y acotado (`include/player_profile.h` + `source/player_profile.cpp`, reutilizando [include/wishes_data.h](../../include/wishes_data.h) sin enlazar `SaveEditor::Save`, `SaveMount`, `SaveWorkflow` ni `Backup`) valida la cabecera, la tabla índice y el heap de `Player.sav`, soporta `DT_WStr64` y `DT_WStr32` con pares sustitutos UTF-16LE completos en pronunciaciones/nombres, resuelve los catálogos de moneda (`11`), región (`9`), idioma (`11`), tono (`0..5`), tamaño (`1..4`) y deseos liberados (`268`), y calcula deterministamente los días de donación respecto a un reloj de referencia explícito.

En Borealis, `PlayerBrowserScreen` reutiliza el adaptador read-only existente (`TextureBrowser::ReadSource` en [apps/borealis/source/texture_read_source.cpp](../../apps/borealis/source/texture_read_source.cpp), cuya lista blanca ya incluye `"/Player.sav"` vía `fsOpen_SaveDataReadOnly`) y el worker serializado `TextureBrowser::Session` ([apps/borealis/source/texture_browser_session.hpp](../../apps/borealis/source/texture_browser_session.hpp)). Siguiendo la aclaración de [spec.md](spec.md), los 15 atributos se renderizan integrados en la misma lista desplazable (`brls::DetailCell` con título y valor principal seguido de un `brls::Label` multilínea indentado con metadatos secundarios y descripción localizada, igual que [apps/borealis/source/texture_settings_screen.cpp](../../apps/borealis/source/texture_settings_screen.cpp)), sin abrir subpantallas ni diálogos por atributo, preservando la pestaña Texturas, Ajustes de texturas y el editor SDL legacy intacto.

## Technical Context

**Language/Version**: C++17. El target SDL legacy compila con `-fno-rtti -fno-exceptions`; el target Borealis (`apps/borealis/CMakeLists.txt`) mantiene RTTI/excepciones para Borealis, mientras el dominio puro (`include/player_profile.h` / `source/player_profile.cpp`) compila bajo ambos conjuntos de flags (`-fno-exceptions -fno-rtti` en tests puros y con RTTI/excepciones en tests de UI Borealis).

**Primary Dependencies**: Borealis fijado en `libs/borealis`, devkitPro/devkitA64, `libnx`, `include/wishes_data.h` (`WishesData::WISH_COUNT = 268`) y componentes ya existentes de `M0` (`TextureBrowser::ReadSource`, `TextureBrowser::Session`, `navigation`, `profile_header`, `exit_menu`). Sin dependencias externas nuevas.

**Storage**: Solo lectura del guardado de cuenta del título `0x010051F0207B2000` archivo `"/Player.sav"` mediante `fsOpen_SaveDataReadOnly` (`TextureBrowser::ReadSource`). Todos los atributos y contextos residen únicamente en memoria; cero escritura en guardado, configuración, backups o caché.

**Testing**: Ejecutables host C++17 bajo [tests/run_texture_browser.sh](../../tests/run_texture_browser.sh) (10 suites existentes de M0 + nuevas suites `player-profile`, `player-context` y cobertura ampliada de `ui-screens` en `tests/borealis_ui_screens_test.cpp`), `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh`, ambos builds cruzados (`make -j4` y `make -j4 borealis`), auditoría de símbolos `aarch64-none-elf-nm` sobre `build/borealis/TomoToolNXPlusBorealis.elf` y matriz manual en Switch física (`en-US`, `es`, `es-419`, claro/oscuro, 720p/docked). Guía en [quickstart.md](quickstart.md).

**Target Platform**: Nintendo Switch homebrew (`TomoToolNXPlusBorealis.nro`), HOS 2.0.0+ para `fsOpen_SaveDataReadOnly`.

**Project Type**: Aplicación nativa Switch con dos binarios independientes (`TomoToolNXPlus.nro` legacy en `Makefile` y `TomoToolNXPlusBorealis.nro` en `apps/borealis/CMakeLists.txt` invocado vía `make borealis`).

**Performance Goals**: Actividad de carga o resumen de 15 atributos visible en `< 1 s` tras entrar a `Jugador` o confirmar cuenta, y navegación/cambio de pestaña o apertura de menú global en `< 250 ms` durante lecturas (`SC-005`), con I/O y parseo fuera del thread UI.

**Constraints**: Cero mutaciones ni aperturas escribibles (`FR-010`, `SC-006`). Tope de `MaxPlayerBytes = 8 MiB`, lectura por bloques de `ReadChunk = 64 KiB` con verificación de cancelación entre bloques y recierre explícito de `ReadSource::close()` tras cada lectura. Validación estructural sin overflow de offsets/tamaños, rechazo de corrupción sin crashes (`FR-011`) y tokens de generación para descartar entregas obsoletas tras cambiar de cuenta o salir de la pestaña (`FR-007`, `FR-009`).

**Scale/Scope**: 1 archivo `/Player.sav` por cuenta (hasta 8 cuentas), 15 atributos principales + metadatos secundarios inline en 3 idiomas (`en-US`, `es`, `es-419`). Sin edición de atributos de jugador (`M6`), sin subpantallas por campo, sin cambios en WebUI ni en `source/main.cpp`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principio | Gate previo (Phase 0) | Revisión posterior al diseño (Phase 1) |
| --- | --- | --- |
| I. Integridad | PASS: alcance estrictamente read-only; no se autorizan escrituras, backups ni mutaciones de guardado | PASS: reutiliza `SwitchSource` (`fsOpen_SaveDataReadOnly`) sin enlazar `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` ni `SaveEditor::Save`; parseo inmutable y acotado a 8 MiB con rechazo tipado de corrupción sin reparar archivos |
| II. Paridad funcional | PASS: slice M1 reemplaza datos de muestra de `Jugador` con datos reales en read-only | PASS: inventario abajo cubre los 15 atributos + metadatos secundarios (`DT_WStr64` corregido) y deja explícitamente diferida la edición de Player hacia `M6` tras la compuerta `M4` |
| III. Separación | PASS: UI Borealis delegará lectura, parseo y formateo al dominio puro | PASS: `PlayerProfile` (`include/player_profile.h`, `source/player_profile.cpp`) no depende de Borealis/SDL/HTTP/libnx; reloj explícito inyectable para pruebas deterministas; cero nuevos globals mutables |
| IV. Mejora incremental | PASS: no crecer `source/main.cpp` | PASS: extracción limpia de hashes/tablas/reglas de `PLAYER_FIELDS` en módulo propio reutilizable por futuras migraciones (`M6`) sin tocar ni desestabilizar `source/main.cpp` |
| V. Interacción | PASS: navegación global unificada (`D-pad`, `A`, `B`, `ZL/ZR`, `Plus`, táctil) sin subtabs | PASS: lista inline desplazable (`DetailCell` + `Label` indentado) operada solo con mando y solo con touch; sin subpantallas ni atajos exclusivos por campo; conservación de foco/scroll y suspensión segura |
| VI. Evidencia | PASS: criterios verificables definidos en `spec.md` | PASS: tests host con `data/seed_Player.bin` + fixtures sintéticos A/B + inyección de errores, ASan/UBSan, ambos builds `Makefile`, auditoría `nm -C` y checklist físico separado en [quickstart.md](quickstart.md) |
| VII. Fork | PASS: preservar trabajo previo, licencias y flujos locales `git`/`gh` | PASS: atribución a formato `alexislours/ltd-save-editor` y `TomoToolNX` preservada en cabeceras nuevas; sin herramientas remotas prohibidas |

No hay excepciones constitucionales. Al no existir rutas de escritura en M1, backup y commit de guardados permanecen deliberadamente fuera del binario Borealis hasta `M4`.

### Inventario de capacidades de esta migración (Slice M1)

| Capacidad legacy (`PLAYER_FIELDS` en `source/main.cpp`) | Resultado en Slice M1 (Borealis) | Aceptación |
| --- | --- | --- |
| Leer 15 campos de `Player.sav` por cuenta activa | **Migrado (Solo Lectura)** con `Name`, `Island`, `Money`, `Currency`, `Boot Count`, `Skin Tone`, `Bday Day/Month/Year`, `Island Size`, `Fountain Lv`, `Wishes`, `Region`, `TT Penalty`, `Donation` | `FR-001`, `FR-002`, `SC-001` |
| Pronunciación e idioma de Nombre e Isla | **Migrado y corregido (Solo Lectura)**: soporta `DT_WStr64` y `DT_WStr32` con pares sustitutos UTF-16LE completos | `FR-002`, `FR-003`, `SC-001` |
| Deseos liberados (`Liberation.WishInfo.*` vs `268`) | **Migrado (Solo Lectura)** con degradación local si falta la tabla | `FR-002`, `FR-003`, `SC-004` |
| Estados de carga, sin perfil, sin guardado, corrupción y recarga | **Migrado** con máquina de estados, tokens de generación y `Actualizar / Reintentar` | `FR-007`–`FR-011`, `SC-003`, `SC-004`, `SC-005` |
| Selector global `Y`, cambio de pestañas `ZL/ZR`, `B`, `Plus` y conservación de contexto | **Integrado** propagando `ProfileSelection` a `PlayerBrowserScreen` y preservando `focusTarget`/`scroll` | `FR-004`–`FR-007`, `SC-002`, `SC-003` |
| Localización en 3 idiomas (`en-US`, `es`, `es-419`) y fuente estándar | **Migrado** en `apps/borealis/resources/i18n/` aprovechando `configurePrimaryFont` | `FR-012`, `SC-007` |
| Editar campos (`swkbd`, moneda/región/idioma/tono/tamaño, borrar VT, ajustar donación, wishes en lote) | **Diferido a `M6`** (requiere compuerta de backup e integridad `M4`); no se exponen controles de escritura | `Assumptions`, `FR-010` |
| Mii, Map, WebUI lifecycle, Texturas escritura | Sin cambios en M1; Texturas y Ajustes de texturas preservados intactos | `FR-013` |

## Project Structure

### Documentation (this feature)

```text
specs/002-browse-player-readonly/
├── spec.md                     # Especificación aprobada y aclarada (M1)
├── plan.md                     # Este archivo ($speckit-plan)
├── research.md                 # Phase 0: verificación binaria, hashes, DT_WStr64 y arquitectura UI
├── data-model.md               # Phase 1: PlayerProfileSummary, PlayerAttributeView, estados y errores
├── quickstart.md               # Phase 1: fixtures sintéticos, comandos host/Switch y checklist físico
├── contracts/
│   └── player-browser.md       # Phase 1: contratos de dominio, sesión, UI y trazabilidad FR/SC
└── checklists/
    └── requirements.md         # Checklist de calidad de spec.md (16/16 aprobados)
```

(`tasks.md` se generará posteriormente con `/speckit-tasks`.)

### Source Code (repository root)

Rutas existentes modificadas y archivos nuevos propuestos para la implementación:

```text
include/
  wishes_data.h                          # Existente (268 deseos; reutilizado en M1 sin cambios)
  texture_browser.h                      # Existente (ReadSource, AccountKey, MaxPlayerBytes)
  player_profile.h                       # Nuevo: estructuras puras, parser read-only, formateador y PlayerBrowserModel
source/
  player_profile.cpp                     # Nuevo: Murmur3, parser acotado de Player.sav, catálogos, UTF-16LE y modelo de sesión
apps/borealis/
  CMakeLists.txt                         # Actualizar: agregar player_profile.cpp y player_browser_screen.cpp al target
  source/
    app.cpp                              # Actualizar: reemplazar sample tab 3 por PlayerBrowserScreen y propagar setProfile/activate
    player_browser_screen.hpp            # Nuevo: pantalla de consulta inline de Jugador
    player_browser_screen.cpp            # Nuevo: DetailCell + Label secundario inline, recarga asíncrona y restauración de foco/scroll
  resources/i18n/
    en-US/app.json                       # Actualizar: etiquetas, descripciones read-only y mensajes de estado de Jugador
    es/app.json                          # Actualizar: traducciones es
    es-419/app.json                      # Actualizar: traducciones es-419
tests/
  player_profile_test.cpp                # Nuevo: tests unitarios puros de parser, hashes, los 15 campos, corrupción e inmutabilidad
  player_browser_context_test.cpp        # Nuevo: tests de máquina de estados, ReadSource falso/SwitchSource shim, cancelación y aislamiento
  borealis_ui_screens_test.cpp           # Actualizar: cobertura UI mando/touch de PlayerBrowserScreen en tab 3 sobre shim Borealis
  run_texture_browser.sh                 # Actualizar: registrar suites player-profile y player-context y fuentes en ui-screens
  README.md                              # Actualizar: documentar nuevas suites host de M1
docs/
  ROADMAP.md                             # Actualizar: avance de M1 y registro posterior de implementación/evidencia
```

**Structure Decision**: Mantener el parser, los catálogos, las reglas de formato y el controlador de estado puros en `include/player_profile.h` y `source/player_profile.cpp`, reutilizando directamente `include/wishes_data.h` y `TextureBrowser::ReadSource`/`TextureBrowser::Session` de `M0`. La presentación inline vive en `apps/borealis/source/player_browser_screen.{hpp,cpp}` y se conecta únicamente en `apps/borealis/source/app.cpp`. No se toca `source/main.cpp` ni se enlaza `source/save_editor.cpp`, `SaveMount`, `SaveWorkflow` o `Backup` dentro de `apps/borealis/CMakeLists.txt`.

## Complexity Tracking

Sin violaciones constitucionales ni excepciones que justificar.

## Phase 0 — Research Outcome

[research.md](research.md) resuelve la reutilización de `TextureBrowser::ReadSource`/`Session` sobre `/Player.sav`, el algoritmo exacto de validación binaria del formato `.sav`, todos los hashes Murmur3 x86_32 y tipos de los 15 atributos (incluyendo la corrección empírica verificada contra [data/seed_Player.bin](../../data/seed_Player.bin) de que `Player.HowToCallName` y `Player.HowToCallIslandName` usan `DT_WStr64` y `BdayYear` usa `DT_UInt`), el reloj explícito para pruebas de `Donation` y el diseño inline en Borealis.

## Phase 1 — Design and Handoff

- [data-model.md](data-model.md): entidades de dominio (`PlayerAttributeId`, `EnumResolution`, `WishesProgress`, `PlayerProfileSummary`, `PlayerAttributeView`), errores tipados (`PlayerReadError`) y transiciones de `PlayerBrowserModel`.
- [contracts/player-browser.md](contracts/player-browser.md): contratos de parser/loader/UI, garantías negativas de solo lectura y matriz de trazabilidad `FR-001`–`FR-013` y `SC-001`–`SC-007`.
- [quickstart.md](quickstart.md): fixtures sintéticos A/B, comandos de prueba host/sanitizers, compilación Switch con `aarch64-none-elf-nm` y checklist de Switch físico.

Orden sugerido para generar `tasks.md` con `/speckit-tasks`:
1. Módulo puro `player_profile.{h,cpp}` y pruebas unitarias exhaustivas (`tests/player_profile_test.cpp`) incluyendo `data/seed_Player.bin`, fixtures sintéticos y corrupción.
2. Controlador de sesión `PlayerBrowserModel` + loader `LoadFromReadSource` sobre `ReadSource` + pruebas de aislamiento/cancelación (`tests/player_browser_context_test.cpp`).
3. Traducciones `en-US` / `es` / `es-419` y pantalla inline `PlayerBrowserScreen` integrada en `apps/borealis/source/app.cpp` y `apps/borealis/CMakeLists.txt`.
4. Ampliación de `tests/borealis_ui_screens_test.cpp` (mando, toque, foco, scroll, cambio `Y` y estados de error), verificación completa con ASan/UBSan, builds `make` + `make borealis`, auditoría `aarch64-none-elf-nm` y validación física en Switch.
