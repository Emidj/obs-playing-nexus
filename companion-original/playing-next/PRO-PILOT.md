# PRO Neon Club — checkpoint de revisión

Fase 17 abierta. Primer estilo nativo y dos variantes de solo texto; los otros
13 diseños PRO siguen pendientes. El usuario pidió reproducir su referencia,
rechazó los marcos procedurales simplificados y valoró después el halo continuo
como casi perfecto. No se certifica igualdad píxel a píxel con la lámina.

## Selección y compatibilidad

Dos controles visibles: `preset_family` (0 Clásicos, 1 PRO) y `preset_style`.
El segundo se llena únicamente con estilos de la categoría elegida. Clásicos:
23 NORMAL; un alias antiguo seleccionado se conserva también en su lista.
Los antiguos `normal_preset`/`pro_preset` siguen existiendo como datos y propiedades
ocultas para cargar escenas anteriores, sin mostrar dos listas largas.

PRO: 1 Neon Club, 2 solo texto horizontal, 3 solo texto vertical; 0 desactivado.
Otros valores se rechazan mediante fallback al diseño básico. Los clásicos no
cargan portada desde la fuente OBS, aunque el ajuste viejo siga guardado.
Fuentes y colores, márgenes, distancias y portada opcional continúan en PRO.
Neon Club inicia a 960x367; dimensiones personalizadas guardadas prevalecen.

## Color y rendimiento

`pro_hue` 0–360, original en 0/360. Se conserva la rotación por GPU del renderer
compartido (`sourceRect` completo, `cellFilter=3`, `cellHue`), sin modificar el
motor externo ni añadir otro renderer. Rotación matricial RGB existente, no HSV;
no promete conservar HSV value ni luminancia perceptual. Texto y portada quedan
fuera del filtro. El usuario pidió conservar ESTE efecto para TODOS los PRO.
La ruta se aplica a cualquier TrackFrame con ProArt, no a un ID particular.

Los píxeles del marco y luz son los mismos al variar tono/intensidad/pulso; el
cambio usa parámetros de dibujo. No hay nueva rasterización ni subida de texturas
por arrastrar el tono. Host conserva snapshot, portada, timeline y reader; fuente
conserva el frame anterior mientras aplica un cambio solo de apariencia.

## Luz

`pro_light_intensity` 0–100 (default 65), `pro_light_pulse` opt-in, default false.
0 conserva íntegro el marco de referencia. 100 suma un halo continuo obtenido
con umbral de brillo y difusión separable de sus propias franjas luminosas.
Sin puntos añadidos, LEDs, centros blancos radiales ni transparencia del marco.
La difusión se prepara a un cuarto de resolución, radio 8, y se cachea. Canvas
mantiene su tamaño y limita el halo; no se introduce crecimiento del marco porque
no es necesario para este ajuste. No hay trabajo de blur/color/layout en render.

Cache débil de 8 entradas por canvas/cover; tono excluido. Dos rasters PM RGBA
por estilo activo, compartidos por el host. Intensidad no es opacidad de la fuente.
Pulso usa el timer central y solo modifica ganancia; desactivado, gráficos estáticos.
OBS sigue dibujando cada fotograma. No se afirma CPU/GPU cero ni profiling final.

## Recursos y revisión

`presets/pro-assets/neon-club-reference-v1.png`: marco preparado con edición
integrada image_gen a partir del recorte del usuario, sin badge/texto/portada.
`neon-demo-artwork-v1.png`: artwork ficticio de demostración separado; no sustituye
la lectura de artwork de proveedores. Prompts y procedencia en outputs.
Recurso del marco embebido; carga/escala una vez por tamaño activo. La portada
solo se usa en el launcher de revisión, no como tema real del producto.

`tests/open-user-preview.ps1` crea una copia OBS 32.2.2 nueva bajo work, incluye
la biblioteca DB verificada para mostrar rekordbox y abre una escena editable.
Sin heartbeat/cierre automático; no modifica OBS habitual ni instala producto.
El bootstrap es para la sesión de revisión, no certifica restauración al reiniciar.
## Controles de revisión actualizados (12 octubre)

Programa DJ primero y Lectura visible inmediatamente después. Tema ficticio usa
texto ya preparado, sin campos de edición; los botones Probar tema anterior y
Probar tema siguiente activan la prueba local y alternan tres canciones para
ver OUT/cambio/IN. No adquieren providers DJ. Fade/Slide/Zoom y su duración
siguen configurables. Float/Breathe retirados de la UI y anulados al cargar
ajustes anteriores; se conserva el código histórico de pruebas.

