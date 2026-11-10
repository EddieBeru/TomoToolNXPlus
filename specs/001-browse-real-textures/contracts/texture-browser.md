# Contract: fuente de lectura y navegación de Texturas

Contrato de diseño para FR-001–FR-014. No añade endpoints HTTP ni cambia WebUI.
Modelo: [data-model.md](../data-model.md). Decisiones: [research.md](../research.md).

## Interfaz de aplicación

Las operaciones siguientes son conceptuales; firmas C++ concretas deben conservar
estas garantías y permitir una fuente falsa para los checks host.

| Operación | Entrada | Resultado | Garantías |
| --- | --- | --- | --- |
| ListAccounts | cancelación | lista completa o error tipado | cero cuentas no equivale a fallo de servicio |
| OpenReadOnly | AccountKey | sesión de lectura o error | metadata distingue ausencia; nunca crea ni abre escribible |
| Enumerate | sesión, generación | catálogo completo o error | solo `/Ugc`, orden estable, límites y auxiliares reconocidos |
| ReadNames | sesión | mapa de nombres + avisos | `/Player.sav`, ≤8 MiB; fallo no invalida catálogo |
| ReadImageBytes | referencia emitida por fuente, límite | bytes completos o error | solo archivos reconocidos de esa cuenta; tamaño y bytes verificados |
| LoadPreview | TextureId, generación, request ID | RGBA/origen/dimensiones o error local | prioridad canvas rectangular, fallback explícito |
| Refresh | contexto actual | nueva generación/catálogo | selección estable y fallback de posición |
| CancelDelivery | token/generación | invalidación inmediata | no terminar threads a la fuerza ni cerrar handles desde UI |
| CloseSession | sesión sin trabajo en uso | recursos liberados | archivos/directorios antes de filesystem; jamás commit |

El adaptador no expone paths arbitrarios, handles, Write, Commit, Create, Delete, Rename,
Repair, Import o Export. No se llama a SaveMount::Mount/SaveExists, MiiManager, Backup,
SaveWorkflow o helpers de escritura al seleccionar cuenta, navegar o salir.

Solo el worker posee la sesión de fuente y sus handles. La UI nunca llama libnx I/O.
Comprobar Result, tamaños no negativos y sumas/multiplicaciones sin overflow antes de
reservar. Lecturas parciales se acumulan con offset comprobado; cero bytes antes del
final esperado es ShortRead. Si el tamaño cambia durante lectura, fallar y permitir refresh.
Cierres libnx devuelven void: se garantiza ejecución, no se informa un éxito ficticio de
un cierre verificable. Cualquier seam host con error de cierre real debe propagarlo.

### Publicación asíncrona

- Evento: tipo, AccountKey, generación de catálogo, request ID si corresponde y payload.
- Se publica en el thread UI solo si sigue vivo el propietario y coincide la identidad
  completa. Un evento descartado libera bytes/recursos sin tocar una vista destruida.
- Máximo un trabajo activo y uno pendiente; una nueva solicitud sustituye la pendiente.
- Navegar fuera cancela la entrega inmediatamente; el worker observa cancelación entre
  operaciones y bloques de lectura. La UI mantiene navegación mientras se drena.
- Si se abandona el catálogo durante carga o refresh, pasa a CatalogSuspended, sin spinner
  activo, conservando cuenta/selección/scroll. Regresar emite una sola carga automática con
  generación nueva; reactivar mientras carga no duplica solicitudes. Se descartan entregas
  anteriores y se restaura el contexto al completar. Ready/Empty/CatalogError sin carga
  suspendida se conservan al cambiar pestaña sin recarga automática.
- Cancelar carga del selector conserva la sesión previa o vuelve a Unselected; cerrar
  detalle invalida su preview y vuelve al listado. Ninguna cancelación reabre un detalle.
- Errores son claves localizables con acciones útiles; códigos de plataforma no sustituyen
  una explicación. Logs no incluyen nombres de cuenta, UID, nombres UGC ni bytes.

## Contrato de pantallas

### Página raíz Texturas

