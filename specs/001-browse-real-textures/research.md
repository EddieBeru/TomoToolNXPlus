# Research: navegador real de solo lectura

Fecha: 2026-11-09. Fuentes primarias locales: código del repositorio, Borealis fijado e
interfaces de libnx instaladas. No se consultó GitHub ni se ejecutó la aplicación.
Las decisiones resuelven el diseño; no constituyen evidencia de funcionamiento en consola.

## 1. Acceso al guardado y clasificación de errores

**Decision**: Un adaptador Switch propio mantiene un `FsFileSystem` privado abierto mediante
`fsOpen_SaveDataReadOnly` para el título `0x010051F0207B2000`. Usa `fsFsOpenDirectory`,
`fsDirRead`, `fsFsOpenFile` únicamente con `FsOpenMode_Read`, `fsFileGetSize` y `fsFileRead`.
No registra un dispositivo global ni expone handles al navegador.

**Rationale**: `/opt/devkitpro/libnx/include/switch/services/fs.h` declara esa apertura
como wrapper de `fsOpenReadOnlySaveDataFileSystem`, disponible desde HOS 2.0.0. Las APIs de
cierre `fsFileClose`, `fsDirClose` y `fsFsClose` retornan void: se exige cierre por alcance,
pero no se inventa un error que la API no devuelve. Cada Result y cantidad leída sí se
comprueba. Los archivos/directorios se cierran antes del filesystem.

La ausencia de guardado se determina mediante recorrido completo y exitoso de metadata
con `fsOpenSaveDataInfoReader`, `fsSaveDataInfoReaderRead` y cierre correspondiente,
comparando título, UID y tipo account-save. Fallo de metadata no significa ausencia.
Si existe metadata pero falla la apertura, se muestra error de acceso recuperable; carreras
con cambios externos producen reintento, nunca creación ni reparación.

**Alternatives considered**: `SaveMount::Mount` y `SaveExists` abren acceso escribible en
`source/save_mount.cpp`; no se reutilizan. `fsdevMountSaveDataReadOnly` existe, pero introduce
un mount global innecesario. No hay fallback escribible en sistemas o permisos incompatibles.

## 2. Cuentas y privacidad

**Decision**: Enumerar mediante account service, con errores diferenciados de lista vacía.
Copiar nickname con límite de 0x20 bytes; retener cuentas cuyo perfil falle con nombre
localizado de reserva. Distinguir nombres repetidos mediante ordinal local de sesión.
UID queda interno; no cargar avatares ni registrar nombres, UIDs, imágenes o rutas personales.

**Rationale**: `switch/services/acc.h` ofrece listado y perfiles; `ACC_USER_LIST_SIZE` es 8.
`SaveMount::GetUsers` mezcla este flujo con otras responsabilidades y omite perfiles fallidos.

**Alternatives considered**: Reutilizar su vector global o usar el nickname como identidad
permitiría omisiones o confusión entre cuentas. El identificador estable es siempre el UID.

## 3. Catálogo y nombres

**Decision**: Extraer el reconocimiento de nombres de archivo de `source/ugc_scanner.cpp`
a una operación pura sobre descriptores enumerados, con resultado tipado. Conservar `Scan`
como adaptador legado de su firma actual. Reconocer `.ugctex.zs` principales y asociar
`.canvas.zs`, `_Thumb_ugctex.zs` y `_Thumb.ugctex.zs` sin crear entradas auxiliares.
Ordenar por stem ASCII sin mayúsculas y después por stem original; identidades conservan
el nombre exacto. Coincidencias auxiliares ambiguas producen aviso y no se eligen al azar.

**Rationale**: El scanner actual convierte un fallo de opendir en vector vacío y no informa
fallos de recorrido/cierre. El nuevo adaptador distingue ausencia, acceso, recorrido parcial
y vacío completo, sin cambiar el contrato de consumidores legados en este paso.

