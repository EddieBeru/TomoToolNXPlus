---
description: "Tareas de implementación para consulta real de datos del jugador e isla en solo lectura (M1)"
---

# Tasks: Consulta de datos reales del jugador e isla en solo lectura (M1)

**Input**: Documentos de diseño en `specs/002-browse-player-readonly/`

**Prerequisites**: [plan.md](plan.md) (obligatorio), [spec.md](spec.md) (obligatorio), [research.md](research.md), [data-model.md](data-model.md), [contracts/player-browser.md](contracts/player-browser.md) y [quickstart.md](quickstart.md). Aplican [AGENTS.md](../../AGENTS.md), [la constitución](../../.specify/memory/constitution.md) y [la guía de interfaz](../../docs/UI_GUIDELINES.md).

**Tests**: Exigidos por `SC-001`–`SC-007` y las reglas de integridad de guardado. Escribir las pruebas antes o junto a su código objetivo usando únicamente fixtures sintéticos o la semilla [data/seed_Player.bin](../../data/seed_Player.bin) (nunca guardados personales).

**Organization**: Setup → Foundational → US1 (`P1` 🎯 MVP) → US3 (`P1`) → US2 (`P2`) → Polish & Cross-Cutting Concerns. Los números `[US1]`, `[US3]` y `[US2]` conservan su identificador en [spec.md](spec.md).

## Format: `[ID] [P?] [Story?] Description with file path`

- **[P]**: Puede ejecutarse en paralelo (archivos distintos, sin dependencias de tareas incompletas).
- **[Story]**: Obligatorio únicamente en fases de historia de usuario (`[US1]`, `[US3]`, `[US2]`); ausente en Setup, Foundational y Polish.
- Incluye rutas exactas desde la raíz del repositorio en cada descripción.

## Path Conventions

- Dominio puro y contratos sin dependencias de plataforma/UI: `include/` y `source/`.
- Integración Borealis/Switch y recursos de idioma: `apps/borealis/source/`, `apps/borealis/resources/i18n/` y `apps/borealis/CMakeLists.txt`.
- Pruebas host y fixtures sintéticos: `tests/` y `tests/support/`.
- No modificar `source/main.cpp` ni enlazar `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` o `SaveEditor::Save` en Borealis.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Preparar la matriz base de verificación y los generadores deterministas de fixtures `.sav` sintéticos en memoria antes de tocar dominio o UI.

- [X] T001 Registrar baseline de verificación y matriz `SC-001`–`SC-007` (separando evidencia de host, emulador y Switch física) en `specs/002-browse-player-readonly/quickstart.md`.
- [X] T002 [P] Crear constructores de fixtures `.sav` sintéticos deterministas (Cuenta A con los 15 atributos válidos, acentos y pares sustitutos UTF-16LE, `DT_WStr64` en pronunciación, tabla de 268 deseos y timestamp de donación; Cuenta B con valores distintos, enums fuera de catálogo, escalares fuera de rango, penalización VT activa, donación futura y sin tabla de deseos; variantes corruptas/truncadas y spy de inmutabilidad) en `tests/support/player_profile_fixtures.h`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Construir y verificar el núcleo puro de validación binaria de `Player.sav`, el decodificador UTF-16LE, el cargador read-only y la máquina de estados `PlayerBrowserModel`.

**⚠️ CRITICAL**: Ninguna fase de historia de usuario (`US1`, `US3`, `US2`) puede comenzar hasta completar esta base verificada en host.

