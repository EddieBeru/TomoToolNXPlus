---
description: "Tareas del navegador real de texturas de solo lectura"
---

# Tasks: Navegador de texturas reales de solo lectura

**Input**: Documentos de specs/001-browse-real-textures/.

**Prerequisites**: [plan.md](plan.md), [spec.md](spec.md), [research.md](research.md),
[data-model.md](data-model.md), [contrato](contracts/texture-browser.md) y
[quickstart.md](quickstart.md). Aplican AGENTS.md, la constitución y la guía UI.

**Tests**: Exigidos por SC-001–SC-007 y las reglas de integridad. Añadir pruebas antes de
los cambios y comprobar que detectan el comportamiento faltante. Solo fixtures sintéticas
o sanitizadas. Las tareas marcadas están verificadas según la evidencia de quickstart.md; los recorridos físicos siguen pendientes.

**Organization**: Setup → fundamentos → US1/P1 → US3/P1 → US2/P2 → verificaciones finales.
Los números US conservan su identidad en spec.md; no indican prioridad.

## Format: ID, marcador P opcional, historia y descripción

- [P] permite solapar trabajo en archivos distintos tras cumplir sus dependencias.
- Las historias llevan [US1], [US3] o [US2]; setup/fundamentos/final no llevan historia.
- Marcar tareas solo tras verificar su resultado; implementación no implica validación física.

## Path Conventions

Rutas desde la raíz: módulos puros en include/ y source/; integración Switch/Borealis
en apps/borealis/source/; pruebas en tests/. El shim tests/support/switch.h es solo host.
No ampliar source/main.cpp ni migrar WebUI/SDL como parte de este slice.

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Preparar evidencia y fixtures sin instalar dependencias ni tocar guardados reales.

- [X] T001 Registrar baseline, inventario de capacidades y matriz pendiente SC-001–SC-007 en specs/001-browse-real-textures/quickstart.md, contrastando plan.md con fuentes existentes; separar evidencia previa de pruebas nuevas y preservar cambios del usuario.
- [X] T002 Crear generadores sintéticos independientes de la interfaz de aplicación en tests/support/texture_browser_fixtures.h (la fuente falsa se implementa después en T003): cuentas A/B con mismos stems y distinto contenido, 0/1/100 entradas, auxiliares de ambas grafías, Unicode/nombres repetidos/ausentes, snapshots de inventario/bytes y diario de intentos de mutación sin datos personales.

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Probar límite de lectura y lifecycle antes de conectar UI a guardados.
Separar extracciones que preservan comportamiento de los cambios de validación.

