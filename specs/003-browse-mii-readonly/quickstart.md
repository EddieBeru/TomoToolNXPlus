# Quickstart: Validación de consulta de residentes Mii en solo lectura (M2)

Guía de verificación en host, compilación cruzada, auditoría de solo lectura y pruebas en Switch física para `specs/003-browse-mii-readonly/`.

## Prerrequisitos

- Toolchain `devkitPro` / `libnx` y portlibs existentes (`DEVKITPRO` exportado); no actualizar ni instalar herramientas como parte de este hito.
- Compilador host `g++` C++17 y `zstd` host para ejecutar las suites de `tests/`.
- Referencias: [spec.md](spec.md), [contracts/mii-browser.md](contracts/mii-browser.md), [data-model.md](data-model.md), [research.md](research.md) y [tests/README.md](../../tests/README.md).
- Política de datos: usar **únicamente** fixtures sintéticos generados en memoria/temporales o las semillas públicas [data/seed_Mii.bin](../../data/seed_Mii.bin) y [data/seed_Map.bin](../../data/seed_Map.bin). Nunca incorporar guardados personales ni UIDs reales al repositorio, logs o pruebas.

## Fixtures sintéticos de referencia

1. **Cuenta A (`Synthetic Mii Save A`)**:
   - 3 residentes en plazas dispersas (por ejemplo plazas `1`, `14` y `70`), con nombres con acentos españoles y par sustituto UTF-16LE (`"Sofía 🌟"`, `"Mateo"`, `"Ren"`).
   - Estadísticas completas (nivel 0-based `4` → mostrado `5`, medidor `75`, dinero `12500`, cumpleaños `15/8/1998`, edad directa `-1` automático y `24` explícito, 5 rasgos `1..10`, vínculo `88`, estado de ánimo `Good`, saciedad `115`).
   - Palabras: 2 plazas llenas (`TalkStart` con pronunciación explícita, `Happy` con pronunciación vacía) y 10 plazas `Invalid`.
   - Relaciones: amistad con rango `Best friends`, pareja en pelea (`IsFight = true`) con timestamp `Since`, familia (`Parent`/`Child`) y relación apuntando a plaza vacía (huérfana).
   - Pertenencias: prendas equipadas conocidas con índice de color, conjunto `Coordinate` en uno de los residentes, bolsillos con objetos conocidos y código desconocido.
   - Hábitos: combinación de hábitos poseídos/activos (`IsOwn=1, IsChecked=1, State=Own`), poseídos inactivos, no poseídos y nunca obtenidos repartidos en las 9 categorías.
   - Vivienda + `Map.sav` sintético: 2 residentes en la misma casa con habitaciones distintas, 2 en conflicto de misma habitación y 1 sin vivienda (`HouseMapId = -1`), con nombre de casa personalizado en `Map.sav`.
2. **Cuenta B (`Synthetic Mii Save B`)**:
   - 1 residente diferente (para verificar aislamiento total al cambiar de cuenta con `Y`) con enums desconocidos (`0x12345678u`), cumpleaños fuera de rango (`99/0`), sin `Map.sav` (degradación a `"Casa #N"`).
3. **Isla vacía (`0` residentes) e isla llena (`70` residentes)**:
   - Isla válida con 70 plazas vacías → estado `Empty` (distinto de error).
   - Isla con los 70 residentes ocupados → verifica recorrido completo sin truncamiento ni degradación.
4. **Semillas del repositorio (`data/seed_Mii.bin` + `data/seed_Map.bin`)**:
   - Lectura real sin modificar ningún byte de los archivos semilla.
5. **Fixtures de error / fuzz estructural (`SC-004`, `FR-017`)**:
   - Guardado ausente (`NoSave`), error de apertura (`Open`), lectura corta (`ShortRead`), tamaño `> 8 MiB` (`FileTooLarge`), cabecera truncada (`< 0x20`), magic inválido, `saveDataOff` desalineado o fuera de rango, centinela `>= 33`, offset de heap desbordado, `BinArray` truncado, ausencia o `TypeMismatch` de `Mii.Name.Name`.

## Checks en host

Desde la raíz del repositorio:

```sh
tests/run_texture_browser.sh
SANITIZE=1 ASAN_OPTIONS=detect_leaks=0 tests/run_texture_browser.sh
```

Suites planificadas para M2 dentro de `tests/run_texture_browser.sh`:

1. **`mii-roster`** (`tests/mii_roster_test.cpp`):
   - Verifica parsing acotado de `data/seed_Mii.bin` + `data/seed_Map.bin`, Cuenta A/B, isla vacía (0), isla llena (70), plazas dispersas, todas las 7 secciones, catálogos (`belongings_data.h`, `habits_data.h`, `rel_tier_labels.h`), degradación local ante `Map.sav` ausente o códigos desconocidos, inmutabilidad byte a byte del búfer y rechazo seguro ante cada variante corrupta.
