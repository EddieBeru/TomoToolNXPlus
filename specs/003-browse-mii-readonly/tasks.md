---
description: "Tareas de implementación para consulta de residentes Mii en modo solo lectura (M2)"
---

# Tasks: Consulta de residentes Mii en modo solo lectura (M2)

**Input**: Documentos de diseño en `specs/003-browse-mii-readonly/`

**Prerequisites**: [plan.md](plan.md) (obligatorio), [spec.md](spec.md) (obligatorio), [research.md](research.md), [data-model.md](data-model.md), [contracts/mii-browser.md](contracts/mii-browser.md) y [quickstart.md](quickstart.md). Aplican [AGENTS.md](../../AGENTS.md), [la constitución](../../.specify/memory/constitution.md) y [la guía de interfaz](../../docs/UI_GUIDELINES.md).

**Tests**: Exigidos por `SC-001`–`SC-007` y las reglas de integridad de guardado. Escribir las pruebas antes o junto a su código objetivo usando únicamente fixtures sintéticos o las semillas públicas [data/seed_Mii.bin](../../data/seed_Mii.bin) y [data/seed_Map.bin](../../data/seed_Map.bin) (nunca guardados personales).

**Organization**: Setup → Foundational → US1 (`P1` 🎯 MVP) → US3 (`P1`) → US2 (`P2`) → Polish & Cross-Cutting Concerns. Los identificadores `[US1]`, `[US3]` y `[US2]` conservan su número en [spec.md](spec.md).

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

**Purpose**: Extender la capa read-only con los topes y lista blanca de `/Mii.sav` y `/Map.sav`, y construir generadores deterministas de fixtures sintéticos en memoria antes de tocar dominio o UI.

- [X] T001 Añadir `MaxMiiBytes = 8 * 1024 * 1024` y `MaxMapBytes = 4 * 1024 * 1024` en `include/texture_browser.h` y ampliar la lista blanca estrictamente read-only (`FsOpenMode_Read` sobre `fsOpen_SaveDataReadOnly`) de `SwitchSource` para permitir `"/Mii.sav"` y `"/Map.sav"` en `apps/borealis/source/texture_read_source.cpp`.
- [X] T002 [P] Crear constructores de fixtures `.sav` sintéticos deterministas (`Mii.sav` y `Map.sav` para Cuenta A con 3 plazas dispersas `1`/`14`/`70`, acentos y pares sustitutos UTF-16LE, frases, relaciones con pelea/rango/huérfana, prendas/`Coordinate`/bolsillos, hábitos en 9 categorías, vivienda con compañeros/conflicto/sin hogar y nombres de casas; Cuenta B con 1 residente, enums desconocidos y sin `Map.sav`; isla vacía `0` residentes; isla llena `70` residentes; variantes corruptas/truncadas/sobredimensionadas y spy de inmutabilidad) en `tests/support/mii_roster_fixtures.h`.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Construir y verificar el núcleo puro de validación binaria de `Mii.sav` (y `Map.sav` opcional), detección segura de plazas ocupadas, decodificación UTF-16LE, cargador read-only y máquina de estados `MiiBrowserModel`.

**⚠️ CRITICAL**: Ninguna fase de historia de usuario (`US1`, `US3`, `US2`) puede comenzar hasta completar esta base verificada en host.