- [X] T003 Definir entidades/resultados/fuente en include/texture_browser.h según data-model.md: AccountKey «UID opaco no vacío, estable para aislamiento interno; nunca nickname», TextureId «par AccountKey + stem exacto del archivo principal, no su posición ni nombre», refs «relativas a /Ugc», account?/selectedId? opcionales, generación/request ID/token y ReadError con clave localizable sin bytes; interfaz sin Write/Commit/Create/Delete/Rename ni handles expuestos; una vez fijada esa interfaz, implementar su fuente falsa en tests/support/texture_browser_fixtures.h usando los datos de T002.
- [X] T004 [P] Añadir tests de equivalencia CPU en tests/texture_decode_test.cpp con pixels esperados conocidos para BC1 512²/384², BC3 256², canvas square/256×512/256×128, alfa y sRGB; fijar comportamiento válido previo a extracción, sin guardados personales.
- [X] T005 Extraer CPU decode/deswizzle/color a include/texture_decode.h y source/texture_decode.cpp, conservando wrappers de include/texture_processor.h y source/texture_processor.cpp, atribución y resultados T004; sin SDL/I/O/escritores en el módulo puro y separado del endurecimiento T007.
- [X] T006 Añadir pruebas de límites en tests/texture_decode_test.cpp: raw vacío/1–3 bytes, overflow, padding/accesos block-linear inválidos, layout desconocido, zstd truncado/concatenado, tamaño desconocido/exagerado, límites exactos y exceso de output/ventana; exigir rechazo antes de reservar tamaños no verificados.
- [X] T007 Implementar validación/descompresión acotadas y LUTs inmutables thread-safe en include/texture_decode.h y source/texture_decode.cpp: «Ejes 1..1024 y hasta 1,048,576 píxeles», RGBA «exactamente width×height×4 bytes», comprimido/output ≤4 MiB y ventana ≤8 MiB; principales raw solo 131072/98304/65536 bytes, canvas y deswizzle validados antes de acceso; ejecutar T004/T006.
- [X] T008 [P] Añadir tests en tests/ugc_scanner_test.cpp para principales, thumbs de ambas grafías, canvas ausente, auxiliares huérfanos/ambiguos y case ties; probar «no aceptar separadores, . / .. ni rutas externas», «máximo 2048» entradas y «exceso de 8192 registros» como error sin truncado, además de compatibilidad del wrapper legado.
- [X] T009 Extraer asociación pura de descriptores en include/ugc_scanner.h y source/ugc_scanner.cpp conservando Scan y compatibilidad legada; «ordenar por stem ASCII normalizado y después original», mantener identidad exacta, «omitir asociación y avisar» ante auxiliares ambiguos; ausencia de auxiliar no elimina principal y auxiliar no crea entrada; ejecutar T008.
- [X] T010 [P] Añadir pruebas de worker en tests/texture_browser_session_test.cpp con fuente/cola de entrega falsas: un activo y un pendiente reemplazable, cancelación entre lecturas, respuestas tardías, cuenta cambiada, token expirado, cierre y navegación sin join bloqueante; sin incluir Borealis/libnx.
- [X] T011 Implementar worker/session en apps/borealis/source/texture_browser_session.hpp y apps/borealis/source/texture_browser_session.cpp con entrega inyectada, ownership de fuente y cancelación cooperativa: aceptar solo «token de vida, cuenta, generación y request ID» coincidentes, «Los handles son usados/cerrados solo por el worker», invalidación inmediata de entrega y drain explícito de shutdown; ejecutar T010.
- [X] T012 Crear shim libnx mínimo en tests/support/switch.h con cuentas, metadata reader, filesystem read-only, directorios/archivos y cierres; programar errores/cantidades y registrar llamadas, fallando ante apertura writable, write/create/delete/rename/commit; conservar firmas reales y excluirlo de targets Switch.
- [X] T013 Añadir pruebas del adaptador real en tests/texture_read_source_test.cpp compilándolo con T012: título/UID correctos, apertura read-only, archivos Read, tamaños negativos/overflow, bloques ≤64 KiB, short reads acumulables y EOF prematuro, cambio de tamaño, cierres ordenados en cada salida y cero mutaciones/mount legado.
- [X] T014 Implementar fuente en apps/borealis/source/texture_read_source.hpp y apps/borealis/source/texture_read_source.cpp usando fsOpen_SaveDataReadOnly y solo FsOpenMode_Read; aceptar refs emitidas por la fuente, validar tamaños/Results/cantidades antes de reservar, cerrar archivos/directorios antes del filesystem y respetar close void; sin SaveMount/MiiManager/SaveWorkflow/Backup ni fallback writable; ejecutar T013.
- [X] T015 Integrar las fuentes ya creadas source/texture_decode.cpp, source/ugc_scanner.cpp, apps/borealis/source/texture_browser_session.cpp y apps/borealis/source/texture_read_source.cpp, junto con zstd existente, en apps/borealis/CMakeLists.txt; no registrar aún archivos de historias futuras, ajustando Makefile solo si la extracción lo requiere; mantener flags/licencias y excluir shim/escritores de Borealis; ejecutar tests fundamentales y make -j4 / make -j4 borealis, registrar resultados en specs/001-browse-real-textures/quickstart.md antes de habilitar acceso desde UI.

**Checkpoint**: Fundamentos probados y ambos targets compilables. Un fallo de integridad
bloquea conectar la UI. No se exige rediseñar todo el código legado.

## Phase 3: User Story 1 - Explorar las texturas de una cuenta (Priority: P1) 🎯 MVP

**Goal**: Elegir cuenta y recorrer catálogo real con nombre/fallback y sin mezcla entre cuentas.

**Independent Test**: Cuentas A/B y 0/1/100 entradas conocidas; comparar identidades/nombres,
recorrer extremos con control y touch, cancelar selector y cambiar cuenta.
El listado entrega valor sin requerir todavía preview.

### Tests for User Story 1

- [X] T016 [P] [US1] Añadir casos de catálogo a tests/texture_browser_test.cpp: 0/1/100 entradas, orden/identidad estable, nombres repetidos/fallback, cuentas por UID, cancelación que conserva sesión y cambio confirmado que limpia datos/invalida generación y estado CatalogSuspended al salir que se reinicia una sola vez al volver con generación nueva, contexto retenido y respuestas anteriores descartadas; cubrir FR-001/002/003/012 y parte de SC-001.
- [X] T017 [P] [US1] Añadir pruebas de nombres a tests/ugc_names_test.cpp: hash falso fuera del índice, magic/índice/tipo 31/count/offset/span inválidos, duplicados, capacidades y límite 8 MiB, índice cero-based, UTF-16 surrogate válido/inválido y tipo UGC sin tabla documentada.

