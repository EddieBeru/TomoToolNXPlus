# Contracts: Consulta de residentes Mii en modo solo lectura (M2)

**Feature**: `specs/003-browse-mii-readonly/` (`M2`) | **Date**: 2026-11-22

---

## 1. Contrato del módulo de dominio puro (`include/mii_roster.h`)

### Operaciones permitidas (estrictamente solo lectura)
- `uint32_t Hash(const char* name);`
- `Result<MiiRosterSummary> Parse(const std::vector<uint8_t>& miiBytes, const std::vector<uint8_t>* mapBytes = nullptr);`
  - **Precondiciones**: `miiBytes` no se modifica jamás (referencia `const`). Si `mapBytes != nullptr` y es inválido/corrupto, **no** invalida `miiBytes`: marca `mapNamesWarning = true` y degrada nombres de casas a reserva numerada.
  - **Validación estructural de `miiBytes`**:
    - `size >= 0x20` y `size <= TextureBrowser::MaxMiiBytes` (`8 MiB`), magic `0x01020304`, `saveDataOff >= 0x20`, `saveDataOff <= size`, `(saveDataOff - 0x20) % 8 == 0`.
    - Rechaza centinelas con `type >= 33`, entradas antes del primer centinela, offsets de heap `< saveDataOff` o desbordados, conteos `> MaxArrayCount` y campos duplicados conflictivos.
    - Campo obligatorio mínimo: `Mii.Name.Name` (`0x2499bfdau`, `DT_WStr32Array`). Su ausencia devuelve `MissingRequiredField`; tipo incompatible devuelve `TypeMismatch`.
- `Result<MiiRosterSummary> LoadFromReadSource(TextureBrowser::ReadSource& source, TextureBrowser::AccountKey account, const TextureBrowser::Cancel& cancel);`
  - Abre el guardado en modo solo lectura (`source.open(account, cancel)`), garantiza `source.close()` en toda ruta de salida (RAII), lee `"/Mii.sav"` con límite `TextureBrowser::MaxMiiBytes` y realiza una lectura **best-effort** de `"/Map.sav"` con límite `TextureBrowser::MaxMapBytes`.
  - Mapea `TextureBrowser::Error` a `MiiReadError` de forma exhaustiva (`NoSave`, `Metadata` → `AccountMetadataFailed`, `Open` → `ReadOnlyOpenFailed`, `ShortRead`, `Limit` → `FileTooLarge`, `Cancelled`).
- Formateadores puros con `Localizer = std::function<std::string(const std::string&)>`:
  - `std::string FormatResidentSummary(const MiiResident& resident, const Localizer& tr);`
  - `std::array<MiiSectionOverview, SectionCount> FormatSectionOverviews(const MiiResident& resident, const Localizer& tr);`
  - `std::vector<MiiRowView> FormatSectionRows(const MiiResident& resident, MiiSectionId section, const Localizer& tr);`

### Garantías negativas de solo lectura (`FR-016`, `SC-006`)
- `include/mii_roster.h` y `source/mii_roster.cpp` **NO** exponen ni enlazan `Save`, `Write`, `Commit`, `Repair`, `Backup`, `Restore`, `CreateBlankMii`, `DeleteMii`, `ImportMii`, `ExportMii`, `SaveMount`, `SaveWorkflow`, `MiiManager` ni `SaveEditor::Save`.
- Auditoría obligatoria del binario final `build/borealis/TomoToolNXPlusBorealis.elf` con `aarch64-none-elf-nm -C`: `0` coincidencias para `SaveMount|SaveWorkflow|Backup|MiiManager|SaveEditor::Save`.

---

## 2. Contrato de extensión de `TextureBrowser::ReadSource`

- `include/texture_browser.h`:
  - Añadir `constexpr size_t MaxMiiBytes = 8 * 1024 * 1024;` y `constexpr size_t MaxMapBytes = 4 * 1024 * 1024;`.
- `apps/borealis/source/texture_read_source.cpp`:
  - `SwitchSource::open()` y `SwitchSource::enumerate()` incluyen `"/Player.sav"`, `"/Mii.sav"` y `"/Map.sav"` en el conjunto `allowed`.
  - `SwitchSource::read()` aplica el límite máximo específico por archivo (`MaxPlayerBytes`, `MaxMiiBytes`, `MaxMapBytes`, `MaxImageBytes`) y mantiene exclusivamente `FsOpenMode_Read` sobre `fsOpen_SaveDataReadOnly`.