- [X] T003 Declarar las entidades de dominio y contratos puros de solo lectura (`PlayerAttributeId` `0..14`, `EnumResolution`, `WishesProgress` con `totalCatalogWishes = 268`, `PlayerProfileSummary`, `PlayerAttributeView` con `focusKey` `"attr_0"`..`"attr_14"`, `PlayerReadError`, `PlayerBrowserState` `{ Unselected, Loading, Suspended, Ready, Error }`, `PlayerBrowserModel`, `Parse`, `LoadFromReadSource` y `FormatAttributes` con `int64_t referenceUnixSeconds` explícito, sin exponer `Save`, `Write`, `Commit`, `Repair`, `Backup`, `Set*` ni `Restore`) en `include/player_profile.h`.
- [X] T004 [P] Escribir pruebas unitarias de validación estructural y límites del parser (`bytes.size() >= 0x20` y `bytes.size() <= TextureBrowser::MaxPlayerBytes` (`8 MiB`), magic `0x01020304`, `saveDataOff >= 0x20`, `saveDataOff <= bytes.size()`, `(saveDataOff - 0x20) % 8 == 0`, centinelas `slot < 33`, rechazo de entradas previas a centinela, offsets de heap `< saveDataOff` o desbordados, duplicados conflictivos, ausencia o `TypeMismatch` en campos obligatorios `Player.Name`, `Player.IslandName`, `Player.Money`, `Player.Currency` e inmutabilidad byte a byte del búfer de entrada) en `tests/player_profile_test.cpp`.
- [X] T005 Implementar la validación estructural acotada de cabecera, tabla índice y heap de `Player.sav`, el hash Murmur3 x86_32 (semilla `0`) y la decodificación UTF-16LE → UTF-8 para `DT_WStr32` (`64` bytes) y `DT_WStr64` (`128` bytes) con soporte de pares sustitutos (`0xD800..0xDBFF` + `0xDC00..0xDFFF`) y reemplazo `U+FFFD` en `source/player_profile.cpp`, verificando que pasen los casos estructurales de `tests/player_profile_test.cpp`.
- [X] T006 [P] Escribir pruebas del cargador `LoadFromReadSource` y de la máquina de estados `PlayerBrowserModel` (transiciones `Unselected` → `Loading` → `Ready`/`Error`, `suspend()`/`resume()`, incremento de `generation`, descarte de entregas obsoletas, cierre garantizado `source.close()` en cada salida y cero intentos de escritura sobre `TextureBrowser::ReadSource`) en `tests/player_browser_context_test.cpp`.
- [X] T007 Implementar `LoadFromReadSource` y `PlayerBrowserModel` (`select`, `refresh`, `accept`, `suspend`, `resume`) en `source/player_profile.cpp` y registrar las suites `player-profile` y `player-context` en `tests/run_texture_browser.sh`.

**Checkpoint**: Parser binario puro, loader read-only y máquina de estados `PlayerBrowserModel` aprobados en host y bajo ASan/UBSan.

---

## Phase 3: User Story 1 - Consultar la identidad y el estado general del jugador y la isla (Priority: P1) 🎯 MVP

**Goal**: Reemplazar las entradas de ejemplo de la pestaña **Jugador** por los 15 atributos reales de `/Player.sav` para la cuenta activa en Switch, operando en modo solo lectura con cambio de perfil y paridad mando/táctil.

**Independent Test**: Con fixtures sintéticos de Cuenta A y Cuenta B y `data/seed_Player.bin`, abrir la pestaña `Jugador`, verificar los 15 atributos principales reales de Cuenta A, cambiar a Cuenta B con `Y` comprobando aislamiento total, cancelar `Y` conservando la sesión y repetir el recorrido solo con mando (`D-pad`/`A`/`B`) y solo con pantalla táctil.

### Tests for User Story 1

- [X] T008 [P] [US1] Ampliar `tests/player_profile_test.cpp` con pruebas de extracción y formato de los 15 atributos para Cuenta A, Cuenta B y la plantilla `data/seed_Player.bin`: hashes exactos (`Player.Name` `0xc7fb279bu`, `Player.IslandName` `0xd46df986u`, `Player.Money` `0x365fab1fu` en centésimas `1500000` → `15 000,00`, `Player.Currency` `0xdc88f139u` sobre 11 monedas, `Player.BootNum` `0x2f19f777u`, `Player.SkinColorIndex` `0xafbdf7dcu` `0..5` → `1..6`, cumpleaños `0xdb7786bbu` `1..31` / `0xc754bef3u` `1..12` / `0x11996629u`, tamaño de isla `0x870a807cu` `1..4` → `50 x 36`, `70 x 50`, `90 x 64`, `120 x 80`, `Liberation.FountainLevel` `0xa4e65f79u`, deseos `0xa32f7e47u`, `Player.Region` `0x10631cf8u` sobre 9 regiones, penalización VT `0x71e3ba15u` `0` vs `!= 0` y `Player.LastDonationGameTime` `0xbba9c090u`).

### Implementation for User Story 1

