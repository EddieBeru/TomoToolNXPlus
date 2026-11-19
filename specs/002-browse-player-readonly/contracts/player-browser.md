# Contract: Consulta de datos del jugador e isla en solo lectura (M1)

Contrato de diseño para `FR-001`–`FR-013` y `SC-001`–`SC-007`. No agrega rutas de escritura en partidas, no modifica WebUI ni altera el navegador de Texturas o Ajustes de texturas.
Modelo: [data-model.md](../data-model.md). Investigación binaria y de UI: [research.md](../research.md).

---

## 1. Contrato de dominio y parser (`include/player_profile.h`)

### Operaciones públicas

1. **`Parse(const std::vector<uint8_t>& bytes)` → `ParseResult` (`PlayerProfileSummary`, `PlayerReadError`)**
   - Entrada inmutable (`const std::vector<uint8_t>&`), sin dependencias de I/O, libnx, SDL ni Borealis.
   - Valida:
     - `bytes.size() >= 0x20` y `bytes.size() <= TextureBrowser::MaxPlayerBytes` (`8 MiB`).
     - Cabecera: magic `0x01020304`, `saveDataOff >= 0x20`, `saveDataOff <= bytes.size()`, `(saveDataOff - 0x20) % 8 == 0`.
     - Tabla índice: cada centinela (`hash == 0`) tiene tipo `< 33`; ninguna entrada aparece antes de un centinela; entradas inline mantienen su valor `slot`; entradas de heap verifican `slot >= saveDataOff` y payload completo dentro de `bytes.size()` sin overflow.
     - Ausencia o tipo incompatible de los campos obligatorios de identidad (`Player.Name`, `Player.IslandName`, `Player.Money`, `Player.Currency`) produce `MissingRequiredField` o `TypeMismatch`.
     - Campos opcionales o tablas complementarias (`HowToCall*`, `NameRegionLanguageID`, `IslandNameRegionLanguageID`, `Liberation.WishInfo.*`) degradan localmente con marcas de ausencia/advertencia sin descartar el perfil válido.
2. **`LoadFromReadSource(TextureBrowser::ReadSource& source, TextureBrowser::AccountKey account, const TextureBrowser::Cancel& cancel)` → `LoadResult`**
   - Llama `source.open(account, cancel)` y, si tiene éxito, `source.read("/Player.sav", TextureBrowser::MaxPlayerBytes, cancel)`.
   - Mapea errores de `TextureBrowser::Error` (`NoSave`, `Metadata`, `Open`, `ShortRead`, `Limit`, `InvalidData`, `Cancelled`) hacia `PlayerReadError` y preserva el código técnico opcional `platformCode`.
   - Cierra el adaptador (`source.close()`) en todas las salidas antes de regresar al invocador.
3. **`FormatAttributes(const PlayerProfileSummary& summary, const Localizer& tr, int64_t referenceUnixSeconds)` → `std::array<PlayerAttributeView, 15>`**
   - Función pura y determinista (acepta reloj explícito para testeo host sin depender de `time(nullptr)`).
   - Emite siempre las 15 filas en orden canónico (`0..14`), con títulos, valores principales, metadatos secundarios, descripciones y avisos localizables.
4. **Controlador de sesión (`PlayerBrowserModel`)**:
   - Métodos: `select(Account)`, `refresh()`, `accept(generation, LoadResult)`, `suspend()`, `resume()`.
   - Garantiza que solo resultados cuya `generation` coincida con `model.generation` y cuyo `AccountKey` coincida con la cuenta activa sean aceptados.

### Garantías negativas de seguridad

- El dominio y el adaptador de lectura **no** exponen métodos `Save`, `Write`, `Commit`, `Repair`, `Backup`, `Set*` ni `Restore`.
- Ningún código enlazado por `TomoToolNXPlusBorealis` bajo la ruta de Jugador ni Texturas llama a `SaveMount`, `SaveWorkflow`, `Backup`, `MiiManager` ni `SaveEditor::Save`.

---

## 2. Contrato de sesión asíncrona y publicación en UI

