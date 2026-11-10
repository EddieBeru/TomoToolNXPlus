# Feature Specification: Navegador de texturas reales de solo lectura

**Feature Branch**: `main` (rama actual; esta ejecución no crea una rama)

**Created**: 2026-11-09

**Status**: Draft

**Input**: User description: "Migrar el navegador de texturas de Borealis de datos de ejemplo a datos reales en modo de solo lectura, con navegación por control y touch, vistas previas, estados de carga/vacío/error y preservación del contexto al volver de detalles. No habilitar escrituras en saves."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Explorar las texturas de una cuenta (Priority: P1)

Como usuario, quiero elegir mi cuenta y consultar sus texturas reales para reconocer qué
contenido existe sin arriesgar mi guardado.

**Why this priority**: Sustituye los ejemplos por información útil y establece el límite
de lectura del que dependen los detalles.

**Independent Test**: Con una colección sintética de contenido conocido, seleccionar una
cuenta y recorrer el listado, incluidos sus extremos, primero solo con control y después
solo con touch. Comparar los elementos y sus identidades con la colección esperada.

**Acceptance Scenarios**:

1. **Given** varias cuentas disponibles, **When** el usuario entra en Texturas y elige una,
   **Then** ve la cuenta activa y únicamente los elementos de su guardado, ordenados por
   identificador de manera estable y sin datos ficticios.
2. **Given** una colección válida, **When** se recorre con D-pad o stick y A, o con touch,
   **Then** todos los elementos y acciones esenciales son alcanzables, el foco permanece
   visible y desplazarse no activa elementos accidentalmente.
3. **Given** una colección con nombres personalizados, nombres ausentes y nombres repetidos,
   **When** se muestra el listado, **Then** los nombres válidos se conservan y cada elemento
   sigue siendo distinguible por su identificador; sin nombre se usa un identificador legible.
4. **Given** una cuenta activa, **When** se elige otra, **Then** no se mezclan elementos,
   nombres ni vistas previas entre cuentas y se inicia un contexto propio para la nueva.
5. **Given** se cancela la selección de cuenta, **When** se vuelve al navegador,
   **Then** se conserva la sesión previa o se vuelve al punto de entrada si no existía una.

---

### User Story 2 - Inspeccionar una textura y volver al mismo lugar (Priority: P2)

Como usuario, quiero abrir una textura, ver su imagen y sus datos de identificación y volver
al listado sin perder mi posición.

**Why this priority**: La imagen permite reconocer el contenido y la restauración del
contexto hace posible revisar una colección larga.

**Independent Test**: Abrir una entrada situada fuera de la primera pantalla, inspeccionar
su detalle y volver, alternando control y touch. Repetir con una textura rectangular,
una con transparencia y una que no pueda previsualizarse.

**Acceptance Scenarios**:

1. **Given** un elemento con imagen compatible, **When** se abre con A o un toque,
   **Then** aparece su vista previa con proporciones correctas, nombre, identificador,
   dimensiones de la imagen mostrada y origen de la vista previa; la transparencia se
   distingue del fondo y la imagen completa puede inspeccionarse sin recorte obligatorio.
2. **Given** un elemento con lienzo rectangular válido, **When** se abre su detalle,
   **Then** se prioriza ese lienzo; en los demás casos se muestra la textura principal.
   Si el lienzo rectangular falla pero la principal es válida, se muestra esta con un aviso.
3. **Given** el detalle de un elemento, **When** se pulsa B o el control táctil Volver,
   **Then** se restaura el foco en el elemento de origen y la posición de desplazamiento
   del listado, sin volver a elegir cuenta.
4. **Given** un listado ya consultado, **When** se cambia de pestaña y se regresa o se visita
   Texture Settings y se vuelve, **Then** se conservan cuenta, selección y desplazamiento;
   una preferencia sin guardar no se confirma ni descarta por esta navegación.
5. **Given** se solicita otra imagen o cuenta mientras hay una carga pendiente, **When**
   termina la carga anterior, **Then** no reemplaza la imagen ni los datos de la selección actual.

---

### User Story 3 - Entender y recuperar estados sin contenido o con fallos (Priority: P1)