- [X] T009 [US1] Implementar la extracción completa de los 15 atributos principales, tablas de catálogo (11 monedas, 9 regiones, 11 idiomas, 6 tonos de piel, 4 tamaños de isla) y valores principales de `FormatAttributes` en `source/player_profile.cpp`, ejecutando `tests/player_profile_test.cpp`.
- [X] T010 [P] [US1] Añadir las claves de localización de la pestaña Jugador (títulos de los 15 atributos, nota de solo lectura, monedas, regiones, idiomas, estados de penalización y donación) en `apps/borealis/resources/i18n/en-US/app.json`, `apps/borealis/resources/i18n/es/app.json` y `apps/borealis/resources/i18n/es-419/app.json`.
- [X] T011 [US1] Implementar `PlayerBrowserScreen` con encabezado `app/player`, nota de solo lectura, acción `Actualizar / Reintentar`, etiqueta de estado, lista desplazable de 15 filas `brls::DetailCell` en orden canónico (`0..14`) y botón `Volver a la lista de secciones` en `apps/borealis/source/player_browser_screen.hpp` y `apps/borealis/source/player_browser_screen.cpp`.
- [X] T012 [US1] Integrar `PlayerBrowserScreen` en la pestaña índice `3` (`app/player`) de `Navigation` en `apps/borealis/source/app.cpp` (reemplazando `addSample` de Jugador, propagando `setProfile` en arranque y en `changeProfile()`, y registrando `source/player_profile.cpp` y `apps/borealis/source/player_browser_screen.cpp` en `apps/borealis/CMakeLists.txt`).
- [X] T013 [US1] Añadir pruebas de UI en host para `US1` (reemplazo de datos de muestra en tab `3`, verificación de los 15 atributos de Cuenta A, cambio a Cuenta B con `Y` sin mezcla de datos, cancelación de `Y` conservando sesión y recorrido completo por mando y por pantalla táctil) en `tests/borealis_ui_screens_test.cpp` y actualizar `tests/run_texture_browser.sh`.

**Checkpoint**: MVP de `US1` funcional y verificable en host sin datos de muestra en la pestaña Jugador.

---

## Phase 4: User Story 3 - Distinguir y recuperar estados sin guardado, incompletos o con fallos de lectura (Priority: P1)

**Goal**: Distinguir explícitamente los estados sin perfil, carga lenta, suspensión al cambiar de pestaña, ausencia de guardado, acceso denegado, corrupción o campos fuera de catálogo, con recuperación mediante `Actualizar / Reintentar` o cambio de cuenta sin mutar guardados.

**Independent Test**: Probar sesión sin cuenta (`NoAccountSelected`), cuenta sin guardado (`NoSave`), error de apertura (`ReadOnlyOpenFailed`), archivo truncado/corrupto/sobredimensionado (`> 8 MiB`), cambio de pestaña durante `Loading` y recuperación con `Actualizar / Reintentar` por mando y táctil, además de campos individuales ausentes o fuera de catálogo.

### Tests for User Story 3

- [X] T014 [P] [US3] Ampliar `tests/player_profile_test.cpp` y `tests/player_browser_context_test.cpp` con pruebas de degradación local (`FR-003`) y errores recuperables (`FR-008`, `FR-009`, `FR-011`): perfil sin cuenta seleccionada (`NoAccountSelected`), cuenta sin guardado (`NoSave`), fallo de metadata/apertura (`AccountMetadataFailed`/`ReadOnlyOpenFailed`), `ShortRead`, `FileTooLarge` (`> 8 MiB`), pronunciación vacía, moneda/región/idioma desconocido (`0xXXXXXXXX`), tono `> 5`, día/mes fuera de `1..31`/`1..12`, tamaño de isla fuera de `1..4`, donación en el futuro (`stamp > referenceUnixSeconds`), tabla `Liberation.WishInfo.*` ausente o truncada, suspensión al salir de pestaña durante `Loading` y reintento exitoso tras recuperar el origen.

### Implementation for User Story 3