- [X] T003 Declarar las entidades de dominio y contratos puros de solo lectura (`MaxSlots = 70`, `WordsPerMii = 12`, `PocketSlotsPerMii = 12`, `WornClothSlots = 8`, `HabitCategoryCount = 9`, `MiiSectionId` `0..6`, `MiiStats`, `MiiWordSlot`, `MiiRelationEntry`, `MiiBelongings`, `MiiHabitEntry`, `MiiHabitCategorySummary`, `MiiHousingInfo`, `MiiSocialSummary`, `MiiResident`, `MiiRosterSummary`, `MiiSectionOverview`, `MiiRowView`, `MiiReadError`, `MiiBrowserState` `{ Unselected, Loading, Suspended, Empty, Ready, Error }`, `MiiBrowserModel`, `Parse`, `LoadFromReadSource`, `FormatResidentSummary`, `FormatSectionOverviews` y `FormatSectionRows`, sin exponer ninguna operación de escritura/mutación) en `include/mii_roster.h`.
- [X] T004 [P] Escribir pruebas unitarias de validación estructural, límites y ocupación de plazas (`bytes.size() >= 0x20` y `<= MaxMiiBytes`, magic `0x01020304`, `saveDataOff` alineado a 8 bytes, centinelas `type < 33`, rechazo de entradas previas a centinela, offsets de heap `< saveDataOff` o desbordados, `BinArray` truncado, conteos `> MaxArrayCount`, ausencia o `TypeMismatch` de `Mii.Name.Name` `0x2499bfdau`, regla de ocupación `nameNonEmpty && charInfoNonZero` en plazas dispersas/0/70 e inmutabilidad byte a byte del búfer) en `tests/mii_roster_test.cpp`.
- [X] T005 Implementar la validación estructural acotada de cabecera, tabla índice y heap de `Mii.sav` y `Map.sav`, el hash Murmur3 x86_32 (semilla `0`), la decodificación UTF-16LE → UTF-8 (`WStr32Array` y `WStr64Array` con pares sustitutos y reemplazo `U+FFFD`) y la detección de plazas ocupadas (`1..70`) en `source/mii_roster.cpp`, verificando que pasen los casos estructurales de `tests/mii_roster_test.cpp`.
- [X] T006 [P] Escribir pruebas del cargador `LoadFromReadSource` (`"/Mii.sav"` obligatorio + `"/Map.sav"` opcional best-effort) y de la máquina de estados `MiiBrowserModel` (transiciones `Unselected` → `Loading` → `Ready`/`Empty`/`Error`, `suspend()`/`resume()`, incremento de `generation`, descarte de entregas obsoletas, cierre garantizado `source.close()` y cero escrituras sobre `TextureBrowser::ReadSource`) en `tests/mii_browser_context_test.cpp`.
- [X] T007 Implementar `LoadFromReadSource` y `MiiBrowserModel` (`select`, `loading`, `accept`, `suspend`, `resume`) en `source/mii_roster.cpp` y registrar las suites `mii-roster` y `mii-context` en `tests/run_texture_browser.sh`.

**Checkpoint**: Parser binario puro, loader read-only y máquina de estados `MiiBrowserModel` aprobados en host y bajo ASan/UBSan.

---

## Phase 3: User Story 1 - Listar los residentes reales de la isla y consultar sus estadísticas principales (Priority: P1) 🎯 MVP

**Goal**: Reemplazar las entradas de ejemplo de la pestaña **Mii** por la lista real de residentes ocupados (`1..70`), pantalla de detalle con 7 secciones enfocables (sin subpestañas) y vista real de los 16 atributos de **Estadísticas**, con cambio de cuenta y paridad mando/táctil.

**Independent Test**: Con fixtures sintéticos de Cuenta A y Cuenta B y `data/seed_Mii.bin`, abrir la pestaña `Mii`, verificar la lista real de residentes de Cuenta A, abrir un residente, inspeccionar sus 16 atributos de Estadísticas, cambiar a Cuenta B con `Y` comprobando aislamiento total, cancelar `Y` conservando la sesión y repetir el recorrido solo con mando (`D-pad`/`A`/`B`) y solo con pantalla táctil.

### Tests for User Story 1

- [X] T008 [P] [US1] Ampliar `tests/mii_roster_test.cpp` con pruebas de extracción y formato de los 16 atributos de Estadísticas para Cuenta A, Cuenta B, isla de 70 residentes y `data/seed_Mii.bin`: `Mii.Name.Name`, `Level` (`+1`), `Meter` (`0..100`), `Money`, `BirthdayInfo.{Day,Month,Year,DirectAge}` (`-1` → automático), 5 `CharacterParam` (`1..10`), `BondInfo.Meter` (`0..100`), `Feeling.Type` (9 estados de ánimo) y `EatFullness`, verificando ausencia de acciones de escritura/borrado.

### Implementation for User Story 1

