# Quickstart: validación del navegador de texturas

Esta guía contiene los checks de la implementación y las rutas de aceptación pendientes.
Las suites host y cross-builds se ejecutaron; no se ha validado este navegador en emulador
ni Switch físico. Los resultados de cada plataforma se informan por separado.

## Prerrequisitos

- Toolchain devkitPro/libnx y portlibs existentes; no instalar ni actualizar como parte
  de este trabajo. Compilador host C++17 y zstd host para las pruebas de codec.
- Consultar [contrato](contracts/texture-browser.md), [modelo](data-model.md),
  [límites y formatos](research.md) y [checks existentes](../../tests/README.md).
- Usar datos sintéticos/sanitizados en temporales para fixtures; no subir guardados,
  nombres o UIDs personales ni incluirlos en logs/capturas publicadas.
- Switch físico compatible para aceptación final. Si no está disponible, registrar
  pendientes; compilar o usar emulador no sustituye esa evidencia.

## Preparación de las colecciones

Los tests nuevos generarán fixtures en directorios temporales y los eliminarán al acabar:

1. Cuenta A: colecciones de 0, 1 y 100 entradas conocidas, incluyendo BC1 512², BC1 384²,
   BC3 256², canvas 256×512 y 256×128, transparencia y nombre Unicode válido.
2. Cuenta B: stems iguales con imágenes/nombres distintos y nickname visible duplicado.
3. Auxiliares ausentes, ambas grafías de thumb, auxiliares huérfanos, case ties y nombres
   repetidos; nombres no disponibles y campo de hash falso dentro del heap.
4. Colección fallida, guardado ausente, metadata fallida, error de apertura, directorio
   ausente, lectura incompleta, demora controlada y cambios de tamaño entre lecturas.
5. Datos corruptos/vacíos, tipos/offsets/counts inválidos, UTF-16 inválido, exceso de límites,
   zstd truncado, tamaño desconocido, output excesivo y ventana excesiva.

Registrar inventario y hashes de archivos antes/después. La fuente instrumentada registra
operaciones intentadas sin almacenar contenido; cualquier write/create/delete/rename/commit
es un fallo de test aunque el resultado final de los archivos sea idéntico.

## Checks host

Desde la raíz del repositorio, con compilador C++17 y zstd host ya disponibles:

```sh
tests/run_texture_browser.sh
SANITIZE=1 tests/run_texture_browser.sh
```

El script contiene comandos exactos para siete ejecutables: decoder, scanner, nombres,
modelo, preview, sesión y adaptador real con shim libnx. Véase
[tests/README.md](../../tests/README.md) para cobertura, limitaciones y regresiones
existentes de settings/config/save safety. El shim nunca se usa en builds Switch.
Los fallos se programan sobre fuentes sintéticas; no se necesitan guardados personales.

## Compilación Switch y auditoría del límite de lectura

```sh
make -j4
make -j4 borealis
```

Esperado: ambos NRO se producen con toolchain existente. Revisar fuentes del adaptador
y `build/borealis/TomoToolNXPlusBorealis.map`: apertura read-only y archivos Read-only;
no llamadas a SaveMount/MiiManager/SaveWorkflow, importadores ni escritores del codec.
No exigir ausencia global de símbolos de escritura porque ConfigStore y el framework
siguen escribiendo preferencias u otros recursos: auditar alcance y call paths del navegador.
La inspección del mapa es evidencia complementaria, no prueba suficiente de ausencia de mutaciones.

## Recorrido end-to-end en consola

No sembrar ni sustituir guardados personales para probar. Para escenarios de fixture en
consola usar un entorno de prueba con guardado sintético preparado fuera del navegador,
mediante un procedimiento de restauración ya verificado con backup si afecta datos existentes.
La creación/restauración de ese entorno no forma parte de este slice y no se automatiza aquí.
Los casos inyectados se cubren en host cuando no se puedan reproducir físicamente sin riesgo.

1. Copiar `TomoToolNXPlusBorealis.nro` a su carpeta bajo `/switch/` y abrir desde hbmenu.
2. Entrar en Texturas: no deben aparecer ejemplos. Elegir A, reconocer cuenta, recorrer
   las 100 entradas solo con control y repetir solo con touch. Cancelar selector conserva sesión.
3. Abrir una entrada fuera de la primera pantalla; comprobar imagen completa, proporción,
   alfa, nombre, identificador, dimensiones y origen. Volver 20 veces; foco y posición iguales.
4. Comprobar canvas rectangular prioritario, principal con canvas square, fallback con
   aviso si el canvas falla y error local si ninguna imagen válida existe.
5. Cambiar de pestaña y volver 10 veces; conservar contexto. Visitar Texture Settings,
   probar cancelación de selector, borrador, descarte y guardado explícito independientes.
