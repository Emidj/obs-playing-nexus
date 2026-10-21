# Correcciones solicitadas durante las pruebas

Lista iniciada el 14 de octubre de 2026. Se incorporarán aquí los nuevos puntos
que comunique el usuario. Registrar un punto no implica que esté implementado
ni validado.

| ID | Corrección | Estado |
| --- | --- | --- |
| COR-016 | Enviar las carátulas desde Companion al plugin del otro PC. | Implementado en RC3.1; TLS, imágenes tardías, cambio, reconexión y compatibilidad anteriores PASS local. Validación en dos PCs actualizados pendiente. |
| COR-017 | Documentar la configuración Companion con imágenes. | README en español e inglés, HTML offline, cinco esquemas por idioma y captura real con datos de prueba; incluido en ambos paquetes. |
| COR-001 | Obtener y mostrar la portada real de la canción de Rekordbox. | Corregido timeout de búsqueda; biblioteca real devuelve portada Ready; visual OBS pendiente |
| COR-002 | Corregir el tachado (strikeout) y subrayado (underline) del texto. | Implementado; raster por capa PASS |
| COR-003 | Mostrar la imagen de perfil obtenida del canal del streamer; URL manual opcional. | Perfil Twitch visible en OBS oficial, confirmado por usuario y observado en pantalla |
| COR-004 | Al detener la música conservar solo el marco, sin texto ni portada. | Implementado; host y marco PRO en reposo PASS |
| COR-005 | Obtener y mostrar la portada real de la canción de VirtualDJ. | Portada y cambio a otra imagen comprobados visualmente en OBS oficial 32.2.2; caso sin portada pendiente |
| COR-006 | Corregir la ausencia del título de la canción al utilizar Serato. | Usuario confirma título y cambio en 4.0.10 tras activar External Control; portada pendiente |
| COR-008 | Centrar portada y texto en Vertical Vinyl, Retro y Glass; conservar el marco al ajustar opacidad Vinyl. | Implementado; aperturas medidas y capturas de OBS aislado PASS; DLL aplicada al OBS oficial |
| COR-009 | Restaurar la geometría original al seleccionar cualquier estilo. | Recorrido de todos los PRO y retorno PASS en OBS aislado; conserva apariencia |
| COR-010 | Categoría/Estilo debajo de Programa DJ/Lectura y pruebas al final. | Orden de propiedades comprobado en OBS aislado |
| COR-011 | Imagen manual mediante selector de archivo en lugar de campo vacío. | Usuario confirma que abre el selector nativo correctamente; URL opcional en Advanced |
| COR-012 | Retirar perfiles de rendimiento si sus ventajas son mínimas. | Selector retirado; fuente usa cadencia fija Balanced; valores antiguos ignorados |
| COR-013 | Simplificar posición del texto a dos sliders y portada de tamaño automático. | Horizontal/vertical con cero original; prueba de movimiento y retorno sin rerasterizar PASS |
| COR-014 | Rellenar y centrar la portada en todos los horizontales, incluidos Glassmorph, Urban, HUD y Particles. | Nueve aperturas verificadas por píxeles GPU, sin huecos interiores; recorrido OBS PASS |
| COR-015 | Eliminar la mitad transparente del disco giratorio Vinyl. | Recorte de media superficie eliminado en horizontal y vertical; portada superpuesta como ocultación natural; capturas GPU PASS |

## COR-016 — Portadas entre dos PCs

La causa verificada es que PNX1 exportaba la portada local como no disponible;
no transfería sus bytes. La espera de Rekordbox era un problema adicional: los
resultados sin portada se reintentaban cada diez segundos y ahora cada segundo,
solo mientras existe demanda de lectura.

RC3.1 negocia una extensión de imágenes dentro de la conexión TLS autenticada.
La portada se prepara en un worker (hasta 512 px y 60 KiB), se verifica por hash
y se guarda atómicamente en una caché local del receptor. Epoch y secuencia de
snapshot impiden aplicar una imagen a otra canción o a una conexión posterior.
Las rutas locales del emisor no atraviesan la red. La caché tiene un límite de
64 MiB, sin eliminación automática; al agotarlo, las nuevas imágenes no se guardan
y se mantiene la alternativa de perfil. PNG conserva el canal alfa; JPEG se usa
para imágenes opacas cuando es necesario reducir tamaño.

