# Research: Consulta de residentes Mii en modo solo lectura (M2)

**Feature**: `specs/003-browse-mii-readonly/` (`M2`) | **Date**: 2026-11-22

Este documento consolida la verificación empírica del formato binario de `Mii.sav` y `Map.sav` contra las semillas del repositorio (`data/seed_Mii.bin` y `data/seed_Map.bin`), la extensión estrictamente read-only de `TextureBrowser::ReadSource`, la reutilización de catálogos existentes y la arquitectura jerárquica de Borealis sin subpestañas.

---

## 1. Acceso read-only a `/Mii.sav` y `/Map.sav` en Switch

- **Decision**: Extender la lista blanca read-only de `TextureBrowser::ReadSource` ([apps/borealis/source/texture_read_source.cpp](../../apps/borealis/source/texture_read_source.cpp)) y las constantes de límite en [include/texture_browser.h](../../include/texture_browser.h) para permitir `"/Mii.sav"` (`MaxMiiBytes = 8 * 1024 * 1024`) y `"/Map.sav"` (`MaxMapBytes = 4 * 1024 * 1024`) bajo el mismo montaje `fsOpen_SaveDataReadOnly` (`FsOpenMode_Read`).
- **Rationale**:
  - `data/seed_Mii.bin` pesa `2,945,804` bytes (~2.81 MiB) y `data/seed_Map.bin` pesa `192,576` bytes (~188 KiB). Los topes `8 MiB` y `4 MiB` ofrecen margen amplio para partidas completas de 70 residentes manteniendo protección contra archivos corruptos o sobredimensionados (`FR-017`).
  - Reutilizar `SwitchSource` garantiza un único punto de apertura de solo lectura (`fsOpen_SaveDataReadOnly`), lectura acotada por bloques `ReadChunk = 64 KiB`, verificación de cancelación entre bloques, validación de tamaño final y cierre garantizado (`source.close()`).
  - `"/Map.sav"` es **opcional** en M2: solo se lee para resolver los nombres personalizados de casas (`House.MapId` / `House.RoommateGroupName`). Si no existe o falla su lectura/parseo, la carga de `Mii.sav` sigue teniendo éxito y Vivienda degrada localmente a etiquetas numeradas (`Casa #N` / `House #N`) con advertencia no bloqueante (`FR-008`, `FR-010`).
- **Alternatives considered**:
  - Enlazar `SaveMount` o `MiiManager`: **rechazado** porque `SaveMount` abre guardados con permiso de escritura y `MiiManager` mezcla lectura con mutaciones (`WriteFile`, `CreateBlankMii`, `ImportMii`, `ExportMii`).

---

## 2. Verificación empírica del formato binario (`data/seed_Mii.bin` y `data/seed_Map.bin`)

Verificado directamente contra `data/seed_Mii.bin` (`2,945,804` bytes, `magic = 0x01020304`, `ver = 1`, `saveDataOff = 0x1d98`) y `data/seed_Map.bin` (`192,576` bytes, `magic = 0x01020304`, `ver = 1`, `saveDataOff = 0x0a98`).

- **Decision**: Implementar un lector binario acotado e inmutable en `include/mii_roster.h` / `source/mii_roster.cpp` que valide la cabecera (`>= 0x20`, magic `0x01020304`, `saveDataOff` alineado a 8 bytes), centinelas de tipo (`key == 0`, `type < 33`) y offsets/conteos del heap (igual que `source/player_profile.cpp`), sin modificar el búfer de entrada.
- **Campos verificados empíricamente en `data/seed_Mii.bin` (`count` y `DataType` exactos)**:

| Sección | Campo | Hash Murmur3 x86_32 | Tipo verificado | Conteo |
| --- | --- | --- | --- | --- |
| Identidad / Ocupación | `Mii.Name.Name` | `0x2499bfdau` | `DT_WStr32Array (29)` | `70` |
| Identidad / Ocupación | `Mii.CharInfoEx` | `0x881ca27au` | `DT_BinArray (19)` | `70` |
| Estadísticas (16) | `Mii.MiiMisc.SatisfyInfo.Level` | `0x9999b7d9u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.MiiMisc.SatisfyInfo.Meter` | `0x197237e7u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.Belongings.Money` | `0xf334fd2eu` | `DT_UIntArray (21)` | `70` |
| Estadísticas | `Mii.MiiMisc.BirthdayInfo.Day` | `0xab8ae08bu` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.MiiMisc.BirthdayInfo.Month` | `0x6cf484f4u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.MiiMisc.BirthdayInfo.Year` | `0x5d7d3f45u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.MiiMisc.BirthdayInfo.DirectAge` | `0x2545e583u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.CharacterParam.Activeness` | `0x25b48224u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.CharacterParam.Audaciousness` | `0xcd8dbaf8u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.CharacterParam.Commonsense` | `0x607ba160u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.CharacterParam.Gaiety` | `0x68e1134eu` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.CharacterParam.Sociability` | `0x43cd364fu` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.MiiMisc.BondInfo.Meter` | `0xd0882636u` | `DT_IntArray (3)` | `70` |
| Estadísticas | `Mii.Feeling.Type` | `0x176c29ebu` | `DT_EnumArray (7)` | `70` |
| Estadísticas | `Mii.MiiMisc.EatInfo.EatFullness` | `0xef25b456u` | `DT_IntArray (3)` | `70` |
| Palabras (12 × 70) | `Mii.MiiMisc.WordInfo.WordArray.WordKind` | `0x5f7bfcc8u` | `DT_EnumArray (7)` | `840` |
| Palabras | `Mii.MiiMisc.WordInfo.WordArray.WordText` | `0x09ef8656u` | `DT_WStr64Array (31)` | `840` |
| Palabras | `Mii.MiiMisc.WordInfo.WordArray.WordHowToCall` | `0x0b2a261eu` | `DT_WStr64Array (31)` | `840` |
| Relaciones (2415 pares) | `RelationId.Id_a` | `0xf7420afbu` | `DT_IntArray (3)` | `2415` |
| Relaciones | `RelationId.Id_b` | `0x4071f71cu` | `DT_IntArray (3)` | `2415` |
| Relaciones (4830 dir.) | `Relation.DirectionalInfo.BaseRelationType` | `0x8b41897eu` | `DT_EnumArray (7)` | `4830` |
| Relaciones | `Relation.DirectionalInfo.Meter` | `0x42c2fc2fu` | `DT_IntArray (3)` | `4830` |
| Relaciones | `Relation.Info.IsFight` | `0xc2db3a72u` | `DT_BoolArray (1)` | `2415` |
| Relaciones | `Relation.SinceTimestamp` | `0x1a892e50u` | `DT_UInt64Array (25)` | `2415` |
| Pertenencias (8 prendas + Coord + 12 Pocket) | `BL_WORN_DEFS[0..7]` (`kh` / `ch`) | Ver `main.cpp:7367-7375` | `DT_UIntArray (21)` (o `EnumArray`) / `DT_IntArray (3)` | `70` |
| Pertenencias | `BL_COORD_KH` / `BL_COORD_CH` | `0xd5ee521au` / `0x763c97ccu` | `DT_UIntArray (21)` / `DT_IntArray (3)` | `70` |
| Pertenencias | `BL_H_GOODS_ID` (Pocket 12 × 70) | `0xe3d6005eu` | `DT_UIntArray (21)` | `840` |
| Hábitos (74 × 3) | `Mii.Belongings.HabitOwnInfo.<name>.{IsOwn,IsChecked,State}` | Dinámico por `HABITS[i].name` | `BoolArray (1)` / `BoolArray (1)` / `EnumArray (7)` | `70` |
| Vivienda (`Mii.sav`) | `Mii.Location.HouseMapId` / `Mii.Location.RoomIndex` | `0xb8ab3cacu` / `0xfc296601u` | `DT_IntArray (3)` | `70` |
| Vivienda (`Map.sav` opcional) | `House.MapId` / `House.RoommateGroupName` | `0x0b5276e9u` / `0x0d96409eu` | `DT_IntArray (3)` / `DT_WStr32Array (29)` | `70` |

- **Regla de ocupación de plaza (`0..69`)**:
  - **Decision**: Una plaza `idx` (`0..69`, mostrada como plaza `idx + 1`) está ocupada si `Mii.Name.Name[idx]` tiene al menos un code unit UTF-16LE distinto de `0` **y** (si `Mii.CharInfoEx` está presente con índice válido) el payload de `Mii.CharInfoEx[idx]` contiene al menos un byte distinto de `0`.
  - **Rationale**: Evita el frágil `sum == 152` de `source/mii_manager.cpp` (que depende de la longitud de prefijo `0x98`), trata de forma correcta las plazas recién borradas (nombre vacío) y soporta fixtures sintéticos con o sin `CharInfoEx`.
- **Decodificación UTF-16LE**: Reutilizar la decodificación con soporte completo de pares sustitutos (`0xD800..0xDBFF` + `0xDC00..0xDFFF`) y reemplazo `U+FFFD` con bandera de advertencia, tanto para `WStr32Array` (`64` bytes) como para `WStr64Array` (`128` bytes).

---

## 3. Reutilización de catálogos existentes

- **Decision**: Reutilizar directamente los encabezados generados ya presentes en `include/`:
  - [include/belongings_data.h](../../include/belongings_data.h): `BL_CLOTH_ITEMS`, `BL_COORD_ITEMS`, `BL_GOODS_ITEMS` (prendas, conjuntos y objetos de bolsillo).
  - [include/habits_data.h](../../include/habits_data.h): `HABITS` (`74` hábitos) y `HABIT_CAT_LABEL` (`9` categorías).
  - [include/rel_tier_labels.h](../../include/rel_tier_labels.h): tablas `REL_TIER_*` y umbrales `[11, 41, 81, 120, 160, 190]` para etiquetas de rango de relación.
- **Rationale**: Mantiene consistencia exacta con el juego y el editor legado sin duplicar decenas de miles de líneas de tablas estáticas ni agregar dependencias externas. Las etiquetas de interfaz, nombres de sección, estados de ánimo, tipos de frase, tipos de relación, categorías de hábitos y avisos se localizan en `apps/borealis/resources/i18n/{en-US,es,es-419}/app.json`.
- **Alternatives considered**: Duplicar los catálogos dentro de `source/mii_roster.cpp` (rechazado por mantenimiento y duplicación innecesaria).

---

## 4. Arquitectura de navegación jerárquica en Borealis (sin subpestañas)

- **Decision**: Implementar 3 niveles limpios sobre `navigation::push` / `navigation::pop`:
  1. **Raíz de la pestaña `Mii` (índice `2` en `app.cpp`) — `MiiBrowserScreen`** (`brls::ScrollingFrame`): encabezado `app/mii`, nota de solo lectura, control `Actualizar / Reintentar`, etiqueta de estado (incluyendo estado explícito de isla vacía `0` residentes), lista desplazable de residentes ocupados (`brls::DetailCell` con nombre, plaza y resumen `Nivel N · Estado de ánimo`) y botón `Volver a la lista de secciones`.
  2. **Detalle del residente — `MiiDetailScreen`** (`brls::AppletFrame` empujado con `navigation::push`): encabezado con nombre del residente y plaza, botón estable `Volver` (mismo patrón que `TextureDetailScreen` para entrada segura de foco), nota de solo lectura y 7 filas `brls::DetailCell` enfocables para cada sección (`Estadísticas`, `Palabras`, `Relaciones`, `Pertenencias`, `Hábitos`, `Vivienda`, `Resumen social`) con un resumen compacto en el texto de detalle (ej. `Nivel 5`, `3 / 12`, `8 vínculos`, `12 / 74`, `Casa #1 · Hab. 1`).
  3. **Vista de sección — `MiiSectionScreen`** (`brls::AppletFrame` empujado con `navigation::push`): encabezado con `<Nombre del residente> — <Sección>`, botón estable `Volver`, lista desplazable de solo lectura con filas `brls::DetailCell` y etiquetas multilínea indentadas (`setSingleLine(false)`) cuando haya metadatos secundarios (pronunciación de frases, rango/pelea/fecha de relación, compañeros/conflicto de vivienda, desglose social).