- [X] T015 [US3] Completar en `source/player_profile.cpp` el manejo de reservas/advertencias por campo (`hasWarning`, `hasFieldWarnings`, `tableWarning`, etiquetas de reserva con hash hex o aviso fuera de rango y fecha futura sin conteos negativos) y el mapeo exhaustivo de `TextureBrowser::Error` a `PlayerReadError`.
- [X] T016 [US3] Implementar en `apps/borealis/source/player_browser_screen.cpp` y `apps/borealis/source/app.cpp` la presentación diferenciada de estados (`Unselected`, `Loading`, `Suspended`, `Ready`, `Error`), bloqueo de `Actualizar / Reintentar` durante `Loading`, suspensión al salir de la pestaña Jugador (`deactivateBrowser()`) y recarga única al regresar (`activateBrowser()`).
- [X] T017 [US3] Ampliar `tests/borealis_ui_screens_test.cpp` con pruebas de UI en host para `Unselected`, `NoSave`, acceso fallido, archivo corrupto/sobredimensionado, suspensión al cambiar de pestaña durante `Loading` y recuperación con `Actualizar / Reintentar` por mando y toque.

**Checkpoint**: Todos los estados de error, suspensión y degradación local recuperan limpiamente sin inventar ceros ni bloquear la navegación.

---

## Phase 5: User Story 2 - Inspeccionar el detalle completo de cada atributo y conservar el contexto (Priority: P2)

**Goal**: Presentar de forma integrada debajo de cada fila todos los metadatos secundarios y descripciones multilínea localizadas (`FR-002`, `FR-004`), y conservar el atributo enfocado y la posición de desplazamiento tras cambios de pestaña o diálogos globales.

**Independent Test**: Inspeccionar los metadatos secundarios (pronunciación e idioma de nombre e isla, deseos liberados `X / 268`, días transcurridos desde la donación, dimensiones y descripción larga), cambiar 10 veces consecutivas de pestaña principal y abrir/cancelar `Y` y `Plus`, comprobando restauración exacta de foco y desplazamiento, y retorno a la barra lateral con `B`.

### Tests for User Story 2

- [X] T018 [P] [US2] Ampliar `tests/player_profile_test.cpp` para verificar el contenido completo de `secondaryMetadata` y `description` en los 15 `PlayerAttributeView`: pronunciación (`Player.HowToCallName` `0xbd8c4da1u` y `Player.HowToCallIslandName` `0xc6482c14u` en `DT_WStr64`/`DT_WStr32`) + idioma (`Player.NameRegionLanguageID` `0xb3229c6bu` y `Player.IslandNameRegionLanguageID` `0x7bbff932u`), monto bruto en centésimas, nombre completo y símbolo de moneda, color RGB de referencia del tono de piel (`#F6D9BD`..`#5A391C`), preset y dimensiones de isla, progreso de deseos liberados `<liberados> / 268` desde `Liberation.WishInfo.WishIdValue` (`0xf91e67beu`) + `Liberation.WishInfo.IsLiberated` (`0xe38c7611u`), estado/timestamp de penalización VT y días transcurridos desde la donación.

### Implementation for User Story 2

- [X] T019 [US2] Completar en `source/player_profile.cpp` y en `apps/borealis/resources/i18n/{en-US,es,es-419}/app.json` la composición multilínea de `secondaryMetadata` y `description` de solo lectura para los 15 atributos.
- [X] T020 [US2] Implementar en `apps/borealis/source/player_browser_screen.cpp` las etiquetas multilínea indentadas (`brls/listitem/descriptionIndent`, `setSingleLine(false)`) integradas debajo de cada `brls::DetailCell`, la acción inocua de `A`/toque sobre filas de atributo (solo enfocar sin abrir subpantallas) y la preservación/restauración de `focusTarget` (`"attr_0"`..`"attr_14"`, `"refresh"`, `"tabs"`) y `scrollOffset` tras cambios de pestaña, recargas y diálogos globales (`Y`, `Plus`).
- [X] T021 [US2] Ampliar `tests/borealis_ui_screens_test.cpp` para verificar las 15 etiquetas secundarias/descripciones inline, que `A`/toque en cualquier atributo no empuja actividades hijas, el retorno a la barra lateral con `B`, y la conservación del atributo enfocado y `scrollOffset` tras 10 cambios consecutivos de pestaña principal y tras abrir/cancelar `Y` y `Plus`.

**Checkpoint**: Las tres historias de usuario (`US1`, `US3`, `US2`) funcionan de forma integrada manteniendo intactas las pestañas de Texturas y Ajustes de texturas.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentación final, batería completa bajo sanitizers, compilación cruzada Switch, auditoría negativa de símbolos de escritura y validación física en Switch.