### Implementation for User Story 1

- [X] T018 [US1] Implementar parser puro en include/ugc_names.h y source/ugc_names.cpp: magic, índice desde 0x20 en filas de 8 bytes, WStr64Array, rango comprobado 4 + count * 128, hashes/capacidades exactos de research.md y cero-based; «nombre opcional validado UTF-16LE->UTF-8; si falta usar stem legible», fallback con aviso según nombre/fuente; sin escaneo arbitrario ni caché global; ejecutar T017.
- [X] T019 [US1] Implementar catálogo en source/texture_browser.cpp y apps/borealis/source/texture_browser_session.cpp con fuente T014, asociación T009 y parser T018; «namesStatus: available / absent / warning, sin bloquear texturas válidas», avisos locales, catálogo anterior inactivo en refresh y publicación completa, nunca parcial; salir durante CatalogLoading pasa a CatalogSuspended, regresar reinicia exactamente una carga con generación nueva y contexto retenido; agregar source/ugc_names.cpp y source/texture_browser.cpp a apps/borealis/CMakeLists.txt después de crearlos, sin duplicar fuentes, y ejecutar T016.
- [X] T020 [US1] Completar cuentas/metadata en apps/borealis/source/texture_read_source.cpp y tests/texture_read_source_test.cpp: hasta 8 cuentas, nickname ≤0x20 bytes con fallback y ordinal para duplicados sin UID visible; perfil fallido conserva cuenta; NoSave solo tras metadata completa sin match, error de metadata/apertura distinto de ausencia; sin crear/reparar.
- [X] T021 [US1] Crear listado/selector en apps/borealis/source/texture_browser_screen.hpp y apps/borealis/source/texture_browser_screen.cpp y reemplazar solo samples de Texturas en apps/borealis/source/app.cpp: brls::sync con token débil, cuenta activa, Elegir/Cambiar/Actualizar/Ajustes/Pestañas, nombre+stem, página retenida, A/tap equivalentes, B al padre y ZL/ZR solo pestañas; estados básicos seguros y sin controles de edición/exportación; conectar desactivación/reactivación a suspensión/reinicio de catálogo y registrar apps/borealis/source/texture_browser_screen.cpp en apps/borealis/CMakeLists.txt antes de T023.
- [X] T022 [US1] Añadir textos de cuentas/listado/estados y fallback inglés en apps/borealis/resources/i18n/en-US/app.json, apps/borealis/resources/i18n/es/app.json y apps/borealis/resources/i18n/es-419/app.json; conservar claves existentes y evitar datos personales en logs.
- [ ] T023 [US1] Ejecutar tests de US1/read-only, make -j4 borealis y recorrido US1 de specs/001-browse-real-textures/quickstart.md; registrar control/touch, cancelación/cambio de cuenta y pendientes en docs/BOREALIS_MIGRATION.md, distinguiendo fuente real/fixture y host/emulador/hardware.

**Checkpoint**: MVP de listado revisable, sin declarar preview ni función completa.
La seguridad básica existe desde fundamentos; US3 completa recuperación y lifecycle.

## Phase 4: User Story 3 - Entender y recuperar estados sin contenido o con fallos (Priority: P1)

**Goal**: Distinguir vacío/ausencia/error, navegar durante lectura y reintentar sin reiniciar.

**Independent Test**: Inyectar cada fallo del contrato y carga lenta, recuperar el origen
y reintentar; comparar archivos y diario de operaciones en éxito, error y cancelación.
No depende de que exista detalle de imagen.

### Tests for User Story 3

- [X] T024 [P] [US3] Ampliar tests/texture_read_source_test.cpp con fallo de servicio/perfil/metadata, NoSave, read-only denied, UgcMissing, recorrido interrumpido, tamaño cambiante, límites 8192 registros/2048 principales y Player.sav >8 MiB como aviso; comprobar recursos cerrados, cero mutaciones y Empty solo tras recorrido completo.
- [X] T025 [P] [US3] Ampliar tests/texture_browser_session_test.cpp con demora, duplicados, retry tras corregir fuente, cancelación/cambio durante lectura, evento ya encolado, token expirado, cierre y salida/regreso de pestaña durante CatalogLoading o refresh; verificar CatalogSuspended sin spinner activo, reinicio único con generación nueva y mismo contexto, descarte de entregas anteriores, pendiente reemplazable, navegación disponible y ninguna entrega de otra cuenta.