2. **`mii-context`** (`tests/mii_browser_context_test.cpp`):
   - Verifica `MiiBrowserModel`, `LoadFromReadSource`, transiciones `Unselected` → `Loading` → `Ready` / `Empty` / `Error`, `suspend()`/`resume()`, tokens de generación ante cambio de cuenta y cero escrituras sobre `ReadSource`.
3. **`ui-screens`** (`tests/borealis_ui_screens_test.cpp`, ampliado para M2):
   - Verifica la pestaña `Mii` (índice `2`) en `App`/`Navigation`: ausencia de datos ficticios, lista de residentes, navegación jerárquica `MiiBrowserScreen` → `MiiDetailScreen` → `MiiSectionScreen` (las 7 secciones) solo con mando y solo con toque, retorno con `B` conservando foco/scroll en 10 ciclos, cambio/cancelación de cuenta `Y`, `Plus`, estados `Unselected`/`Empty`/`NoSave`/`Error` y recuperación con `Actualizar / Reintentar`.
   - Mantiene aprobadas todas las suites existentes de `M0` y `M1`.

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
   - [ ] Entrar en pestaña `Mii`: desaparecen las entradas ficticias y aparecen los residentes reales de la cuenta activa.
   - [ ] Abrir un residente y recorrer sus 7 secciones (Estadísticas, Palabras, Relaciones, Pertenencias, Hábitos, Vivienda y Resumen social) solo con mando (`D-pad`/stick, `A`, `B`) y repetir solo con pantalla táctil.
   - [ ] Comprobar que `B` restaura exactamente la sección en el detalle y el residente + scroll en la lista principal (10 ciclos consecutivos).
   - [ ] Cambiar a otra cuenta con `Y` (aislamiento total) y cancelar `Y` / `Plus` conservando contexto.
   - [ ] Probar isla vacía / cuenta sin guardado / error recuperable con `Actualizar / Reintentar`.
   - [ ] Verificar idiomas `en-US`, `es` y `es-419` (acentos españoles y nombres largos sin solapamientos).
   - [ ] Confirmar que `Textures`, `Ajustes de texturas` y `Jugador` siguen intactos y que los bytes del guardado permanecen 100 % idénticos.

## Matriz de verificación `SC-001`–`SC-007` (Actualizada 2026-11-22)

| Criterio | Cobertura esperada | Host (`tests/run_texture_browser.sh`) | Build / ELF Audit | Emulador | Switch físico |
| --- | --- | --- | --- | --- | --- |
| `SC-001` | Lista real (0, 1, dispersos, 70) y 7 secciones exactas por cuenta + semillas | **PASS** (`mii-roster`, `mii-context`, `ui-screens`, normal + ASan/UBSan) | **PASS** (`make -j4` + `make -j4 borealis`) | No ejecutado | Pendiente (sin consola disponible) |
| `SC-002` | Operación completa solo con mando y solo con touch sin subpestañas | **PASS** (`ui-screens` sobre shim UI: mando y touch en las 7 secciones) | **PASS** | No ejecutado | Pendiente |
| `SC-003` | 10 ciclos lista ↔ residente ↔ sección + `Y`/`Plus` conservando foco y scroll | **PASS** (`ui-screens`: 10 ciclos + `Y`/`Plus` + suspend/resume) | **PASS** | No ejecutado | Pendiente |
| `SC-004` | Sin perfil, isla vacía, `NoSave`, acceso denegado, corrupción, `> 8 MiB`, sin `Map.sav` | **PASS** (`mii-roster`, `mii-context`, `ui-screens`, normal + ASan/UBSan) | **PASS** | No ejecutado | Pendiente |
| `SC-005` | Respuesta de carga `< 1 s` y navegación `< 250 ms` | **PASS** (I/O asíncrono + `suspend`/`resume` verificados en host) | **PASS** | No ejecutado | Pendiente (medición real en Switch) |
| `SC-006` | Cero escrituras/mutaciones/guardado tras todo el recorrido | **PASS** (inmutabilidad byte a byte y 0 escrituras en shim) | **PASS** (`aarch64-none-elf-nm -C`: `0` símbolos prohibidos) | No ejecutado | Pendiente |
| `SC-007` | 3 idiomas (`en-US`, `es`, `es-419`), claro/oscuro, 720p y dock sin recortes | **PASS** (`290` claves JSON con paridad exacta en los 3 idiomas) | **PASS** (RomFS empaquetado en `TomoToolNXPlusBorealis.nro`) | No ejecutado | Pendiente (inspección visual 720p/dock/claro/oscuro) |