- [X] T009 [US1] Implementar la extracción y formateo de `MiiStats`, `FormatResidentSummary` y `FormatSectionRows(..., MiiSectionId::Stats, ...)` en `source/mii_roster.cpp`, ejecutando `tests/mii_roster_test.cpp`.
- [X] T010 [P] [US1] Añadir las claves de localización de la pestaña Mii (encabezados, nota de solo lectura, lista, estados vacío/carga/error, 7 secciones, 16 campos de Estadísticas, 9 estados de ánimo, tipos de palabra, tipos de relación, 9 categorías de hábitos, vivienda y resumen social) en `apps/borealis/resources/i18n/en-US/app.json`, `apps/borealis/resources/i18n/es/app.json` y `apps/borealis/resources/i18n/es-419/app.json`.
- [X] T011 [US1] Implementar `MiiBrowserScreen` (lista raíz desplazable de residentes con `Actualizar / Reintentar`, etiqueta de estado y `Volver a la lista de secciones`), `MiiDetailScreen` (pantalla hija empujada con `navigation::push`, botón estable `Volver` y 7 filas enfocables de sección sin subpestañas) y `MiiSectionScreen` (vista desplazable de solo lectura con botón `Volver`) en `apps/borealis/source/mii_browser_screen.hpp` y `apps/borealis/source/mii_browser_screen.cpp`.
- [X] T012 [US1] Integrar `MiiBrowserScreen` en la pestaña índice `2` (`app/mii`) de `Navigation` en `apps/borealis/source/app.cpp` (reemplazando `addSample` de Mii, propagando `setProfile`, `activateBrowser` y `deactivateBrowser` en arranque, cambio de pestaña y `changeProfile()`, y registrando `source/mii_roster.cpp` y `apps/borealis/source/mii_browser_screen.cpp` en `apps/borealis/CMakeLists.txt`).
- [X] T013 [US1] Añadir pruebas de UI en host para `US1` (reemplazo de datos de muestra en tab `2`, lista real de Cuenta A, entrada a `MiiDetailScreen` y `MiiSectionScreen` de Estadísticas, cambio a Cuenta B con `Y` sin mezcla de datos, cancelación de `Y` conservando sesión y recorrido completo por mando y por pantalla táctil) en `tests/borealis_ui_screens_test.cpp` y actualizar `tests/run_texture_browser.sh`.

**Checkpoint**: MVP de `US1` funcional y verificable en host sin datos de muestra en la pestaña Mii.

---

## Phase 4: User Story 3 - Distinguir y recuperar estados vacíos, sin guardado, incompletos o con fallos de lectura (Priority: P1)

**Goal**: Distinguir explícitamente sin perfil, cargando, suspensión al cambiar de pestaña, isla vacía (`0` residentes), sin guardado, acceso denegado, datos corruptos/sobredimensionados y degradación local ante `Map.sav` ausente o códigos desconocidos, recuperables con `Actualizar / Reintentar`.

**Independent Test**: Probar `NoAccountSelected`, `Empty` (`0` residentes), `NoSave`, `ReadOnlyOpenFailed`, archivo truncado/corrupto/sobredimensionado (`> 8 MiB`), ausencia/corrupción de `Map.sav`, cambio de pestaña durante `Loading` y recuperación con `Actualizar / Reintentar` por mando y táctil.

### Tests for User Story 3

- [X] T014 [P] [US3] Ampliar `tests/mii_roster_test.cpp` y `tests/mii_browser_context_test.cpp` con pruebas de degradación local (`FR-010`) y errores recuperables (`FR-014`, `FR-015`, `FR-017`): `NoAccountSelected`, isla válida con `0` residentes (`Empty`), `NoSave`, `AccountMetadataFailed`, `ReadOnlyOpenFailed`, `ShortRead`, `FileTooLarge` (`> 8 MiB`), `Map.sav` ausente o corrupto sin invalidar `Mii.sav`, estado de ánimo/palabra/relación/prenda/hábito fuera de catálogo, cumpleaños fuera de rango, relación apuntando a plaza vacía, suspensión durante `Loading` y reintento exitoso.

### Implementation for User Story 3