### Implementation for User Story 3

- [X] T026 [US3] Completar categorías/recuperación de errores en apps/borealis/source/texture_read_source.cpp según contracts/texture-browser.md: propagar metadata/lectura/recorrido fallidos, degradar fallo de Player.sav a aviso y jamás reparar/escribir; ejecutar T024.
- [X] T027 [US3] Completar Refresh/Retry/Cancel en source/texture_browser.cpp y apps/borealis/source/texture_browser_session.cpp: «Solo se publica un catálogo tras terminar el recorrido sin errores», «Un recorrido completo con cero principales es Empty», generación nueva por refresh y rechazo de eventos obsoletos/duplicados; completar CatalogSuspended y reinicio automático al volver según data-model.md, sin dejar Loading sin trabajo vigente; ejecutar T025.
- [X] T028 [US3] Completar AccountsLoading/NoAccounts/CatalogLoading/CatalogSuspended/Ready/Empty/CatalogError y avisos en apps/borealis/source/texture_browser_screen.cpp: actividad inmediata sin porcentaje inventado, Reintentar/Cambiar cuenta/Volver según contrato, acciones seguras durante carga, modal isolation y prevención de doble activación por held input/touch-scroll.
- [X] T029 [US3] Integrar cierre de sesión en apps/borealis/source/app.cpp y apps/borealis/source/main.cpp y ampliar tests/texture_browser_session_test.cpp: «No ejecutar join bloqueante en un callback de navegación; shutdown espera después de mostrar estado de cierre», invalidar tokens antes de destruir vistas, drenar worker antes de finalizar servicios y sin commit; no tocar source/main.cpp.
- [X] T030 [US3] Completar textos accionables de carga/vacío/error/retry/cierre en apps/borealis/resources/i18n/en-US/app.json, apps/borealis/resources/i18n/es/app.json y apps/borealis/resources/i18n/es-419/app.json; distinguir nombres ausentes, acceso fallido y vacío sin exponer datos personales.
- [ ] T031 [US3] Ejecutar matriz de fallos/retry de specs/001-browse-real-textures/quickstart.md y registrar estado, recuperación sin reinicio, navegación durante carga, ausencia de mutaciones y evidencia disponible en docs/BOREALIS_MIGRATION.md; dejar comprobaciones físicas no realizadas como pendientes.

**Checkpoint**: Listado y recuperación P1 completos sin depender de preview P2.

## Phase 5: User Story 2 - Inspeccionar una textura y volver al mismo lugar (Priority: P2)

**Goal**: Imagen real con origen/dimensiones/fallback, regreso sin perder cuenta/foco/scroll.

**Independent Test**: Entrada fuera de primera pantalla, canvas rectangular/square, alfa y
corrupción; 20 ciclos detalle/volver y 10 cambios de pestaña, refresh con entrada desaparecida;
control y touch por separado y alternados.

### Tests for User Story 2

- [X] T032 [P] [US2] Añadir a tests/texture_browser_preview_test.cpp casos de canvas rectangular prioritario, square que usa principal, canvas fallido con fallback+aviso, principal corrupta con error local, dimensiones/origen exactos, eliminación de imagen anterior y preview tardío; incluir los tres layouts y límites T007.
- [X] T033 [P] [US2] Añadir a tests/texture_browser_test.cpp casos de BrowserContext: «focusTarget: entrada o acción estable», «scrollOffset: offset del ScrollingFrame, ajustado tras layout al rango válido», vuelta/cambio de pestaña/refresh por TextureId y fallback por posición anterior/último/Actualizar; sin punteros destruidos ni persistencia entre reinicios.

### Implementation for User Story 2

