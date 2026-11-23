# Quickstart: Validación de consulta de jugador en solo lectura (M1)

Guía de verificación en host, compilación cruzada, auditoría de solo lectura y pruebas en Switch física para `specs/002-browse-player-readonly/`.

## Prerrequisitos

- Toolchain `devkitPro` / `libnx` y portlibs existentes (`DEVKITPRO` exportado); no actualizar ni instalar herramientas como parte de este hito.
- Compilador host `g++` C++17 y `zstd` host para ejecutar las suites de `tests/`.
- Referencias: [spec.md](spec.md), [contracts/player-browser.md](contracts/player-browser.md), [data-model.md](data-model.md), [research.md](research.md) y [tests/README.md](../../tests/README.md).
- Política de datos: usar **únicamente** fixtures sintéticos generados en memoria/temporales o la semilla pública [data/seed_Player.bin](../../data/seed_Player.bin). Nunca incorporar guardados personales ni UIDs reales al repositorio, logs o pruebas.

## Fixtures sintéticos de referencia

Los tests automatizados generan búferes `.sav` sintéticos deterministas en memoria y en directorios temporales:

1. **Cuenta A (`Synthetic Save A`)**:
   - Identidad: nombre `"Álex 😀"` (acento español + par sustituto UTF-16LE), pronunciación `"A-lex"` (`DT_WStr64`), idioma `EUes`.
   - Isla: `"Ítaca"` (`DT_WStr32`), pronunciación `"I-ta-ca"` (`DT_WStr64`), idioma `EUes`.
   - Economía y perfil: dinero `1500000` (`15 000,00`), moneda `Euro` (`0x038c20a9u`), arranques `42`, tono de piel `2` (`3` de `1..6`), cumpleaños `15 / 8 / 1998`, tamaño de isla `3` (`90 x 64`), nivel fuente `12`, deseos `105` con tabla `Liberation.WishInfo.*` (`100` deseos liberados de `268`), región `Europe` (`0x88fa1449u`), penalización VT `0` (inactiva), donación en timestamp UNIX conocido (`1700000000`, probado contra reloj fijo `1700259200` = hace `3` días).
2. **Cuenta B (`Synthetic Save B`)**:
   - Valores distintos en los 15 atributos para detectar mezclas entre cuentas (`SC-001`).
   - Casos límite: pronunciación vacía, código de moneda desconocido (`0x12345678u`), región desconocida, tono de piel fuera de rango (`9`), día/mes fuera de rango (`99` / `0`), tamaño de isla no estándar (`7`), penalización VT activa (`!= 0`), donación `0` (nunca recogida) y donación futura respecto al reloj de referencia (`stamp > refClock`), además de ausencia de `Liberation.WishInfo.IsLiberated`.
3. **Semilla del repositorio (`data/seed_Player.bin`)**:
   - Validación de lectura real sobre una plantilla completa de Tomodachi Life sin modificar ningún byte del archivo fuente.
4. **Fixtures de error / fuzz estructural (`SC-004`, `FR-011`)**:
   - Guardado ausente (`NoSave`), error de apertura (`Open`), lectura incompleta (`ShortRead`), tamaño `> 8 MiB` (`FileTooLarge`), cabecera truncada (`< 0x20`), magic inválido, `saveDataOff` desalineado o fuera de rango, centinela de tipo `>= 33`, offset de heap `< saveDataOff` o desbordado, tabla de array con `count` absurdo, ausencia de campos mínimos obligatorios o conflicto de tipo.

## Checks en host (a ejecutar durante la implementación)

Desde la raíz del repositorio:

```sh
tests/run_texture_browser.sh
SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh
```

Suites planificadas para M1 dentro de `tests/run_texture_browser.sh`:

1. **`player-profile`** (`tests/player_profile_test.cpp`):
   - Verifica hashes Murmur3 x86_32, lectura de `data/seed_Player.bin` y de Cuenta A / Cuenta B, decodificación UTF-16LE + pares sustitutos + sustitutos inválidos, `DT_WStr64` y `DT_WStr32` en pronunciación, formateo de los 15 atributos, catálogo completo de monedas (`11`), regiones (`9`), idiomas (`11`), tamaños (`1..4`), conteo de deseos liberados (`BitArray` de `IsLiberated`), penalización VT y cálculo determinista de días de donación (pasado, `1` día, `0` / nunca y futuro).
   - Comprueba inmutabilidad byte a byte del búfer de entrada y rechazo sin crash ni UB ante cada variante corrupta o desbordada.
2. **`player-context`** (`tests/player_browser_context_test.cpp`):
   - Verifica `PlayerBrowserModel` y lectura sobre `SwitchSource` / fuente falsa: transiciones `Unselected` → `Loading` → `Ready` / `Error`, suspensión y reanudación sin solicitudes duplicadas, descarte de entregas obsoletas tras cambiar cuenta, conservación de contexto y conteo de cero escrituras en `ReadSource`.