Velocidad de animación 25–300%, con continuidad de fase; desplazamiento de texto
con su velocidad propia. Tono y luz GPU sin volver a rasterizar ni cargar portada.
Halo amplio de bandas existentes reforzado al doble respecto a la revisión v7,
sin puntos. Opacidad del fondo del título 0–100% separa el material oscuro interior
del marco: no afecta letras, portada ni luces. Separación exacta de píxeles y
composición original íntegra a 100%; a 0 muestra lo que haya detrás en la escena.

En Neon Club, margen exterior ahora añade inset al bloque de texto por ambos
lados; no mueve el marco ni la portada. Distancia portada/texto desplaza el texto
por píxel respecto a la posición de referencia (64 px); solo aparece con portada.
El marco fijo impone límites para no sacar el texto del área disponible.

## Corrección Q — desaparición durante cambio de título

El usuario detectó que la tarjeta entera desaparecía al cambiar canción en v8.
HECHO: el host asignaba wholeTransition a toda fuente sin NORMAL, incluido PRO.
Ahora Neon Club conserva material, panel y luces mientras Fade/Slide/Zoom se
aplican solo a título/artista. Los cambios de datos demo conservan la publicación
hasta aplicar el comando GUI. La versión v8 queda anterior a esta corrección.

phase17q-transition-32.2.2-result.txt: 36 checks, 10 capturas, 23 originales,
Exit=0. phase17q-pixels.json: 8/8 gates, incluido marco sin cambios durante cambio
(frameChanged=0, 30669 píxeles visibles). Dos suites afectadas 2/2 PASS, 13.88s.
Regresión completa anterior a Q: L 44/44. No se declara nuevo full CTest Q.
Binario final Q hooks OFF SHA256:
801CE4189B0C7B3AE062594AAE647D6583BCBA902F369EAD693E12F3353FDA16.
Exports en phase17q-final-exports.txt y lifecycle final en phase17q-final-lifecycle.
Preview actualizada v9 bajo work, sin harness/auto-cierre.

## Corrección R — texto fuera del marco

Slide desplazaba texto sin límite fijo y Zoom podía ampliar la capa fuera de su
caja. El frame ahora conserva las dos cajas de texto para todos los modos.
Renderer PRO aplica clip fijo a título/artista y a los tiles de ping-pong, sin
mover el clip con la animación. Sin cambio del motor compartido ni ABI del frame.

OBS32.2.2: 36 checks, 12 capturas, 23 originales, Exit=0.
phase17r-pixels.json: 10/10 gates; Slide escaped=0/12720 píxeles de texto cambian,
Zoom escaped=0/17166. Marco intacto durante transición. Suite visual 1/1 PASS.
Final hooks OFF SHA B40CE7E363034A977F2DBAC8FBEC40691C6E92B5B7DFEED2C017AA03AFDD3BD4;
exports sin diagnóstico. Preview v10 es la revisión actual. Vinyl Classic todavía
no implementado: correcciones solicitadas tuvieron prioridad antes de ampliarlo.

Vinyl Classic setting4 integrado: disco realista separado, giro GPU propio con velocidad continua; portada rectangular fill-crop bajo el borde y tipografia PRO InkCenter/padding/ajuste vertical. Informe actual outputs/OBS-Playing-Next-fase-17s-v-vinyl.md. Otros12PRO pendientes.

Correccion W/X: prevalece portada CUADRADA, 50% disco acotado y reflejos FIJOS. Capa de impresion gira sobre disco con iluminacion fija.15capturas/40checks OBS32.2.2,14gatesPASS,2/2suites afectadas. Previewactualv14. Informe17s-v con anexoW/X.

## Correcciones AA/AB — giro evidente y reflejos que tiemblan ligeramente

Nueva petición del usuario prevalece sobre intensidad totalmente fija: dirección
fija de reflejos y vibración muy suave de intensidad durante el giro. Impresión
asimétrica crema sobre etiqueta roja, mayor contraste. Giro base ahora6seg/vuelta
al100%; slider25–300 conserva ajuste/continuidad. No afirma RPM físicos de33⅓.

Reflejos extraídos una vez de áreas neutras/brillantes del propio disco, excluye
etiqueta roja. Capa separada sin rotación/desplazamiento, ganancia2–6% de esa capa
con dos oscilaciones ligadas al reloj de animación. Sin variar opacidad del disco
completo ni mover marco/portada/texto. Una textura adicional cacheada; no I/O ni
preparación de píxeles durante el draw. Sin medición GPU universal de este efecto.

AA: giro contrastado,40checks/15capturas/Exit0. AB final:40checks/15capturas,
23originales,Exit0 OBS32.2.2;14/14 gatesPASS. Giro/reflejos1844pix cambiados,
fuera del disco0, reflejos188pix cambian como máximo2 nivelesRGBA/255 entre
capturas; mitad izquierdaalpha0/derecha252. Portada cuadrada rellenada y límites
verticales alineados conservados. Regresión completa AB: ver phase17ab-ctest.txt.
