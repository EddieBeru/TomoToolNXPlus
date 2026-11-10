# Quickstart: validación del navegador de texturas

Esta guía describe verificaciones a ejecutar después de implementar el slice. Actualmente
solo existen los documentos de diseño; no se han creado los tests nuevos ni se ha validado
este navegador. Resultados host, emulador y físicos deben informarse por separado.

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

Desde la raíz del repositorio, ejecutar primero los comandos existentes de `tests/README.md`
para settings, config y save safety. Ejecutar estos comandos nuevos una vez existan las
fuentes propuestas y sus tests; todavía no son pruebas disponibles en este checkout:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
  source/ugc_scanner.cpp source/ugc_names.cpp source/texture_browser.cpp \
  tests/texture_browser_test.cpp -o /tmp/tomotool-texture-browser-test
/tmp/tomotool-texture-browser-test

g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
  source/texture_decode.cpp tests/texture_decode_test.cpp -lzstd \
  -o /tmp/tomotool-texture-decode-test
/tmp/tomotool-texture-decode-test

g++ -std=c++17 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -Iinclude \
  source/ugc_names.cpp tests/ugc_names_test.cpp -o /tmp/tomotool-ugc-names-test
/tmp/tomotool-ugc-names-test
```

El adaptador real se compila contra el shim host de libnx propuesto en research; ese
include path nunca debe usarse en los targets Switch. El worker recibe una cola de entrega
inyectada, sin incluir Borealis. Una vez implementados, ejecutar:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Itests/support -Iinclude \
  -Iapps/borealis/source apps/borealis/source/texture_read_source.cpp \
  tests/texture_read_source_test.cpp -o /tmp/tomotool-read-source-test
/tmp/tomotool-read-source-test

g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Iapps/borealis/source \
  source/ugc_scanner.cpp source/ugc_names.cpp source/texture_decode.cpp \
  source/texture_browser.cpp apps/borealis/source/texture_browser_session.cpp \
  tests/texture_browser_session_test.cpp -pthread -lzstd \
  -o /tmp/tomotool-browser-session-test
/tmp/tomotool-browser-session-test
```

La implementación mantendrá estos comandos y sus dependencias finales en `tests/README.md`.
Se exige verificar secuencia real de apertura read-only, flags de archivos y cierres con
el shim, además de probar resultados del modelo. El shim no verifica el servicio HOS real.

Repetir nuevos tests con `-fsanitize=address,undefined -fno-omit-frame-pointer -g`.
Esperado: salida exitosa, ninguna violación de memoria, ningún intento de escritura,
fixtures idénticos y todos los fallos con categoría prevista. Verificar los tres layouts,
comparación de pixels conocidos tras la extracción y regressions de decode legado.

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
