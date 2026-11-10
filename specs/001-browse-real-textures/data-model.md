# Data Model: navegador de texturas de solo lectura

Modelo lógico de diseño; no es una serialización ni introduce archivos persistentes.
Límites y evidencia de formato: [research.md](research.md).

## Entidades

### AccountSummary / AccountKey

- `AccountKey`: UID opaco no vacío, estable para aislamiento interno; nunca nickname.
- `displayName`: nickname copiado con límite o traducción de reserva.
- `sessionOrdinal`: distingue nombres repetidos sin imprimir UID; no persiste entre reinicios.
- `profileWarning`: fallo opcional de perfil sin eliminar una cuenta válida.

### TextureId / TextureEntry

- `TextureId`: par AccountKey + stem exacto del archivo principal, no su posición ni nombre.
- `stem`: basename sin `.ugctex.zs`; no aceptar separadores, `.`/`..` ni rutas externas.
- `displayName`: nombre opcional validado UTF-16LE->UTF-8; si falta usar stem legible.
- `mainRef`, `canvasRef?`, `thumbRef?`: referencias emitidas por la fuente, relativas a `/Ugc`.
- `nameWarning`, `associationWarning`: avisos locales sin sustituir identidad.

Reconocer sufijos con comparación ASCII insensible a mayúsculas; ordenar por stem ASCII
normalizado y después original. Nombre exacto evita colisión de archivos que difieren en
case. Para auxiliares ambiguos no elegir arbitrariamente: omitir asociación y avisar.
Ausencia de auxiliar es normal. Referencias no son rutas introducidas por el usuario.

### TextureCatalog

- `account`, `generation`: propietario y versión de solicitud.
- `entries`: vector completo ordenado, máximo 2048, sin auxiliares independientes.
- `namesStatus`: available / absent / warning, sin bloquear texturas válidas.
- `warnings`: claves localizables, sin dumps de Player.sav o datos personales.

Solo se publica un catálogo tras terminar el recorrido sin errores. Directorio ausente,
fallo de acceso, recorrido incompleto y exceso de 8192 registros son errores, no vacío.
Un recorrido completo con cero principales es Empty. Actualización conserva el catálogo
anterior solo como dato inactivo hasta resultado; no mostrarlo como resultado actualizado.

### PreviewResult

- `textureId`, `catalogGeneration`, `previewRequestId`: identidad de la solicitud.
- `source`: rectangular-canvas / main-texture.
- `width`, `height`, `rgba`: dimensiones válidas y exactamente width×height×4 bytes.
- `warning?`: fallback desde canvas ilegible/incompatible.

Prioridad: canvas rectangular válido; en otro caso principal. Canvas square no reemplaza
principal. Principal acepta solo tres layouts conocidos; canvas valida dimensiones y
accesos block-linear. No interpretar bytes ambiguos como un formato nuevo.
Ejes 1..1024 y hasta 1,048,576 píxeles; datos de imagen y zstd según límites de research.
Una imagen fallida no conserva pixels de la selección anterior. RGBA es CPU-only; handles
NanoVG pertenecen exclusivamente a la vista del thread gráfico y nunca al modelo.

### BrowserContext

- `account?`, `selectedId?`, `selectedOrdinalBeforeRefresh`.
- `focusTarget`: entrada o acción estable (Elegir cuenta, Actualizar, Ajustes, Pestañas).
- `scrollOffset`: offset del ScrollingFrame, ajustado tras layout al rango válido.
- `catalogGeneration`, `previewRequestId`, `lifetimeToken`: invalidación de resultados.

No hay flag dirty de guardado. Al cambiar cuenta se limpian catálogo, imagen y contexto
previo una vez confirmada la selección. Cancelar selector no cambia la sesión.

### ReadError

Categorías: AccountServiceUnavailable, NoAccounts, NoSave, SaveMetadataFailed,
ReadOnlyOpenFailed, UgcMissing, EnumerationFailed, ShortRead, InvalidData, UnsupportedLayout,
LimitExceeded, PreviewUnavailable y Cancelled. Contiene clave de mensaje, acción de
recuperación y código técnico opcional solo donde ayude; nunca bytes del guardado.
Fallo de Player.sav se transforma en aviso de nombres; fallo de imagen es local al detalle.

