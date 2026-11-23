# Feature Specification: Consulta de residentes Mii en modo solo lectura (M2)

**Feature Branch**: `main` (rama actual; esta ejecución no crea una rama)

**Created**: 2026-11-22

**Status**: Draft

**Milestone**: `M2` ([Roadmap](../../docs/ROADMAP.md))

**Input**: User description: "M2 — Reemplazar los datos de muestra de la pestaña Mii por la inspección real en modo solo lectura de los residentes de la isla (hasta 70 plazas) y sus 7 secciones (estadísticas, palabras, relaciones, pertenencias, hábitos, vivienda y resumen social) para la cuenta seleccionada, sustituyendo las subpestañas legadas por navegación jerárquica y secciones enfocables."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Listar los residentes reales de la isla y consultar sus estadísticas principales (Priority: P1)

Como usuario, quiero ver en la pestaña Mii la lista real de residentes de mi isla correspondientes a la cuenta seleccionada y abrir cualquier residente para consultar sus estadísticas principales, en lugar de entradas ficticias, sin arriesgar mi partida guardada.

**Why this priority**: Elimina los datos de muestra de la pestaña Mii, valida el acceso estrictamente de solo lectura a la lista de hasta 70 residentes y entrega el flujo principal de consulta (lista de residentes → pantalla de detalle del residente → estadísticas principales).

**Independent Test**: Con guardados sintéticos para dos cuentas distintas (Cuenta A con varios residentes y valores conocidos; Cuenta B con un residente distinto y otra configuración), seleccionar la cuenta A, abrir la pestaña Mii y comprobar que aparece la lista real de residentes con su resumen; entrar en un residente y verificar sus 16 atributos principales (nombre, nivel, barra de nivel, dinero, día/mes/año de cumpleaños, edad directa, 5 rasgos de personalidad, vínculo, estado de ánimo y saciedad). Cambiar después a la cuenta B desde el selector de perfil global y verificar que la lista y los datos se sustituyen por completo sin mezclar información de A. Repetir el recorrido solo con mando y solo con pantalla táctil.

**Acceptance Scenarios**:

1. **Given** una cuenta seleccionada con una partida válida que contiene entre 1 y 70 residentes, **When** el usuario abre la pestaña Mii, **Then** desaparecen las entradas de ejemplo y se muestra en orden estable la lista real de residentes ocupados (omitiendo plazas vacías), indicando para cada uno su nombre real, número de plaza y un resumen breve de estado (por ejemplo, nivel y estado de ánimo).
2. **Given** la lista de residentes cargada, **When** el usuario selecciona un residente con `A` o toque, **Then** se abre la pantalla de detalle del residente mostrando un índice claro de sus 7 secciones consultables (Estadísticas, Palabras, Relaciones, Pertenencias, Hábitos, Vivienda y Resumen social) sin utilizar subpestañas con botones laterales.
3. **Given** un residente abierto en la sección de Estadísticas, **When** el usuario inspecciona sus datos, **Then** se muestran en orden estable los 16 atributos de solo lectura del residente: Nombre, Nivel (mostrado desde 1), Barra de nivel (0–100), Dinero, Día de cumpleaños (1–31), Mes de cumpleaños (1–12), Año de cumpleaños, Edad directa (indicando «Automática» cuando no hay anulación), Actividad (1–10), Audacia (1–10), Sentido común (1–10), Alegría (1–10), Sociabilidad (1–10), Vínculo (0–100), Estado de ánimo (etiqueta legible) y Saciedad. No aparece ninguna acción destructiva ni de escritura (como borrar, crear, importar o exportar Mii).
4. **Given** dos cuentas con listas de residentes diferentes, **When** el usuario cambia de cuenta desde la barra superior (incluso desde dentro de un residente o sección), **Then** se limpian los datos de la cuenta anterior, se cancelan lecturas pendientes, la vista vuelve de forma segura a la lista raíz de Mii si el residente previo ya no aplica y se cargan únicamente los residentes de la nueva cuenta.
5. **Given** una sesión activa en la pestaña Mii o en el detalle de un residente, **When** el usuario abre el selector de perfil y lo cancela, **Then** se conservan la cuenta activa, el residente abierto o enfocado y la posición de desplazamiento sin recargar ni perder contexto.

