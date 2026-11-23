# Implementation Plan: Consulta de residentes Mii en modo solo lectura (M2)

**Branch**: `main` (`.specify/feature.json` → `003-browse-mii-readonly`) | **Date**: 2026-11-22 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/003-browse-mii-readonly/spec.md`

## Summary

Reemplazar los datos ficticios de la pestaña **Mii** (índice `2` en [apps/borealis/source/app.cpp](../../apps/borealis/source/app.cpp)) por la consulta real y estrictamente en modo **solo lectura** de los residentes en `/Mii.sav` (hasta `70` plazas ocupadas) y sus 7 secciones (Estadísticas, Palabras, Relaciones, Pertenencias, Hábitos, Vivienda —con lectura opcional de nombres de casas desde `/Map.sav`— y Resumen social) para la cuenta activa en Switch.

Un módulo de dominio puro y acotado (`include/mii_roster.h` + `source/mii_roster.cpp`, reutilizando [include/belongings_data.h](../../include/belongings_data.h), [include/habits_data.h](../../include/habits_data.h) y [include/rel_tier_labels.h](../../include/rel_tier_labels.h) sin enlazar `SaveEditor::Save`, `SaveMount`, `SaveWorkflow`, `Backup` ni `MiiManager`) valida la cabecera, la tabla índice y el heap de `Mii.sav` (y opcionalmente `Map.sav`), decodifica UTF-16LE con pares sustitutos y controla el ciclo de vida mediante `MiiBrowserModel`. En Borealis, se extiende la lista blanca read-only de [apps/borealis/source/texture_read_source.cpp](../../apps/borealis/source/texture_read_source.cpp) (`fsOpen_SaveDataReadOnly`) para `/Mii.sav` y `/Map.sav`, y se sustituye la barra legada de 7 subpestañas por una navegación jerárquica de 3 niveles sobre `navigation::push`/`pop`: `MiiBrowserScreen` (lista de residentes) → `MiiDetailScreen` (7 secciones enfocables) → `MiiSectionScreen` (detalle desplazable de solo lectura), preservando intactas las pestañas Texturas, Ajustes de texturas, Jugador y el editor SDL legacy.

## Technical Context

**Language/Version**: C++17. El dominio puro (`include/mii_roster.h` / `source/mii_roster.cpp`) compila tanto con `-fno-exceptions -fno-rtti` (tests puros y compatibilidad con el target SDL) como con RTTI/excepciones para el target Borealis (`apps/borealis/CMakeLists.txt`).

**Primary Dependencies**: Borealis fijado en `libs/borealis`, devkitPro/devkitA64, `libnx`, catálogos existentes en `include/belongings_data.h`, `include/habits_data.h` e `include/rel_tier_labels.h`, y componentes de `M0`/`M1` (`TextureBrowser::ReadSource`, `TextureBrowser::Session`, `navigation`, `profile_header`, `exit_menu`). Sin dependencias externas nuevas.

**Storage**: Solo lectura del guardado de cuenta del título `0x010051F0207B2000`, archivos `"/Mii.sav"` (obligatorio, tope `8 MiB`) y `"/Map.sav"` (opcional para nombres de casas, tope `4 MiB`) mediante `fsOpen_SaveDataReadOnly` (`TextureBrowser::ReadSource`). Todos los datos y contextos viven únicamente en memoria; cero escritura en guardado, configuración, backups o caché.

**Testing**: Ejecutables host C++17 bajo [tests/run_texture_browser.sh](../../tests/run_texture_browser.sh) (12 suites existentes de `M0`/`M1` + nuevas suites `mii-roster`, `mii-context` y cobertura ampliada de `ui-screens` en `tests/borealis_ui_screens_test.cpp`), `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh`, ambos builds cruzados (`make -j4` y `make -j4 borealis`), auditoría `aarch64-none-elf-nm -C` sobre `build/borealis/TomoToolNXPlusBorealis.elf` y matriz manual en Switch física cuando la consola esté disponible. Guía en [quickstart.md](quickstart.md).

**Target Platform**: Nintendo Switch homebrew (`TomoToolNXPlusBorealis.nro`), HOS 2.0.0+ para `fsOpen_SaveDataReadOnly`.

**Project Type**: Aplicación nativa Switch con dos binarios independientes (`TomoToolNXPlus.nro` legacy en `Makefile` y `TomoToolNXPlusBorealis.nro` en `apps/borealis/CMakeLists.txt` vía `make borealis`).

**Performance Goals**: Actividad de carga o lista de hasta 70 residentes visible en `< 1 s` tras entrar a `Mii` o confirmar cuenta, y navegación/cambio de pestaña o apertura de menú global en `< 250 ms` durante lecturas (`SC-005`), con I/O y parseo fuera del hilo UI.

**Constraints**: Cero mutaciones ni aperturas escribibles (`FR-016`, `SC-006`). Topes `MaxMiiBytes = 8 MiB` y `MaxMapBytes = 4 MiB`, lectura por bloques de `64 KiB` con cancelación entre bloques, recierre explícito `source.close()`, validación estructural sin overflow y tokens de generación para descartar entregas obsoletas (`FR-013`, `FR-015`, `FR-017`).

**Scale/Scope**: Hasta `70` residentes por cuenta (`840` entradas de palabras, `2415` pares de relación, `4830` direcciones de relación, `840` bolsillos, `74` hábitos en `9` categorías), 3 idiomas (`en-US`, `es`, `es-419`). Sin edición/creación/borrado de Mii, sin `.ltd` ni ShareMii (`M6`), sin cambios en WebUI ni en `source/main.cpp`.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principio | Gate previo (Phase 0) | Revisión posterior al diseño (Phase 1) |
| --- | --- | --- |
| I. Integridad | PASS: alcance estrictamente read-only; no se autorizan escrituras ni mutaciones de guardado | PASS: reutiliza `SwitchSource` (`fsOpen_SaveDataReadOnly`) extendido solo con `"/Mii.sav"` y `"/Map.sav"` en `FsOpenMode_Read`; no enlaza `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` ni `SaveEditor::Save`; parseo inmutable y acotado con rechazo tipado de corrupción |
| II. Paridad funcional | PASS: slice M2 reemplaza datos de muestra de `Mii` con consulta real de las 7 secciones | PASS: inventario abajo cubre residentes (0–70) y las 7 secciones en solo lectura, diferenciando explícitamente la edición/ShareMii diferida a `M6` tras `M4` |
| III. Separación | PASS: UI Borealis delegará lectura, parseo y formateo al dominio puro | PASS: `MiiRoster` (`include/mii_roster.h`, `source/mii_roster.cpp`) no depende de Borealis/SDL/HTTP/libnx; reutiliza catálogos de `include/`; cero nuevos globals mutables |
| IV. Mejora incremental | PASS: no crecer `source/main.cpp` | PASS: extrae reglas de lectura de las 7 secciones en módulo puro reutilizable para `M6` sin modificar `source/main.cpp` |
| V. Interacción | PASS: navegación jerárquica unificada sin subpestañas (`D-pad`, `A`, `B`, `ZL/ZR`, `Plus`, táctil) | PASS: 3 niveles limpios (`MiiBrowserScreen` → `MiiDetailScreen` → `MiiSectionScreen`) vía `navigation::push`/`pop`, foco inicial estable (`Volver`), restauración de residente/sección/scroll al volver con `B` |
| VI. Evidencia | PASS: criterios verificables definidos en `spec.md` | PASS: pruebas host con `data/seed_Mii.bin` + `data/seed_Map.bin` + fixtures sintéticos (0, 1, dispersos, 70, A/B, corrupción), ASan/UBSan, ambos builds `Makefile`, auditoría `nm -C` y checklist físico separado en [quickstart.md](quickstart.md) |
| VII. Fork | PASS: preservar trabajo previo, licencias y flujos locales `git`/`gh` | PASS: atribución a `alexislours/ltd-save-editor` y `TomoToolNX` preservada; sin herramientas remotas prohibidas |

No hay excepciones constitucionales. Al no existir rutas de escritura en M2, backup y commit de guardados permanecen deliberadamente fuera del binario Borealis hasta `M4`.

### Inventario de capacidades de esta migración (Slice M2)

| Capacidad legacy (pestaña Mii en `source/main.cpp`) | Resultado en Slice M2 (Borealis) | Aceptación |
| --- | --- | --- |
| Listar hasta 70 residentes ocupados de `Mii.sav` | **Migrado (Solo Lectura)** con detección acotada de plazas ocupadas, soporte de isla vacía (`0`) y 70 plazas | `FR-001`, `FR-014`, `SC-001` |
| Estadísticas principales (16 campos de solo lectura) | **Migrado (Solo Lectura)** en sección `Estadísticas` (excluyendo `Delete Mii`) | `FR-003`, `SC-001` |
| Palabras / frases (12 plazas con tipo, texto y pronunciación) | **Migrado (Solo Lectura)** en sección `Palabras` con UTF-16LE completo | `FR-004`, `SC-001` |
| Relaciones (pares, tipo, medidor, rango `rel_tier_labels.h`, pelea, fecha) | **Migrado (Solo Lectura)** en sección `Relaciones` | `FR-005`, `SC-001` |
| Pertenencias (8 prendas + Coordinate + 12 Pocket) | **Migrado (Solo Lectura)** en sección `Pertenencias` reutilizando `belongings_data.h` | `FR-006`, `SC-001` |
| Hábitos (74 hábitos en 9 categorías) | **Migrado (Solo Lectura)** en sección `Hábitos` reutilizando `habits_data.h` | `FR-007`, `SC-001` |
| Vivienda (casa, habitación, compañeros, conflictos + nombres desde `Map.sav`) | **Migrado (Solo Lectura)** en sección `Vivienda` con degradación local si falta `Map.sav` | `FR-008`, `FR-010`, `SC-004` |
| Resumen social (conteos por grupo y vínculos agrupados) | **Migrado (Solo Lectura)** en sección `Resumen social` | `FR-009`, `SC-001` |
| Navegación sin subpestañas, cambio de perfil `Y`, `B`, `Plus`, 3 idiomas | **Integrado** en `MiiBrowserScreen` / `MiiDetailScreen` / `MiiSectionScreen` | `FR-002`, `FR-011`–`FR-015`, `FR-018` |
| Editar campos/frases/relaciones/ropa/hábitos/vivienda, crear/borrar Mii, `.ltd`, ShareMii | **Diferido a `M6`** (requiere compuerta `M4`); no se exponen controles de escritura | `Assumptions`, `FR-016` |
| Texturas, Ajustes de texturas, Jugador, editor SDL legacy y WebUI | Preservados intactos sin regresiones | `FR-019` |

## Project Structure

### Documentation (this feature)

```text
specs/003-browse-mii-readonly/
├── spec.md                     # Especificación aprobada (M2)
├── plan.md                     # Este archivo (/speckit-plan)
├── research.md                 # Phase 0: verificación binaria seed_Mii/seed_Map, ReadSource y jerarquía UI
├── data-model.md               # Phase 1: MiiRosterSummary, MiiResident, 7 secciones, errores y MiiBrowserModel
├── quickstart.md               # Phase 1: fixtures sintéticos, comandos host/Switch y checklist físico
├── contracts/
│   └── mii-browser.md          # Phase 1: contratos de dominio, ReadSource, UI Borealis y trazabilidad FR/SC
└── checklists/
    └── requirements.md         # Checklist de calidad de spec.md (16/16 aprobados)