- [X] T015 [US3] Completar en `source/mii_roster.cpp` las advertencias/etiquetas de reserva por campo (`hasWarnings`, `mapNamesWarning`, fallback `"Casa #N"` / `"House #N"`, hashes hex `0xXXXXXXXX` para enums desconocidos) y el mapeo exhaustivo de `TextureBrowser::Error` a `MiiReadError`.
- [X] T016 [US3] Completar en `apps/borealis/source/mii_browser_screen.cpp` la presentación diferenciada de estados (`Unselected`, `Loading`, `Suspended`, `Empty`, `Ready`, `Error`), bloqueo de `Actualizar / Reintentar` durante `Loading`, suspensión al salir de la pestaña Mii (`deactivateBrowser()`) y recarga única al regresar (`activateBrowser()`).
- [X] T017 [US3] Ampliar `tests/borealis_ui_screens_test.cpp` con pruebas de UI en host para `Unselected`, `Empty` (`0` residentes), `NoSave`, acceso fallido, archivo corrupto/sobredimensionado, suspensión al cambiar de pestaña durante `Loading` y recuperación con `Actualizar / Reintentar` por mando y toque.

**Checkpoint**: Todos los estados vacíos, de error, suspensión y degradación local recuperan limpiamente sin inventar residentes ni bloquear la navegación.

---

## Phase 5: User Story 2 - Inspeccionar palabras, relaciones, pertenencias, hábitos, vivienda y resumen social del residente (Priority: P2)

**Goal**: Completar la lectura y visualización de las 6 secciones restantes del residente (`Palabras`, `Relaciones`, `Pertenencias`, `Hábitos`, `Vivienda`, `Resumen social`) y verificar la conservación exacta de foco y desplazamiento en 10 ciclos de entrada/salida y diálogos globales.

**Independent Test**: Abrir cada una de las 6 secciones restantes en Cuenta A y Cuenta B comprobando todos los datos y degradaciones, y ejecutar 10 ciclos consecutivos lista ↔ residente ↔ sección y cambios de pestaña / cancelaciones de `Y` y `Plus` verificando restauración exacta de foco y scroll.

### Tests for User Story 2

- [X] T018 [P] [US2] Ampliar `tests/mii_roster_test.cpp` para verificar extracción y formato de las 6 secciones restantes: Palabras (`12` plazas, `WordKind`/`WordText`/`WordHowToCall`), Relaciones (`2415` pares, `4830` direcciones, `rel_tier_labels.h`, `IsFight`, `SinceTimestamp`, plazas huérfanas), Pertenencias (`8` prendas + `Coordinate` + `12` bolsillos contra `include/belongings_data.h`), Hábitos (`74` hábitos en `9` categorías contra `include/habits_data.h` con conteos), Vivienda (`HouseMapId`, `RoomIndex`, compañeros, conflictos de habitación y nombres desde `Map.sav` o reserva) y Resumen social (conteos por categoría y lista agrupada).

### Implementation for User Story 2

- [X] T019 [US2] Implementar la extracción completa y formateadores `FormatSectionOverviews` y `FormatSectionRows` para Palabras, Relaciones, Pertenencias, Hábitos, Vivienda y Resumen social en `source/mii_roster.cpp` (reutilizando `include/belongings_data.h`, `include/habits_data.h` e `include/rel_tier_labels.h`).
- [X] T020 [US2] Conectar las 7 secciones y sus resúmenes en `MiiDetailScreen` y `MiiSectionScreen` en `apps/borealis/source/mii_browser_screen.cpp` con etiquetas secundarias multilínea indentadas (`setSingleLine(false)`), acción inocua de `A`/toque sobre filas informativas y preservación/restauración exacta de `focusTarget` (`"mii_<slot>"`, `"refresh"`, `"tabs"`) y `scrollOffset` al volver con `B` o tras diálogos globales (`Y`, `Plus`).
- [X] T021 [US2] Ampliar `tests/borealis_ui_screens_test.cpp` para verificar la apertura y contenido real de las 7 secciones de un residente, retorno con `B` desde sección → detalle → lista → barra lateral, y conservación de residente enfocado, sección enfocada y desplazamiento en 10 ciclos consecutivos y tras abrir/cancelar `Y` y `Plus`.

**Checkpoint**: Las tres historias de usuario (`US1`, `US3`, `US2`) funcionan de forma integrada manteniendo intactas Texturas, Ajustes de texturas y Jugador.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentación final, batería completa bajo sanitizers, compilación cruzada Switch, auditoría negativa de símbolos de escritura y preparación de validación física en Switch.