Validación: 45 suites previas y la nueva suite de protocolo de imágenes PASS,
incluidos receptor/emisor anteriores, hash incorrecto, dimensiones y tamaño,
imágenes obsoletas, cambio de canción, pausa, ausencia de archivo y reconexión.
No equivale todavía a comprobar las portadas con los dos PCs físicos actualizados.

## COR-006 — Título de Serato ausente

- Reportado por el usuario: al utilizar Serato no aparece el título de la canción.
- Pendiente de reproducir y comprobar detección, recepción de metadata,
  selección de deck, publicación en Track State y visualización. No se ha
  identificado todavía la causa.
- Validación necesaria: título durante reproducción, cambio de canción y
  comportamiento de pausa/stop conforme a COR-004. No asumir que artista o
  portada funcionen ni añadir fallos no reportados sin verificarlos.

## COR-005 — Portada de VirtualDJ

- Reportado por el usuario: la portada tampoco aparece al utilizar VirtualDJ.
- Pendiente de reproducir y revisar la obtención de artwork en el provider,
  su publicación en Track State y su carga/renderizado. No atribuir todavía
  una causa ni asumir que sea idéntica a la incidencia de Rekordbox.
- Validación necesaria: tema con portada, cambio de canción, portada ausente y
  fallback opcional al perfil del streamer, sin mostrar una portada del tema anterior.

## COR-004 — Layout persistente al detener la música

- Requisito del usuario: al detenerse la música, el layout debe permanecer en
  pantalla; no debe desaparecer la composición completa.
- Pendiente de revisar el comportamiento y reproducir la desaparición.
- Validación necesaria: reproducción → pausa y reproducción → stop, manteniendo
  visible el layout, y reanudación sin desapariciones inesperadas.
- El usuario no ha especificado aún si el texto y la portada deben conservar
  el último tema o cambiar durante la detención; no asumir ese comportamiento.

## COR-003 — Imagen de perfil del streamer

- Reportado por el usuario: la imagen de perfil tampoco se visualiza.
- Requisito: obtener la imagen de perfil desde el canal del streamer para usarla
  como fallback cuando no haya portada. La URL manual debe ser opcional, no un
  requisito para este funcionamiento.
- Pendiente de reproducir el fallo y revisar la integración existente. No se
  afirma todavía una causa ni que la obtención automática esté implementada.
- Falta concretar la plataforma del canal; no asumir Twitch, YouTube u otra.
- Validación necesaria: obtención desde el canal, visualización, fallback ante
  portada ausente, prioridad de la portada disponible y opción manual de URL.

## COR-002 — Tachado y subrayado

- Reportado por el usuario: activar strikeout o underline no produce el efecto
  esperado en el texto.
- Pendiente de reproducir y localizar la causa; no verificado todavía en código.
- Validación necesaria: cada efecto por separado y ambos juntos, en título y
  artista, y conservación de los ajustes al reabrir la fuente.

## COR-001 — Portada de Rekordbox

- Reportado por el usuario al probar la candidata instalada con Rekordbox.
- Hecho verificado en código: los lectores Automatic y History publican título
  y artista, sin referencia de artwork. El renderer y cargador de imágenes ya
  existen, pero falta la obtención de portada en el provider.
- Trabajo pendiente: investigar y verificar la procedencia de la portada,
  integrarla sin bloquear el render ni asociar imágenes tardías a otro tema.
- Validación necesaria: canción con portada, cambio de tema, portada ausente y
  fallback opcional al perfil del streamer. No afirmar compatibilidad real hasta
  verificarla con Rekordbox.
- La ausencia del regulador de posición del texto no se registra como fallo:
  el usuario confirmó que lo encontró en las opciones Advanced.


## COR-007 — Acceso directo del Companion

La instalación de Companion crea «OBS Playing Companion» en el escritorio.
La desinstalación retira solo el acceso directo propio intacto; conserva cambios
ajenos y restaura el anterior si había uno. 37 comprobaciones del instalador final PASS, incluido destino del acceso directo.

## Evidencia de implementación, 14 de octubre

No confundir cambios implementados con compatibilidad DJ certificada.
La cuenta Twitch conectada fue confirmada por el usuario. La validación oficial
sobre el token guardado en el perfil devolvió inicialmente HTTP 401. Tras reconectar la cuenta, obtención de perfil propio y decodificación nativa 300×300 PASS; no se almacenaron ni
mostraron credenciales. La URL HTTPS o ruta local manual sigue siendo opcional.
VirtualDJ: no se ha descifrado ni supuesto el formato de sus cachés propietarios;
la extracción actual lee imágenes embebidas del archivo de la canción identificado
sin ambigüedad en database.xml. El disco de caché está limitado a 64 MiB, sin borrar
archivos al alcanzar el límite. La lectura de portada ocurre en el worker.
Serato: la fuente forzaba antes History; su provider solo acepta Automatic.
Se normalizan modo/ruta/codificación. Esto no prueba telemetría de Serato 4.0.10.