```

(`tasks.md` se generará posteriormente con `/speckit-tasks`.)

### Source Code (repository root)

```text
include/
  texture_browser.h                      # Actualizar: añadir MaxMiiBytes (8 MiB) y MaxMapBytes (4 MiB)
  belongings_data.h                      # Existente (reutilizado sin cambios)
  habits_data.h                          # Existente (reutilizado sin cambios)
  rel_tier_labels.h                      # Existente (reutilizado sin cambios)
  mii_roster.h                           # Nuevo: dominio puro read-only, parser, formateadores y MiiBrowserModel
source/
  mii_roster.cpp                         # Nuevo: parser acotado de Mii.sav + Map.sav opcional, 7 secciones y modelo
apps/borealis/
  CMakeLists.txt                         # Actualizar: añadir mii_roster.cpp y mii_browser_screen.cpp
  source/
    texture_read_source.cpp              # Actualizar: permitir "/Mii.sav" y "/Map.sav" en lista blanca read-only
    app.cpp                              # Actualizar: reemplazar sample tab 2 por MiiBrowserScreen y propagar perfil/ciclo de vida
    mii_browser_screen.hpp               # Nuevo: MiiBrowserScreen (+ pantallas hijas MiiDetailScreen y MiiSectionScreen)
    mii_browser_screen.cpp               # Nuevo: lista de residentes, detalle de 7 secciones, vista de sección y restauración de foco/scroll
  resources/i18n/
    en-US/app.json                       # Actualizar: claves de localización de Mii y las 7 secciones
    es/app.json                          # Actualizar: traducciones es
    es-419/app.json                      # Actualizar: traducciones es-419