Los nombres se extraen con un parser puro y acotado de Player.sav, usando el índice real
y los hashes documentados en `source/mii_manager.cpp`, no buscando bytes arbitrarios.
Validar magic `04 03 02 01`, límite de índice desde 0x20, filas de 8 bytes, marcadores
de tipo, tipo 31 WStr64Array, offset de heap, contador y rango `4 + count * 128` con
aritmética comprobada. Rechazar hashes objetivo duplicados o offsets/tipos ambiguos.
Ignorar campos no relacionados que no se necesitan, sin normalizar ni escribir nada.

| Prefijo | Hash numérico | Capacidad conocida |
| --- | --- | --- |
| Food | 0x408494F5 | 99 |
| Cloth | 0x40710642 | 299 |
| Goods | 0x2F793EB1 | 99 |
| Interior | 0x3DE2C5DD | 99 |
| Exterior | 0x27C875D6 | 99 |
| MapObject | 0x56F99338 | 99 |
| MapFloor | 0x918875A9 | 99 |

El índice de nombre es cero-based: el primer elemento corresponde a `UgcFood000`.
Un stem desconocido, incluidos tipos sin hash documentado, se muestra por identificador.
UTF-16LE admite pares surrogate válidos; secuencias inválidas causan fallback del nombre
con aviso. Fallo del archivo o estructura de nombres produce fallback global, no catálogo fallido.

**Alternatives considered**: `MiiManager::LoadUgcNames` busca hashes por todo el archivo,
ignora contador y usa conversión BMP incompleta y caché global. No se enlaza ese módulo.
Tampoco se enlaza todo SaveEditor y sus escritores para leer siete campos opcionales;
se reutiliza el formato documentado por su parser, con fixtures que prueben equivalencia.

## 4. Decodificación y límites

**Decision**: Extraer CPU decode/deswizzle y conversión de color a `texture_decode`, sin SDL,
I/O ni escritores. Mantener wrappers públicos actuales de TextureProcessor. Separar primero
la extracción preservando comportamiento; después añadir validación y descompresión acotada
para el navegador, con pruebas independientes. Resolver LUTs mediante inicialización
inmutable segura para threads, manteniendo resultados conocidos.

**Rationale**: `source/texture_processor.cpp` mezcla encoder, SDL e I/O. Sus helpers zstd
confían en el tamaño declarado; `DecodeCanvas` divide por cero con entrada vacía o muy corta.
No se llama a `DecodeFile` ni a los helpers vector-return que pierden errores.

| Recurso | Límite inicial del navegador | Resultado al exceder |
| --- | --- | --- |
| Player.sav | 8 MiB | Aviso; nombres por identificador |
| Archivo comprimido de imagen | 4 MiB | Error local de vista previa |
| Salida zstd | 4 MiB; ventana máxima 8 MiB | Error local; nunca reservar tamaño anunciado sin validar |
| Eje de imagen | 1..1024 píxeles | Error local |
| RGBA | 1..1,048,576 píxeles, exactamente 4 bytes/píxel | Error local |
| Registros totales enumerados en Ugc | 8192 | Error de colección, no truncado silencioso |
| Texturas principales | 2048 | Error de colección, no listado parcial |
| Lectura individual | bloques hasta 64 KiB | Comprobar cancelación y bytes leídos |
| Trabajo simultáneo | un worker; activo + una solicitud más reciente pendiente | Reemplazar pendiente obsoleta |
| Imágenes retenidas | una RGBA vigente y una textura GPU vigente | Liberar antes de sustituir |

Son límites operativos, no afirmaciones del máximo del formato. Permiten los tamaños
actuales de ugctex (131072 bytes/512² BC1; 98304/384² BC1; 65536/256² BC3) y lienzos
256², 256×512 y 256×128 documentados. Validar dimensiones, padding y todos los accesos
block-linear antes de decodificar; no inferir nuevos layouts por tamaño desconocido.
Tamaños de frame desconocidos usan salida acotada; rechazar stream truncado, concatenado
no admitido, salida excedida o ventana excesiva mediante zstd existente.