## Verificación posterior, 14–15 de octubre

Rekordbox: la portada de «The Roller Wh0 Remix» existe en la biblioteca real.
La búsqueda tardaba 54–56 ms y excedía el presupuesto anterior de 50 ms.
Se amplió a 250 ms exclusivamente en el worker; la misma lectura nativa devuelve
Ready con referencia local. El perfil solo es fallback; no tiene prioridad sobre
una portada Ready. Falta comprobar la imagen de este tema en OBS actualizado.

VirtualDJ: la biblioteca real del usuario está también en H:/VirtualDJ/database.xml.
El lector anterior solo examinaba AppData y leía Artist en lugar de Author.
Ahora descubre ubicaciones conocidas en discos locales montados, interpreta Author
y el sufijo Remix, y exige una coincidencia única en un escaneo XML completo.
El XML malformado, DTD, duplicados ambiguos o límites excedidos no eligen portada.
La prueba con el provider Automatic real recupera «Sabotage (Sickmix Intro) (Clean)
110 — Beastie Boys», encuentra el archivo sin ambigüedad en 20 ms y extrae su JPEG
de 142116 bytes (Ready). Imagen inspeccionada: portada amarilla «90's Pop Vol.3».
Seis suites relacionadas PASS; lifecycle OBS 32.2.2, 23 originales y cierre Exit=0.

Serato: el usuario confirma recepción y cambio de título en 4.0.10 después de
activar OBS Playing en Setup → Expansion Packs → External Control.
La retirada del panel Serato Remote no demuestra la retirada del protocolo local.
Se evitan nombres Bonjour en conflicto entre lectores simultáneos y se conserva
el número de error en el diagnóstico. La portada de Serato sigue pendiente;
no se ha implementado ni certificado con esta actualización.

El 15 de octubre se aplicó al OBS oficial, con autorización y copia de seguridad,
la DLL SHA256 1161B4B0B143FBE77955A99E1EC9E7F78E1E0A0BAFC77C06DE55BA3FEE692877.
Se conservaron escenas, configuración y versiones anteriores. Es un parche de
prueba sobre RC2; aún no se ha generado ni cerrado RC3. Evidencias en
outputs/official-vdj-live-cover-probe.txt, official-vdj-regression-authorized.txt,
official-vdj-lifecycle-run.txt y official-artwork-update.json.

Comprobación visual en OBS oficial 32.2.2 el 15 de octubre: VirtualDJ Automatic,
tema ficticio desactivado, «Turn Your Lights Down Low (Redrum) — Bob Marley &
Lauryn Hill» muestra en la fuente PRO Vertical Urban la portada blanca DJPool,
igual que en el deck de VirtualDJ. El perfil del streamer deja de ocupar la
portada cuando llega la imagen real. Cambio entre dos portadas distintas todavía
pendiente de observación en ese momento; esta evidencia no certifica todos los temas/versiones.

Verificación posterior del cambio solicitado: el usuario cargó «Bohemian Rhapsody
(Dj Mhark Redrum) (Clean) — The Braids». OBS oficial muestra la portada azul con
las dos integrantes y el texto «Bohemian Rhapsody», igual que el deck B de
VirtualDJ. La imagen blanca DJPool del tema anterior fue sustituida. Cambio de
título, artista e imagen comprobado visualmente; no se midió latencia ni se
observó toda la transición intermedia. Caso sin portada todavía pendiente.

## Geometría y panel, 15 de octubre

Vinyl, Retro y Glass utilizan el centro y límites de las aberturas medidos en
sus assets preservados. Una portada más pequeña se centra en ambos ejes.
Todas las portadas PRO rellenan su caja manteniendo proporciones y con recorte
centrado; pruebas con imágenes cuadradas, horizontales y verticales PASS.
El fondo de Vinyl se separa antes de aplicar el mismo recorte/rotación que el
marco. Detalles dorados permanecen en la capa fija; el fondo no cubre la portada.

