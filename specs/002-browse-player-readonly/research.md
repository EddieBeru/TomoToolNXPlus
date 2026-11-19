# Research: Consulta de datos reales del jugador e isla en solo lectura (M1)

Hallazgos técnicos e decisiones de diseño para [spec.md](spec.md), alineados con [la constitución](../../.specify/memory/constitution.md) y los módulos existentes de `M0`.

## 1. Fuente de lectura de `/Player.sav` y serialización por worker

- **Decisión**: Reutilizar `TextureBrowser::ReadSource` ([include/texture_browser.h](../../include/texture_browser.h), [apps/borealis/source/texture_read_source.cpp](../../apps/borealis/source/texture_read_source.cpp)) y `TextureBrowser::Session` ([apps/borealis/source/texture_browser_session.hpp](../../apps/borealis/source/texture_browser_session.hpp), [apps/borealis/source/texture_browser_session.cpp](../../apps/borealis/source/texture_browser_session.cpp)) para abrir y leer `/Player.sav` en el worker serializado.
- **Evidencia observada en código**:
  - `SwitchSource::open(AccountKey, Cancel)` consulta `FsSaveDataInfoReader` en `FsSaveDataSpaceId_User` buscando el título `0x010051F0207B2000ULL` y tipo `FsSaveDataType_Account` para la cuenta elegida, abre el filesystem mediante `fsOpen_SaveDataReadOnly(&fs, 0x010051F0207B2000ULL, uid)` y agrega `"/Player.sav"` a `allowed` (sin requerir enumerar `/Ugc` previamente).
  - `SwitchSource::read("/Player.sav", MaxPlayerBytes, cancel)` aplica el tope de `MaxPlayerBytes = 8 * 1024 * 1024` (`8 MiB`), lee en bloques de `ReadChunk = 64 KiB` verificando cancelación entre bloques y comprueba que el tamaño del archivo no haya cambiado entre lecturas.
  - Un único `TextureBrowser::Session` compartido entre Texturas y Jugador garantiza que nunca coexistan dos montajes concurrentes de `savr` en Switch y reutiliza la misma política de generación/cancelación y entrega en el hilo principal (`brls::async`).
  - Importante para `Player`: tras leer `/Player.sav` en el worker, el loader debe cerrar la sesión del adaptador (`source.close()`) antes de entregar el resultado, igual que hace `ReadSource` mediante su destructor / `open()` siguiente, para no mantener handles abiertos mientras el usuario navega otras pestañas.
- **Alternativas rechazadas**:
  - *Enlazar `source/save_editor.cpp` en Borealis*: `source/save_editor.cpp` contiene `SaveEditor::Save` y setters mutables (`SetInt`, `SetWStr32`, etc.); enlazarlo violaría la auditoría de símbolos read-only de `TomoToolNXPlusBorealis.elf` y la compuerta `M4`.
  - *Crear un segundo hilo/worker paralelo*: duplicaría ciclo de vida, colisionaría en la apertura del guardado de la misma cuenta y complicaría la salida limpia desde `ExitMenu`.

## 2. Parser binario puro y acotado para `Player.sav`

