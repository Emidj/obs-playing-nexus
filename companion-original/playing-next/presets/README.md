# Definiciones NORMAL — fase13 completada

Activo: `normal-v5.json` (schema5). V1/V2/V3/V4 preservados históricos fuera del
recurso activo. Protegido por `normal-compatibility-v1.json`, referencia congelada
23IDs/labels/renderer/settings/preview y tres aliases. El módulo valida ambos
antes de registrar la fuente. Los campos visuales siguen el contrato13-A/B/C.

Metadata canónica: id, name, setting, preview{height,scaled}, renderer y campos
del renderer. Aliases: array de id/name/target/setting, target solo canónico.
IDs ASCII1..64 [a-z0-9_-], nombres1..128 sin controles, settings enteros únicos
1..4096,1..128 canónicos y0..128aliases. Preview entero1..640 sin escala o2..640
si escalado a width/500. Parser all-or-nothing<=256KiB; orden por setting, no por
posición en archivo. Referencias/colisiones/campos desconocidos se rechazan.

Nuevos IDs pueden añadirse al recurso con una definición y setting libre, sin
nuevos enums/switches ni renderer específico. Requiere rebuild; no imports ni
reload en caliente. Los IDs/aliases históricos deben continuar cumpliendo la
referencia de compatibilidad. Enum tokens antiguos permanecen como API, sin ser
el catálogo autoritativo. Alias comparte el handle/caché de su canónico.

Test de extensión: un fixture de Oblique con setting27 y alias28 se parsea/prepara
con el mismo motor, conservando23/3. No nuevo preset visible ni PRO añadido.
La UI real verifica27opciones vigentes y roundtripJSON de26settings en cuatroOBS.
QJsonDocument usa last-key-wins para claves JSON duplicadas; no importador externo.

## Contrato histórico 13-C2 (v4)
# Definiciones NORMAL — checkpoint 13-C2

Recurso activo `normal-v4.json` (schema4): las definiciones visuales de los23
NORMAL están en datos, incluidos texto/layout/fondos/adorno de los tres clásicos.
Catálogos v1/v2/v3 preservados históricos. El lector acepta únicamente v4.

`classic.layout`: scaled, center, top, bottom, minimumHeight y tres rows
(left/right/available/gap). available=0 resta márgenes del canvas; otro valor
limita la fila. scaled usa referencia500px. Canvas derivado de métricas de texto.

`classic.rectangles`:1..32 rectángulos; cada box tiene cuatro scalars. Cada
scalar es una lista ordenada de1..8 pares [variable,coefficient], con variables
limitadas a offset,w,h,s,ly,lw,lh,ty,th,ay,ah. Orden preservado para el redondeo
anterior. Sin expresiones arbitrarias, loops, scripts o acceso a archivos.
Coeficientes±2048; coordenadas evaluadas finitas±65536. color #RRGGBB o accent,
alpha0..1, opacityScaled y moving booleanos. roundExtents/roundQuads preservan
los redondeos del diseño. Una pieza móvil solo puede ser la última; motion
travel0..32 y period200..60000, coherentes con ese flag. Su desplazamiento
mantiene el mismo coseno en el renderer existente y escala width/500.

Las formas se preparan en el motor compartido, fuera del draw. La publicación
continúa inmutable, con timer central bajo demanda y frame preparado reutilizado.
Fase13 abierta solo por catálogo de compatibilidad/IDs/aliases y cierre; las
formas de los23 NORMAL ya no dependen de switches de diseño. No import/reload.

## Contrato histórico 13-C1 (v3)
# Definiciones NORMAL — checkpoint 13-C1

Recurso activo: `normal-v3.json` (versión3). Conserva las recetas completas de
los veinte rediseños y añade tipografía declarativa para Prism, Air y Cantilever.
Los catálogos v1/v2 quedan como evidencia histórica y no se cargan en producto.

