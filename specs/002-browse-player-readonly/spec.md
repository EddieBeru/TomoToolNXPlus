# Feature Specification: Consulta de datos reales del jugador e isla en solo lectura (M1)

**Feature Branch**: `main` (rama actual; esta ejecución no crea una rama)

**Created**: 2026-11-19

**Status**: Draft

**Milestone**: `M1` ([Roadmap](../../docs/ROADMAP.md))

**Input**: User description: "M1 — Reemplazar los datos de muestra de la pestaña Player por la consulta real en modo solo lectura de los datos del jugador y de la isla para la cuenta seleccionada."

## Clarifications

### Session 2026-11-19

- Q: ¿Cómo prefieres presentar los 15 atributos del jugador y sus metadatos secundarios (pronunciación, idioma, deseos liberados, días de donación y descripción) en la pestaña Jugador? (FR-002) → A: Todo integrado en la lista (cada fila muestra su valor principal y debajo sus metadatos secundarios y descripción multilínea, como en Ajustes de texturas, sin abrir subpantallas).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Consultar la identidad y el estado general del jugador y la isla (Priority: P1)

Como usuario, quiero ver en la pestaña Jugador los datos reales del jugador y de la isla correspondientes a mi cuenta seleccionada, en lugar de entradas ficticias, para verificar su estado sin arriesgar mi partida guardada.

**Why this priority**: Elimina los datos de muestra de la pestaña Jugador y entrega información útil y real de la partida desde un acceso estrictamente de solo lectura.

**Independent Test**: Con dos guardados sintéticos de cuentas distintas cuyos 15 atributos y valores asociados sean conocidos, seleccionar la cuenta A, entrar en Jugador y confirmar que cada atributo muestra su valor real; después cambiar a la cuenta B desde el selector de perfil global y verificar que los valores se actualizan sin mezclarse con los de A. Repetir el recorrido solo con mando y solo con pantalla táctil.

**Acceptance Scenarios**:

1. **Given** una cuenta seleccionada con una partida válida de Tomodachi Life, **When** el usuario abre la pestaña Jugador, **Then** desaparecen las entradas de ejemplo y se muestran en orden estable los 15 atributos reales de la partida:
   - Nombre del jugador (con su pronunciación e idioma configurados).
   - Nombre de la isla (con su pronunciación e idioma configurados).
   - Saldo de dinero (formateado de forma legible).
   - Moneda regional (símbolo y denominación reconocible).
   - Conteo de inicios del juego.
   - Tono de piel (identificado dentro de los 6 tonos del juego).
   - Día de cumpleaños (1–31).
   - Mes de cumpleaños (1–12).
   - Año de cumpleaños.
   - Tamaño de la isla (con las dimensiones correspondientes a los 4 tamaños estándar o indicación explícita de valor no estándar).
   - Nivel de la fuente de los deseos.
   - Deseos realizados (junto con el progreso de deseos liberados respecto al total del catálogo).
   - Región configurada (nombre legible de las 9 regiones o indicador de valor desconocido).
   - Estado de penalización por cambio de hora (activa o inactiva/limpia).
   - Última donación recogida (fecha legible y días transcurridos, o indicación de nunca recogida).
2. **Given** dos cuentas con datos de jugador diferentes, **When** el usuario cambia de cuenta desde la barra superior, **Then** se limpian los datos de la cuenta anterior, se cancela cualquier lectura pendiente de ella y se muestran únicamente los atributos de la nueva cuenta.
3. **Given** una sesión activa en la pestaña Jugador, **When** el usuario abre el selector de perfil y lo cancela, **Then** se conservan la cuenta activa, los datos mostrados y la posición de foco actual sin recargar ni perder contexto.
4. **Given** una pestaña Jugador con datos cargados, **When** el usuario recorre todos los atributos y acciones disponibles solo con mando (`D-pad`/stick y `A`) y posteriormente solo con pantalla táctil, **Then** todos los elementos esenciales son alcanzables y operables, el foco permanece visible y desplazarse por la pantalla no activa controles por accidente.

---

### User Story 2 - Inspeccionar el detalle completo de cada atributo y conservar el contexto (Priority: P2)

Como usuario, quiero consultar la descripción y los datos secundarios de cada atributo del jugador —incluso cuando los textos son largos— y mantener mi lugar si cambio de pestaña o consulto menús globales.

**Why this priority**: Algunos atributos incluyen metadatos adicionales (pronunciación, idioma, desglose de deseos liberados, días transcurridos desde la donación, dimensiones de isla o descripciones explicativas) que ayudan a entender el estado real de la partida sin abarrotar la vista principal ni perder la posición al navegar.