- [X] T022 [P] Documentar las nuevas suites `player-profile`, `player-context` y la cobertura M1 de `ui-screens` en `tests/README.md`, `docs/BOREALIS_MIGRATION.md` y `docs/ROADMAP.md`.
- [X] T023 Ejecutar la batería completa en host (`tests/run_texture_browser.sh`, `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh` y tests unitarios existentes), compilar ambos binarios Switch (`make -j4` y `make -j4 borealis`) y auditar `build/borealis/TomoToolNXPlusBorealis.elf` con `aarch64-none-elf-nm -C` confirmando `0` referencias a `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` o `SaveEditor::Save`, registrando la evidencia de host/build en `specs/002-browse-player-readonly/quickstart.md`.
- [ ] T024 Desplegar `TomoToolNXPlusBorealis.nro` a la Switch física (`make -j4 deploy-borealis DEPLOY_ARGS='--replace-undated'`), completar la validación en hardware real de `specs/002-browse-player-readonly/quickstart.md` (`en-US`, `es`, `es-419`, claro/oscuro, 720p/dock, mando/táctil e integridad del guardado) y registrar la evidencia física en `specs/002-browse-player-readonly/quickstart.md` y `docs/HARDWARE_UI_FEEDBACK.md`.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: Sin dependencias; inicia de inmediato (`T001` y `T002` en paralelo).
- **Phase 2 (Foundational)**: Depende de Phase 1 (`T003` depende de los tipos/fixtures base de `T002`; `T004` y `T006` pueden ir en paralelo tras `T003`; `T005` y `T007` cierran la base) — **bloquea todas las User Stories**.
- **Phase 3 (US1 / P1 MVP)**: Depende de Phase 2 (`T008`, `T010` en paralelo; `T009` → `T011` → `T012` → `T013`).
- **Phase 4 (US3 / P1)** y **Phase 5 (US2 / P2)**: Dependen de `US1` (al compartir `source/player_profile.cpp`, `apps/borealis/source/player_browser_screen.cpp` y `tests/borealis_ui_screens_test.cpp`). Dentro de cada fase, los tests `[P]` pueden redactarse en paralelo antes de la implementación correspondiente. Se recomienda ejecutar `US3` antes que `US2` por prioridad `P1` antes de `P2`, manteniendo las validaciones independientes de cada historia.
- **Phase 6 (Polish)**: Depende de completar `US1`, `US3` y `US2`.

### Parallel Opportunities

```bash
# Phase 1:
T001 ("specs/002-browse-player-readonly/quickstart.md") || T002 ("tests/support/player_profile_fixtures.h")

# Phase 2 (después de T003):
T004 ("tests/player_profile_test.cpp") || T006 ("tests/player_browser_context_test.cpp")

# Phase 3 (después de Phase 2):
T008 ("tests/player_profile_test.cpp") || T010 ("apps/borealis/resources/i18n/*/app.json")
```

---

## Implementation Strategy

### MVP First (Solo User Story 1)

1. Completar **Phase 1 (Setup)** y **Phase 2 (Foundational)**.
2. Completar **Phase 3 (`US1`)**: parser/formateador de los 15 atributos principales, pantalla `PlayerBrowserScreen` básica e integración en tab `3` de `apps/borealis/source/app.cpp`.
3. **Detener y validar**: ejecutar `player-profile`, `player-context`, `ui-screens` y `make -j4 borealis` verificando que los datos ficticios de Jugador desaparecieron y la lectura real funciona entre cuentas.

### Incremental Delivery

1. Setup + Foundational → Núcleo binario puro y modelo de estado verificados en host.
2. **US1 (`P1`)** → Lista real de 15 atributos en solo lectura y cambio de cuenta (`Y`).
3. **US3 (`P1`)** → Estados claros, suspensión al cambiar de pestaña y recuperación ante fallos/corrupción.
4. **US2 (`P2`)** → Metadatos secundarios integrados bajo cada fila (pronunciación/idioma, deseos liberados, días de donación, descripciones) y restauración de foco/scroll.
5. **Polish (`Phase 6`)** → Sanitizers, compilación dual, auditoría de símbolos `aarch64-none-elf-nm` y validación física en Switch.