Como usuario, quiero distinguir una colección vacía de un fallo y poder reintentar o volver
sin que el programa invente contenido o cambie mis archivos.

**Why this priority**: La lectura real puede fallar; una lista vacía engañosa o un fallo
que bloquee la navegación impide utilizar el navegador de forma segura.

**Independent Test**: Probar cuenta sin guardado, colección vacía, acceso denegado, lectura
incompleta, imagen corrupta y lectura lenta. Recuperar el origen y usar Reintentar.

**Acceptance Scenarios**:

1. **Given** una lectura en curso, **When** se espera el resultado, **Then** se muestra
   actividad sin porcentajes inventados, se evitan solicitudes duplicadas y se puede volver
   o salir del navegador sin bloquear la interfaz ni publicar resultados obsoletos.
2. **Given** una lectura completa con cero texturas, **When** termina, **Then** se informa
   que la colección está vacía con acciones para reintentar, cambiar cuenta o volver.
3. **Given** no hay cuentas, falta el guardado o falla su acceso o el recorrido de la
   colección, **When** se intenta cargar, **Then** se informa la causa recuperable sin
   presentar una colección vacía como resultado válido; no se crea ni repara un guardado.
4. **Given** un elemento cuya imagen es corrupta o incompatible, **When** se abre,
   **Then** conserva su identidad y muestra un error local con Reintentar y Volver;
   los demás elementos siguen siendo navegables y no se muestra la imagen de otro elemento.
5. **Given** un error recuperable ya corregido, **When** se reintenta, **Then** se actualiza
   el resultado sin reiniciar la aplicación y sin cambiar el guardado.
6. **Given** desaparece un elemento tras actualizar, **When** se reconstruye el listado,
   **Then** el foco pasa al siguiente elemento en su posición anterior, al último si esa
   posición ya no existe o a una acción útil si el listado queda vacío.

### Edge Cases

- Cuentas con el mismo nombre visible: deben distinguirse sin exponer identificadores
  personales innecesarios; ninguna selección puede reutilizar datos de la otra.
- Directorio UGC ausente: se diferencia de un directorio leído correctamente y vacío;
  se explica la ausencia y se permite reintentar o cambiar cuenta, sin crearlo.
- Miniatura o lienzo opcional ausente: no elimina una entrada con textura principal;
  un archivo auxiliar sin textura principal no crea una entrada independiente.
- Nombres largos, Unicode o repetidos: el contenido esencial se puede consultar en detalle
  sin solapamientos; la identidad no depende del nombre visible.
- Nombres de usuario no disponibles: se informa la limitación y se usa el identificador
  de la textura sin bloquear imágenes válidas ni inventar nombres personalizados.
- Datos truncados, dimensiones inválidas o contenido excesivo para los límites admitidos:
  se rechazan con error recuperable, sin cierre inesperado ni intento de reparación.
- Recorrido interrumpido: un listado parcial no se presenta como colección completa;
  el estado informa el fallo y permite reintentar.
- Desplazamiento táctil, entrada mantenida y apertura de diálogos: no activan controles
  detrás de la capa actual ni generan una doble activación.
- Actualización o cierre durante una carga: no publica datos en una vista ya cerrada.
- Salida de la aplicación: no ofrece guardar cambios del navegador, puesto que no existen;
  respeta por separado los borradores de preferencias que ya proporciona la aplicación.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: El navegador MUST sustituir las entradas ficticias de Texturas por la colección
  UGC real del guardado de la cuenta elegida. MUST permitir elegir y cambiar cuenta y
  mostrar la cuenta activa. Cancelar la elección MUST respetar el escenario 1.5.
- **FR-002**: Cada entrada MUST corresponder a una textura principal reconocida; MUST asociar
  sus archivos auxiliares sin listarlos como elementos independientes. MUST conservar un
  identificador estable dentro de su cuenta y ordenar por identificador sin distinguir
  mayúsculas; un empate MUST resolverse con el identificador original.
- **FR-003**: Las entradas MUST mostrar su nombre personalizado cuando sea válido y accesible,
  junto con el identificador. La ausencia o fallo de los nombres MUST producir un fallback
  legible y un aviso si hubo un fallo de lectura, sin inutilizar la colección válida.