Los clásicos declaran tres filas `text` con `family`, `cellHeight`, `bold`,
`italic` y `color` (#RRGGBB), además de `scaleText` y `outline`. La familia
permite1..31 caracteres sin controles; cellHeight entero8..128 sin escala o
10..80 con escala width/500. Estos límites garantizan8..256 en el backend GDI
para anchos400..1600. Se conserva el orden aritmético y truncado del producto.
No cambian el backend, el fallback Unicode ni el contorno de Air. Preparación
solo en GUI; no parseo, I/O ni configuración mutable durante el draw.

Fase13 sigue abierta: geometría/formas/movimiento específico de los tres
clásicos y catálogo de compatibilidad C++ pendientes. No importación ni reload.

## Contrato histórico 13-B (v2)
# Definiciones NORMAL — checkpoint 13-B

Estado vigente: `normal-v2.json` sustituye el recurso activo v1 y añade las
recetas de fondos/adornos de los veinte rediseños. `normal-v1.json` se conserva
como contrato histórico de13-A. El lector activo exige versión2; no acepta un
catálogo v1 incompleto como si tuviera las nuevas capas. Ambas definiciones son
GPL-2.0-or-later. El límite del catálogo activo es256KiB.

Cada receta tiene `background` y `ornament`. Sus primitivas son rect/ellipse/
line/path, con curvas cúbicas, cierre de contornos, radios, fill y stroke. Los
colores de arte son arrays `[r,g,b,a]` de enteros0..255; los brushes pueden ser
none, solid, linear o radial. Degradados con2..8 stops ordenados y extremos0/1.
No se admiten shaders, scripts, URLs, rutas de archivos o llamadas externas.

Coordenadas: número o combinación afín de `offset,h,ty,th,ay,ah,cy,index`, en el
canvas de referencia500px. `stepBelow` añade una de dos constantes según una
comparación simple, sin anidar condiciones. Se conserva así la orientación de
Lens incluso con un canvas corto. `index` solo se admite dentro de `repeat`.
Repeat usa ceil(count), máximo1..512 y body sin otro repeat. Cada capa permite
hasta256 nodos directos y presupuesto de1024 operaciones expandidas, verificado
tanto en parseo como al preparar. Paths<=128 segmentos. Coeficientes±4096,
resultados finitos±8192, strokes0,1..64 y radios0..4096 (radial>0). No son metas
de latencia medidas; los rasters siguen acotados al canvas nativo.

La representación validada se guarda en el catálogo inmutable. El preparador
CPU genérico construye los rasters antes del draw, reutilizando el motor actual
y su cache. Se ha eliminado el switch de formas por preset en el producto. La
implementación13-A está congelada solo en tests como oráculo independiente de
120 comparaciones exactas de píxeles. No se compila en la DLL distribuible.

Fase13 sigue abierta: faltan los tres diseños clásicos y el catálogo de
compatibilidad C++. El formato aún es interno y necesita rebuild para editarlo;
no import/export, reload en caliente ni personalización UI en este checkpoint.

## Contrato histórico 13-A (v1)

`normal-v1.json` es el catálogo interno versionado de los 23 NORMAL. Sus datos
se derivan de la implementación validada de fase12; licencia GPL-2.0-or-later,
como los adapters visuales de los que se extrajeron. No es todavía el formato
de import/export `.obsplayingpreset` ni una interfaz de personalización.

Los veinte registros `native-recipe` definen tres capas de texto (etiqueta,
título y artista), tipografía, colores `#RRGGBB`, peso, cursiva, mayúsculas,
geometría relativa a 500px y movimiento del adorno. La anchura real escala esos
valores; gap, duración de transición, speed y pause continúan siendo ajustes
por consumidor. Los tres registros `classic` conservan Prism, Air y Cantilever
con sus implementaciones vigentes. Los IDs, nombres y aliases siguen estables.

CMake incorpora el JSON como recurso Qt de `playing-track-visual`; el primer
acceso valida y construye un catálogo inmutable en memoria. No se abre ningún
archivo externo, ni se realizan lecturas periódicas o parseo durante render.
Editar estos datos requiere reconstruir este build. No hay recarga en caliente
ni imports del usuario en este checkpoint.

El lector exige formato `obs-playing.preset-catalogue`, versión1, 23 entradas
únicas y los tres aliases originales. Rechaza claves no admitidas, campos
ausentes, tipos incorrectos y valores fuera de límites. Entrada <=64KiB;
strings <=128 unidades UTF-16, sin controles ASCII; RGB explícito; tamaños
tipográficos8..128, pesos enteros100..900, canvas de referencia500px,
travel±32px, period200..60000ms y pulse0..1. Las validaciones adicionales de
texto/layout/raster siguen vigentes. La publicación es completa o falla; no
hay mezcla parcial ni fallback silencioso a estilos hardcoded.

Las claves JSON repetidas se interpretan según QJsonDocument (última clave);
los IDs repetidos del catálogo sí se rechazan. No exponer este lector como
importador de archivos no confiables antes de establecer el contrato de
duplicados de la fase21. Actualmente consume exclusivamente el recurso incluido.

Fase13 sigue abierta: faltan primitivas/recetas decorativas data-driven, la
representación de los tres diseños clásicos y la separación del catálogo de
compatibilidad C++ antes de cerrar el motor de presets. No introducir scripts,
un renderer por preset o temporizadores por definición. Los fixtures de tests
permiten modificar/copiar un catálogo validado, sin publicar esas copias en el
host ni modificar el catálogo inmutable en uso.