tests/
  support/
    mii_roster_fixtures.h                # Nuevo: generadores deterministas de Mii.sav y Map.sav sintéticos (0, 1, dispersos, 70, A/B, corruptos)
  mii_roster_test.cpp                    # Nuevo: tests unitarios puros de parser, 7 secciones, semillas, corrupción e inmutabilidad
  mii_browser_context_test.cpp           # Nuevo: tests de MiiBrowserModel, ReadSource falso, Map.sav opcional, cancelación y aislamiento
  borealis_ui_screens_test.cpp           # Actualizar: cobertura UI mando/touch de Mii (lista → detalle → 7 secciones → B, 10 ciclos, Y, Plus, errores)
  run_texture_browser.sh                 # Actualizar: registrar suites mii-roster y mii-context y fuentes en ui-screens
  README.md                              # Actualizar: documentar nuevas suites host de M2
docs/
  ROADMAP.md                             # Actualizar: avance de M2 y registro posterior de implementación/evidencia
  BOREALIS_MIGRATION.md                  # Actualizar: registro de implementación M2 al cerrar desarrollo
```

**Structure Decision**: Mantener todo el parseo binario, resolución de catálogos, formateo de las 7 secciones y máquina de estados puros en `include/mii_roster.h` y `source/mii_roster.cpp`. La extensión de lectura en `apps/borealis/source/texture_read_source.cpp` permanece estrictamente read-only (`FsOpenMode_Read` sobre `fsOpen_SaveDataReadOnly`). La jerarquía de pantallas (`MiiBrowserScreen`, `MiiDetailScreen`, `MiiSectionScreen`) vive en `apps/borealis/source/mii_browser_screen.{hpp,cpp}` y se conecta únicamente en `apps/borealis/source/app.cpp`, sin tocar `source/main.cpp`.

## Complexity Tracking

Sin violaciones constitucionales ni excepciones que justificar.

## Phase 0 — Research Outcome

[research.md](research.md) confirma empíricamente contra [data/seed_Mii.bin](../../data/seed_Mii.bin) y [data/seed_Map.bin](../../data/seed_Map.bin) los 38 hashes Murmur3 x86_32, tipos binarios exactos (`WStr32Array`, `BinArray`, `IntArray`, `UIntArray`, `EnumArray`, `BoolArray`, `UInt64Array`, `WStr64Array`) y tamaños (`70`, `840`, `2415`, `4830`), resuelve la regla segura de ocupación de plaza, define la degradación local ante ausencia de `Map.sav` y fija la navegación jerárquica de 3 niveles sin subpestañas.

## Phase 1 — Design and Handoff

- [data-model.md](data-model.md): entidades de dominio (`MiiResident`, `MiiStats`, `MiiWordSlot`, `MiiRelationEntry`, `MiiBelongings`, `MiiHabitCategorySummary`, `MiiHousingInfo`, `MiiSocialSummary`, `MiiRosterSummary`), errores tipados (`MiiReadError`) y máquina de estados `MiiBrowserModel`.
- [contracts/mii-browser.md](contracts/mii-browser.md): contratos del dominio puro, extensión read-only de `ReadSource`, navegación Borealis de 3 niveles, garantías negativas de solo lectura y trazabilidad `FR-001`–`FR-019` / `SC-001`–`SC-007`.
- [quickstart.md](quickstart.md): fixtures sintéticos de referencia, comandos de prueba host/sanitizers, compilación dual Switch, auditoría `aarch64-none-elf-nm` y checklist físico.

Orden sugerido para generar `tasks.md` con `/speckit-tasks`:
1. Extensión read-only de `TextureBrowser::ReadSource` (`MaxMiiBytes`, `MaxMapBytes`, lista blanca `"/Mii.sav"` y `"/Map.sav"`) y generadores de fixtures sintéticos (`tests/support/mii_roster_fixtures.h`).
2. Módulo puro `include/mii_roster.h` / `source/mii_roster.cpp` (parser acotado de `Mii.sav` + `Map.sav` opcional, las 7 secciones y `MiiBrowserModel`) + suites `mii-roster` y `mii-context`.
3. Traducciones `en-US` / `es` / `es-419` y pantallas jerárquicas `MiiBrowserScreen` → `MiiDetailScreen` → `MiiSectionScreen` integradas en `apps/borealis/source/app.cpp` y `apps/borealis/CMakeLists.txt`.
4. Ampliación de `tests/borealis_ui_screens_test.cpp`, ejecución completa (normal + ASan/UBSan), compilación dual (`make -j4` y `make -j4 borealis`), auditoría `aarch64-none-elf-nm` y actualización de documentación.