## Transiciones

| Estado | Evento | Siguiente estado / efecto |
| --- | --- | --- |
| Unselected | Elegir cuenta | AccountsLoading; no mutaciones |
| AccountsLoading | Resultado válido | AccountChooser o NoAccounts |
| AccountChooser | Cancelar | Sesión previa intacta o entrada sin cuenta |
| AccountChooser | Confirmar | Invalidar generación, limpiar datos; CatalogLoading |
| CatalogLoading | Éxito completo | Ready o Empty |
| CatalogLoading | Error | CatalogError con Reintentar/Cambiar cuenta/Volver |
| Ready/Empty/CatalogError | Actualizar/Reintentar | Nueva generación, CatalogLoading |
| Ready | Abrir entrada | PreviewLoading con identidad actual y sin imagen antigua |
| PreviewLoading | Éxito vigente | PreviewReady con origen y dimensiones |
| PreviewLoading | Fallo vigente | PreviewError sin imagen, mismo elemento |
| PreviewReady/PreviewError | Reintentar | Nuevo request ID; PreviewLoading |
| Cualquier detalle | Volver | Listado con foco/scroll restaurados |
| Página raíz | Cambiar pestaña | Mantener contexto, sin commit ni descarte de preferencias |
| CatalogLoading | Salir de la página, incluso durante refresh | CatalogSuspended; invalidar entrega y cancelar, conservar cuenta/selección/scroll |
| CatalogSuspended | Regresar a la página | CatalogLoading con generación nueva y una única solicitud automática |
| AccountsLoading | Cancelar o cerrar selector | Invalidar entrega y cancelar; sesión previa intacta o Unselected |
| PreviewLoading | Cerrar detalle | Invalidar request ID y cancelar; volver al listado con contexto |
| Cualquier carga | Confirmar otra cuenta | Invalidar entrega y cancelar; limpiar datos, nueva cuenta y CatalogLoading |
| Cualquier estado | Cierre de app | Invalidar, cancelar, drenar worker y cerrar recursos |

Una entrega se acepta solo si coinciden token de vida, cuenta, generación y request ID
aplicables. Cola acotada: un trabajo activo y una solicitud pendiente que reemplaza la
anterior. Los handles son usados/cerrados solo por el worker. No ejecutar join bloqueante
en un callback de navegación; shutdown espera después de mostrar estado de cierre.

## Regreso después de una carga cancelada

CatalogSuspended conserva cuenta, identidad seleccionada, ordinal y scroll, pero no representa
trabajo activo ni muestra spinner. El catálogo anterior, si existe, queda inactivo hasta
la recarga completa. Al reactivar la página se emite exactamente una solicitud de catálogo
con generación nueva; activaciones repetidas mientras ya está cargando no duplican trabajo.
Una solicitud anterior aún drenándose no bloquea navegación: la nueva queda como pendiente
más reciente. Sus entregas antiguas se rechazan incluso si ya estaban en la cola UI.
La nueva respuesta pasa a Ready/Empty/CatalogError y aplica la restauración normal.

Salir de una página Ready/Empty/CatalogError conserva su resultado y contexto; regresar no
recarga automáticamente si no se suspendió una carga. Cancelar la selección de cuenta
mantiene la sesión previa. Cerrar un preview cancela ese detalle, sin reabrirlo al volver
al listado; abrirlo de nuevo genera un request ID nuevo.

## Restauración y validación

Tras refresh, buscar el TextureId previo. Si no existe, usar el elemento que ocupa su
posición anterior, o el último si queda fuera de rango; si no hay elementos, enfocar
Actualizar. Restaurar offset tras layout sin ocultar el foco. Al volver de detalle sin
refresh conservar exactamente el elemento y la posición visible. No preservar punteros
a controles destruidos. No persistir contexto ni imágenes al cerrar la aplicación.