**Independent Test**: Seleccionar un atributo situado fuera de la primera pantalla (por ejemplo, Deseos, Región o Donación), revisar su descripción completa y valores secundarios, cambiar de pestaña principal y regresar 10 veces consecutivas, y abrir y cancelar el menú global de salida. Comprobar que el foco y el desplazamiento regresan al mismo atributo.

**Acceptance Scenarios**:

1. **Given** atributos con metadatos secundarios o textos largos (como Nombre del jugador, Nombre de la isla, Deseos, Tamaño de la isla o Última donación), **When** el usuario recorre la lista de la pestaña Jugador, **Then** cada atributo muestra en su fila el título y valor principal y, justo debajo de forma integrada en la misma lista (sin abrir subpantallas ni diálogos por campo), sus valores secundarios (pronunciación e idioma, deseos liberados frente al total, días transcurridos desde la donación o dimensiones de la isla) y su descripción multilínea sin recorte irreversible ni solapamientos.
2. **Given** una posición de foco y desplazamiento en la pestaña Jugador, **When** el usuario cambia a otra pestaña principal y regresa, o abre y cancela el selector de perfil o el menú Guardar/Salir, **Then** se restauran la cuenta activa, el atributo enfocado y la posición de desplazamiento.
3. **Given** que el usuario navega desde cualquier control de la pestaña Jugador, **When** pulsa `B` (o la acción equivalente para volver a secciones), **Then** el foco regresa al selector de pestañas principales respetando el modelo global de navegación, sin introducir subpestañas ni atajos exclusivos por atributo.

---

### User Story 3 - Distinguir y recuperar estados sin guardado, incompletos o con fallos de lectura (Priority: P1)

Como usuario, quiero distinguir claramente si la pestaña está cargando, si falta elegir cuenta, si la cuenta no tiene partida, si los datos están dañados o si un campo falta, y poder reintentar sin reiniciar ni alterar archivos.

**Why this priority**: Las lecturas sobre guardados reales pueden fallar o encontrarse con cuentas vacías o campos ausentes; confundir un error con valores en cero o bloquear la interfaz compromete la confianza del usuario.

**Independent Test**: Probar sesión sin perfil elegido, cuenta sin guardado del juego, acceso denegado, archivo de jugador ausente/truncado/corrupto, archivo sobredimensionado, carga lenta interrumpida al cambiar de pestaña y campos individuales ausentes o fuera de catálogo. Recuperar el origen y activar Actualizar/Reintentar sin reiniciar la aplicación.

**Acceptance Scenarios**:

1. **Given** que aún no se ha elegido una cuenta en la barra superior, **When** el usuario entra en Jugador, **Then** ve un aviso claro invitando a elegir un perfil desde la barra superior, sin mostrar valores inventados ni ficticios.
2. **Given** una lectura en curso, **When** tarda en completarse, **Then** se muestra actividad de carga sin porcentajes inventados, se bloquean solicitudes duplicadas y el usuario puede cambiar de pestaña o abrir el menú de salida sin bloquear la interfaz.
3. **Given** que el usuario sale de la pestaña Jugador mientras una lectura está en curso, **When** cambia a otra pestaña y regresa más tarde, **Then** el trabajo anterior se suspende sin publicar resultados en vistas inactivas y al regresar se lanza una única lectura vigente conservando la cuenta y contexto.
4. **Given** una cuenta sin guardado del juego, un error de acceso o datos del jugador corruptos, truncados o sobredimensionados, **When** termina el intento de lectura, **Then** se muestra el motivo específico y recuperable con acciones de Actualizar/Reintentar y cambio de cuenta, sin inventar valores en cero ni intentar crear o reparar archivos.
5. **Given** un error recuperable ya corregido en el origen, **When** el usuario activa Actualizar/Reintentar con mando o toque, **Then** la pestaña vuelve a leer los datos y muestra los atributos reales sin necesidad de reiniciar la aplicación.

### Edge Cases