---

### User Story 2 - Inspeccionar palabras, relaciones, pertenencias, hábitos, vivienda y resumen social del residente (Priority: P2)

Como usuario, quiero explorar desde el detalle de un residente sus palabras guardadas, relaciones con otros residentes, pertenencias y atuendo, hábitos por categoría, asignación de vivienda y resumen social en modo de solo lectura, regresando con `B` sin perder mi lugar.

**Why this priority**: Completa la paridad de consulta de solo lectura frente a las 7 secciones del editor legado sin introducir subpestañas nuevas, preparando la experiencia antes de habilitar futuras ediciones en hitos posteriores.

**Independent Test**: Con un guardado sintético que incluya al menos 3 residentes con frases, relaciones de distintos tipos (amistad, pareja, familia, pelea), atuendo y pertenencias conocidas, hábitos en varias categorías y asignaciones de vivienda (con compañeros de casa, un residente sin vivienda y un conflicto de habitación), abrir cada una de las 6 secciones restantes de un residente, comprobar todos los datos mostrados y verificar que pulsar `B` regresa en cada nivel conservando el foco y el desplazamiento.

**Acceptance Scenarios**:

1. **Given** un residente con frases configuradas, **When** el usuario entra en la sección **Palabras**, **Then** ve las 12 plazas de frases con su tipo (o indicación de plaza vacía), el texto guardado y su pronunciación (indicando de forma explícita cuando coincide con el texto o está vacía).
2. **Given** un residente con relaciones hacia otros residentes de la isla, **When** el usuario entra en la sección **Relaciones**, **Then** ve la lista de relaciones del residente con el nombre del otro residente, tipo de relación, nivel/medidor de afinidad, etiqueta descriptiva del rango (cuando exista para ese tipo) e indicador explícito si existe una pelea o conflicto activo; si no tiene relaciones, se muestra un estado vacío claro.
3. **Given** un residente con atuendo equipado y objetos en pertenencias, **When** el usuario entra en la sección **Pertenencias**, **Then** ve el atuendo/conjunto equipado y las pertenencias del residente con nombres legibles del catálogo (o una etiqueta de reserva clara para códigos desconocidos o espacios vacíos).
4. **Given** un residente con hábitos registrados, **When** el usuario entra en la sección **Hábitos**, **Then** puede consultar los hábitos organizados en las 9 categorías del juego (Caminar, Postura, Comer, Saludo, Enfado, Expresión, Voz, Estómago y Otros), distinguiendo claramente cuáles posee actualmente frente a los no poseídos o nunca obtenidos, con conteos resumidos por categoría.
5. **Given** una isla con información de vivienda y residentes alojados o sin hogar, **When** el usuario entra en la sección **Vivienda**, **Then** ve si el residente está alojado o sin vivienda; si está alojado, muestra el nombre de la casa (o una etiqueta de reserva numerada cuando los nombres de casas del mapa no estén disponibles), el número de habitación, la lista de compañeros de vivienda y una advertencia explícita si otro residente ocupa exactamente la misma habitación.
6. **Given** un residente en la isla, **When** el usuario entra en la sección **Resumen social**, **Then** ve un resumen estructurado en solo lectura de su red social: total de vínculos, desglose por categoría de relación (amistad, romance, familia, conocidos, anteriores) y lista agrupada de residentes vinculados con su rango actual.
7. **Given** que el usuario navega desde cualquier sección hija hacia atrás con `B` (o control táctil equivalente), **When** retrocede al detalle del residente y luego a la lista raíz de Mii, **Then** en cada nivel se restaura el elemento exacto desde el cual entró (sección enfocada en el detalle y residente enfocado con su posición de desplazamiento en la lista raíz).