- **Aislamiento de cuenta y restauración de foco**:
  - `MiiBrowserScreen` mantiene `std::shared_ptr<const MiiRoster::MiiRosterSummary>` inmutable (snapshot de solo lectura). Al abrir `MiiDetailScreen` o `MiiSectionScreen`, pasan el snapshot y el índice de residente.
  - Si el usuario cambia de cuenta desde el encabezado global (`Y`) o se activa `setProfile`, `MiiBrowserScreen` incrementa la generación, descarta cualquier lectura pendiente y limpia el estado previo; el `ProfileHeader` global está en el marco raíz, mientras que las pantallas hijas usan el modelo estándar de `B` / `Plus` (Save/Exit) igual que `TextureDetailScreen`.
  - Borealis's focus stack + `navigation::push`/`pop` restauran automáticamente la fila de sección al salir de `MiiSectionScreen` y la fila de residente + `scrollOffset` al salir de `MiiDetailScreen`.
- **Alternatives considered**:
  - Replicar las 7 subpestañas horizontales con `L`/`R` o `ZL`/`ZR`: **rechazado** por violar Constitución §V y `docs/UI_GUIDELINES.md` ("Do not introduce new subtabs").
  - Volcar las 7 secciones en una sola lista gigante por residente: **rechazado** porque 16 estadísticas + 12 frases + hasta 69 relaciones + 21 pertenencias + 74 hábitos + vivienda + social excedería ~200 filas por residente y haría impracticable la navegación con mando.