- **Pronunciación vacía o texto largo/Unicode**: Si la pronunciación está vacía, debe indicarse de forma clara (mostrando el nombre principal como referencia o un marcador explícito de vacío según corresponda) sin fallar; nombres con caracteres Unicode o longitud máxima deben leerse íntegramente sin solapamientos.
- **Valores enumerados o numéricos fuera de catálogo**: Si moneda, región, idioma, tono de piel o tamaño de isla contienen un código no reconocido o fuera de los valores estándar (por ejemplo, tamaño distinto de 1–4), debe mostrarse una etiqueta de reserva comprensible con advertencia explícita en lugar de colapsar o inventar un valor válido.
- **Donación nunca recogida o fecha futura**: Una marca de tiempo igual a cero debe mostrarse como nunca recogida; una fecha posterior al reloj actual de la consola debe indicarse explícitamente como fecha futura sin mostrar conteos negativos confusos.
- **Tabla de deseos ausente o incompleta**: Si el guardado contiene el contador de deseos realizados pero falta la lista de deseos desbloqueados, debe mostrarse el contador disponible e indicarse que el desglose de deseos liberados no está disponible sin invalidar los demás atributos.
- **Cambios concurrentes de cuenta o cierre de aplicación**: Si el usuario cambia de cuenta o solicita salir mientras hay una lectura activa, los resultados obsoletos deben descartarse antes de actualizar la vista y la salida debe esperar el cierre seguro de las lecturas activas sin ofrecer ni ejecutar guardado de partida.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: La pestaña Jugador MUST reemplazar todas las entradas ficticias por la consulta real de los atributos del jugador e isla pertenecientes a la cuenta seleccionada en el encabezado global.
- **FR-002**: La pestaña MUST presentar en orden estable y reconocible los 15 atributos principales (Nombre, Isla, Dinero, Moneda, Conteo de inicios, Tono de piel, Día/Mes/Año de cumpleaños, Tamaño de isla, Nivel de fuente, Deseos, Región, Penalización por cambio de hora y Última donación) con formato legible y todos sus datos secundarios asociados (pronunciación e idioma de nombre e isla, símbolo y nombre de moneda, dimensiones del tamaño de isla, deseos liberados frente al total del catálogo, estado activo/limpio de penalización temporal y fecha junto con días transcurridos desde la donación). Cada atributo MUST presentarse de forma integrada en la propia lista desplazable (fila con título y valor principal seguida debajo por sus metadatos secundarios y descripción multilínea, al estilo de Ajustes de texturas), sin abrir pantallas hijas ni diálogos por atributo.
- **FR-003**: Cuando falte un campo opcional o contenga un código desconocido o fuera de rango, la vista MUST mostrar un indicador o valor de reserva claro para ese atributo sin bloquear ni descartar los atributos válidos restantes.
- **FR-004**: Todas las acciones de la pestaña Jugador (navegar por los 15 atributos, consultar descripciones y metadatos secundarios integrados en la lista, donde enfocar o tocar una fila de atributo sitúa el foco en ella sin abrir subpantallas, actualizar/reintentar, volver al selector de pestañas, cambiar de perfil y abrir el menú de salida) MUST poder completarse tanto exclusivamente con mando como exclusivamente con pantalla táctil.
- **FR-005**: La interacción MUST seguir el modelo global de navegación (`ZL`/`ZR` solo para pestañas principales, `D-pad`/stick izquierdo para foco, `A` para activar/entrar, `B` para volver o cancelar, `Plus` para Guardar/Salir), manteniendo foco visible y distinto de selección o deshabilitado, y sin introducir subpestañas ni atajos exclusivos por campo.
- **FR-006**: Cambiar entre pestañas principales o abrir y cancelar diálogos globales MUST conservar la cuenta activa, el atributo enfocado y el desplazamiento de la pestaña Jugador mientras la información siga vigente.
- **FR-007**: Confirmar un cambio de cuenta desde la barra superior MUST limpiar los atributos de la cuenta anterior, invalidar lecturas pendientes y cargar únicamente los datos de la nueva cuenta; cancelar o fallar el selector MUST conservar la sesión previa sin alteraciones.
- **FR-008**: La pestaña MUST distinguir explícitamente los estados de sin perfil seleccionado, cargando, listo, cuenta sin guardado, acceso fallido y datos dañados o incompatibles, ofreciendo acciones claras para Actualizar/Reintentar, elegir otra cuenta o volver.
- **FR-009**: Abandonar la pestaña Jugador durante una carga en curso MUST suspender el trabajo de actualización de vista para impedir entregas tardías sobre vistas inactivas; volver a la pestaña MUST ejecutar una única carga vigente conservando el contexto.
- **FR-010**: Toda ruta de la pestaña Jugador MUST operar estrictamente en modo de solo lectura respecto al guardado del juego y la configuración; MUST NOT abrir el guardado para escritura ni crear, modificar, reparar, respaldar o confirmar archivos de partida.
- **FR-011**: El lector de datos del jugador MUST aplicar límites acotados de tamaño y validación estructural, rechazando entradas truncadas, sobredimensionadas o con conteos/desplazamientos inválidos mediante error recuperable sin cierres inesperados.
- **FR-012**: Todas las etiquetas, nombres y descripciones de los 15 atributos, opciones de moneda/región/idioma/tono/tamaño y mensajes de estado MUST ser localizables y estar disponibles en inglés, español y español latinoamericano (con fallback al inglés), ser legibles a 1280 × 720 en temas claro y oscuro y conservar legibilidad en modo dock.
- **FR-013**: La migración de la pestaña Jugador MUST mantener intactos el navegador de Texturas de solo lectura, Ajustes de texturas (incluidos sus borradores) y las capacidades del editor legado y WebUI.