- **FR-004**: Abrir una entrada MUST mostrar exclusivamente sus datos y una vista previa
  completa que respete proporciones y transparencia. MUST seguir la prioridad y el fallback
  del escenario 2.2, informar el origen y las dimensiones mostradas y no confundir un fallo
  con una imagen válida. No se requiere selector entre variantes ni herramientas de edición.
- **FR-005**: Listado, detalles, selección de cuenta, actualización, recuperación y salida
  MUST ser utilizables solo con control y solo con touch. MUST respetar ZL/ZR para pestañas
  principales, D-pad/stick para foco, A para activar y B para volver en el ámbito actual.
- **FR-006**: El foco MUST permanecer visible y distinguirse de selección o deshabilitado.
  Modales MUST aislar entrada; el cambio entre touch y control MUST conservar contexto.
  No se añadirán subtabs ni atajos obligatorios por acción. Minus/Plus, si están disponibles,
  MUST conservar sus funciones globales y no exponer escrituras desde este navegador.
- **FR-007**: Volver de detalles o preferencias y regresar desde otra pestaña MUST conservar
  cuenta, identidad seleccionada y desplazamiento mientras sigan válidos. Actualizar MUST
  conservar la identidad si existe y aplicar el fallback del escenario 3.6 si desaparece.
- **FR-008**: El navegador MUST exponer estados diferenciados de carga, contenido, vacío y
  error. Una ausencia de guardado, acceso fallido o lectura incompleta MUST NOT presentarse
  como una colección vacía leída con éxito. Los errores MUST ofrecer un siguiente paso útil.
- **FR-009**: El usuario MUST poder actualizar la colección y reintentar cargas fallidas.
  Una solicitud MUST mostrar actividad inmediatamente, impedir duplicados y conservar una
  vía de navegación de salida. Resultados obsoletos MUST NOT sustituir la selección actual.
- **FR-010**: Un fallo de previsualización MUST limitarse al elemento afectado, conservar
  su identidad, eliminar imágenes anteriores y permitir Reintentar o Volver. Los límites
  admitidos de datos e imágenes MUST definirse en la planificación y rechazarse con mensaje
  cuando se excedan, manteniendo la aplicación utilizable.
- **FR-011**: Todas las rutas del navegador, incluidas selección de cuenta, lectura de
  nombres, actualización, reintento, errores y salida, MUST ser exclusivamente de lectura
  respecto al guardado. MUST NOT escribir, crear, borrar, renombrar, reparar ni confirmar
  cambios en sus archivos o contenedor, ni ejecutar tareas automáticas que lo hagan.
- **FR-012**: Cambiar cuenta MUST invalidar sus datos pendientes y separar nombres, entradas
  e imágenes. Datos personales del guardado MUST NOT copiarse a registros, fixtures ni
  archivos nuevos como efecto de navegar. No se requiere persistir el contexto entre reinicios.
- **FR-013**: Los textos nuevos MUST ser localizables y estar disponibles en inglés, español
  y español latinoamericano, con fallback al inglés para otros idiomas. Nombres largos,
  traducciones y estados relevantes MUST ser legibles a resolución nativa, en temas claro
  y oscuro; MUST verificarse el escalado en modo dock para las vistas nuevas.
- **FR-014**: Texture Settings MUST seguir siendo accesible y conservar su comportamiento
  de borrador, guardado explícito y descarte. El navegador MUST NOT modificar preferencias
  al explorar ni cambiar las capacidades del editor SDL o de WebUI.

### Key Entities *(include if feature involves data)*

- **Cuenta de origen**: propietario del guardado, identidad estable y nombre visible;
  delimita toda la información y los resultados de lectura de una sesión.
- **Colección de texturas**: conjunto ordenado de entradas de una cuenta, con estado de
  carga y resultado completo, vacío o fallido; no es un inventario de todos los slots posibles.
- **Entrada de textura**: identificador, nombre opcional, textura principal y referencias
  opcionales a lienzo y miniatura. Su identidad es independiente de su posición y nombre.
- **Vista previa**: representación de la imagen seleccionada, origen, dimensiones y estado
  propio de carga/error; pertenece a una entrada y cuenta concretas.