- **Decisión**: Implementar un módulo puro en C++17 (`include/player_profile.h` + `source/player_profile.cpp`), sin dependencias de Borealis, SDL, HTTP ni libnx, compilable tanto en pruebas host (`-fno-exceptions -fno-rtti` y con RTTI/excepciones para tests de pantallas Borealis) como en el target Switch de Borealis (`apps/borealis/CMakeLists.txt`).
- **Estructura verificada del formato `.sav`** (verificada en [source/save_editor.cpp](../../source/save_editor.cpp), [include/save_editor.h](../../include/save_editor.h), [source/ugc_names.cpp](../../source/ugc_names.cpp) y [data/seed_Player.bin](../../data/seed_Player.bin)):
  - **Cabecera (`0x20` = 32 bytes)**:
    - `0x00..0x03`: `magic` little-endian `0x01020304` (`04 03 02 01`).
    - `0x04..0x07`: `version` (`uint32_t`, observado `1` en `data/seed_Player.bin`).
    - `0x08..0x0B`: `saveDataOff` (`uint32_t`, fin de la tabla índice e inicio del heap).
  - **Validación estructural de cabecera y tabla índice**:
    - Rechazar con error tipado si `size < 0x20` o `size > 8 * 1024 * 1024`.
    - Rechazar si `magic != 0x01020304`.
    - Rechazar si `saveDataOff < 0x20`, `saveDataOff > size` o `(saveDataOff - 0x20) % 8 != 0`.
  - **Filas índice (`0x20 .. saveDataOff`, 8 bytes cada una: `uint32_t hash`, `uint32_t slot`)**:
    - Fila centinela (`hash == 0`): establece `curType = slot`; rechazar con `CorruptIndex` si `slot >= 33` (`DT_COUNT`).
    - Fila de entrada (`hash != 0`): rechazar si aparece antes de cualquier centinela o si su `hash` relevante aparece duplicado con definición conflictiva.
    - Tipos inline (`DT_Bool = 0`, `DT_Int = 2`, `DT_Float = 4`, `DT_Enum = 6`, `DT_UInt = 20`): `slot` contiene el valor de 32 bits directamente.
    - Tipos en heap utilizados en `Player.sav`: rechazar si `slot < saveDataOff` o si el payload calculado desborda `size` (comprobando aritmética sin overflow de `size_t` / `uint64_t`):
      - `DT_UInt64 = 24`: 8 bytes fijos (`slot + 8 <= size`).
      - `DT_WStr32 = 28`: 64 bytes fijos (32 code units UTF-16LE).
      - `DT_WStr64 = 30`: 128 bytes fijos (64 code units UTF-16LE).
      - `DT_BoolArray = 1`: `uint32_t count` en `slot`, seguido de `max<size_t>(4, ((count + 7) / 8 + 3) & ~3)` bytes (con tope operativo acotado, p. ej. `count <= 65536`, para impedir reservas desmedidas por corrupción).
      - `DT_UIntArray = 21` / `DT_EnumArray = 7` / `DT_IntArray = 3`: `uint32_t count` en `slot`, seguido de `count * 4` bytes (con tope acotado).
      - Los demás tipos heap (`0..32`) también validan `slot >= saveDataOff` y su longitud esperada dentro de `size` siguiendo los tamaños fijos/variables de `source/save_editor.cpp` para detectar archivos truncados o con offsets fuera de rango aunque correspondan a campos no mostrados.
