# Implementation Plan: Navegador de texturas reales de solo lectura

**Branch**: `main` | **Date**: 2026-11-09 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/001-browse-real-textures/spec.md`

## Summary

Reemplazar los ejemplos de Texturas por un catálogo UGC de la cuenta elegida, con lectura
real sin mutaciones, nombres opcionales, detalle con imagen y navegación control/touch.
Un adaptador libnx abre un filesystem estrictamente read-only; un controlador independiente
de Borealis serializa lectura/decodificación y descarta resultados obsoletos. Reutilizar
reglas de asociación y CPU decode mediante extracciones acotadas; no enlazar los módulos
legados que exponen escrituras. Preservar Texture Settings y las demás pestañas.

El setup informa `BRANCH=001-browse-real-textures` como identificador derivado del directorio;
`git branch --show-current` confirma `main`. No se creó ni cambió una rama en esta fase.

## Technical Context

**Language/Version**: C++17. Flags legados `-fno-rtti -fno-exceptions`; Borealis mantiene
RTTI/excepciones en su target independiente.

**Primary Dependencies**: Borealis fijado en `libs/borealis`, devkitPro/devkitA64, libnx,
zstd ya instalado y NanoVG incluido. SDL/EGL siguen siendo infraestructura de Borealis,
no dependencias del modelo o decoder puro. Sin dependencias nuevas.

**Storage**: Guardado de cuenta del título 0x010051F0207B2000 mediante filesystem read-only;
`/Ugc` y `/Player.sav`. Contexto e imágenes solo en memoria. Preferencias existentes siguen
su store separado; el navegador no escribe config, guardados, caché ni backups.

**Testing**: Ejecutables host C++17 con fuentes puras, fuente/ejecutor falsos y fallos
inyectados; ASan/UBSan; regresiones existentes de settings/config/save safety; ambos builds
Makefile; inspección del mapa de enlace y revisión del adaptador; emulador y Switch físico
para UI, permisos, lifecycle y tiempos. Guía: [quickstart.md](quickstart.md).

**Target Platform**: Nintendo Switch homebrew con toolchain existente. La API read-only
requiere HOS 2.0.0+; incompatibilidad o permisos insuficientes son error, sin fallback writable.

**Project Type**: Aplicación nativa Switch con target legado SDL y target Borealis separado.

**Performance Goals**: Actividad visible <1 s y respuesta visible <250 ms en al menos 95 %
de las órdenes de navegación durante carga con 100 entradas, según SC-005 en Switch físico.
I/O y decode fuera del thread gráfico; una imagen GPU vigente y sin precargar el catálogo.
Una carga abandonada pasa a CatalogSuspended; al regresar se solicita una sola recarga
con generación nueva, sin perder cuenta/selección/scroll ni aceptar entregas anteriores.

**Constraints**: Cero intentos de mutación/commit. Worker único con una petición pendiente
reemplazable; 64 KiB por lectura. Player ≤8 MiB; imagen comprimida ≤4 MiB; salida zstd
≤4 MiB y ventana ≤8 MiB; ejes ≤1024 y RGBA ≤4 MiB; catálogo ≤2048 texturas y ≤8192
registros enumerados. Son límites operativos, no máximos del juego. Rechazo explícito,
sin truncado silencioso. Handles cerrados por alcance; resultados con tokens de vida y generación.

**Scale/Scope**: Hasta 8 cuentas por servicio de cuentas. Catálogo de referencia de 0, 1 y
100 entradas y pruebas de límites. Listado textual con detalle de una imagen; sin búsqueda,
filtros nuevos, selección de variantes, import/export, edición ni migración WebUI.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principio | Gate previo | Revisión posterior al diseño |
| --- | --- | --- |
| I. Integridad | PASS: alcance de lectura; no mutaciones autorizadas | PASS: filesystem read-only, interfaz sin writes, spies y comparación de archivos; errores sin reparación |
| II. Paridad funcional | PASS: slice delimitado con datos reales | PASS: inventario abajo y estados/criterios explícitos; no se declara reemplazo completo |
| III. Separación | PASS: UI delegará a operaciones | PASS: modelo/worker, fuente platform y decoder separados; sin globals nuevos |
| IV. Mejora incremental | PASS: no ampliar main.cpp | PASS: extraer reglas/decoder antes de cambios; no rediseñar legado ni mover bloques arbitrarios |
| V. Interacción | PASS: guía UI aplicable | PASS: contrato de scopes, foco, touch y borradores; sin subtabs ni atajos obligatorios |
| VI. Evidencia | PASS: aceptación definida en spec | PASS: checks host, builds y matriz física planeados y diferenciados; no ejecutados aquí |
| VII. Fork | PASS: preservar trabajo/licencias | PASS: conservar atribución en extracciones, sin acceso remoto ni cambios de conexión |

No hay excepciones constitucionales. Backup, commit y recuperación de escritura no se
implementan: no existe escritura del navegador. La fuente debe demostrar esa ausencia
antes de habilitar acceso en el target; cualquier edición futura requiere otra especificación.

### Inventario de capacidades de esta migración

| Capacidad existente/esperada | Resultado de este slice | Aceptación |
| --- | --- | --- |
| Fuente UGC por cuenta | Lectura real con selección y cambio aislados | US1, FR-001/002/012, SC-001/006 |
| Identidad/nombre del elemento | Nombre válido opcional y stem estable | US1.3, FR-003 |
| Preview principal/canvas rectangular | Detalle con fallback, alfa y proporciones | US2, FR-004/010 |
| Recorrer, abrir y volver | Control/touch, foco y contexto de sesión | FR-005/006/007, SC-002/003 |
| Carga, vacío y error | Estados reales, retry y actualización | US3, FR-008/009 |
| Texture Settings funcional | Conservar store y flujo existentes | FR-014 |
| Import/export, crear/eliminar/renombrar/editar | Diferido, sin controles activos ni nuevas rutas | Fuera de alcance explícito |
| Mii, Player, Map, servidor WebUI | Sin migración ni afirmación de paridad | Funciones aún pendientes del reemplazo completo |

## Project Structure

### Documentation (this feature)

```text
specs/001-browse-real-textures/
├── spec.md
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   └── texture-browser.md
└── checklists/requirements.md
```

`tasks.md` se generará con `$speckit-tasks`; no forma parte de esta ejecución.

### Source Code (repository root)

Rutas propuestas para la implementación, todavía no creadas:

```text
include/
  texture_browser.h              # entidades, estados, comandos y fuente de lectura
  texture_decode.h               # CPU decode puro, sin I/O o renderer
  ugc_names.h                    # parser acotado de nombres
  ugc_scanner.h                  # asociación pura y wrapper legado
