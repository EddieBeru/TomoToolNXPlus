# Data Model: Consulta de datos reales del jugador e isla (M1)

Modelo lógico de diseño en memoria para el dominio `PlayerProfile` y el controlador `PlayerBrowserContext`. No introduce persistencia en disco ni mutación de partidas. Las decisiones binarias y hashes provienen de [research.md](research.md).

## Entidades de dominio (`include/player_profile.h`)

### `PlayerAttributeId`

Identificador estable de los 15 atributos en orden canónico de presentación:

1. `Name` (`0`)
2. `Island` (`1`)
3. `Money` (`2`)
4. `Currency` (`3`)
5. `BootCount` (`4`)
6. `SkinTone` (`5`)
7. `BdayDay` (`6`)
8. `BdayMonth` (`7`)
9. `BdayYear` (`8`)
10. `IslandSize` (`9`)
11. `FountainLv` (`10`)
12. `Wishes` (`11`)
13. `Region` (`12`)
14. `TTPenalty` (`13`)
15. `Donation` (`14`)

### `EnumResolution`

Resultado normalizado de un campo enumerado (`Currency`, `Region`, `NameRegionLanguageID`, `IslandNameRegionLanguageID`):

- `rawHash` (`uint32_t`): valor de 32 bits almacenado en `.sav`.
- `present` (`bool`): indica si la entrada existía en la tabla índice.
- `known` (`bool`): `true` si coincide con una entrada del catálogo oficial.
- `code` (`std::string`): código interno estable (`"EUR"`, `"Japan"`, `"EUes"`, etc.) o vacío si es desconocido.
- `symbol` (`std::string`): símbolo de moneda (`"¥"`, `"$"`, `"€"`, etc.) para `Currency`; vacío en los demás casos.

### `WishesProgress`

Relación entre los deseos realizados y la tabla de deseos liberados (`Liberation.WishInfo.*`):

- `countPresent` (`bool`): si existe el escalar `0xa32f7e47u`.
- `wishesMade` (`uint32_t`): valor del contador de deseos realizados.
- `liberatedAvailable` (`bool`): `true` si existen tanto `Liberation.WishInfo.WishIdValue` (`DT_UIntArray`) como `Liberation.WishInfo.IsLiberated` (`DT_BoolArray`) con longitud válida.
- `liberatedCount` (`int`): cantidad de elementos `true` en `IsLiberated`.
- `totalCatalogWishes` (`int`): `268` (`WishesData::WISH_COUNT` de `include/wishes_data.h`).
- `tableWarning` (`bool`): `true` si la tabla falta, su longitud es inconsistente o truncada respecto a `WishIdValue`.

### `PlayerProfileSummary`

Conjunto validado y estrictamente de solo lectura de los datos extraídos de `/Player.sav`:

- **Identidad del jugador e isla**:
  - `playerName` (`std::string`), `playerNamePresent` (`bool`), `playerNameTextWarning` (`bool`)
  - `howToCallName` (`std::string`), `howToCallNamePresent` (`bool`), `howToCallNameTextWarning` (`bool`)
  - `nameLanguage` (`EnumResolution`)
  - `islandName` (`std::string`), `islandNamePresent` (`bool`), `islandNameTextWarning` (`bool`)
  - `howToCallIslandName` (`std::string`), `howToCallIslandNamePresent` (`bool`), `howToCallIslandNameTextWarning` (`bool`)
  - `islandLanguage` (`EnumResolution`)
- **Economía y juego**:
  - `moneyCents` (`uint32_t`), `moneyPresent` (`bool`)
  - `currency` (`EnumResolution`)
  - `bootCount` (`uint32_t`), `bootCountPresent` (`bool`)
  - `skinToneIndex` (`uint32_t`), `skinTonePresent` (`bool`) — válido en `0..5`
- **Cumpleaños**:
  - `bdayDay` (`int32_t`), `bdayDayPresent` (`bool`) — válido en `1..31`
  - `bdayMonth` (`int32_t`), `bdayMonthPresent` (`bool`) — válido en `1..12`
  - `bdayYear` (`int32_t`), `bdayYearPresent` (`bool`)
- **Isla y liberación**:
  - `islandSizeRaw` (`int32_t`), `islandSizePresent` (`bool`) — presets válidos `1..4` (`50x36`, `70x50`, `90x64`, `120x80`)
  - `fountainLevel` (`uint32_t`), `fountainLevelPresent` (`bool`)
  - `wishes` (`WishesProgress`)
  - `region` (`EnumResolution`)
- **Marcas temporales (`DT_UInt64`)**:
  - `ttPenaltyRaw` (`uint64_t`), `ttPenaltyPresent` (`bool`) — `0` limpia, `!= 0` activa
  - `lastDonationSeconds` (`uint64_t`), `lastDonationPresent` (`bool`) — `0` nunca recogida, `> 0` POSIX segundos
- **Metadatos estructurales**:
  - `saveVersion` (`uint32_t`)
  - `hasFieldWarnings` (`bool`): indica si algún campo opcional faltó, tuvo UTF-16 inválido o cayó fuera de catálogo/rango sin impedir el perfil completo.

### `PlayerAttributeView`

Unidad lista para presentación generada por el formateador puro `FormatAttributes(...)` a partir de `PlayerProfileSummary`, un resolvedor i18n inyectado y un reloj explícito (`int64_t referenceUnixSeconds`):