- [X] T022 [P] Documentar las nuevas suites `mii-roster`, `mii-context` y cobertura M2 de `ui-screens` en `tests/README.md`, `docs/BOREALIS_MIGRATION.md` y `docs/ROADMAP.md`.
- [X] T023 Ejecutar la batería completa en host (`tests/run_texture_browser.sh`, `SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh`), compilar ambos binarios Switch (`make -j4` y `make -j4 borealis`), auditar `build/borealis/TomoToolNXPlusBorealis.elf` con `aarch64-none-elf-nm -C` confirmando `0` referencias a `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` o `SaveEditor::Save`, y registrar la evidencia de host/build en `specs/003-browse-mii-readonly/quickstart.md`.
- [ ] T024 Desplegar `TomoToolNXPlusBorealis.nro` a la Switch física cuando la consola esté disponible (`make -j4 deploy-borealis DEPLOY_ARGS='--replace-undated'`), completar la checklist física en `specs/003-browse-mii-readonly/quickstart.md` (`en-US`, `es`, `es-419`, claro/oscuro, 720p/dock, mando/táctil e integridad del guardado) y registrar la evidencia en `specs/003-browse-mii-readonly/quickstart.md` y `docs/HARDWARE_UI_FEEDBACK.md`.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: Sin dependencias; `T001` y `T002` pueden ejecutarse en paralelo.
- **Phase 2 (Foundational)**: Depende de Phase 1 (`T003` → `T004`/`T006` en paralelo → `T005`/`T007`) — **bloquea todas las User Stories**.
- **Phase 3 (US1 / P1 MVP)**: Depende de Phase 2 (`T008`, `T010` en paralelo; `T009` → `T011` → `T012` → `T013`).
- **Phase 4 (US3 / P1)** y **Phase 5 (US2 / P2)**: Dependen de `US1` por compartir `source/mii_roster.cpp`, `apps/borealis/source/mii_browser_screen.cpp` y `tests/borealis_ui_screens_test.cpp`. Se recomienda ejecutar `US3` (`P1`) antes que `US2` (`P2`).
- **Phase 6 (Polish)**: Depende de completar `US1`, `US3` y `US2`.

### Parallel Opportunities

```bash
# Phase 1:
T001 ("include/texture_browser.h + texture_read_source.cpp") || T002 ("tests/support/mii_roster_fixtures.h")

# Phase 2 (después de T003):
T004 ("tests/mii_roster_test.cpp") || T006 ("tests/mii_browser_context_test.cpp")

# Phase 3 (después de Phase 2):
T008 ("tests/mii_roster_test.cpp") || T010 ("apps/borealis/resources/i18n/*/app.json")
```

---

## Implementation Strategy

### MVP First (Solo User Story 1)

1. Completar **Phase 1 (Setup)** y **Phase 2 (Foundational)**.
2. Completar **Phase 3 (`US1`)**: lista real de residentes, pantalla de detalle con 7 secciones, sección Estadísticas e integración en tab `2` de `apps/borealis/source/app.cpp`.
3. **Detener y validar**: ejecutar `mii-roster`, `mii-context`, `ui-screens` y `make -j4 borealis` verificando que los datos ficticios de Mii desaparecieron y el flujo jerárquico funciona entre cuentas.

### Incremental Delivery

1. Setup + Foundational → Extensión read-only de `ReadSource`, núcleo binario puro y `MiiBrowserModel`.
2. **US1 (`P1`)** → Lista real de residentes (hasta 70), detalle jerárquico sin subpestañas y 16 Estadísticas.
3. **US3 (`P1`)** → Estados diferenciados (incluido `Empty` para 0 residentes), degradación local de `Map.sav`, suspensión y recuperación.
4. **US2 (`P2`)** → 6 secciones restantes (Palabras, Relaciones, Pertenencias, Hábitos, Vivienda, Resumen social) y restauración exacta de foco/scroll en 10 ciclos.
5. **Polish (`Phase 6`)** → Sanitizers, compilación dual, auditoría `aarch64-none-elf-nm` y checklist físico listo para cuando haya Switch disponible.