source/
  texture_browser.cpp            # transiciones, contexto, selección y validaciones
  texture_browser_preview.cpp    # prioridad de fuentes y lectura/CPU decode de preview
  texture_decode.cpp             # extracción CPU de texture_processor.cpp
  ugc_names.cpp                  # lectura pura de índice WStr64Array
  ugc_scanner.cpp                # extracción/adaptación preservando Scan
  texture_processor.cpp          # wrappers legados tras extracción; conserva encoder/I/O
apps/borealis/
  source/
    app.cpp                      # composición de nueva página Texturas
    texture_browser_screen.hpp
    texture_browser_screen.cpp   # listado, selector de cuenta y detalle
    texture_browser_session.hpp
    texture_browser_session.cpp  # worker owned, cancelación, entrega al thread UI
    texture_read_source.hpp
    texture_read_source.cpp      # libnx read-only; no dependencia SaveMount
    texture_preview_view.hpp
    texture_preview_view.cpp     # RGBA->NanoVG, ownership y fondo para alfa
  resources/i18n/{en-US,es,es-419}/ # traducciones en carpetas existentes
  CMakeLists.txt                 # fuentes CPU, adaptador y zstd existente
tests/
  texture_browser_preview_test.cpp
  ugc_scanner_test.cpp
  run_texture_browser.sh
  texture_browser_test.cpp
  texture_decode_test.cpp
  ugc_names_test.cpp
  texture_read_source_test.cpp   # adaptador real compilado con shim libnx host
  texture_browser_session_test.cpp # worker y entrega inyectada
  support/switch.h               # shim host limitado a APIs utilizadas
  README.md                      # comandos reales de los tests nuevos
docs/
  BOREALIS_MIGRATION.md           # límites/capacidades y evidencia del slice implementado
```

**Structure Decision**: Mantener responsabilidades puras en `include/` y `source/`, como
settings actuales, y la integración Borealis/libnx en `apps/borealis/source/`. Extraer
CPU decode y reconocimiento de archivos sin cambiar resultados legados antes de integrar
UI. No se modifica `source/main.cpp`: mover su `LoadPreview` completo arrastraría globals
y SDL; el nuevo controlador expresa la prioridad de fuentes con datos compartidos.
Registrar fuentes en CMake por incremento: fundamentos existentes primero; nombres/modelo
al crearlos en US1; pantalla antes del build de US1; preview al crearlo en US2. No registrar
archivos futuros ni dejar la primera integración de pantalla para US2.
No introducir framework de repositorios ni servidor nuevo. No enlazar SaveMount,
MiiManager, SaveWorkflow, HTTP ni escritores de TextureProcessor en el target Borealis.

## Complexity Tracking

Sin violaciones ni excepciones que justificar.

## Phase 0 — Research Outcome

[research.md](research.md) resuelve acceso libnx estrictamente read-only, clasificación
de ausencia/error, nombres desde índice real, límites, decoder y ownership gráfico.
Las capacidades se comprobaron en fuentes locales; permisos físicos y tiempos permanecen
como verificaciones de implementación, no preguntas de diseño sin resolver.

## Phase 1 — Design and Handoff

- [data-model.md](data-model.md): entidades, validación, estados y generaciones.
- [contracts/texture-browser.md](contracts/texture-browser.md): interfaz de lectura y UI.
- [quickstart.md](quickstart.md): fixtures, comandos y matriz de validación.

Orden para tareas: extracciones con pruebas de equivalencia; modelo/parser acotados;
adaptador y worker con fallos/aislamiento; listado P1 y recuperación P1; preview/contexto P2;
traducciones, ambos builds y verificación de UI/hardware. Revisar la constitución otra vez
al generar tareas y antes de declarar implementación completa.