Al cambiar de estilo en el selector se restablecen sus medidas originales:
lienzo, portada, márgenes, separación y cajas de texto. Tipografía, colores y
animación se conservan. No se recuperan los ajustes geométricos de un estilo
abandonado, según la petición explícita posterior del usuario. Abrir propiedades
sin cambiar estilo conserva la geometría actual; cargar un preset guardado
sigue aplicando su apariencia guardada.

Programa DJ, Lectura, Categoría y Estilo son los primeros cuatro controles.
Tema ficticio, pruebas anterior/siguiente y portada de prueba quedan al final.
Seleccionar imagen manual abre QFileDialog nativo; cancelar no modifica ajustes.
URL HTTPS opcional se introduce mediante diálogo en Advanced; quitar la imagen
manual permite volver al perfil automático sin borrar el archivo seleccionado.

El informe de fase 24 no demuestra ahorro causal de CPU/GPU entre perfiles en
el equipo medido. El usuario solicita retirarlos: la fuente fija perfil interno
Balanced y omite el selector, manteniendo lectura de ajustes antiguos.
No se altera la API de cadencia utilizada por los bancos ni el motor compartido.

Validación: banco completo anterior 191 comprobaciones y 95 capturas; banco
focalizado final 133 comprobaciones, 21 capturas, 23 originales, Exit=0. Dos
suites CPU/host PASS, incluyendo preservación de luces, imágenes no cuadradas
y centrado al cambiar tamaño. Se corrigió el diagnóstico TEST_HOOKS que intentaba
leer metadata de un frame de reposo; esta instrumentación se excluye de la DLL
de producto. Primer crash de banco conservado en work/vertical-layout-pro-pilot.

Petición posterior: en diseños automáticos PRO/solo texto, los controles de
margen, separación, distancia desde arriba/portada y tamaño de portada se ocultan.
La geometría base del estilo se fija; la portada PRO ocupa automáticamente su
abertura. Dos sliders firmados mueven el bloque de título/artista sin desplazar
portada ni marco. Cero conserva la base; las traslaciones se limitan al lienzo.
Los offsets se restablecen al cambiar estilo y no invalidan el frame visible.
La adaptación escala con proporciones conservadas y recorte centrado, sin deformar
una portada rectangular. Se conservan los datos anteriores y la geometría manual
BASIC. La separación de líneas propia de los clásicos permanece disponible.

Recorrido posterior final: 270 comprobaciones, incluyendo dos recorridos por
todos los clásicos y todos los PRO, y 21 capturas de los tres verticales.
Movimientos +20/-20 y retorno a cero verificados en OBS, sin nuevas rasterizaciones
ni frame visible vacío. CPU/host: dos suites PASS; tamaños cuadrado, horizontal,
vertical y rectangular no desplazan el centro de imagen. Prueba y DLL sin hooks
quedan identificadas en outputs/final-all-style-geometry-run.txt y los informes
de actualización; el instalador RC3 sigue pendiente de las demás correcciones.

## Portadas horizontales y superficie del disco, 15 de octubre

El usuario detectó que las aproximaciones cuadradas no ocupaban los huecos reales
de varios horizontales. Se sustituyen por la abertura cerrada del asset original,
con su contorno y borde suavizado. Marco y portada comparten la misma transformación:
Minimal y Glassmorph recortan primero el margen del recurso original. Urban conserva
la inclinación mediante un cuadrilátero y máscara, sin una segunda rotación.
Las fotos de diferentes proporciones se escalan y recortan desde el centro; los
bordes decorativos permanecen delante. No se alteran los archivos gráficos originales.

El disco tenía un clip explícito que eliminaba la mitad izquierda (horizontal)
o inferior (vertical), dejando una zona transparente cuando la portada no coincidía
con ese límite. Se dibuja ahora su superficie completa y después la portada y el
marco. Conserva la animación del sello y los reflejos existentes.

Los desplazamientos text_offset_x/y se incluyen en guardar, actualizar, cargar,
exportar e importar presets, con validación de enteros entre -2048 y 2048.
Se actualiza la ayuda del panel a los dos sliders y portada automática.

Validación final: tres suites CPU/host/presets PASS; tres proporciones de imagen
en los nueve horizontales sin huecos interiores. OBS portable 32.2.2, 333 checks,
95 capturas, 23 fuentes originales y cierre normal Exit=0. Análisis independiente
de los PNG GPU confirma nueve aberturas sin píxeles transparentes interiores y
la superficie antes recortada del disco horizontal/vertical. La DLL de producto
sin exports de prueba pasa lifecycle con consumidor compartido, 23 originales y
Exit=0. Evidencias horizontal-cover-cpu-tests.txt, horizontal-cover-normalized-all-pro-*
y horizontal-cover-gpu-pixels.json en outputs. Las primeras iteraciones fallidas
se conservan en work; no se presentan como validación final.

