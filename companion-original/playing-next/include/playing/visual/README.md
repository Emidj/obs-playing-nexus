# Contrato de integración visual — fase 09

Implementación real reutilizada desde OBS Twitch Overlays. `playing-shared-visual`
compila layout/VisualEngine/AssetManager; `playing-shared-obs-renderer` compila
su renderer y shaders originales. No add_subdirectory de Twitch ni copia de
su aplicación. Los metadatos CMake están en un único lugar, con14 inputs externos
fijados por hash. El motor conserva GPL-2.0-or-later; adaptadores propios MIT.

## Host y consumidores

El módulo Playing debe construir **un TrackVisualHost** en el hilo GUI y
**un oto::ObsVisualRenderer** compartido. Las fuentes obtienen referencias al
host; no construyen otro engine/cache/renderer por fuente o preset. Cada
consumidor conserva sus frames y su `TrackPublication`. La biblioteca no crea
singletons globales, sesiones DJ, threads, timers o procesos de red.

Esto comparte código y caché entre consumidores del mismo host. No es un servicio
ABI entre DLLs, ni comparte memoria con un segundo plugin/proceso Twitch. Esa
capacidad no debe atribuirse a una biblioteca estática. No hay source OBS
registrada todavía: integración de su host/lifecycle corresponde a fase10.

## De Track State al render

1. El owner del Core aplica una observación. Snapshot const puede cruzar hilos;
   nunca mutar Core desde un callback de otra afinidad.
2. Encauzar el cambio de snapshot/visibilidad/layout a GUI. No esperar a GUI
   sosteniendo el lock gráfico. `TrackVisualHost::prepare` comprueba GUI.
3. Proporcionar TrackLayout: lienzo, dos cajas y estilos TextNode originales.
   El host sustituye sus textos por title/artist canónicos UTF8 del Track State.
   Tamaños/colores no son un esquema persistente de presets o editor.
4. Dos escenas independientes de texto comparten el mismo VisualEngine. Un
   cambio de artista reutiliza el título; mover la caja conserva sus píxeles.
   Las posiciones forman parte de TrackFrame, no de las claves de rasterización.
5. Publicar el TrackFramePtr const por `TrackPublication`. Render obtiene una
   lectura atómica y llama `drawTrack` con el único renderer, contexto OBS
   adquirido y proyección/target SDR adecuados. No layout, I/O o refresh DJ aquí.

Null/inactivo/Waiting/Paused/Unavailable/stop no prepara texto. `prepare` devuelve
null; el host debe publicar ese null para retirar la publicación anterior.
Paused conserva el Track State en Core, pero no muestra sus textos como activos.
`drawTrack` null/inactivo sale antes de tocar GPU o exigir contexto. El renderer
reutiliza texturas al componer frames idénticos; OBS requiere componer las fuentes
visibles cada frame aunque no haya metadata nueva. Nunca llamar prepare por frame.

## Recursos y límites

CPU VisualEngine32 entradas/16MiB; GPU renderer128 entradas/64MiB; AssetManager8
entradas/8MiB por defecto. No representan memoria total de Qt/driver ni retenciones
externas. La documentación upstream aún dice GPU16MiB; se toma el valor del código
fijado, sin modificar esa documentación ajena. Lienzo≤2048×2048 y límites tipográficos
del TextLayoutEngine; TrackLayout exige ambas cajas dentro del lienzo. Engine/caché
solo en GUI; snapshots/frames publicados solo const. Publicación no realiza I/O.

AssetManager compilado y validado con PNG de fixture: lazy load, identidad compartida,
LRU, rechazo traversal y retención tras clear. Su load hace I/O/decodificación en GUI
y acepta PNG relativo seguro. No es resolver HTTPS/SVG/archivos arbitrarios, ni
puede recibir directamente todos los ImageReference del Core. No llamar load desde
draw. Artwork/fallback de perfil del streamer sigue siendo política del Core;
la resolución visual y adquisición de esos recursos pertenece a fase14. No
inventar un ID numérico Twitch para cumplir la API profileImage existente.

Continuidad fase14: el componente propio ArtworkLoader resuelve referencias
locales y assets de paquete de forma diferida, conservando este AssetManager
externo intacto. SessionHost integra sus ImageData mediante ImageNode/Frame const
y el mismo renderer. Contrato vigente en [ARTWORK.md](ARTWORK.md); no realiza
descargas HTTPS ni fabrica portadas que los providers no suministran.

La integración transmite title/artist y retiene snapshot completo (identidad,
artwork y origen incluidos), sin migrar presets. NORMAL/PRO, animaciones multicapa,
transiciones, personalización UI y perfiles de rendimiento siguen sus fases.

## Cierre

Detener nuevos eventos y cancelar callbacks pendientes del host antes del unload.
Publicar null y liberar publicaciones de consumidores; con contexto OBS vivo,
vaciar GPU. En GUI, vaciar CPU/AssetManager y liberar los frames restantes; después
detener Core y descargar el módulo. Clear no invalida handles externamente retenidos:
esos handles deben soltarse **antes de descargar la DLL**, no confiar en clear.

El probe verifica ese orden con todos sus objetos destruidos antes de publicar
PASS; no registra fuente ni forma parte de un payload instalable. Hace readback
solo en desarrollo para comparar CPU/GPU, nunca en el render de producto.

## Build opt-in

En el build actual `work/core-phase08h-build`, Qt6_DIR ya apunta al SDKQt6.8.3:

```powershell
cmake -S playing-next -B work/core-phase08h-build `
  -DPLAYING_BUILD_SHARED_VISUAL=ON -DPLAYING_BUILD_VISUAL_OBS_PROBE=ON `
  '-DPLAYING_SHARED_ROOT=C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays' `
  '-Dlibobs_DIR=C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/cmake' `
  '-DCMAKE_PREFIX_PATH=C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-2025-07-11-x64;C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps/obs-deps-qt6-2025-07-11-x64;C:/NEXUS-Dependencies/OBS Twitch alerts/OBS Twitch Overlays/.deps'
cmake --build work/core-phase08h-build --config RelWithDebInfo --parallel 2
ctest --test-dir work/core-phase08h-build -C RelWithDebInfo -V
./playing-next/tests/run-visual-obs-matrix.ps1 -WorkRoot work/phase09-new-matrix
```

Para un build nuevo, usar la configuración/identidad de fixture del README si
también se activa Companion. Para CPU solamente omitir VISUAL_OBS_PROBE y libobs;
SHARED_VISUAL sigue desactivado por defecto. No depende de ejecutables antiguos.
Runtimes de prueba existentes bajo tests/obs32 se copian a un WorkRoot nuevo,
configuración portable independiente. El helper rehúsa sobreescribir WorkRoot.

Original renderer bajo /W4 /WX tiene una excepción acotada /wd4456 por el nombre
`layer` sombreado dentro de un bloque shader; se conserva byte a byte. Los
adaptadores propios mantienen /W4 /WX sin esa excepción. No modificar lockfiles
para aceptar fuentes nuevos sin volver a auditarlos.