---

## 3. Contrato de interfaz Borealis y navegación

- **Pestaña raíz (`MiiBrowserScreen`, tab índice `2` en `apps/borealis/source/app.cpp`)**:
  - Reemplaza `addSample` de la pestaña `Mii`.
  - Recibe `setProfile(selection)`, `activateBrowser()` y `deactivateBrowser()` igual que `TextureBrowserScreen` y `PlayerBrowserScreen`.
  - Estados diferenciados: `Unselected`, `Loading` (sin porcentajes inventados; deshabilita `Actualizar / Reintentar`), `Suspended`, `Empty` (0 residentes, distinto de error), `Ready` (1–70 residentes) y `Error` (motivo recuperable).
  - Controles: `Actualizar / Reintentar`, lista de residentes (`focusKey = "mii_<slotNumber>"`), `Volver a la lista de secciones`.
- **Pantalla hija nivel 1 (`MiiDetailScreen`)**:
  - Empujada mediante `navigation::push(screen)` al activar un residente con `A` o toque.
  - Entrada estable de foco: botón `Volver` al inicio (igual que `TextureDetailScreen`), seguido de las 7 filas enfocables de sección (`Stats`, `Words`, `Relations`, `Belongings`, `Habits`, `Housing`, `Social`).
  - `B` y el botón `Volver` ejecutan `navigation::pop()` restaurando exactamente el residente y desplazamiento en `MiiBrowserScreen`.
  - `Plus` abre el menú global Guardar/Salir.
- **Pantalla hija nivel 2 (`MiiSectionScreen`)**:
  - Empujada mediante `navigation::push(screen)` al activar cualquiera de las 7 secciones en `MiiDetailScreen`.
  - Muestra el botón estable `Volver` y la lista desplazable de `MiiRowView` (`DetailCell` + `Label` multilínea indentado opcional cuando `secondaryText` no está vacío).
  - `A`/toque sobre una fila informativa mantiene el foco sin abrir más niveles; `B` y `Volver` ejecutan `navigation::pop()` restaurando exactamente la sección enfocada en `MiiDetailScreen`.
- **Cambio de cuenta (`Y`)**:
  - Al confirmar otra cuenta desde el encabezado global, `MiiBrowserScreen::setProfile` limpia los residentes previos, invalida la generación en curso y recarga los residentes de la nueva cuenta. Cancelar `Y` conserva la sesión intacta.

---

## 4. Matriz de trazabilidad `FR` / `SC`

| Requisito / Criterio | Componente responsable | Verificación |
| --- | --- | --- |
| `FR-001`, `SC-001` | `MiiRoster::Parse`, `MiiBrowserScreen` | `tests/mii_roster_test.cpp` (0, 1, dispersos, 70 residentes, A/B, `seed_Mii.bin`) + `ui-screens` |
| `FR-002`, `FR-011`, `SC-002` | `MiiBrowserScreen` → `MiiDetailScreen` → `MiiSectionScreen` + `navigation::push/pop` | `tests/borealis_ui_screens_test.cpp` (mando y toque, sin subtabs) |
| `FR-003`–`FR-009` | `MiiRoster::Parse` + `FormatSectionRows` (7 secciones) | `tests/mii_roster_test.cpp` |
| `FR-010`, `FR-014`, `FR-017`, `SC-004` | Validación acotada + degradación local (`Map.sav` ausente, códigos desconocidos, corrupción, `> 8 MiB`) | `tests/mii_roster_test.cpp` + `tests/mii_browser_context_test.cpp` + `ui-screens` |
| `FR-012`, `FR-013`, `FR-015`, `SC-003`, `SC-005` | `MiiBrowserModel` (generación, `suspend`/`resume`) + pila de foco Borealis | `tests/mii_browser_context_test.cpp` + `ui-screens` (10 ciclos + `Y`/`Plus`) |
| `FR-016`, `SC-006` | `SwitchSource` read-only + spy inmutable + auditoría `nm` | `tests/mii_roster_test.cpp` + `tests/mii_browser_context_test.cpp` + `aarch64-none-elf-nm` |
| `FR-018`, `FR-019`, `SC-007` | `apps/borealis/resources/i18n/{en-US,es,es-419}/app.json` + builds duales | Paridad de claves JSON + `make -j4` + `make -j4 borealis` + checklist Switch físico |