- **Contexto de navegación**: cuenta, identidad seleccionada, foco y desplazamiento de la
  sesión. No supone cambios pendientes en el guardado.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: En colecciones de referencia vacía, de una entrada y de 100 entradas mixtas,
  el 100 % de las texturas principales esperadas aparece una sola vez, sin ejemplos ni
  archivos auxiliares independientes; nombres e imágenes corresponden a la cuenta elegida.
- **SC-002**: El 100 % de las acciones esenciales de las tres historias se completa con
  control y se repite con touch, sin depender del otro método ni de atajos adicionales.
- **SC-003**: En 20 ciclos consecutivos de abrir detalle y volver, y 10 cambios de pestaña,
  se restaura la misma entrada y posición visible en el 100 % de los casos sin cambios
  del origen. Al eliminar una entrada de prueba, se cumple el fallback definido.
- **SC-004**: Cada combinación de cuenta sin guardado, colección vacía, acceso fallido,
  lectura incompleta, nombres no disponibles e imagen corrupta produce su estado previsto;
  los fallos recuperables permiten reintentar sin reiniciar y sin mostrar datos de otra cuenta.
- **SC-005**: En Switch físico con una colección de referencia de 100 entradas, la actividad
  de carga aparece en menos de 1 segundo y al menos el 95 % de las órdenes de navegación
  durante la carga recibe respuesta visible en menos de 250 ms. No se exige un porcentaje
  ni un tiempo total de carga inventado para orígenes de velocidad variable.
- **SC-006**: Tras ejecutar todas las rutas de aceptación, los archivos del guardado y su
  inventario permanecen idénticos al inicio y no se ha intentado ninguna mutación ni commit,
  tanto en éxito como con fallos inyectados. Esta comprobación usa datos sintéticos o
  sanitizados y no incorpora guardados personales al repositorio.
- **SC-007**: Las vistas nuevas se revisan sin solapamientos ni controles esenciales ocultos
  en los tres idiomas incluidos y ambos temas a 1280 x 720, y se comprueba el modo dock.
  Los resultados de host, emulador y Switch físico se informan por separado; una comprobación
  pendiente impide afirmar validación de hardware, no se sustituye por una compilación.

## Assumptions

- «Datos reales» significa el contenido UGC existente del guardado de Tomodachi Life de
  una cuenta de la consola, siguiendo la fuente del navegador legado. No incluye elegir
  carpetas arbitrarias, copias externas ni servicios remotos.
- La selección de cuenta forma parte de esta función porque Borealis aún no accede a
  guardados. No se exige seleccionar cuenta al iniciar otras secciones de la aplicación.
- La colección inicial sigue las texturas principales reconocidas por el navegador legado,
  sin inventar slots vacíos ni descartar entradas por carecer de nombre o auxiliares.
- La planificación debe verificar formatos, límites y acceso sin mutaciones antes de
  habilitar lectura de guardados. La existencia de utilidades legadas no prueba su seguridad.
- Importar, exportar, crear, eliminar, renombrar, editar, reparar y guardar texturas quedan
  fuera de alcance. También quedan fuera filtros nuevos, búsqueda, zoom interactivo,
  selección entre variantes y la migración de Mii, Player, Map o del servidor WebUI.
- No se necesitan backups nuevos para navegar sin mutaciones; esta decisión no exime
  ninguna escritura futura de las garantías de la constitución.
- El contexto se conserva durante la sesión, no después de cerrar la aplicación. Al cambiar
  de cuenta se inicia un contexto nuevo para evitar confundir fuentes; volver de detalles
  o pestañas conserva el contexto de la cuenta activa.
- Se depende de cuentas y guardados accesibles en una consola compatible. Para verificar
  fallos, preservación y aislamiento se usan colecciones sintéticas o sanitizadas.
- Esta función cubre listado, identificación, previsualización y navegación de Texturas.
  Las operaciones de modificación/exportación y las demás pantallas siguen pendientes;
  completarla no constituye paridad total ni reemplazo completo del editor legado.
- Base de requisitos: [README](../../README.md),
  [constitución](../../.specify/memory/constitution.md),
  [guía de interfaz](../../docs/UI_GUIDELINES.md) y
  [límites de migración](../../docs/BOREALIS_MIGRATION.md).