DLL de producto: SHA256 24F30273133917438F1BE3C8013796BF3FD8DB8A411F01741ADAC6FE58C87865,
copia congelada en work/horizontal-cover-product-20261015. RC3 no se cierra:
portada de Serato y comprobación visual de Rekordbox continúan pendientes.

Actualización oficial completada: DLL y hash de propiedad del instalador coinciden
con esa versión. Copia anterior verificada (94650C31...) y estado de instalación
conservados en work/official-horizontal-cover-backup-20261015. La solicitud de
elevación anterior terminó ejecutándose con retraso, después de que el usuario
informara que no veía el aviso. Se comprobó el resultado antes de preparar otro
actualizador; no se generó ni ejecutó una segunda actualización.

## Cierre de correcciones para RC3, 15 de octubre

Las catorce aberturas PRO se obtienen del contorno del recurso original, con la
misma transformación que el marco. Incluye Vinyl, Urban, Retro, Glass y Liquid
verticales; se elimina la doble rotación de Urban. Las proporciones de la foto
se conservan con recorte centrado. El texto de Vertical Vinyl se centra por su
contorno visible, comprobado también con fuentes de márgenes asimétricos.
Neon Club controla la opacidad de todo su interior siguiendo el borde escalonado,
sin eliminar los raíles ni alterar la intensidad de luz independiente.

Serato 4.0.10 comunica la ruta del archivo por su protocolo local de External
Control. El lector extrae la portada embebida en el worker, solo desde unidades
locales, con caché acotada; una canción sin ruta no reutiliza la portada anterior.
Dos canciones reales y dos imágenes distintas verificadas en las sondas nativas.
No se transmite la ruta local al otro PC. El crédito al final del panel conserva
el texto solicitado: «NEXUS OSB Plugins by DjEmiMas».

Validación previa al empaquetado: seis suites CPU/providers PASS; banco OBS
32.2.2 con 334 comprobaciones, 95 capturas y 23 originales, cierre normal.
Análisis de píxeles: las 14 aberturas no dejan huecos transparentes interiores;
discos horizontal y vertical completos. DLL de producto sin hooks validada en
lifecycle, instalada con copia y estado de propiedad actualizado. SHA256:
F5EB85E2113D366F8A27FF2856B6190BFAA9902AD747B7DA60F405F1A5495D9D.
Evidencias: outputs/vertical-neon-serato-*, serato-artwork-*-probe.txt y
official-vertical-neon-serato-update.json. El usuario confirma posteriormente
«ahora todo funciona» y solicita RC3. Esta aceptación no sustituye una nueva
matriz completa OBS 32.x ni la prueba LAN física aplazada.

## Nombre de producto solicitado tras RC3

El nombre visible pasa a «OBS Playing NEXUS» en la lista de fuentes y descripción
del módulo, instalador, guía y entrada de desinstalación. Se retira la etiqueta
Development del nombre mostrado. El ID obs_playing_next_development, las rutas
de instalación, claves de registro, ownership y perfiles conservan su identidad.
El nombre del instalador y ZIP de fuentes incorpora NEXUS; RC3 mantiene versión
1.0.0-rc3. La RC3 anterior se conserva por separado.

Si Windows deniega escritura al instalar o retirar el plugin, el instalador
solicita elevación con runas y reintenta la misma operación. No requiere reinicio
manual como administrador. La confirmación UAC pertenece a Windows y requiere
aceptación del usuario. Companion continúa por usuario; no se eleva por defecto.

## RC3.7 — anuncios opcionales en el chat de Twitch

Casilla desactivada por defecto, mensaje con {artist}/{title}, espera configurable de 1–30 s (3 s inicial), autorización de chat independiente mediante el navegador y credenciales DPAPI. Envío centralizado solo con una canción real mientras OBS transmite a la cuenta Twitch autorizada. Deduplicación entre fuentes, mínimo 10 s entre intentos, cancelación al desactivar/parar y sin repetición automática de un POST no confirmado. Companion no requiere configuración de chat. UI y autorización en cinco idiomas; validación local documentada en outputs/OBS-Playing-NEXUS-RC3.7.md. La autorización de usuario y el ensayo contra Twitch real quedan pendientes.