Preferir canvas rectangular válido, como `LoadPreview` legado; en otros casos, principal.
Si un canvas presente no se puede leer/validar, intentar principal y mostrar aviso.
Square canvas no reemplaza principal. Mantener conversión linear-to-sRGB actual y alfa;
no añadir ajustes de color en esta función. Miniatura queda asociada, no se necesita decodificarla.

**Alternatives considered**: Reutilizar todo TextureProcessor introduciría escritores/SDL
y asignaciones sin límite. Copiar todo el decoder duplicaría reglas. PNG temporal añade
persistencia innecesaria y codificación intermedia.

## 5. Asincronía y UI Borealis

**Decision**: Un controlador de sesión independiente de UI posee worker serial y fuente
inyectada. La UI emite órdenes y consume resultados inmutables en su thread mediante
`brls::sync`, con token débil de vida, generación de cuenta/catálogo y request ID de imagen.
La función de entrega es inyectada: producción usa brls::sync y los tests una cola manual.
No capturar vistas crudas en callbacks pendientes. Volver invalida entrega inmediatamente;
el worker cancela cooperativamente entre lecturas, cierra handles y descarta resultados.
Cambiar cuenta jamás cierra un handle usado por otro thread. Shutdown solicita cancelación
y espera al worker antes de finalizar servicios, manteniendo feedback hasta terminar.

**Rationale**: `borealis/core/thread.hpp` declara que Borealis no es thread-safe.
`app.cpp` ya retiene páginas entre pestañas; conservar ese patrón y una instancia del
navegador. `ScrollingFrame` ofrece get/setContentOffsetY. Restaurar primero contenido/foco
y luego offset tras layout, ajustando al rango válido y manteniendo foco visible.

La vista de imagen recibe RGBA mediante `nvgCreateImageRGBA` en el thread gráfico y
`brls::Image::innerSetImage`, con `setFreeTexture(true)` y escalado FIT. `setImageFromMem`
es para datos codificados y no admite RGBA directamente. Una vista de fondo cuadriculado
sin imágenes aporta contraste para alfa; no se añade un segundo renderer SDL.

**Alternatives considered**: I/O en callbacks UI incumple respuesta durante carga;
threads por entrada producen carreras y memoria creciente. Caché global por stem mezcla
cuentas y puede retener datos personales. Un worker acotado evita ambos problemas.

## 6. Builds y evidencia

**Decision**: Mantener flags existentes: C++17, sin RTTI/excepciones en target legado,
RTTI/excepciones solo en Borealis. Enlazar zstd instalado en el target nuevo; no añadir
bibliotecas ni actualizar toolchain. Compilar ambos targets por las extracciones compartidas.

**Rationale**: `Makefile`, `apps/borealis/CMakeLists.txt` y `tests/README.md` fijan el proceso.
Las fuentes primarias locales bastan para estas decisiones; permisos reales, latencia,
comportamiento HOS, Joy-Con, touch y renderer requieren verificación física posterior.

**Alternatives considered**: Migrar WebUI o rediseñar SaveMount ampliaría el alcance sin
aportar seguridad a este lector. Los módulos legados permanecen disponibles y no se invocan.

## 7. Estrategia de prueba del adaptador real

**Decision**: Compilar `texture_read_source.cpp` en host con un shim mínimo de libnx
en `tests/support/switch.h`, incluyendo resultados/cantidades programables y registro
de llamadas. Probar la misma lógica del adaptador, flags read-only y cierres en todos
los retornos. El worker acepta una función de entrega inyectada y no incluye Borealis;
su test usa cola manual para intercalar cancelación, cambios de cuenta y entrega tardía.

**Rationale**: Un fake del modelo no prueba las llamadas del adaptador real. La compilación
Switch comprueba las firmas contra libnx real; hardware comprueba permisos y comportamiento.
El shim no se añade al include path de ningún target Switch.

**Alternatives considered**: Tests que solo simulan el catálogo omiten el límite de
seguridad. Añadir una biblioteca general de platform mocking sería innecesario.