- `id` (`PlayerAttributeId`)
- `focusKey` (`std::string`): clave estable (`"attr_0"` .. `"attr_14"`) usada para conservar el foco tras recargas y cambios de pestaña.
- `title` (`std::string`): etiqueta localizada del atributo.
- `primaryValue` (`std::string`): valor principal formateado para el `DetailCell` (o marcador localizado de ausencia/desconocido/vacío).
- `secondaryMetadata` (`std::string`): línea o líneas de contexto adicional (pronunciación e idioma, monto bruto, símbolo/nombre completo o hash hex, deseos liberados `X / 268`, índice de tono, dimensiones de isla, estado de penalización temporal, días transcurridos desde la donación o aviso de fecha futura).
- `description` (`std::string`): descripción localizada para el usuario.
- `hasWarning` (`bool`): `true` si el campo usa reserva por ausencia, UTF-16 dañado, código fuera de catálogo, fecha futura o valor fuera de rango.

### `PlayerReadError` y mapeo de errores

Reutiliza y extiende la taxonomía de errores de lectura sin exponer bytes privados del guardado ni trazas de consola personal:

- `None`: sin error.
- `NoAccountSelected`: aún no se elige perfil desde el encabezado global (`Unselected`).
- `NoSave`: la cuenta no tiene guardado del título `0x010051F0207B2000`.
- `AccountMetadataFailed` / `ReadOnlyOpenFailed`: fallo de lectura de metadatos o apertura de solo lectura (ej. juego abierto o permisos insuficientes).
- `ShortRead`: lectura incompleta de `/Player.sav` o cambio de tamaño durante la lectura.
- `FileTooLarge`: `/Player.sav` excede `MaxPlayerBytes` (`8 MiB`).
- `InvalidHeader` / `CorruptIndex` / `TruncatedPayload`: magic distinto de `0x01020304`, `saveDataOff` fuera de límites, tipos desconocidos (`>= 33`), hashes duplicados conflictivos o payloads fuera de rango.
- `MissingRequiredField` / `TypeMismatch`: ausencia o tipo incompatible en los campos estructurales mínimos de jugador.
- `Cancelled`: solicitud cancelada por cambio de cuenta, cambio de pestaña o salida.

## Controlador y máquina de estados (`PlayerBrowserContext` / `PlayerBrowserModel`)

### Propiedades del contexto en sesión

- `hasAccount` (`bool`)
- `account` (`TextureBrowser::Account`): cuenta activa asociada a la lectura vigente; el aislamiento usa `AccountKey` opaco.
- `generation` (`uint64_t`): contador monotónico que invalida entregas asincrónicas de cuentas o recargas anteriores.
- `state` (`PlayerBrowserState`): `{ Unselected, Loading, Suspended, Ready, Error }`.
- `summary` (`PlayerProfileSummary`): último resumen válido de la cuenta activa (`Ready`); se limpia inmediatamente al confirmar cambio a otra cuenta (`FR-007`).
- `lastError` (`PlayerReadError`) + `platformCode` (`uint32_t`).
- `selectedAttrIndex` (`int`, `-1` si foco en control de acción) y `focusTarget` (`std::string`: `"refresh"`, `"tabs"` o `"attr_N"`).
- `scrollOffset` (`float`): desplazamiento vertical del `ScrollingFrame`.

### Transiciones de estado

| Estado actual | Evento | Siguiente estado | Efecto sobre datos, generación y contexto |
| --- | --- | --- | --- |
| `Unselected` | `selectAccount(A)` | `Loading` | Asigna `A`, `++generation`, limpia `summary`/`lastError`, reinicia `focusTarget`/`scrollOffset = 0`. |
| `Loading` | `accept(ok, gen)` vigente | `Ready` | Guarda `summary`; si `focusTarget` era `"attr_N"`, mantiene índice; si venía vacío, fija foco inicial accesible. |
| `Loading` | `accept(err, gen)` vigente | `Error` | Limpia `summary`, almacena `lastError`, desbloquea `Actualizar / Reintentar`. |
| `Loading` | `accept(..., oldGen)` | *(sin cambio)* | Entrega obsoleta descartada sin alterar estado ni vista. |
| `Loading` | `suspend()` (salir de pestaña Jugador) | `Suspended` | `++generation`, cancela entrega pendiente, conserva `account`, `focusTarget` y `scrollOffset` (`FR-009`). |
| `Suspended` | `resume()` (volver a pestaña Jugador) | `Loading` | `++generation`, lanza una única lectura vigente conservando contexto. |
| `Ready` / `Error` | `retry()` (`Actualizar / Reintentar`) | `Loading` | `++generation`, conserva temporalmente `account`, `focusTarget` y `scrollOffset` para restaurar foco al concluir. |
| `Ready` / `Error` / `Loading` / `Suspended` | `selectAccount(B)` (`B != A`) | `Loading` | `++generation`, limpia `summary` y error previos de `A` de inmediato (`FR-007`), reinicia `scrollOffset = 0`, inicia lectura de `B`. |
| Cualquier estado | `selectAccount(A)` (`A == A` o cancelación de selector) | *(sin cambio)* | Actualiza nickname si cambió metadata de perfil sin limpiar ni recargar (`FR-007` escenario 3). |
| `Ready` / `Error` | `leaveTab()` / `openGlobalDialog()` | *(sin cambio)* | Conserva estado, `summary`, atributo enfocado y `scrollOffset` (`FR-006`). |

## Invariantes de integridad y aislamiento

1. `PlayerProfile::Parse` opera sobre `const std::vector<uint8_t>&` o `const uint8_t* + size_t` sin mutar el búfer de entrada.
2. Ninguna clase o función del modelo `PlayerProfile` ni del controlador `PlayerBrowserModel` posee métodos de escritura, serialización ni referencias a `SaveMount`, `SaveWorkflow`, `Backup` o `SaveEditor::Save`.
3. Cambiar de cuenta limpia en memoria el `PlayerProfileSummary` previo antes de iniciar la nueva lectura; los resultados de generaciones previas no pueden repoblar la vista.