- [X] T034 [P] [US2] Implementar preview en source/texture_browser_preview.cpp, invocado por apps/borealis/source/texture_browser_session.cpp con lectura acotada y decoder: «source: rectangular-canvas / main-texture», «warning?: fallback desde canvas ilegible/incompatible», «Una imagen fallida no conserva pixels de la selección anterior»; prioridad rectangular, principal para square/ausencia, fallo de canvas avisado, una RGBA vigente y validación cuenta/generación/request ID; ejecutar T032.
- [X] T035 [P] [US2] Crear apps/borealis/source/texture_preview_view.hpp y apps/borealis/source/texture_preview_view.cpp con nvgCreateImageRGBA, brls::Image::innerSetImage, FIT y fondo cuadriculado; «handles NanoVG pertenecen exclusivamente a la vista del thread gráfico y nunca al modelo», una GPU vigente, upload fallido como error local y liberación al reemplazar/cerrar; sin PNG temporal ni caché global.
- [X] T036 [US2] Añadir detalle en apps/borealis/source/texture_browser_screen.cpp con nombre completo consultable, stem/origen/dimensiones, imagen FIT, carga/error locales, Reintentar/Volver focusables y táctiles; A/tap equivalentes, B al ámbito interno y ZL/ZR nunca para variantes/elementos; sin nuevos atajos X/Y/Minus/Plus.
- [X] T037 [US2] Implementar restauración en source/texture_browser.cpp, apps/borealis/source/texture_browser_screen.cpp y apps/borealis/source/app.cpp: guardar identidad/foco/offset, restaurar tras layout sin ocultar foco, retener página y aplicar fallback de refresh; «No hay flag dirty de guardado», «Cancelar selector no cambia la sesión»; conservar borradores/diálogos de Texture Settings y ejecutar T033.
- [X] T038 [US2] Añadir traducciones de detalle/origen/dimensiones/fallback/errores en apps/borealis/resources/i18n/en-US/app.json, apps/borealis/resources/i18n/es/app.json y apps/borealis/resources/i18n/es-419/app.json; integrar texture_preview_view.cpp en apps/borealis/CMakeLists.txt y compilar Borealis.
- [ ] T039 [US2] Ejecutar US2/SC-003 de specs/001-browse-real-textures/quickstart.md: 20 ciclos detalle/volver, 10 cambios de pestaña, entrada desaparecida, alfa/proporciones y cambio de input; registrar evidencia y límites de renderer/hardware en docs/BOREALIS_MIGRATION.md.

**Checkpoint**: Historias implementadas; aún faltan comprobaciones transversales.

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Demostrar preservación, integración y resultados reales, diferenciando plataformas.

- [ ] T040 Completar SC-006 integrado en tests/texture_browser_session_test.cpp y tests/texture_read_source_test.cpp sobre cuenta/catálogo/preview/refresh/retry/error/cancelación/salida: inventario y bytes idénticos, cero intentos de write/create/delete/rename/commit y ningún cruce A/B, log personal o persistencia de contexto/imágenes.
- [X] T041 Ejecutar tests nuevos y existentes settings/config/save safety, repetir nuevos con ASan/UBSan y documentar comandos finales/resultados en tests/README.md y specs/001-browse-real-textures/quickstart.md, incluyendo tests/ugc_scanner_test.cpp y equivalencia de extracción; ajustar comandos a dependencias reales sin pruebas extra ajenas al riesgo.
- [X] T042 Ejecutar make -j4 y make -j4 borealis, auditar adaptador/call paths y build/borealis/TomoToolNXPlusBorealis.map y registrar evidencia en docs/BOREALIS_MIGRATION.md: sin SaveMount/MiiManager/SaveWorkflow/HTTP/escritores de TextureProcessor alcanzables desde navegador; ConfigStore conserva sus escrituras explícitas separadas, sin exigir ausencia global de símbolos write.
- [ ] T043 Verificar SC-002/SC-007 según docs/UI_GUIDELINES.md y registrar matriz en docs/BOREALIS_MIGRATION.md: todas las acciones solo control y solo touch, foco/modales/held input/scroll/contexto, tres idiomas con fallback inglés, ambos temas, 1280×720 y dock, nombres largos/estados y borrador/Save/Discard de Texture Settings; distinguir pruebas físicas pendientes.
- [ ] T044 Medir SC-005 y lifecycle/permisos en Switch físico según specs/001-browse-real-textures/quickstart.md: colección sintética de 100, actividad <1 s, respuesta <250 ms en al menos 95/100 órdenes durante carga y archivos sin cambios; registrar método/entorno en docs/BOREALIS_MIGRATION.md sin sembrar/restaurar guardados personales ni sustituir hardware por emulador; si falta consola dejar esta tarea sin marcar.
- [X] T045 Actualizar README.md, docs/BOREALIS_MIGRATION.md, docs/BOREALIS_PORT.md y specs/001-browse-real-textures/quickstart.md con inventario final, límites, acceso read-only y SC-001–SC-007 aprobado/fallido/pendiente; separar evidencia host/emulador/física, documentar adaptaciones del emulador si existen y capacidades diferidas, sin declarar paridad total o hardware validado con pendientes.