---

### User Story 3 - Distinguir y recuperar estados vacíos, sin guardado, incompletos o con fallos de lectura (Priority: P1)

Como usuario, quiero distinguir claramente cuando no he elegido cuenta, cuando la lista está cargando, cuando la isla aún no tiene residentes, cuando faltan datos complementarios o cuando la lectura falla por datos dañados o inaccesibles, y poder reintentar sin reiniciar ni alterar archivos.

**Why this priority**: Las islas reales pueden estar vacías, carecer de partida en una cuenta secundaria o contener secciones incompletas; diferenciar errores reales de listas vacías y evitar bloqueos o mutaciones protege la confianza del usuario y la integridad de los datos.

**Independent Test**: Probar sesión sin cuenta elegida, cuenta sin partida del juego, acceso denegado, archivo de residentes ausente/truncado/corrupto/sobredimensionado, isla válida con 0 residentes, ausencia de nombres de casas en el archivo de mapa complementario, cambio de pestaña durante la carga y recuperación mediante Actualizar/Reintentar por mando y toque.

**Acceptance Scenarios**:

1. **Given** que aún no se ha elegido una cuenta en la barra superior, **When** el usuario entra en la pestaña Mii, **Then** ve un aviso claro invitando a seleccionar un perfil desde la barra superior, sin datos ficticios.
2. **Given** una lectura en curso, **When** tarda en completarse, **Then** se muestra actividad de carga sin porcentajes inventados, se bloquean solicitudes duplicadas y el usuario puede cambiar de pestaña o abrir el menú Guardar/Salir sin bloquear la interfaz.
3. **Given** que el usuario abandona la pestaña Mii mientras una lectura está en curso, **When** cambia a otra pestaña y regresa más tarde, **Then** las entregas obsoletas se descartan sin actualizar vistas inactivas y al regresar se lanza una única lectura vigente conservando la cuenta y el contexto.
4. **Given** una cuenta con partida válida pero sin ningún residente ocupado (0 de 70 plazas), **When** termina la lectura, **Then** se muestra un estado vacío específico indicando que la isla aún no tiene residentes (distinto de un error de lectura), con acciones de Actualizar y cambio de cuenta.
5. **Given** una cuenta sin guardado del juego, error de acceso o datos de residentes truncados, corruptos o sobredimensionados, **When** termina el intento de lectura, **Then** se muestra el motivo específico y recuperable con acciones de Actualizar/Reintentar y cambio de cuenta, sin inventar residentes ni intentar crear o reparar archivos.
6. **Given** un archivo complementario de nombres de casas ausente o ilegible mientras los datos de residentes son válidos, **When** el usuario consulta la sección Vivienda, **Then** los residentes y sus asignaciones siguen cargando con normalidad usando etiquetas de reserva comprensibles (por ejemplo, «Casa #N») y un aviso no bloqueante.

### Edge Cases

- **Capacidad máxima (70 residentes) y relaciones densas**: Una isla completa con 70 residentes y hasta 69 relaciones por residente debe listarse y desplazarse con fluidez, manteniendo visible el foco del mando al desplazarse y sin recortes ni solapamientos de nombres largos o Unicode (acentos, caracteres japoneses/chinos/coreanos y emojis/pares sustitutos).
- **Plazas dispersas (huecos entre 1 y 70)**: Si los residentes ocupan plazas no contiguas (por ejemplo, plazas 1, 14 y 70), la lista debe mostrar únicamente las plazas ocupadas preservando su número de plaza real sin mezclar índices ni asignar datos de un residente a otro.
- **Códigos desconocidos o valores fuera de rango**: Si el estado de ánimo, tipo de frase, tipo de relación, prenda/pertenencia, estado de hábito o fecha de cumpleaños contiene un valor fuera de catálogo o rango, debe mostrarse una etiqueta de reserva clara con indicación de advertencia en ese dato sin invalidar el resto del residente ni la lista.
- **Relación apuntando a una plaza vacía o huérfana**: Si un registro de relación referencia a una plaza sin residente ocupado, debe omitirse o marcarse explícitamente como plaza vacía sin fallar ni mostrar nombres corruptos.
- **Nombres o frases vacías y pronunciaciones vacías**: Si un residente tiene pronunciación vacía en una frase, debe indicarse de forma clara; si un residente ocupado carece de texto en algún campo opcional, no debe ocultarse ni bloquear las demás secciones.
- **Cambio de cuenta o salida durante carga o dentro de una pantalla hija**: Confirmar un cambio de cuenta o solicitar salir desde el menú Guardar/Salir debe descartar resultados obsoletos, esperar el cierre seguro de las lecturas activas y nunca ofrecer ni ejecutar guardado de partida.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: La pestaña Mii MUST reemplazar todas las entradas ficticias por la lista real de residentes ocupados (hasta 70 plazas) pertenecientes a la cuenta seleccionada en el encabezado global, omitiendo plazas vacías y mostrando el nombre real, número de plaza y resumen breve de cada residente.
- **FR-002**: La navegación de la pestaña Mii MUST estructurarse de forma jerárquica sin introducir subpestañas: lista de residentes en la raíz de la pestaña → pantalla de detalle del residente con acceso enfocable a sus 7 secciones (Estadísticas, Palabras, Relaciones, Pertenencias, Hábitos, Vivienda y Resumen social) → vista de lectura de la sección elegida.
- **FR-003**: La sección **Estadísticas** MUST presentar en orden estable los 16 atributos de solo lectura del residente (Nombre, Nivel, Barra de nivel, Dinero, Día/Mes/Año de cumpleaños, Edad directa, Actividad, Audacia, Sentido común, Alegría, Sociabilidad, Vínculo, Estado de ánimo y Saciedad) con formato legible, excluyendo cualquier acción de edición, creación, borrado o importación/exportación.
- **FR-004**: La sección **Palabras** MUST mostrar las 12 plazas de frases del residente con su tipo, texto guardado y pronunciación, distinguiendo explícitamente plazas vacías y pronunciaciones vacías.
- **FR-005**: La sección **Relaciones** MUST listar los vínculos del residente con otros residentes ocupados mostrando el nombre del otro residente, tipo de relación, medidor de afinidad, etiqueta descriptiva de rango cuando aplique e indicador de pelea/conflicto activo, así como un estado vacío claro cuando no existan relaciones.
- **FR-006**: La sección **Pertenencias** MUST mostrar el atuendo equipado y las pertenencias del residente resueltos contra los catálogos de nombres legibles, usando etiquetas de reserva comprensibles para códigos desconocidos o espacios vacíos.
- **FR-007**: La sección **Hábitos** MUST organizar y mostrar los hábitos del residente agrupados en las 9 categorías del juego, indicando claramente su estado (poseído, no poseído o nunca obtenido) y el conteo de hábitos poseídos por categoría.
- **FR-008**: La sección **Vivienda** MUST indicar si el residente está alojado o sin vivienda; cuando esté alojado, MUST mostrar el nombre de la casa (con reserva numerada si los nombres del mapa no están disponibles), número de habitación, compañeros de vivienda y aviso explícito ante conflictos de habitación compartida.
- **FR-009**: La sección **Resumen social** MUST presentar un resumen de solo lectura de la red social del residente (conteos por tipo de relación y lista agrupada de residentes vinculados con su rango).
- **FR-010**: Cuando falte un campo opcional o contenga un código desconocido o fuera de rango (o falten datos complementarios de nombres de casas), la vista MUST degradar localmente mostrando una etiqueta de reserva clara sin bloquear ni descartar los datos válidos restantes.
- **FR-011**: Todas las acciones esenciales de la pestaña Mii (recorrer hasta 70 residentes, abrir un residente, entrar en cada una de las 7 secciones, desplazarse por listas largas, actualizar/reintentar, volver al nivel anterior o al selector de pestañas, cambiar de perfil y abrir el menú Guardar/Salir) MUST completarse tanto exclusivamente con mando (`ZL`/`ZR`, `D-pad`/stick, `A`, `B`, `Plus`) como exclusivamente con pantalla táctil, sin atajos exclusivos por sección ni subpestañas.
- **FR-012**: Al regresar con `B` desde una sección al detalle del residente, desde el detalle a la lista de residentes, o al cambiar entre pestañas principales y cancelar diálogos globales, la aplicación MUST conservar la cuenta activa, el elemento enfocado y la posición de desplazamiento mientras los datos sigan vigentes.
- **FR-013**: Confirmar un cambio de cuenta desde la barra superior MUST limpiar los datos de la cuenta anterior, invalidar lecturas pendientes, cerrar vistas hijas ligadas a la cuenta anterior y cargar únicamente los residentes de la nueva cuenta; cancelar o fallar el selector MUST conservar la sesión previa sin alteraciones.
- **FR-014**: La pestaña Mii MUST distinguir explícitamente los estados de sin perfil seleccionado, cargando, listo con residentes, isla vacía (0 residentes), cuenta sin guardado, acceso fallido y datos dañados o incompatibles, ofreciendo acciones claras para Actualizar/Reintentar, elegir otra cuenta o volver.
- **FR-015**: Abandonar la pestaña Mii durante una carga en curso MUST suspender la publicación de resultados sobre vistas inactivas; regresar a la pestaña MUST ejecutar una única lectura vigente conservando el contexto.
- **FR-016**: Toda ruta de la pestaña Mii MUST operar estrictamente en modo de solo lectura respecto al guardado del juego y la configuración; MUST NOT abrir el guardado para escritura ni crear, modificar, reparar, respaldar, borrar, importar, exportar o confirmar archivos de partida.
- **FR-017**: El lector de datos de residentes MUST aplicar límites acotados de tamaño y validación estructural, rechazando entradas truncadas, sobredimensionadas o con conteos/desplazamientos inválidos mediante error recuperable sin cierres inesperados.
- **FR-018**: Todas las etiquetas, nombres de sección, campos de estadísticas, tipos de palabra/relación/ánimo, categorías de hábitos y mensajes de estado MUST ser localizables y estar disponibles en inglés, español y español latinoamericano (con fallback al inglés), legibles a 1280 × 720 en temas claro y oscuro y en modo dock.
- **FR-019**: La migración de la pestaña Mii MUST mantener intactas las pestañas ya migradas (Texturas, Ajustes de texturas con sus borradores y Jugador en solo lectura) así como las capacidades del editor legado y WebUI.

### Key Entities *(include if feature involves data)*

- **Cuenta activa**: perfil seleccionado en el encabezado global cuya identidad delimita la lectura y el aislamiento en memoria de los residentes.
- **Resumen de residente Mii**: registro validado de una plaza ocupada (1–70) con nombre, número de plaza, estado resumido y acceso a sus 7 secciones de solo lectura.
- **Secciones del residente**: las 7 agrupaciones consultables de cada residente —Estadísticas (16 campos), Palabras (12 plazas), Relaciones (pares con otros residentes), Pertenencias (atuendo y objetos), Hábitos (9 categorías), Vivienda (casa, habitación, compañeros y conflictos) y Resumen social (agregados y grupos de vínculos).
- **Contexto de navegación Mii**: cuenta asociada, identificador de generación de lectura, residente seleccionado, sección enfocada y posición de desplazamiento por nivel.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: En guardados sintéticos de referencia para dos cuentas distintas (incluyendo 0 residentes, 1 residente, plazas dispersas y 70 residentes), el 100 % de los residentes listados y de los datos de sus 7 secciones coincide exactamente con los valores reales de cada cuenta, sin mostrar datos ficticios ni mezclar información entre cuentas.
- **SC-002**: El 100 % de las acciones esenciales (listar residentes, abrir detalle, recorrer las 7 secciones, volver atrás, actualizar/reintentar y navegar pestañas) se completa solo con mando y se repite solo con pantalla táctil sin requerir el otro método ni subpestañas.
- **SC-003**: En 10 recorridos consecutivos de entrada/salida (lista → residente → sección → residente → lista) y cambios entre pestañas principales o diálogos globales cancelados, se conserva en el 100 % de los casos el residente enfocado, la sección enfocada y la posición de desplazamiento.
- **SC-004**: Cada condición de sin perfil, isla vacía (0 residentes), cuenta sin guardado, acceso denegado, archivo truncado/corrupto, tamaño excesivo, nombres de casas ausentes y códigos fuera de catálogo produce su estado o aviso previsto y permite recuperarse mediante Actualizar/Reintentar o cambio de cuenta sin reiniciar la aplicación.
- **SC-005**: La indicación de carga o la lista de hasta 70 residentes aparece en menos de 1 segundo tras entrar a la pestaña o confirmar cuenta en condiciones de referencia, y las órdenes de navegación durante la lectura responden en menos de 250 ms sin bloquear la interfaz.
- **SC-006**: Tras recorrer todas las rutas de lectura de residentes, las 7 secciones, errores, cambios de cuenta y salida, el inventario y los bytes de los archivos de guardado permanecen 100 % idénticos al inicio, con cero intentos de apertura escribible, mutación o confirmación.
- **SC-007**: La lista de residentes, las 7 secciones, nombres Unicode largos y avisos se muestran sin solapamientos ni recortes ilegibles en inglés, español y español latinoamericano, en temas claro y oscuro a 1280 × 720 y en modo dock; la evidencia de host, emulador y consola física se reporta por separado.

## Assumptions

- **Alcance estrictamente de solo lectura (`M2`)**: Editar estadísticas, frases, relaciones (incluyendo acciones por lotes Know/Boost/Zero), pertenencias, hábitos o vivienda, así como crear/borrar residentes, importar/exportar archivos `.ltd` y conectar con ShareMii quedan explícitamente fuera de alcance en `M2` y pertenecen a `M6` tras completar la compuerta de backups e integridad en `M4`.
- **Nombres de casas opcionales**: Los nombres personalizados de las casas provienen del archivo de mapa del guardado; su ausencia o fallo de lectura se degrada localmente a etiquetas numeradas de reserva sin impedir la consulta de los residentes.
- **Nombres de catálogo de objetos/hábitos/rangos**: Donde el editor legado ya cuenta con catálogos en inglés generados a partir de los datos del juego (prendas, objetos, hábitos y etiquetas de rango), se reutilizan como base consistente y se mantienen localizables las etiquetas de interfaz, categorías, estados y encabezados en los tres idiomas soportados.
- **Fuente única de perfil**: La pestaña Mii reutiliza el selector global de perfil en el encabezado introducido en `M0` y compartido con `M1`; no añade un selector de cuenta independiente.
- **No persistencia de selección entre cierres**: No se requiere recordar el último residente ni la última sección consultada entre reinicios de la aplicación ni escribir preferencias nuevas en disco.
- **Datos sintéticos para pruebas**: Todas las pruebas automatizadas y de referencia utilizan datos sintéticos o sanitizados; los guardados personales no entran al repositorio, registros ni fixtures.
- **Marco de referencia**: Requisitos alineados con [README](../../README.md), [Roadmap](../../docs/ROADMAP.md), [constitución](../../.specify/memory/constitution.md), [guía de interfaz](../../docs/UI_GUIDELINES.md) y [límites de migración](../../docs/BOREALIS_MIGRATION.md).