### Key Entities *(include if feature involves data)*

- **Cuenta activa**: perfil seleccionado en el encabezado global cuya identidad delimita la lectura y el aislamiento en memoria de los datos del jugador.
- **Resumen del jugador e isla**: conjunto validado de los 15 atributos del jugador y sus metadatos complementarios extraídos del guardado de la cuenta activa.
- **Atributo del jugador**: unidad consultable con título localizado, valor principal formateado, detalles secundarios/descripción y marca de validez o reserva.
- **Progreso de deseos**: relación entre los deseos realizados y la cantidad de deseos liberados frente al total del catálogo del juego.
- **Contexto de consulta del jugador**: cuenta asociada, identificador de generación de lectura, atributo enfocado y desplazamiento vertical en la sesión.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: En guardados sintéticos de referencia para dos cuentas distintas con valores conocidos, el 100 % de los 15 atributos y sus metadatos secundarios coincide exactamente con los valores reales de cada cuenta, sin mostrar ejemplos ficticios ni mezclar información entre cuentas.
- **SC-002**: El 100 % de las acciones esenciales de consulta, inspección de detalles, actualización/reintento y navegación de la pestaña Jugador se completa solo con mando y se repite solo con pantalla táctil sin requerir el otro método ni atajos adicionales.
- **SC-003**: En 10 cambios consecutivos entre pestañas principales y tras abrir y cancelar diálogos globales, se conserva en el 100 % de los casos el atributo enfocado y la posición de desplazamiento.
- **SC-004**: Cada condición de sin perfil, cuenta sin guardado, acceso denegado, archivo truncado/corrupto, tamaño excesivo y valor fuera de catálogo produce su estado o aviso previsto y permite recuperarse mediante Actualizar/Reintentar o cambio de cuenta sin reiniciar la aplicación.
- **SC-005**: La indicación de carga o el resumen leído aparece en menos de 1 segundo tras entrar a la pestaña o confirmar cuenta en condiciones de referencia, y las órdenes de navegación durante la lectura responden en menos de 250 ms sin bloquear la interfaz.
- **SC-006**: Tras recorrer todas las rutas de lectura, actualización, error, cambio de cuenta y salida, el inventario y los bytes de los archivos de guardado permanecen 100 % idénticos al inicio, con cero intentos de apertura escribible, mutación o confirmación.
- **SC-007**: Los 15 atributos, sus valores secundarios, descripciones y avisos se muestran sin solapamientos ni recortes ilegibles en inglés, español y español latinoamericano, en temas claro y oscuro a 1280 × 720 y en modo dock; la evidencia de host, emulador y consola física se reporta por separado.

## Assumptions

- **Alcance estrictamente de solo lectura (`M1`)**: Editar campos del jugador, cambiar moneda/región/idioma, limpiar la penalización temporal, ajustar la fecha de donación o desbloquear/reiniciar deseos queda explícitamente fuera de alcance en `M1` y pertenece a `M6` tras construir la compuerta de backups e integridad en `M4`.
- **Fuente única de verdad en la sesión**: La pestaña Jugador reutiliza la selección de perfil del encabezado global introducida en `M0`; no agrega un selector de cuenta independiente dentro de la pestaña.
- **No persistencia de contexto entre cierres**: No se requiere guardar el último atributo enfocado ni el desplazamiento entre reinicios de la aplicación, ni escribir preferencias nuevas en disco.
- **Datos sintéticos para pruebas**: Todas las pruebas automatizadas y de referencia utilizan datos sintéticos o sanitizados; los guardados personales no entran al repositorio, registros ni fixtures.
- **Marco de referencia**: Requisitos alineados con [README](../../README.md), [Roadmap](../../docs/ROADMAP.md), [constitución](../../.specify/memory/constitution.md), [guía de interfaz](../../docs/UI_GUIDELINES.md) y [límites de migración](../../docs/BOREALIS_MIGRATION.md).