6. Cambiar a B durante carga; ninguna entrega de A puede reaparecer. Probar actualizar,
   pérdida de entrada y fallback de foco según contrato. Una carga cancelada no bloquea B.
   Durante carga inicial y durante refresh, cambiar de pestaña y regresar: debe salir de
   CatalogSuspended y emitir exactamente una recarga con generación nueva, conservando
   cuenta/selección/scroll. Repetir activación mientras carga no duplica trabajo; las entregas
   anteriores no se publican. Al volver desde una página Ready sin carga pendiente no debe
   iniciarse una recarga automática. Probar también estas secuencias con cola host manual.
7. Probar estados vacío/sin guardado/acceso/error disponibles sin modificar archivos desde
   la app. Reintentar tras recuperar el origen; sin reinicio ni creación/reparación automática.
8. Medir actividad <1 s y respuesta visible <250 ms en al menos 95 de 100 órdenes durante
   carga sobre la colección de 100 entradas; registrar método, entorno y resultados.
9. Revisar inglés, español y español latinoamericano, ambos temas, handheld 1280×720 y
   dock. Comprobar nombres largos, foco visible, scroll, held input, modales y cambio de input.
10. Salir durante carga y normalmente; sin commit de guardado, confirmaciones falsas de
    guardado ni cierre prematuro de handles. Comparar inventario/hashes con los iniciales.

Si un emulador requiere adaptar el binario, documentar exactamente qué se adaptó y no
atribuir el resultado al NRO normal. No realizar modificaciones de toolchain para la prueba.

## Informe de aceptación

Registrar cada SC-001–SC-007 como aprobado/fallido/pendiente, con fecha, entorno y evidencia.
Separar identidad de archivos de ausencia de intentos de mutación. Reportar por separado
host, emulador y consola, permisos reales, lifecycle, latencia, traducciones y escalado.
Una matriz incompleta no permite afirmar reemplazo funcional completo ni validación física.

## Implementation baseline — 2026-11-09

Starting commit: 6df9a1d; working tree clean before implementation. Existing Borealis
Texture Settings uses real config; texture entries remain samples. Installed devkitPro,
libnx and portlibs are available; no toolchain installation is planned.
SC-001 through SC-007 are initially pending for this feature. Existing evidence described
above belongs to older slices and does not validate the new read-only browser.

## Evidencia de esta implementación — 2026-11-09

- Siete suites host nuevas: aprobadas, también con AddressSanitizer/UndefinedBehaviorSanitizer.
- Regresiones existentes de focus/settings, config y save safety: aprobadas.
- Decoder: golden hashes de cinco buffers sintéticos calculados antes de la extracción;
  las salidas del módulo extraído coinciden. El host carece de SDL2_image: se compiló la
  sección CPU original aislada para obtener la referencia, sin instalar dependencias.
- Configuración de build: fuentes añadidas por incremento; fuente de preview aislada en
  source/texture_browser_preview.cpp para que el modelo no dependa del codec.
- Modelos/fuentes falsas usan memoria sintética. El adaptador real se prueba con el shim;
  esto no equivale a acceso a un guardado real desde el host.
- No se ejecutó emulador ni Switch físico. No se sembró/restauró ningún guardado.

| Criterio | Evidencia host | Aceptación en consola |
| --- | --- | --- |
| SC-001 | Catálogos 0/1/100 y asociación/preview aprobados | Pendiente de fuente real e inspección visual |
| SC-002 | Rutas de acciones revisadas en código | Pendiente control/touch |
| SC-003 | Identidad, suspensión/reanudación y fallback aprobados | Pendiente 20 ciclos y 10 cambios de pestaña |
| SC-004 | Fallos de fuente/modelo/preview y retry aprobados | Pendiente presentación e interacción |
| SC-005 | Worker fuera de UI, cola acotada | Pendiente medición física, sin tiempos atribuidos |
| SC-006 | Snapshots sintéticos, flags read-only y llamadas auditadas | Pendiente recorrido completo y comparación física |
| SC-007 | JSON válido y mismas claves browser en tres idiomas | Pendiente layout, temas, dock y texto largo |

Las tareas de aceptación que requieren esos recorridos permanecen sin marcar.
El resultado no demuestra compatibilidad del guardado ni reemplazo completo del editor.

Final cross-builds: make -j4 and make -j4 borealis passed after implementation.
ELF symbol audit confirms fsOpen_SaveDataReadOnly and no SaveMount, SaveWorkflow,
SaveEditor, MiiManager or TextureProcessor import/encoder symbols. Browser locale key
sets match across all three included languages. No emulator or hardware result is claimed.