- `PlayerBrowserScreen` reutiliza `TextureBrowser::Session` compartida con `TextureBrowserScreen` (worker serializado único en la aplicación).
- Antes de lanzar una lectura:
  - El modelo entra en `Loading` con nueva `generation`.
  - Se deshabilita y vuelve no enfocable el botón `Actualizar / Reintentar` para impedir solicitudes duplicadas (`US3.2`), manteniendo alcanzables el encabezado de perfil, `Volver a la lista de secciones`, `B` y navegación de pestañas/salida (`Plus`).
- Si el usuario cambia de pestaña mientras está en `Loading`:
  - `Navigation::activate` llama `playerBrowser->deactivateBrowser()`, que invoca `session->cancel()` y `model.suspend()` (`Loading` → `Suspended`).
  - Cualquier finalización tardía del worker comprueba `weak_ptr<Life>`, `active`, y `generation == model.generation` en el hilo principal y se descarta silenciosamente (`FR-009`).
  - Al volver a la pestaña Jugador (`activateBrowser()`), si estaba `Suspended`, ejecuta `model.resume()` y lanza exactamente una lectura vigente conservando la cuenta activa y el contexto (`scroll` y `focusTarget`).
  - Si la pestaña estaba en `Ready`, `Error` o `Unselected` para la cuenta activa, volver a ella restaura inmediatamente el foco y desplazamiento sin relanzar lecturas automáticas (`FR-006`).
- Si el usuario confirma una cuenta diferente mediante el selector global (`Y`):
  - `Navigation::changeProfile()` llama `playerBrowser->setProfile(newSelection)`, lo cual cancela lecturas activas, limpia inmediatamente los atributos de la cuenta anterior (`summary` vacío) y marca carga pendiente.
  - Si el usuario cancela el selector o falla la selección, `Navigation::changeProfile()` mantiene intactos cuenta, vista, foco y desplazamiento (`FR-007`).

---

## 3. Contrato de pantalla Borealis (`PlayerBrowserScreen`)

### Estructura y orden del contenido

1. `brls::Header` con `app/player` (`"Jugador"` / `"Player"`).
2. `brls::Label` con aviso explícito de modo solo lectura (`app/player_note`).
3. `brls::DetailCell` `"Actualizar / Reintentar"` (`app/browser_refresh`). Deshabilitado únicamente mientras está en `Loading`.
4. `brls::Label` de estado (`status`):
   - `Unselected`: `"Elige un perfil desde la barra superior para consultar sus datos de jugador."`
   - `Loading` / `Suspended`: `"Cargando datos del jugador… Puedes cambiar de pestaña."`
   - `Ready`: `"Guardado de solo lectura · 15 atributos"` (más aviso si algún atributo tiene advertencias locales).
   - `Error`: mensaje específico según tabla §4 + acción recomendada.
5. `brls::Box` de atributos (`rowsBox`, poblado únicamente en `Ready`):
   - 15 pares consecutivos `[DetailCell + Label secundario indentado]` en orden `0..14`:
     - `DetailCell`: título localizado a la izquierda, `primaryValue` a la derecha; `onFocusGained` actualiza `focusTarget = "attr_N"` para restaurar foco entre pestañas y recargas; `A` o toque enfoca la fila sin abrir subpantallas ni diálogos de edición (`FR-002`, `FR-004`).
     - `Label` indentado (`brls/listitem/descriptionIndent`, `setSingleLine(false)`): muestra los **metadatos secundarios** seguidos de la **descripción localizada**.
6. `brls::DetailCell` `"Volver a la lista de secciones"` (`app/tabs`), invocando `focusSidebar()`.

### Paridad Mando / Táctil y Reglas de Navegación

- `ZL` / `ZR`: cambian únicamente entre las 5 pestañas principales (`Navigation::tabs`).
- `D-pad` / stick izquierdo: recorren linealmente `Actualizar / Reintentar`, los 15 `DetailCell` de atributos y `Volver a la lista de secciones` (`ScrollingFrame` desplaza automáticamente para mantener la fila enfocada visible sin recorte).
- `B`: devuelve el foco al elemento activo de la barra lateral (`navigation::back`) desde cualquier punto de la pestaña.
- `Plus`: abre `exit_menu` global sin bloquear ni ofrecer guardado de partida.
- Toque: tocar cualquier `DetailCell` le da foco sin disparar acciones no solicitadas sobre los atributos; arrastrar el `ScrollingFrame` no activa ni `Actualizar` ni `Volver`.
- No se agregan subpestañas ni nuevos atajos `X`/`Y`/`Minus` específicos de la pestaña Jugador.