3. **`ui-screens`** (`tests/borealis_ui_screens_test.cpp`, ampliado para M1):
   - Verifica la pestaña `Jugador` en `App`/`Navigation`: ausencia de entradas ficticias, presencia de los 15 `DetailCell` inline y sus `Label` multilínea de metadatos/descripción debajo, navegación completa por mando (`D-pad`, `A`, `B`, `ZL`/`ZR`, `Plus`) y toque, cambio de cuenta con `Y` (limpieza de cuenta A y carga de cuenta B) y cancelación de `Y` sin perder foco/scroll, además de estados `Unselected`, `NoSave`, `Error` y recuperación mediante `Actualizar / Reintentar`.
   - Mantiene aprobadas todas las suites existentes de Texturas, Settings y navegación de `M0`.

## Compilación cruzada Switch y auditoría de solo lectura (`SC-006`)

```sh
make -j4
make -j4 borealis
"$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm" -C build/borealis/TomoToolNXPlusBorealis.elf \
  | grep -E "SaveMount|SaveWorkflow|Backup|MiiManager|SaveEditor::Save"
```

Resultado esperado:
- Ambos targets (`TomoToolNXPlus.nro` y `TomoToolNXPlusBorealis.nro`) compilan sin errores ni regresiones.
- El `grep` sobre símbolos del ELF Borealis devuelve **0 coincidencias**.

## Checklist de aceptación en Switch físico (reportar separado de host y emulador)

1. Desplegar con `make -j4 deploy-borealis DEPLOY_ARGS='--replace-undated'` (verificando SHA-256 readback por MTP).
2. En Switch físico (tema claro y tema oscuro, 720p portátil y modo dock):
   - [ ] Entrar en pestaña `Jugador`: no aparecen datos ficticios (`Jugador de ejemplo`).
   - [ ] Verificar los 15 atributos reales de la cuenta activa en orden, sus metadatos secundarios integrados debajo de cada fila y sus descripciones sin recortes ni solapamientos ilegibles.
   - [ ] Comprobar nombres con acentos españoles (espaciado proporcional correcto) y cualquier nombre largo/Unicode.
   - [ ] Recorrer toda la lista y acciones solo con mando (`D-pad`/stick, `A`, `B`) y repetir solo con pantalla táctil (scroll sin disparos accidentales).
   - [ ] Cambiar a una segunda cuenta desde la barra superior (`Y`): los datos previos se limpian al confirmar y aparecen únicamente los datos de la nueva cuenta.
   - [ ] Probar una cuenta sin partida del juego (o estado de error): muestra motivo claro, `Actualizar / Reintentar` funciona y no intenta reparar/crear guardado.
   - [ ] Abrir `Y` y cancelar, abrir `Plus` y cancelar, y cambiar de pestaña 10 veces regresando a `Jugador`: conserva cuenta, atributo enfocado y scroll.
   - [ ] Verificar idiomas `en-US`, `es` y `es-419`.
   - [ ] Confirmar que `Textures` y `Ajustes de texturas` siguen intactos y que los bytes/inventario del guardado de juego permanecen 100 % idénticos.

## Matriz de verificación `SC-001`–`SC-007`

| Criterio | Cobertura esperada | Host (`tests/run_texture_browser.sh`) | Build / ELF Audit | Emulador | Switch físico |
| --- | --- | --- | --- | --- | --- |
| `SC-001` | 15 atributos reales y aislamiento entre cuentas A/B + `data/seed_Player.bin` | **PASS** (`player-profile`, `player-context`, `ui-screens` normal y `SANITIZE=1`) | Compilado (`make borealis`) | No ejecutado | Pendiente de prueba en consola (`T024`) |
| `SC-002` | Operación completa solo con mando y solo con touch | **PASS** (shim UI `borealis_ui_screens_test.cpp` normal y `SANITIZE=1`) | Compilado | No ejecutado | Pendiente de prueba física (`T024`) |
| `SC-003` | 10 cambios de pestaña + diálogos `Y`/`Plus` conservando foco y scroll | **PASS** (shim UI `borealis_ui_screens_test.cpp` normal y `SANITIZE=1`) | Compilado | No ejecutado | Pendiente de prueba física (`T024`) |
| `SC-004` | Sin perfil, `NoSave`, acceso denegado, corrupción, límite `8 MiB`, reservas y `Actualizar` | **PASS** (`player-profile`, `player-context`, `ui-screens` normal y `SANITIZE=1`) | Compilado | No ejecutado | Pendiente de prueba física (`T024`) |
| `SC-005` | Respuesta de carga `< 1 s` y navegación `< 250 ms` | **Parcial PASS** (suspensión/reanudación no bloqueante en host) | Compilado | No ejecutado | Pendiente de medición física (`T024`) |
| `SC-006` | Cero escrituras/mutaciones/guardado tras todo el recorrido | **PASS** (spy `ReadSource` + inmutabilidad exacta de búfer) | **PASS** (`aarch64-none-elf-nm`: 0 símbolos prohibidos) | No ejecutado | Pendiente de comprobación física (`T024`) |
| `SC-007` | 3 idiomas (`en-US`, `es`, `es-419`), claro/oscuro, 720p y dock sin recortes | **Parcial PASS** (paridad `162` claves JSON en 3 idiomas y shim UI) | **PASS** (`make -j4` y `make -j4 borealis`) | No ejecutado | Pendiente de verificación visual (`T024`) |