Orden de contenido: título y descripción de solo lectura; cuenta activa o Elegir cuenta;
Cambiar cuenta si aplica; Actualizar; Texture Settings; estado de carga/aviso/error;
lista de entradas cuando Ready; acceso a Pestañas. Mantener acciones visibles y focusables
sin duplicar el esquema global de botones.

Cada entrada muestra nombre y stem. A o tap abre detalle y guarda target de retorno.
B devuelve foco al selector de pestañas como el contenedor actual. ZL/ZR siguen siendo
solo pestañas principales. Mientras carga, mostrar estado y deshabilitar la solicitud
duplicada, manteniendo Cambiar cuenta/Volver/Pestañas donde sean seguros.

### Selector de cuenta

Pantalla hija con cuentas focusables, etiquetas distintas para nicknames repetidos y
Cancelar/Volver táctil. B cancela; A/tap confirma. Abrir selector no invalida sesión previa;
confirmar nueva cuenta sí. Ningún botón de apertura puede también confirmar una cuenta.
Error de servicio tiene Reintentar y Volver; cero cuentas tiene explicación y Volver.

### Detalle

Nombre completo consultable, stem, imagen FIT sobre fondo cuadriculado, dimensiones y
origen de preview. Carga y error reemplazan la imagen anterior. Error incluye Reintentar;
Volver siempre existe. No hay controles de editar, guardar o exportar.

A y touch tienen las mismas acciones; B actúa primero sobre el diálogo más interno y
luego vuelve al listado. No convertir ZL/ZR en cambio de variante o de elemento. La capa
hija y sus diálogos no dejan pasar atajos a contenido subyacente. Al salir restaurar
identidad de entrada y offset después del layout; no retener punteros a controles reemplazados.

### Menús y preferencias

Minus no tiene acciones de contexto en este slice. No añadir nuevas asignaciones X/Y.
Si Plus ya está disponible, respeta el Save/Exit global sin guardar el navegador; no
implementar un menú alternativo ni atajo de commit. Si sigue sin asignarse en el target,
las acciones esenciales permanecen focusables/táctiles. Un menú global futuro es otro slice.
Texture Settings conserva sus selectores, borrador y decisiones de Save/Discard/Keep editing.
Los atajos no saltan esos diálogos; navegar por Texturas no escribe preferencias.

## Matriz de errores y recuperación

| Resultado | Presentación | Acciones mínimas |
| --- | --- | --- |
| NoAccounts | Estado sin cuentas, distinto de error de servicio | Reintentar, Volver |
| NoSave | La cuenta no tiene guardado accesible según metadata completa | Cambiar cuenta, Reintentar, Volver |
| ReadOnlyOpenFailed / metadata failure | Error de acceso, sin asumir ausencia | Reintentar, Cambiar cuenta, Volver |
| UgcMissing | No está la colección esperada; no crear directorio | Reintentar, Cambiar cuenta, Volver |
| Empty | Lectura completa sin texturas principales | Actualizar, Cambiar cuenta, Volver |
| EnumerationFailed / catalog limit | Error, sin listado parcial presentado como completo | Reintentar, Cambiar cuenta, Volver |
| Names warning | Catálogo válido con identificadores y aviso | Actualizar, continuar navegación |
| Canvas failure + main success | Imagen principal y aviso de fallback | Volver |
| Main/preview failure | Error del elemento sin imagen previa | Reintentar, Volver |
| Obsolete / cancelled | Sin publicación en pantalla | Continuar navegación vigente |

## Matriz de paridad y comprobación

| Requisitos | Evidencia requerida |
| --- | --- |
| FR-001/002/003 | Fuente falsa de dos cuentas, 0/1/100 elementos, nombres y auxiliares |
| FR-004/010 | Layouts BC1/BC3, canvas rectangular/square, alfa, corrupción y límites |
| FR-005/006/007 | Control/touch, modal isolation, restauración, refresh y cambios de pestaña |
| FR-008/009 | Inyección de cada error y carga lenta, retry sin reinicio, respuesta SC-005 |
| FR-011/012 | Registro de operaciones sin mutaciones, identidad de archivos y stale-result tests |
| FR-013/014 | Tres locales, dos temas, handheld/dock, regresiones de preferencias |