- **Hallazgo empírico clave en `data/seed_Player.bin` vs `source/save_editor.cpp`**:
  - `Player.HowToCallName` (`0xbd8c4da1u`) y `Player.HowToCallIslandName` (`0xc6482c14u`) se almacenan en el guardado como **`DT_WStr64` (tipo `30`, 128 bytes)**, aunque [source/main.cpp](../../source/main.cpp#L7021) intentaba leerlos con `SaveEditor::GetWStr32`, lo que devolvía cadena vacía cuando el tipo era `DT_WStr64`! Nuestro parser acepta tanto `DT_WStr64` como `DT_WStr32` para los campos de pronunciación, corrigiendo la lectura silenciosamente perdida del legado sin alterar el guardado.
  - `BdayYear` (`0x11996629u`) figura como `isInt = true` en `PLAYER_FIELDS` ([source/main.cpp](../../source/main.cpp#L1406)), pero en `data/seed_Player.bin` tiene tipo **`DT_UInt` (`20`)**. El lector acepta valores escalares (`DT_Int`, `DT_UInt`, `DT_Enum`, `DT_Bool`) mediante lectura escalar tipada, preservando interpretación con signo donde aplica.
- **Decodificación UTF-16LE → UTF-8**:
  - A diferencia de `Utf16LeToUtf8` en `source/save_editor.cpp` (que truncaba pares sustitutos), `PlayerProfile` reutiliza la lógica completa de pares sustitutos UTF-16 (`0xD800..0xDBFF` + `0xDC00..0xDFFF` → UTF-8 de 4 bytes) de `source/ugc_names.cpp`, sustituyendo sustitutos desemparejados por `U+FFFD` (`\xEF\xBF\xBD`) y señalando advertencia sin abortar el resto del perfil.
- **Requisito mínimo de validez de identidad**:
  - Si faltan todos o alguno de los campos estructurales mínimos de jugador (`Player.Name`, `Player.IslandName`, `Player.Money`, `Player.Currency`) o su tipo es incompatible (por ejemplo, string donde corresponde escalar o viceversa), reportar `MissingRequiredField` / `TypeMismatch` en lugar de inventar ceros. Campos opcionales o complementarios (p. ej. tabla de deseos `Liberation.WishInfo.*` o `HowToCall*`) degradan localmente ese atributo con marca de advertencia sin invalidar los demás (`FR-003`).

## 3. Especificación completa de los 15 atributos de `Player.sav`

Hashes calculados con **Murmur3 x86 32-bit (semilla `0`)**, verificados contra `SaveEditor::Hash` y `data/seed_Player.bin`:

| # | Campo UI / clave legacy | Clave i18n / campo `.sav` | Hash (`uint32_t`) | Tipo esperado | Regla de valor principal | Metadatos secundarios y descripción |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Name | `Player.Name` | `0xc7fb279bu` | `DT_WStr32` (acepta `WStr64`) | Cadena UTF-8 o marcador vacío (`Vacío`/`Empty`) | Pronunciación `Player.HowToCallName` (`0xbd8c4da1u`, `WStr64`/`WStr32`; si vacía usa nombre o marcador) + Idioma `Player.NameRegionLanguageID` (`0xb3229c6bu`, `DT_Enum`, 11 opciones) + descripción |
| 2 | Island | `Player.IslandName` | `0xd46df986u` | `DT_WStr32` (acepta `WStr64`) | Cadena UTF-8 o marcador vacío | Pronunciación `Player.HowToCallIslandName` (`0xc6482c14u`, `WStr64`/`WStr32`) + Idioma `Player.IslandNameRegionLanguageID` (`0x7bbff932u`, `DT_Enum`, 11 opciones) + descripción |
| 3 | Money | `Player.Money` | `0x365fab1fu` | `DT_UInt` | Formato legible de moneda del juego: `FormatMoney(raw)` (ej. `1500000` → `15 000,00` + símbolo de moneda si es conocido) | Valor bruto entero almacenado (`cents`/centésimas) + descripción |
| 4 | Currency | `Player.Currency` | `0xdc88f139u` | `DT_Enum` | Símbolo + denominación localizada (11 monedas conocidas o `0xXXXXXXXX` si desconocida) | Código interno / hash hexadecimal si fuera de catálogo + descripción |
| 5 | Boot Count | `Player.BootNum` | `0x2f19f777u` | `DT_UInt` | Entero sin signo | Descripción localizada |
| 6 | Skin Tone | `Player.SkinColorIndex` | `0xafbdf7dcu` | `DT_UInt` | Tono `1..6` (`raw + 1`) si `raw <= 5`; si `> 5`, valor bruto + aviso fuera de rango | Índice `0..5` y color de referencia RGB (`#F6D9BD`, `#ECC19D`, `#D2A07A`, `#B07A54`, `#825533`, `#5A391C`) + descripción |
| 7 | Bday Day | rawHash | `0xdb7786bbu` | Escalar (`UInt`/`Int`) | Entero; rango válido `1..31` (si fuera de rango, advertencia explícita) | Estado de rango + descripción |
| 8 | Bday Month | rawHash | `0xc754bef3u` | Escalar (`UInt`/`Int`) | Entero; rango válido `1..12` (si fuera de rango, advertencia explícita) | Estado de rango + descripción |
| 9 | Bday Year | rawHash | `0x11996629u` | Escalar (`UInt`/`Int`) | Entero (con signo si `DT_Int` o entero positivo en `UInt`) | Descripción localizada |
| 10 | Island Size | rawHash | `0x870a807cu` | Escalar (`Int`/`UInt`) | Presets `1..4`: `1` = `50 x 36`, `2` = `70 x 50`, `3` = `90 x 64`, `4` = `120 x 80`; otro valor = bruto + aviso no estándar | Número de preset (`1–4`) y advertencia de tamaño no estándar si aplica + descripción |
| 11 | Fountain Lv | `Liberation.FountainLevel` | `0xa4e65f79u` | Escalar (`UInt`/`Int`) | Entero sin signo | Descripción localizada |
| 12 | Wishes | rawHash | `0xa32f7e47u` | Escalar (`UInt`/`Int`) | Entero (`wishesMade`) | Progreso de deseos liberados de `Liberation.WishInfo.WishIdValue` (`0xf91e67beu`, `DT_UIntArray`) + `Liberation.WishInfo.IsLiberated` (`0xe38c7611u`, `DT_BoolArray`): `<liberados> / 268` (`WishesData::WISH_COUNT`) o aviso "no disponible" si falta la tabla + descripción |
| 13 | Region | `Player.Region` | `0x10631cf8u` | `DT_Enum` | Nombre legible de las 9 regiones o reserva `Desconocida (0xXXXXXXXX)` | Identificador interno de región + descripción |
| 14 | TT Penalty | rawHash | `0x71e3ba15u` | `DT_UInt64` | `0` → Inactiva / borrada; `!= 0` → Activa | Valor de marca temporal bruta (si activa) + explicación de estado y descripción |
| 15 | Donation | `Player.LastDonationGameTime` | `0xbba9c090u` | `DT_UInt64` | `0` → Nunca recogida; `> 0` → Fecha/hora `YYYY-MM-DD HH:MM` (UTC / reloj de consola) | Días transcurridos respecto al reloj de referencia (`Hace N días` / `Hace 1 día`), o aviso si la fecha está en el futuro (`days < 0`), o aviso "disponible de inmediato" si es `0` + descripción |

### Tablas de catálogos embebidas (coincidencia exacta con `source/main.cpp`)

- **Monedas (`11` entradas, hashes verificados)**:
  - `0x7e3d1e46u`: `"---"` / Invalid (`None`)
  - `0xce8d18e1u`: `"¥"` / Yen (`JPY`)
  - `0x2bef288au`: `"$"` / Dollar (`USD`)
  - `0x038c20a9u`: `"€"` / Euro (`EUR`)
  - `0x2f084465u`: `"£"` / Pound (`GBP`)
  - `0x78915cbeu`: `"HK$"` / AsiaDollar
  - `0xd73beb47u`: `"KRW"` / Won
  - `0xaca685c9u`: `"¥"` / Yuan (`CNY`)
  - `0x4616c424u`: `"RUB"` / Rouble
  - `0xa60f72e0u`: `"PHP"` / Peso
  - `0x6362048bu`: `"G$"` / GeneralUse
- **Regiones (`9` entradas, `Murmur3(name)`)**:
  - `Japan` (`0x664e4939u`), `Europe` (`0x88fa1449u`), `NorthAmerica` (`0x723581f3u`), `SouthAmericaN` (`0x8db0e23fu`), `SouthAmericaS` (`0xad49e916u`), `Australia` (`0x9a93ed38u`), `Asia` (`0x7e2ac35cu`), `OthersN` (`0xc20e61d0u`), `OthersS` (`0xde93ecdau`).
- **Idiomas de nombre/isla (`11` entradas, `Murmur3(name)`)**:
  - `JPja` (`0xf618046eu`), `USen` (`0xeb419930u`), `EUen` (`0x5883c71au`), `EUfr` (`0x70b94da7u`), `EUde` (`0xf4f3ee10u`), `EUes` (`0xbdd674dcu`), `EUit` (`0x469e188eu`), `EUnl` (`0x19c0ea41u`), `CNzh` (`0x46d2f975u`), `KRko` (`0x58d25172u`), `TWzh` (`0x44d6fc24u`).

### Regla determinista de tiempo y formato de dinero

- **`Money`**: Se conserva la convención verificada de `FormatThousands` en [source/main.cpp](../../source/main.cpp#L2645-L2667): el valor almacenado representa centésimas (`1500000` → `15 000,00`), usando espacio como separador de miles y coma decimal con dos dígitos, más el valor entero bruto en los metadatos secundarios para no ocultar el dato real.
- **`Donation`**: La función de formateo recibe explícitamente `int64_t referenceUnixSeconds` (usando `time(nullptr)` en runtime Switch y un reloj fijo en pruebas unitarias) y formatea en UTC mediante `gmtime_r` / aritmética de calendario segura (sin depender de zonas horarias del entorno host) o la hora local de consola consistente entre el timestamp mostrado y el cálculo de días transcurridos (`(referenceUnixSeconds - stampSeconds) / 86400`), marcando `inFuture = true` cuando `stampSeconds > referenceUnixSeconds` para evitar conteos negativos confusos (`Edge Cases`).

## 4. Presentación inline en Borealis (`PlayerBrowserScreen`)

- **Decisión**: Implementar `PlayerBrowserScreen` ([apps/borealis/source/player_browser_screen.hpp](../../apps/borealis/source/player_browser_screen.hpp) y [apps/borealis/source/player_browser_screen.cpp](../../apps/borealis/source/player_browser_screen.cpp)) dentro de `Navigation` en [apps/borealis/source/app.cpp](../../apps/borealis/source/app.cpp) reemplazando el contenido de muestra de la pestaña índice `3` (`app/player` / `Jugador`).
- **Estructura visual (según aclaración `FR-002` / `FR-004` y patrón de `TextureSettingsScreen`)**:
  1. Contenedor `brls::ScrollingFrame` con `brls::Box(brls::Axis::COLUMN)` y rellenos estándar (`padding` `24, 28, 24, 28`).
  2. `brls::Header` con el título localizado `app/player`.
  3. Etiqueta de nota fija: modo **solo lectura** (no modifica ni escribe la partida).
  4. Control `brls::DetailCell` **"Actualizar / Reintentar"** (deshabilitado visualmente y sin foco durante `Loading`, igual que `TextureBrowserScreen::refreshControl`).
  5. Etiqueta `status` multilínea mostrando el estado actual: sin cuenta elegida en el encabezado global, cargando, resumen cargado (`15 atributos cargados`) o error específico con próximo paso accionable.
  6. Contenedor de atributos (`rowsBox`): cuando está en `Ready`, contiene los 15 atributos en orden canónico. Cada atributo se renderiza como:
     - Un `brls::DetailCell` enfocable cuyo título es el nombre localizado del atributo y cuyo texto derecho (`detailText`) es el valor principal legible. Activar `A` o tocar una fila solo sitúa el foco en ella sin abrir subpantallas ni diálogos de edición.
     - Justo debajo en la misma columna, un `brls::Label` multilínea con margen izquierdo (`brls/listitem/descriptionIndent`) que muestra los **metadatos secundarios** (p. ej. pronunciación e idioma, símbolo/nombre de moneda o hash desconocido, deseos liberados `X / 268`, días transcurridos desde la donación, advertencia de rango o tamaño) y la **descripción localizada** del atributo.
  7. Control final `brls::DetailCell` **"Volver a la lista de secciones"** + binding global `navigation::back(this, tabs, "app/tabs_hint"_i18n)`.
- **Propagación del selector global de perfil (`Navigation::changeProfile()`)**:
  - `Navigation` en [apps/borealis/source/app.cpp](../../apps/borealis/source/app.cpp) actualizará tanto `browser` (`TextureBrowserScreen`) como `playerBrowser` (`PlayerBrowserScreen`) cuando se inicie o cambie la cuenta activa desde el header (`Y`), y activará/suspenderá el tab correspondiente en `Navigation::activate(size_t index)`.
  - `PlayerBrowserScreen` restaurará la identidad del atributo enfocado (`PlayerAttributeId` o `"refresh"` / `"tabs"`) y la posición de desplazamiento (`getContentOffsetY()` / `setContentOffsetY(..., false)`) tras cambios de pestaña (`FR-006`) o apertura/cancelación de diálogos globales (`Y`, `Plus`).

## 5. Traducción y tipografía

- **Claves nuevas**: Se añaden en las 3 carpetas requeridas ([apps/borealis/resources/i18n/en-US/app.json](../../apps/borealis/resources/i18n/en-US/app.json), [apps/borealis/resources/i18n/es/app.json](../../apps/borealis/resources/i18n/es/app.json) y [apps/borealis/resources/i18n/es-419/app.json](../../apps/borealis/resources/i18n/es-419/app.json)), reutilizando las traducciones y semántica ya establecidas en `romfs/lang/en.txt` y `romfs/lang/es.txt` adaptadas al modo de solo lectura (por ejemplo, omitiendo instrucciones de pulsar `A` para editar o limpiar en las descripciones).
- **Fuente tipográfica**: Gracias a `navigation::configurePrimaryFont` resuelto al cierre de `M0` (promoviendo `PlSharedFontType_Standard` sobre `FONT_CHINESE_SIMPLIFIED`), los acentos del español en nombres, metadatos y descripciones mantienen espaciado proporcional correcto mientras el fallback CJK/coreano/iconos preserva los glifos de nombres de jugador o isla Unicode.