---

## 4. Matriz de errores y recuperación

| Condición (`PlayerReadError`) | Origen / Trigger | Mensaje usuario esperado (resumido) | Recuperación ofrecida |
| --- | --- | --- | --- |
| `NoAccountSelected` | Inicio sin perfil válido seleccionado | Invitación a elegir perfil desde barra superior (`Y`) | Selector global en barra superior |
| `NoSave` | Cuenta válida sin save `0x010051F0207B2000` | Cuenta sin partida guardada del juego | `Actualizar / Reintentar` o cambiar perfil (`Y`) |
| `AccountMetadataFailed` / `ReadOnlyOpenFailed` | Error en `fsOpenSaveDataInfoReader` o `fsOpen_SaveDataReadOnly` (ej. juego abierto) | No se pudo abrir en solo lectura; cierra el juego y reintenta | `Actualizar / Reintentar` o cambiar perfil (`Y`) |
| `ShortRead` | Error/cambio de tamaño durante `fsFileRead` | Lectura incompleta de `Player.sav` | `Actualizar / Reintentar` |
| `FileTooLarge` | Archivo `> 8 MiB` | `Player.sav` excede el límite seguro de 8 MiB | Cambiar perfil (`Y`) o volver |
| `InvalidHeader` / `CorruptIndex` / `TruncatedPayload` / `MissingRequiredField` / `TypeMismatch` | Archivo vacío, magic inválido, offsets corruptos o ausencia de identidad mínima | Datos del jugador dañados o incompatibles (con motivo tipado) | `Actualizar / Reintentar` o cambiar perfil (`Y`), sin reparar ni sobrescribir |

---

## 5. Matriz de trazabilidad (`FR` / `SC` → diseño y verificación)

| Requisito / Criterio | Componente que lo implementa | Verificación principal |
| --- | --- | --- |
| `FR-001`, `FR-002`, `SC-001` | `PlayerProfile::Parse`, `FormatAttributes`, `PlayerBrowserScreen` | `tests/player_profile_test.cpp` (2 cuentas sintéticas A/B + `data/seed_Player.bin`), Switch física |
| `FR-003`, `SC-004` | `PlayerProfileSummary` warnings + reservas de catálogo/rango | `tests/player_profile_test.cpp`, `tests/player_browser_context_test.cpp` |
| `FR-004`, `FR-005`, `SC-002` | `PlayerBrowserScreen` + `navigation.cpp` | `tests/borealis_ui_screens_test.cpp` (mando y touch), Switch física |
| `FR-006`, `FR-009`, `SC-003` | `PlayerBrowserModel` + `Navigation` (`activate`/`deactivate`) | `tests/player_browser_context_test.cpp`, `tests/borealis_ui_screens_test.cpp` |
| `FR-007`, `FR-008`, `SC-004` | `Navigation::changeProfile()` + `PlayerBrowserModel::select()` + tabla errores | `tests/player_browser_context_test.cpp`, `tests/borealis_ui_screens_test.cpp` |
| `FR-010`, `SC-006` | `TextureBrowser::ReadSource` (`fsOpen_SaveDataReadOnly`) + cero escritores enlazados | Auditoría `nm -C` en ELF + spy de `ReadSource` sin writes en tests |
| `FR-011`, `SC-005` | Límites de `8 MiB`, validación aritmética y worker serializado | `tests/player_profile_test.cpp` (corrupción/truncado/overflow) + ASan/UBSan + Switch física |
| `FR-012`, `SC-007` | Claves `en-US`, `es`, `es-419` + fuente estándar (`configurePrimaryFont`) | Script/revisión de i18n + verificación visual 720p/docked/temas en Switch |
| `FR-013` | Reutilización aislada de `Session` sin alterar `TextureBrowserScreen` / `TextureSettings` | Todas las 10 suites previas de `tests/run_texture_browser.sh` + `make && make borealis` |