## Dependencies & Execution Order

### Phase Dependencies

Setup T001–T002 → fundamentos T003–T015 → US1/P1 T016–T023 → US3/P1 T024–T031 →
US2/P2 T032–T039 → transversales T040–T045.

- T003 fija contratos tras Setup y solo entonces crea la fuente falsa usando T002; T004, T008, T010 y T012 tienen salidas distintas.
- Codec: T004 → T005 → T006 → T007; extracción separada de cambio de validación.
- Asociación: T008 → T009. Worker: T010 → T011. Adaptador: T012 → T013 → T014.
- T007 + T009 + T011 + T014 → T015. T015 bloquea acceso de UI a guardados.

### User Story Dependencies

- US1 requiere fundamentos: T016/T017 → T018 → T019; T020 usa T014.
  T019 registra nombres/modelo en CMake; T019/T020/T011 → T021, que registra la pantalla → T022 → T023. Cada build incluye solo archivos ya creados.
- US3 requiere US1 para integración UI, pero se prueba sin preview:
  T024 → T026; T025 → T027; T026/T027 → T028 → T029 → T030 → T031.
  Las salvaguardas básicas ya existen en fundamentos/US1; no se posponen hasta US3.
- US2 requiere catálogo/recuperación P1: T032 → T034; T035 usa fundamentos y contrato gráfico.
  T034/T035 → T036; T033/T036 → T037 → T038 → T039.
- Final: T040 → T041 → T042 → T043 → T044 → T045.
  T045 puede documentar T044 pendiente sin marcarla completa ni afirmar validación física.

### Within Each User Story

Tests antes de cambios; modelos/servicios antes de UI; integración antes de aceptación.
No editar el mismo archivo simultáneamente. Traducciones forman parte de cada entrega.
La independencia de prueba de historias no elimina la dependencia real del detalle en el catálogo.

### Parallel Opportunities

Tras sus prerequisitos, pueden solaparse T004/T008/T010/T012, T016/T017, T024/T025,
T032/T033 y después T034/T035. El marcador describe oportunidades de planificación,
no solicita agentes ni obliga a ejecutar en paralelo.

## Parallel Example: User Story 1

Tras T015, T016 prueba catálogo en tests/texture_browser_test.cpp mientras T017 prueba
nombres en tests/ugc_names_test.cpp. Completar ambos antes de implementar/integrar parser.

## Parallel Example: User Story 3

Tras T023, T024 amplía fallos del adaptador y T025 amplía entrega/cancelación de sesión.
No compartir archivos; después implementar T026/T027 y conectar estados T028.

## Parallel Example: User Story 2

Tras T031, T032/T033 prueban sesión/contexto en archivos distintos. Cuando terminen,
T034 (CPU/worker) y T035 (GPU/view) pueden avanzar sobre el contrato PreviewResult fijo;
T036 integra ambos.

## Implementation Strategy

### MVP First (User Story 1 Only)

Completar Setup, fundamentos y US1: listado real por cuenta, nombres/fallback,
control/touch y estados seguros. No declarar preview ni función completa.
Nunca posponer aislamiento, límite read-only o cierre seguro para entregar el MVP.

### Incremental Delivery

1. Extraer preservando comportamiento y después añadir validación acotada.
2. Entregar listado P1 con evidencia de lectura segura.
3. Completar recuperación/lifecycle P1 con retry sin reinicio.
4. Añadir preview/contexto P2 mediante el mismo flujo de lectura.
5. Completar regresiones, builds, matriz UI y evidencia física; publicar el estado real.

### Completion Rules

No instalar/actualizar toolchain ni borrar trabajo ajeno; preservar atribución y licencias.
No hacer operaciones remotas ni commits implícitos. Si falta hardware, continuar verificaciones
host independientes y mantener tareas físicas pendientes. Generar esta lista no ejecuta pasos.

## Execution status — 2026-11-09

Host implementation, seven new test suites (including ASan/UBSan), existing regressions
and both Makefile builds passed. Source/ELF audit confirms the private read-only save
API and absence of linked legacy save editors/importers. Locale browser key sets match.
No emulator or physical Switch was available for this run. T023/T031/T039/T043/T044
remain open for their UI/hardware acceptance portions. T040 remains open for the complete
end-to-end acceptance matrix; synthetic snapshots, source flags and cancellation tests
already pass on host. No personal save was accessed and no commit was created.
