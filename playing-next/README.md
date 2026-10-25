# OBS Playing NEXUS 1.0.1

25 de octubre de 2026. Actualización del plugin con consulta pública para OBS News Ticker NEXUS.
Instalador único, con el Companion estable 1.0.0 sin cambios. Identidades, escenas y ajustes conservados.
[Notas 1.0.1](RELEASE-1.0.1.md).

# OBS Playing NEXUS 1.0.0

Versión estable Windows x64 / OBS Studio 32.x, cerrada por petición del usuario
el 22 de octubre de 2026. Instalador único para el plugin y Companion, que se
utiliza únicamente en el PC DJ. Incluye Traktor Pro 3/4, VirtualDJ, rekordbox y
Serato, portadas, estilos, personalización y anuncios opcionales de Twitch.
El Companion usa una ventana compacta adaptable al área de pantalla disponible.

Instalador: `OBS-Playing-NEXUS-Setup-1.0.0.exe`.
Fuentes y avisos: `OBS-Playing-NEXUS-1.0.0-Sources.zip`, entregados aparte.
[Notas de versión y alcance de validación](RELEASE-1.0.0.md).
Las secciones siguientes conservan el historial de desarrollo.

## Integración News Ticker — desarrollo, 25 de octubre de 2026

La fuente nativa publica ahora una consulta de solo lectura para OBS News Ticker
NEXUS. El contrato y la prueba están en [NOW-PLAYING-BRIDGE.md](NOW-PLAYING-BRIDGE.md).
Se incorpora a las compilaciones de desarrollo; el nuevo instalador 1.0.1 incluye esta actualización; el OBS habitual no se modifica automáticamente. No cambia Core, providers,
Companion ni los presets existentes; tampoco usa exports de diagnóstico.

## RC3.9 — Traktor Pro 3.x y 4.x

Un mismo selector Traktor Pro 3 / 4 detecta ambas familias. Layouts separados
con RTTI validada, deck MASTER y portada de su caché nativa. Pruebas reales:
3.5.0 build 270 y 4.4.1 build 22; no equivalen a probar cada versión 3.x/4.x.
Contrato y límites: [TRAKTOR.md](TRAKTOR.md).

## RC3.8 — Traktor Pro 4.x

Nuevo lector nativo en OBS y Companion: título, artista, deck MASTER y portada
desde la caché de Traktor. Detección de la rama 4.x y validación dinámica de RTTI.
Prueba real en 4.4.1 build 22; otros 4.x habilitados, sin matriz de instalaciones.
Contrato, configuración y límites: [TRAKTOR.md](TRAKTOR.md).

## RC3.1 — portadas entre dos PCs y guía ilustrada

El transporte Companion incorpora envío de la portada local por TLS, después de
negociar compatibilidad. El plugin y el Companion deben actualizarse a RC3.1 para
recibir estas imágenes. El título sigue siendo compatible con la RC3 anterior.
Las imágenes tardías se asocian a la canción y conexión que las originaron;
no se interpretan rutas del PC DJ como archivos del PC de streaming.

[Configurar Companion paso a paso, con imágenes](docs/companion/README.md).
[English setup guide with illustrations](docs/companion/README.en.md).
La versión HTML de esa guía se abre sin conexión y se incluye en ambos componentes.
Pruebas locales de transporte, correlación y compatibilidad superadas; comprobación
de las portadas con ambos PCs físicos actualizados pendiente.

## Nombre de producto y RC3 actualizada — 15 de octubre de 2026

Nombre visible: **OBS Playing NEXUS**, tanto en la lista de fuentes de OBS como
en el instalador, guía y registro de desinstalación. RC3 conserva los identificadores
de fuente, directorios y perfil Companion existentes para mantener compatibilidad.
El instalador actualizado se denomina OBS-Playing-NEXUS-Setup-1.0.0-rc3.exe.
Las candidatas anteriores permanecen en outputs.

## Estado de fase 28 — instalador RC1 verificado localmente

Un EXE de 37,4 MB para plugin/Companion con imagen NEXUS y fuentes separadas.
34 checks, carga normal/vídeo OBS 32.0.0/32.2.2 y Companion instalado PASS.
Corregida raíz relativa de assets al descubrir el módulo. Windows limpio,
Program Files/elevación y LAN física siguen pendientes. Siguiente: fase 29.
[Cierre 28](../outputs/OBS-Playing-Next-fase-28-cierre.md).

## Estado vigente: fase 27 — candidata local RC1 preparada

Plugin y Companion congelados con manifiestos, fuentes y avisos. Lifecycle OBS
32.2.2 y paquete Companion verificados. Instalación, Windows limpio y LAN física
pendientes explícitos; siguiente fase 28 Installer/Uninstaller.
[Cierre 27](../outputs/OBS-Playing-Next-fase-27-cierre.md).

## Estado vigente: fase 26 completada en estrés local de 30 minutos

Sesión abreviada por petición del usuario: 60 FPS, cuatro frames retrasados
adicionales y cierre normal. Memoria privada 407,14 a 408,39 MiB entre los
minutos 5 y 30; tres cierres instrumentados adicionales sin excepciones.
No certifica estabilidad de horas ni incluye recuperación final. Incidencia
histórica PLAYING-18-001 no reproducida, prioridad reducida. Siguiente: fase 27.
[Cierre 26](../outputs/OBS-Playing-Next-fase-26-cierre.md) y [método](HARDENING.md).
## Estado vigente: fase 25 completada en validación local

Compatibilidad comprobada en OBS 32.0.0/32.0.4/32.1.2/32.2.2 Windows x64.
Corregida la sustitución de tipografías por defecto en ajustes compartidos y
una conversión RGB del shader compartido para OpenGL. DLL final sin instrumentos
verificada en los cuatro runtimes; 45/45 suites Playing y 25/25 Twitch PASS.
Siguiente fase 26: estrés/hardening; PLAYING-18-001 sigue pendiente.
[Cierre 25](../outputs/OBS-Playing-Next-fase-25-cierre.md) y [contrato](COMPATIBILITY.md).

## Estado vigente: fase 24 completada en profiling local

45/45 suites PASS y DLL final sin instrumentos verificada en OBS 32.2.2 aislado.
861 muestras de recursos y 84 observaciones de latencia; alcance limitado al
equipo y fixtures documentados. Conservación de memoria por cachés observada;
no certifica estabilidad de horas ni todos los equipos. Siguiente: fase 25.
[Cierre 24](../outputs/OBS-Playing-Next-fase-24-cierre.md) y [método](PROFILING.md).

# Playing Next — fase 15 completada en desarrollo

Preset Engine data-driven:23 NORMAL y3aliases en `presets/normal-v5.json`.
Texto/layout/arte/movimiento, metadata, settings y preview declarativos; un
renderer compartido y catálogo inmutable embebido. Compatibilidad23/3 validada
antes del registro, mismos IDs/nombres/números en escenas y selector.
265checks del parser y2060del catálogo,42/42 suites PASS; cuatroOBS32.x verifican
render completo, selector real y roundtripJSON de settings. Originales intactos.
[Contrato actual](presets/README.md) y [cierre13](../outputs/OBS-Playing-Next-fase-13-cierre.md).
Fase14 integra carga local diferida y cache compartida, PNG/JPEG/WebP y SVG
estatico acotado, assets PNG/SVG bajo raiz explicita y fallback opcional al perfil.
Fuente BASIC/NORMAL con imagen configurable y recarga, renderer compartido,
cancelacion/correlacion y cierre; defaults conservan apariencia23/3.
43/43 suites PASS (loader117/host74), cuatroOBS32.x artwork/pixels PASS;
23 NORMAL/3aliases y DLL final hooksOFF verificados. Sin nueva distribucion.
[Contrato del cargador](include/playing/visual/ARTWORK.md) y
[cierre14](../outputs/OBS-Playing-Next-fase-14-cierre.md).
Fase15 completa: keyframes acotados, Linear/Cosine/Step, Once/Repeat/PingPong,
relojes pausables por consumidor y muestras const para cuatro capas logicas.
Perfiles de movimiento embebidos, opcionales, conservando default23/3. Un unico
renderer/timer de animacion; sin nuevos rasters/uploads durante los intervalos
probados.44/44CTest PASS,1030checks sampler y81host, cuatroOBS32.x y catalogo23/3.
DLL final hooksOFF verificada. [Contrato](include/playing/visual/ANIMATION.md)
y [cierre15](../outputs/OBS-Playing-Next-fase-15-cierre.md).
Siguiente16: Track Transitions. Fisica08/profiling pendientes; no CPU/GPU/IdleZero
medidos por estos tests funcionales.

## Historial de checkpoints (contratos anteriores, no estado vigente)
Checkpoint13-C2: texto/geometría/fondos/adorno de Prism/Air/Cantilever en datos,
recurso activo `presets/normal-v4.json`, renderer compartido conservado. Oráculos
anteriores de texto y geometría separados del producto. Fase13 abierta: catálogo
de compatibilidad y cierre pendientes. [Informe13-C2](../outputs/OBS-Playing-Next-fase-13c2-clasicos.md).
Los contratos anteriores conservados debajo son históricos, no el recurso activo.
Checkpoint13-C1 añade tipografías de Prism/Air/Cantilever al recurso activo
`presets/normal-v3.json`, conservando GDI y fallback Unicode. Las definiciones
v1/v2 se conservan históricas. Fase13 sigue abierta: geometría/formas de los
clásicos y catálogo de compatibilidad pendientes. Véase el
[informe13-C1](../outputs/OBS-Playing-Next-fase-13c1-tipografia.md).

Fase13 en curso: checkpoint13-B añade fondos/adornos declarativos a los ajustes
de texto/layout/motion de13-A. Veinte rediseños completos en `normal-v2.json`,
preparador CPU genérico y mismo renderer compartido;120comparaciones exactas de
píxeles PASS. 23IDs/3aliases conservados;42/42 suites PASS y catálogo probado con
la misma DLL en cuatroOBS32.x. [Contrato de datos](presets/README.md) e
[informe13-B](../outputs/OBS-Playing-Next-fase-13b-recetas.md). Tres clásicos y
catálogo de compatibilidad aún requieren separación antes de cerrar fase13.
No imports ni reload en caliente en este checkpoint. El catálogo v1 se conserva
como evidencia histórica; recurso activo v2 con límite256KiB.

Dirección vigente desde el 11/10/2026: el usuario pide **rediseñar los NORMAL
desde sus imágenes antiguas para el motor nativo actual**. La reproducción
exacta CEF/GDI de checkpoints anteriores queda como evidencia histórica;
no es el gate para los nuevos rediseños. Conservar nombres, aliases y originales.
Catálogo completo: BASIC (0), 23 NORMAL (1..23) y tres aliases (24..26).
Prism (1), Air (2) y Cantilever (3) conservan sus valores anteriores.
Usa el renderer compartido y el timeline central, con scroll/OUT→IN y
movimiento decorativo sin volver a rasterizar en cada frame. Terminal conservado.
Ver [criterios de rediseño](../outputs/OBS-Playing-Next-fase-12-redisenos.md).
La matriz completa de OBS32.x y las 40 suites pasan. Cierre autoritativo y
límites en [fase12 completada](../outputs/OBS-Playing-Next-fase-12-cierre.md).
Fase13 en curso: Preset Engine data-driven. Los checkpoints siguientes son
históricos y no sustituyen este cierre; las pruebas físicas aplazadas de fase08
y el profiling siguen pendientes. No distribución ni instalación final.

Checkpoint12-K: corregida la acumulación de avances del adapter de texto nativo.
108anchos Terminal dentro de1/64unidadCSS (máximo0,01275); tinta real y límites
verificados. No certifica glifos/sombras/pixel parity ni conecta Terminal alOBS.
[Informe12-K](../outputs/OBS-Playing-Next-fase-12k-terminal-font.md).21DOM pendientes.

Checkpoint12-J: geometría Terminal contrastada con36layouts originales y
preparación GUI nativa de texto con espaciado, tiles y caché del motor real.
Terminal sigue candidato: diferencias de ancho de fuente medidas; faltan sombras,
capas y conexión/pixel parity enOBS.21DOM pendientes. Informe
[fase12-J](../outputs/OBS-Playing-Next-fase-12j-terminal-preparacion.md).


Checkpoint12-H: composición de grupos incorporada al renderer compartido real,
identidades/revisiones/LRU32 y presupuesto conjunto64MiB. Copias exactas/pins/
rollback para los dos archivos. Playing34/34PASS; Twitch25/25 y226gruposGPU PASS.
APIreal16checks por cuatro OBS32.x, Exit0. HooksOFF carga/cierra en32.2.2.
Solo premultiplied/sRGBcodificado;21DOM y aliases aún pendientes. Informe
[fase12-H](../outputs/OBS-Playing-Next-fase-12h-motor-grupos-compartido.md).
Para el contrato actual usar tests/prepare-shared-group-contract.ps1; el helper
sRGB12-D queda como reproducción histórica de su versión anterior.

Checkpoint12-G: prototipo GPU separado del producto verifica composición de
grupos (opacidad/escala después de hijos), anidamiento, caché y límites.
19checks por cuatro OBS32.x, Exit0,23originales. Motor compartido sin cambios;
no se integra ningún nuevo preset. Informe
[fase12-G](../outputs/OBS-Playing-Next-fase-12g-grupos-gpu.md).

Checkpoint12-F (11/10/2026): curvas de las53animaciones de los21DOM portadas
al contrato normal-motion existente y comprobadas con2866muestras del navegador.
34/34CTest PASS; módulo hooksOFF carga/cierra enOBS32.2.2 aislado, Exit0.
Esto valida tiempos/easing; todavía no interpola las propiedades ni integra
los21diseños al renderer. Fase12 ABIERTA. Informe
[fase12-F](../outputs/OBS-Playing-Next-fase-12f-curvas-nativas.md).

Fase12 incluye movimiento por petición del usuario. Checkpoint12-E: Prism/Air
conectados al host/renderer compartido, texto GDI, scroll y OUT/IN; selector
de desarrollo y opciones Fade/Slide/Zoom. Build y33/33CTest PASS. Matriz funcional
de dos diseños en cuatro OBS32.x PASS. Color exacto en cuatro mattes a720px
en las cuatro versiones. Comparación ampliada:36/36PASS en32.2.2, verificando
los18títulos/artistas reales del provider frente a entradas originales normalizadas.
Los otros21 siguen pendientes; no generar bancos del experimento con gateFAIL.
Auditoría21DOM:460instantes/keyframes; solo el fondo Terminal pasa la separación
actual. Conservar composición de grupos antes de aplicar escala/filtros; no
confundir ese PASS del fondo con una fuente Terminal nativa lista.
Informe [fase12-E](../outputs/OBS-Playing-Next-fase-12e-entradas-y-composicion.md).
119/119Playing intactos; cuatro archivos shared ampliados con autorización,
copias exactas y pins actualizados; otros17 intactos. HooksOFF al terminar.
No declarar cerrada fase12 ni eliminar presets originales. Prueba física08 pendiente.

Fase11: overflow por título/artista, mínimos auto-fit, geometría de canvas/cajas
y alineación. Layout se actualiza sin reiniciar reader/Core/TLS; frames const,
caché reutilizada y validación de límites. Regresión28/28,1374checks; matriz65checks
por cuatro OBS32.x,23originales yExit0. HooksOFF al terminar. Informe
../outputs/OBS-Playing-Next-fase-11-cierre.md. Siguiente fase12:23NORMAL y aliases,
migración con paridad verificada antes de retirar recursos originales.

Fuente nativa de desarrollo con SessionHost y renderer compartido, texto básico
640×120, VirtualDJ/Serato/rekordbox y receptor Companion integrado en OBS.
Vinculación y perfil protegido, políticas de tema inicial/expiración por fuente.
28/28 suites y1351checks PASS; matriz final47checks por OBS32.0.0/32.0.4/32.1.2/
32.2.2, cierres normales y23fuentes originales. Controles comparables contador2
igual a nativo2: delta0, sin afirmar ausencia absoluta de fugas. HooksOFF al terminar.
Informe ../outputs/OBS-Playing-Next-fase-10-cierre.md. No instalador ni migración
de presets todavía. Fase11: Dynamic Text/Layout. Prueba física de
dos PCs pendiente; las pruebas TLS de esta fase fueron locales.

Fase09 cerrada: motor original compartido integrado opt-in con Track State,
preparación GUI, frames const/publicación atómica y dibujo por renderer OBS real.
Una instancia por host, title/artist en capas que reutilizan caché entre consumidores.
26/26 suites/1298 checks; prueba original visual1/1; probe nativo19 checks en
OBS32.0.0/32.0.4/32.1.2/32.2.2 x64, GPU/CPU coincidentes y cierres normales.
Counter final igual al baseline1, ninguna asignación adicional; no afirmar leak0.
119/119 originales Playing y21/21 motor intactos; GPL original preservado.
Contrato/build: include/playing/visual/README.md. Informe:
outputs/OBS-Playing-Next-fase-09d-cierre.md. Próximo: fase10 fuente básica OBS.
Fase08 conserva prueba física/estabilidad pendientes; Companion08-K sin cambios.
No plugin instalable nuevo, artwork visual/presets/editor o profiling certificados.

Fase09 iniciada con checkpoint09-A: auditoría del motor real Overlays y banco
aislado tests/visual-audit, sin integrar la aplicación Twitch. Nueve fuentes/
contratos fijados por SHA256; CPU visual/layout/assets compilados directamente,
prueba original1/1 PASS. Renderer aún requiere contexto/probe OBS y conserva
funciones específicas Twitch; no hay caché entre DLLs certificada. GPL declarado
en el motor, MIT propio conservado; ningún cambio al paquete Companion08-K.
Inventarios21/21 compartidos y119/119 Playing intactos. Informe y reproducción:
outputs/OBS-Playing-Next-fase-09a-auditoria-visual.md. Próximo09-B: mapping opt-in
Track State→Scene y caché compartida por host, antes del probe gráfico.
Fase08 permanece abierta con prueba física apartada por el usuario.

08-L amplía el harness del ejecutable con `--soak`, opt-in fuera de CTest:
60s detenido, 60 cambios Unicode, 60s sin cambios y reinicio del emisor con
reconexión del observador nativo. Dos GUI reales, perfiles efímeros, loopback;
249 checks y cierres normales. GetProcessTimes/MemoryInfo/HandleCount producen
mediciones por ventana; no mide GPU ni certifica Idle≈Zero o estabilidad larga.
No cambia producto; paquete vigente 08-K. Prueba física apartada por el usuario.
Informe y reproducción: outputs/OBS-Playing-Next-fase-08l-estabilidad-local.md.

08-K diferencia el dashboard del receptor: «PC DJ VINCULADO», destino guardado
y estado de conexión, sin el contador de clientes que pertenece al emisor.
Conserva IP/puerto durante los reintentos. La prueba manual en dos PCs confirmó
descubrimiento y conexión, sin software DJ abierto. El usuario cerró el PC DJ;
recepción de canciones, reconexión real y estabilidad siguen pendientes.
Validación local: UI78, ejecutable56, paquete portable60; baseline119 intacto.
Informe: outputs/OBS-Playing-Next-fase-08k-lan-y-receptor.md. Fase08 abierta.

08-J añade paquete portable de pruebas x64, guía para dos PCs y scripts
packaging/build-portable.ps1 y packaging/test-portable.ps1. Incluye Qt dinámico,
plugins Windows/Schannel/JPEG/WebP, CRT release app-local, DB auditada, licencias,
SBOM, código propio y fuentes completas Qt6.8.3 verificadas con receta OBS.
El harness opcional --portable elimina rutas Qt del entorno de los hijos y
comprueba DLLs cargadas desde el ZIP extraído, dos perfiles y cierre normal.
No instala OBS ni toca firewall. El segundo GUI es receptor provisional de prueba.
Paquete y resultado local: outputs/OBS-Playing-Next-fase-08j-paquete-portable.md.
Siguiente: ensayo en dos PCs físicos, estabilidad y profiling medido. Fase08 abierta.

08-I incorpora búsqueda de PC DJ en LAN IPv4 privada y vinculación guiada sin
copiar códigos. El PC DJ abre una ventana explícita de dos minutos; ambos PCs
comparan la identidad pública del certificado y confirman la solicitud inicial.
Las credenciales se transfieren por TLS con pin y se guardan bajo DPAPI. La
conexión siguiente utiliza el destino guardado y la reconexión automática.
«Revocar vinculación seleccionada» en el PC DJ retira el acceso inmediatamente;
«Olvidar vinculación anterior» en el receptor borra el destino y desactiva su
startup/retry, conservando la identidad local. Olvidar no revoca acceso remoto.
UDP 39009 para búsqueda; TCP temporal para pairing y puerto de datos configurado
(9000 por defecto). Sin cambios automáticos de firewall. Búsqueda de ocho segundos,
sin anuncios ni búsqueda autónoma en idle. Código manual disponible como alternativa.
Informe: outputs/OBS-Playing-Next-fase-08i-vinculacion-guiada.md. Validación local
con fixtures; LAN entre dos PCs físicos, cambio de IP por DHCP, paquete portable,
estabilidad y profiling pendientes. Companion final solo en DJ-PC; el receptor
GUI actual sigue siendo un banco de pruebas previo a la fuente OBS nativa.

08-H añade inicio automático opt-in al abrir: envío con ajustes guardados o
conexión al PC vinculado (modos excluyentes). Activar startup receptor activa
retry transitorio; Stop cancela sesión y evento pendiente. Profile v3; v1/v2
mantienen credenciales y migran a manual. No inicia con Windows, ni descubre LAN,
ni reemplaza aún el primer código de pairing. Companion final solo en DJ-PC,
receptor OBS pendiente; segundo GUI actual es de test. 24 suites/1218 checks,
OBS32.2.2 portable/23 fuentes PASS. Build aislado work/core-phase08h-build;
preparar su identidad de fixture con el helper antes de CTest. Informe:
outputs/OBS-Playing-Next-fase-08h-inicio-automatico.md. Siguiente: descubrimiento y
vinculación guiada inicial, después paquete portable y prueba en dos PCs.

08-G valida por interfaz dos ejecutables GUI reales: configuración, pairing,
recepción/cambio Unicode, desconexión que limpia canción y reconexión al PC
guardado. Perfiles ficticios bajo work, loopback; evidencia sin secretos en
outputs/phase08g-gui-evidence.json e informe OBS-Playing-Next-fase-08g-flujo-gui.md.
Ensayo interactivo separado de CTest; no cambia el producto. Próximo: paquete
portable de prueba y guía para dos PCs físicos, después estabilidad y profiling.

08-F incorpora `companion-actual-gui-processes`: ejecuta el binario GUI real con
dos perfiles aislados bajo work, dos rondas de arranque/cierre normal y reapertura.
37 checks; regresión 24 suites/1184 checks. No agrega hooks de test al producto.
Valida ciclo de vida, identidades, persistencia y listener cerrado al arrancar;
pairing/transmisión interactiva entre estos GUI, dos PCs físicos y mediciones
siguen pendientes. Informe: outputs/OBS-Playing-Next-fase-08f-ejecutable.md.

## Companion — fase 08 abierta, checkpoints 08-A/B/C/D/E

08-E añade reconexión opcional en la configuración del PC streaming, desactivada
por defecto. Solo errores transitorios de transporte o timeout programan un
reintento: 1, 2, 4, 8, 16 y hasta 30 segundos. Certificado, pairing, protocolo
o configuración inválidos detienen los reintentos. Stop y desactivar la opción
cancelan la espera; habilitarla no inicia una sesión por sí solo. Desconectar
limpia el tema remoto. Callbacks de sockets anteriores se ignoran.

Profile v2 guarda esta preferencia bajo DPAPI; v1 sigue leyendo con la opción
desactivada y conserva identidad/credenciales. Diagnósticos tipados alimentan
mensajes de UI sin registrar secretos. El harness conserva trazas acotadas y
fixtures si falla. La causa del fallo inicial de 08-D sigue sin demostrarse.
23/23 suites, 1147 checks; tres repeticiones entre procesos y OBS 32.2.2 portable
con 23 fuentes originales y cierres normales. Informe:
outputs/OBS-Playing-Next-fase-08e-reconexion.md. Pendientes: GUI final en procesos
independientes, dos PCs físicos, estabilidad prolongada y profiling medido.

08-D añade ensayo con dos procesos independientes usando los mismos
Sender/Receiver/Detector, perfil DPAPI e identidad nativa. History real aislado
→ TLS → Core remoto, cambio de canción y revocación; ambos exits normales=0.
CTest 22/22, 1110 checks. No son dos ejecuciones interactivas del GUI final.

`tests/run-companion-obs-smoke.ps1` prepara exclusivamente OBS portable bajo work
y ejecuta el receptor externo mientras OBS consume sus snapshots autenticados
mediante archivo atómico PNX1 de prueba. Lua lee frame acotado y el probe valida
codec/Channel/Registry/Core, con dos consumidores compartidos y fallback de perfil.
Este puente es solo test, no IPC instalable, fuente OBS ni autenticación nueva;
solo se confía en los frames de la fixture local controlada. Core/probe sin Qt.
OBS 32.2.2 y procesos cierran con Exit=0; las 23 fuentes originales pasan.

Primer ensayo entre procesos no entregó metadata en 7s; causa no demostrada.
Pasaron diagnóstico, tres repeticiones y regresión final, además de dos ensayos
OBS. No confundir esas pruebas con certificación de estabilidad prolongada.
Informe: outputs/OBS-Playing-Next-fase-08d-procesos-obs.md. Fase 08 abierta:
reconexión/diagnóstico, GUI en procesos independientes, dos PCs físicos y profiling.

08-C añade `obs-playing-companion.exe` opt-in con Qt Widgets, siguiendo la
referencia visual del usuario: dashboard oscuro/cian, canción, red, clientes,
detector, pairing y configuración secundaria. Iconos vectoriales provisionales;
barras decorativas estáticas, sin medidor de audio ni timer de animación.
Estados e IP proceden del Registry y transporte autenticado.

Perfil versionado dentro de ProtectedStore: identidad, credenciales/nombres,
destino receptor, dirección/puerto/selección DJ y perfil del streamer. RSA 2048
y certificado SHA256 se generan en memoria con Windows CNG, sin almacén de
certificados ni PEM privado en disco. CERT_CREATE_SELFSIGN_NO_KEY_INFO evita
referencias a containers persistentes; NCrypt usa flags silenciosos. Identidad
vence en tres años; renovación/re-pairing explícitos están pendientes.

Iniciar es explícito; por defecto loopback. Red local requiere opción expresa
y dirección IPv4 del PC DJ. No configura firewall ni discovery LAN. Código
PNXPAIR1 con endpoint/pin/credencial se muestra tras guardar y autorizar, sin
copia automática al portapapeles. Receptor lo guarda con DPAPI; revocar guarda
antes de retirar acceso. Cerrar detiene sesiones. Sin autoconexión al abrir.

Artwork preview local acotado/caché por referencia y fallback al perfil elegido.
No descarga imágenes HTTPS ni transporta rutas del DJ-PC al Streaming-PC. Es
preview de Companion, sin migración del renderer/presets ni integración fase 09.

21/21 suites, 1099 checks; 53 de identidad/profile/GUI. Dos ventanas en un proceso
y loopback; no dos PCs físicos. Informe: outputs/OBS-Playing-Next-fase-08c-companion-ui.md.
Abrir manualmente la vista previa aislada, después del build:

```powershell
./playing-next/tests/run-companion-preview.ps1
```

Helper restringe perfil/build a work y fija Qt windows. No es instalador; usa Qt
del paquete local y la DLL DB auditada original. Tests ejercitan la misma clase
Window del exe con perfiles aislados; sesión interactiva del exe final pendiente.

08-B añade `Detector`: reutiliza Registry/factories de VirtualDJ, rekordbox y
Serato, sin otro lector DJ. Auto selecciona únicamente un provider Playing;
varios candidatos producen Ambiguous sin canción arbitraria. Selección manual
permite Automatic/History según descriptor. History explícito exige ruta absoluta
UTF-8, normalizada; vacío conserva discovery del adapter. Paused manual conserva
Track State sin texto activo. Providers remotos se excluyen del detector.

Host conecta `Sender::clientsChanged` a `Detector::syncDemand()`. Con clientes
autenticados o preview explícita usa un timer central de 100ms para solicitar
sample no bloqueante. Sin demanda detiene timer y libera sus tokens; consumidores
ajenos mantienen su sesión. Registry/Sender deben sobrevivir a Detector en el mismo
hilo Qt. Este diseño todavía no constituye una medición de Idle≈Zero.

`ProtectedStore` guarda un payload opaco acotado con DPAPI del usuario Windows,
envelope PCS1, bloqueo de archivo y QSaveFile sin escritura directa de respaldo.
El host suministra directorio propio existente y ruta absoluta. Rechaza archivos
desconocidos/corruptos al guardar; no los sobrescribe. En 08-B aún faltaba el esquema
de perfil, generación de identidad de producción o pantalla de emparejamiento.
No se garantiza borrado seguro de todas las copias en memoria Qt.

Validación 08-B: 20/20 suites, 1046 checks; 48 nuevos de host y DPAPI Windows.
Incluye historial real aislado → Detector → TLS Schannel → Core remoto, todo
en un proceso y loopback. UI, procesos separados, dos PCs físicos y prueba del
Companion integrado en OBS pendientes en ese checkpoint. Informe en outputs/OBS-Playing-Next-fase-08b-host.md.

`Playing::Companion` añade Sender/Receiver con Qt Network y TLS de Windows
(Schannel en la prueba). Target opt-in; Core/Providers y probe OBS siguen sin
enlazar Qt. No se copió código de la aplicación Twitch ni se integró su renderer.

Sender escucha solo la dirección que le indica el host; tests exclusivamente
127.0.0.1 y puerto efímero. Identidad TLS proporcionada por host. TLS 1.2 o posterior,
pin SHA256 del certificado obtenido por pairing fuera de banda y nombre verificado.
Se exceptúan únicamente errores de confianza de la cadena del certificado fijado;
no se ignoran errores de nombre/fecha ni se permite plaintext.

`newDeviceCredential()` genera ID opaco y secreto de 32 bytes con el RNG Windows.
`Sender::pair()` autoriza explícitamente credenciales; `revoke()` desconecta el
dispositivo. Se autentica mediante credencial solo dentro de TLS; conexión activa
duplicada del mismo dispositivo se rechaza. Hasta 16 credenciales y 8 conexiones
incluyendo handshakes pendientes. Timeout de handshake/auth 10s; heartbeat 5s solo
con clientes autenticados. Receiver usa deadline del Channel (por defecto 15s;
debe superar heartbeat para mantener conexión). Sin reconnect autónomo todavía.

Tras autenticar, Sender entrega un epoch aleatorio nuevo y snapshot completo PNX1.
Receiver delimita frames acotados y alimenta Channel; changed avisa al owner para
refresh de una sesión compartida. Revocación/error/stop limpian estado; reconexión
inválida también lo limpia. Cola de salida hasta dos frames máximos; cliente lento
se desconecta en lugar de acumular indefinidamente. Máximo ocho frames por callback,
continuaciones por evento, sin bucle de espera en el host. Todos los métodos y
destrucción de Sender/Receiver pertenecen al hilo del event loop Qt.

Build opt-in de este checkpoint (Qt6_DIR apunta a dependencia existente):

```powershell
cmake -S playing-next -B work/core-phase08-build -G "Visual Studio 17 2022" -A x64 -DPLAYING_BUILD_COMPANION=ON -DPLAYING_BUILD_OBS_PROBE=ON -DPLAYING_TEST_REKORDBOX_DB=ON -DQt6_DIR="<Qt6/lib/cmake/Qt6>"
./playing-next/tests/create-companion-test-identity.ps1 -BuildRoot ./work/core-phase08-build
cmake --build work/core-phase08-build --config RelWithDebInfo --parallel 2
ctest --test-dir work/core-phase08-build -C RelWithDebInfo --output-on-failure
```

Helper genera certificado/clave efímeros **solo bajo work**, no en almacén Windows;
son fixtures, no identidad de distribución. CTest establece PATH al Qt de build.
Qt debe aportar backend TLS Schannel. Ver procedencia en THIRD-PARTY-NOTICES.md.
Resultado: 19/19 suites, 998 checks; 56 de transporte TLS real en loopback.

La fase 08 sigue abierta: falta host del detector usando providers existentes,
identidad y credenciales persistentes protegidas, flujo Pair New Device y GUI
Current Track/Network/Connected Clients/Auto Detector; proceso separado, integración
de receiver en host OBS y pruebas reales de dos PCs. No hay aún ejecutable Companion
de usuario, instalación, pairing interactivo ni nueva prueba TLS dentro de OBS.
Informe: `../outputs/OBS-Playing-Next-fase-08a-companion.md`.

## Network Provider — fase 07

`playing/providers/network-provider.hpp` prepara el funcionamiento remoto desde
el Core, conforme a la hoja de ruta. `network::registerProvider(registry, channel,
id="network")` registra Automatic explícitamente. Un Channel por origen remoto,
ID distinto si hay varios; Request exacto comparte provider/Core entre consumidores.
Descriptor.remote permite conservar procedencia DJ original en este registro de
confianza. Los registros locales siguen rechazando atribuciones a otro provider.

Channel recibe frames completos por eventos desde el transporte del host, admite
ingress desde otro hilo y mantiene un único estado pendiente acotado, coalesciendo
ráfagas. Provider.sample no hace red ni crea worker/timer; el host refresca una
vez por sesión, en el hilo propietario. Es estado actual, no archivo de eventos
ni garantía de conservar cada transición intermedia.

Formato binario propio **PNX1**, versión inicial: longitud big-endian u32, magic,
tipo Snapshot/Heartbeat, epoch de conexión, secuencia u64 y observación completa.
Máximo frame 65536 bytes; strings con longitud u16 y máximo 4096 bytes, epoch
ASCII alfanumérico/guion/underscore de 1..128 bytes. Decoder exige frame exacto,
flags 0/1, enums conocidos y Track State válido; rechaza trailing bytes/truncación.
No se transmiten punteros, trackRevision local, credenciales ni bytes de imagen.
El transporte futuro debe delimitar frames antes de entregar a Channel.

connect autoriza un epoch nuevo explícitamente desde el host; no descubre ni
autentica peers. El host genera epochs únicos después del pairing autenticado.
receive exige epoch actual y secuencia creciente no cero. Duplicados, mensajes
fuera de orden y de otro epoch no alteran estado ni renuevan actividad. Heartbeat
renueva actividad sin publicar canción. Timeout configurable 1..300s (15s por
defecto), reloj monotónico local del host; expire se sirve desde su deadline.
Tráfico recibido después del deadline no revive la sesión; desconexión limpia
Core con ConnectionLost, y reconnect exige un epoch nuevo y snapshot completo.

Snapshots remotos Paused pueden incluir retainedTrack sin texto activo, incluso
en la primera conexión. replaceRetainedTrack permite indicar ausencia explícita;
las observaciones locales de pausa mantienen su contrato anterior. El receiver
genera su propio token de artwork. Referencias locales/asset del DJ-PC se convierten
en Missing al exportar; decoder rechaza frames que intenten entregarlas como Ready.
HTTPS conserva referencia, su carga futura puede fallar y activar fallback al perfil
del streamer configurado en el receptor. Validar HTTPS no autentica ni descarga.

**Límite de esta fase:** codec y adapter receptor en memoria, probados con eventos
y DLL en OBS; sin listener, sockets LAN, cifrado o pairing. No usar Channel como
autenticación. Transporte autenticado/cifrado, generación de epoch, delimitación
de stream, avisos al owner, deadlines y cierre al último cliente corresponden al
host Companion en fase 08. No existe todavía una aplicación Companion funcional
ni certificación con dos PCs. Core continúa sin I/O.

Build x64 PASS, CTest 18/18 y 942 checks (89 específicos de Network). OBS portable
32.2.2: codec → Channel → Registry → Core, snapshot en pausa, dos consumidores,
fallback y expiración PASS dentro de la DLL. 23 fuentes originales y cierre normal.
Contrato wire y evidencias: `../outputs/OBS-Playing-Next-fase-07-cierre.md`.

## Track State — fase 06

`TrackState` unifica título, artista, identidad opcional con scope, procedencia y
estado/referencia de artwork. `Metadata` es un alias compatible con readers de
fase 05; `Snapshot::track` publica el tema completo inmutable. `metadata` conserva
la vista de texto activo para consumidores anteriores. Se rechazan UTF-8 inválido,
controles y campos de más de 4096 bytes; identidad presente exige scope/value.
El registry incorpora provider/modo como procedencia si el reader no la aporta;
rechaza una procedencia que atribuya el resultado a otro provider.

Identidad estable debe proceder del reader: no se genera con título, artista,
dirección de proceso ni un ID de fila de historial confundido con ID de canción.
Los readers auditados actuales entregan título/artista y no han verificado un
ID estable ni portada de canción; esos campos quedan desconocidos. El token
`trackRevision` es correlación local al proceso, distinto de identidad estable:
se renueva al cambiar tema/identidad/origen o tras limpiar/recrear la sesión.
No se serializa como identidad para Companion. Sin identidad proporcionada,
dos reproducciones indistinguibles por metadata no se pueden distinguir.

Playing publica texto y tema. Paused vacía texto activo y conserva el tema/portada
conocidos; Waiting, Unavailable y stop limpian ambos. Los readers que solo conocen
Waiting siguen sin inventar pausa/canción cargada. Snapshot retained no muta.
Actualizaciones idénticas no publican. `updateArtwork(trackRevision, artwork)`
rechaza resultados de temas/sesiones anteriores con StaleTrack, incluido el registry.
Solo hilo propietario; publicación conjunta de texto/tema. No timers, red, disco,
decodificación de imágenes ni renderer en Core.

Artwork: Unknown (reader sin información), Missing, Pending, Ready o Failed.
Solo Ready lleva una referencia válida; cambiar exclusivamente artwork conserva
el token de tema. Un reader que entrega Unknown para el mismo tema conserva
la portada enriquecida por el host. Missing/Failed explícitos la descartan.
Referencias: archivo absoluto Windows de unidad, HTTPS o asset ID del host,
con versión opaca para caché. Validación de referencia no verifica existencia,
descarga ni decodificación; el host marcará Failed si no puede usar la imagen.
UNC/device paths no forman parte de este contrato inicial.

**Opción solicitada: usar imagen de perfil del streamer cuando no haya portada.**
`resolveArtwork(artwork, streamerProfile, useProfileFallback=true)` elige portada
Ready; para Unknown/Missing/Pending/Failed elige el perfil configurado. Desactivar
la opción o no configurar perfil deja la imagen vacía. Perfil válido puede ser
local, HTTPS o asset. Es una política por consumidor: dos fuentes pueden elegir
distintos perfiles sobre una misma sesión sin iniciar otro reader. La portada
del tema tiene prioridad al estar disponible. Configuración/persistencia de UI y
carga/caché real de imágenes corresponden a las fases de fuente/Assets/Customization;
no se solicita ahora login ni se descarga un avatar de una plataforma.

Validación fase 06: 71 checks específicos más regresión existente; CTest 17/17,
853 checks. Probe DLL en OBS portable 32.2.2 prueba track, portada, pausa y fallback
con referencias de fixture; no afirma render de imágenes. 23 fuentes originales
PASS y OBS/peer externos Exit=0. Informe: `../outputs/OBS-Playing-Next-fase-06-cierre.md`.

## Serato — checkpoints 05-D1 a 05-D5

Auditados `obs-playing/serato-protocol.lua`, `serato-live.lua`, sus tests y la
evidencia histórica de Serato DJ Pro 4.0.10.213. Procedencia MIT preservada en
`THIRD-PARTY-NOTICES.txt`. No se certifican versiones nuevas ni una sesión nueva.

`serato-state.hpp` traslada State sobre eventos de status ya decodificados:
índices cero-basados, Title/Artist/Valid/Playhead y faders/crossfader. Admite
telemetría 0..3, pero selecciona únicamente decks 1/2. Nivel de video combinado,
umbral de rate .001, nivel .01, ventaja .08 para cambiar de selección y espera
de metadata .1s suministrada por el reloj del caller. Conserva selección en empate,
acepta artista vacío solo tras recibirlo y clear descarta todo al desconectar.
State no contiene reloj, timers ni I/O; el transporte y provider están separados.

AwaitingTelemetry no produce observación; sin candidato después de status,
Waiting sin metadata (no inferir Paused). Timestamp inválido no se publica.
Ingress conserva límite Lua de 8192 bytes/campo; el ganador debe pasar el
contrato del Core (4096, UTF-8/controles). Si falla devuelve InvalidMetadata sin
observación ni cambio de selección. No truncar ni presentar metadata anterior
como recibida. El adapter conserva el snapshot ante InvalidMetadata.

55 pruebas nativas de fixtures trasladan las secuencias de selección del Lua y
añaden límites, valores no finitos, Unicode y clear. No se ha realizado todavía
comparación automatizada byte a byte entre runtimes. Faders de
video no certifican mezcla audible física. La espera .1s no garantiza atomicidad
Title/Artist si llegan separados por más tiempo; mantiene comportamiento auditado.
El transporte gestiona pairing, desconexión y heartbeat compartido; State
no expira status silencioso por su cuenta, conforme al Lua.

`serato-protocol.hpp` implementa framing/OSC y status con límites: frames 512 KiB,
strings 8192 bytes, profundidad 8 y trabajo acotado. `serato-transport.hpp`
implementa sockets Windows de loopback y anuncio Bonjour LocalOnly, cargando
dnssd.dll solo desde System32. Autenticación MD5, pairing, seis suscripciones,
Ping y UnPair proceden del protocolo auditado; no envía comandos de reproducción.
Hasta cuatro peers, una sesión emparejada, timeout de tráfico 45s y desconexión
Unavailable/ConnectionLost. El protocolo conserva la limitación original del
sentinel sin escape en blobs; no se promete compatibilidad OSC universal.

`registerSerato(registry)` registra Automatic únicamente. Worker por Request,
sin sondeo autónomo; sample solicita/recoge trabajo. Recursos de transporte se
crean, usan y liberan en el worker. 68 checks de protocolo, 19 de sockets reales,
27 de worker y 42 adicionales con Bonjour instalado. Probe OBS portable 32.2.2:
peer externo, selección, Waiting, recuperación y UnPair; dos consumidores y
ambos procesos Exit=0. Fixtures no certifican una sesión Serato DJ nueva.

## Rekordbox Automatic — checkpoints 05-E1 a 05-E3

`rekordbox-live.hpp` descubre MainComponent mediante RTTI en secciones PE de
datos, recorriendo managers, players y controles con límites y validación de
punteros. Backend Windows solo lectura compartido con VirtualDJ, sin escritura
ni inyección. Acepta familias de versión 6/7 con cuatro componentes numéricos;
la estructura se valida en ejecución, sin prometer todos los builds de esas familias.
No usa una dirección fija de módulo por versión. Slice de búsqueda nominal 4ms,
strings hasta 2048 bytes incluyendo NUL; metadata publicada cumple el Core.

Selecciona MASTER activo, deck anterior activo o primer deck activo; actividad
requiere dos avances válidos de playhead y conserva gracia .65s. Ningún activo
produce Waiting; MASTER ambiguo conserva snapshot con AmbiguousSelection.
Revalida controles MASTER, sin garantía de atomicidad conjunta ni ABA. La API
del decoder admite selección manual; la factory Automatic usa selección automática.

`rekordbox::registerProvider(registry, absoluteDllPath)` registra History y
Automatic bajo el mismo ID. No combinar con registerRekordboxHistory. Dependencia
de base de datos confiable fija por registry; Automatic no abre esa base.
113 checks de decoder y 76 de worker con layouts desplazados 6/7 y lecturas
Windows reales. Probe OBS 32.2.2 con proceso propio: reproducción, MASTER 4,
Waiting, recuperación y salida del proceso; dos consumidores, 23 fuentes,
OBS/fixture Exit=0. La evidencia histórica y estos fixtures no sustituyen una
certificación nativa nueva con rekordbox real.

## Cierre de fase 05 — checkpoint 05-E4

Build MSVC x64 RelWithDebInfo /W4 /WX: PASS. CTest: 16/16 suites, 782 checks.
Worker compartido: 21 checks de excepciones, recursos/hilo, stop y ausencia de
lecturas autónomas; 40 ciclos sin crecimiento de handles en el fixture.
Discovery Windows añade gates de procesos múltiples, versión rechazada y ausencia.
Informe completo: `../outputs/OBS-Playing-Next-fase-05-cierre.md`.

Implementados los tres providers existentes: VirtualDJ y rekordbox Automatic/History,
Serato Automatic. Registro sigue opt-in; no fuente nativa instalada. Original Lua,
119 archivos de baseline y 23 presets conservados. Siguiente fase: 06 Track State.
Sesiones DJ reales nuevas, matriz OBS 32.x, profiling e instalación siguen pendientes
en los bloques de integración/certificación; Idle≈Zero no está medido.

## VirtualDJ Automatic — checkpoints 05-C1 a 05-C5

`virtualdj-selection.hpp` recibe frames ya decodificados y decide MASTER activo,
selección anterior activa o primer deck activo con tema. Solo versión auditada
8.5.9295.0 y 2/4 decks; incoherencia, direcciones/metadata inválidas rechazan
publicación. coherent=false por defecto; el decoder revalida los bytes usados.
Idle emite Waiting sin Metadata, sin inferir pausa o unload. Límite de strings:
2047 bytes más NUL, sujeto también a validación del Core. Veinte checks de
regresiones del Lua. El selector sigue siendo puro y separado del backend.

`virtualdj-memory.hpp` decodifica el layout real auditado mediante lecturas exactas
acotadas. Valida RTTI, índices, flags, CSong/CFakeSong, cadenas y segunda lectura
de todos los bytes usados, incluido texto. No garantiza atomicidad ni cambios ABA.
`virtualdj-process.hpp` implementa Toolhelp, versión de recurso estricta, módulo
AMD64 y OpenProcess QUERY_INFORMATION|VM_READ. No escribe memoria ni inyecta.
También comprueba ruta/versión del handle abierto y PE antes de usar los offsets.

`registerVirtualDj(registry)` registra un único ID con History y Automatic;
despacha cada modo al adapter correspondiente. Factory inerte, worker compartido
por Request exacto, sin polling/timers autónomos. sample solicita o recoge un
resultado sin I/O de proceso en el propietario. Stop une el worker y libera su
conexión antes de retornar; latencia bajo fallos de sistema no certificada.
`registerVirtualDjHistory()` se conserva History-only. No registrar ambas con
el mismo ID. El host futuro debe aportar el scheduler y manejar Pending/issues.

Missing/Multiple/UnsupportedVersion/InvalidLayout limpian Core; Access y
SnapshotChanged conservan snapshot. Metadata inválida en memoria invalida la
conexión. Sin decks activos se publica Waiting sin metadata, no Paused inferido.
Un error transitorio no publica estado inventado. Requests Automatic no admiten
ruta History ni encoding manual.

Tests: 22 de decoder, 17 de transporte Windows contra otro proceso propio y 58
de worker con layout sintético/lectura Windows real, además del selector y suite
existente. Probe OBS 32.2.2: proceso externo de fixture, MASTER 1→4, fallback 4,
Waiting y deck 2; dos consumidores comparten sesión; última liberación la cierra.
Ambos procesos terminan normalmente y se comprueban los 23 presets originales.
Ejecutar `tests/run-virtualdj-obs-smoke.ps1 -PortableTests <obs32-bajo-work>
-BuildRoot <build-bajo-work>` con runtime/helpers originales preparados. Solo
opera los procesos que inicia. El proceso sintético tiene su nombre propio;
el ABI de prueba no modifica el gate de discovery de producto.

Pendiente prueba nueva con VirtualDJ real 8.5.9295.0 y matriz OBS 32.x. No certifica versiones nuevas,
profiling Idle/CPU/GPU/RAM ni convivencia con plugins de terceros. Automatic
está implementado en la biblioteca opt-in; las fuentes instaladas siguen Lua.
El probe DLL y fixture son pruebas, no fuente OBS ni payload instalable.

## Rekordbox History — checkpoints 05-B1 a 05-B6

Se auditó el reader real `obs-playing/rekordbox.lua` y se añadió una prueba
de dependencia C++ x64 en 05-B1. El adapter real History quedó implementado
en 05-B2–B4 y probado dentro de OBS aislado en 05-B5–B6.
Activar con `-DPLAYING_TEST_REKORDBOX_DB=ON` al configurar CMake. Es opt-in y
no enlaza SQLite al Core ni al target Providers. Verifica el SHA256 del DLL
existente y carga esa ruta absoluta con búsqueda limitada a su directorio/System32.
Usa headers de `vendor/sqlite3mc/files/include`, conserva avisos MIT existentes.

La prueba extrae la consulta SQL del Lua auditado y crea únicamente un fixture
cifrado propio en el directorio de build. Verifica READONLY/FULLMUTEX,
`legacy=4`, query_only/trusted_schema, WAL/data_version, Unicode, sesiones,
entradas borradas, artista ausente y error de esquema. Los fixtures se conservan
como evidencia, no se usan bases del usuario. Un prepare correcto no garantiza
un step correcto cuando cambia el esquema: comprobar ambos en el adapter.

La prueba de dependencia opt-in requiere los headers de vendor ya auditados
(no versionados actualmente). El adapter de producto usa un subconjunto del
ABI C público, carga el DLL existente y no necesita esos headers ni import libs.
Los avisos de procedencia/licencia permanecen en THIRD-PARTY-NOTICES.txt.

Registro explícito: `registerRekordboxHistory(registry, absoluteLibraryPath)`.
El host aporta la dependencia de confianza verificada por empaquetado; no se
busca por basename/PATH. Factory y DLL son fijos por registry. Request:
`{"rekordbox", Mode::History, absoluteDatabasePath}`; ruta vacía admite discovery.
Solo encoding Automatic tiene significado; ANSI/UTF16 se rechazan en start.
Este registro History-only no anuncia Automatic. El registro unificado descrito
arriba ofrece ambos modos; el reader Lua instalado sigue intacto.

Un worker por Request exacto, una solicitud/resultado como máximo, sin timers
ni polling propio. Conexión y statements RAII, loader/unload confinados al worker.
sample solicita o recoge trabajo sin esperar disco; el host central trata Pending.
READONLY | FULLMUTEX, busy timeout 25ms, legacy=4, query_only/trusted_schema;
cache data_version y volumen/índice de archivo, con comprobación tras lectura.
Historial vacío limpia el tema, conforme al Lua; no inventa pause/MASTER.
Metadata inicial se publica; show_existing/expiry siguen pendientes del host OBS.

Discovery reconoce APPDATA/Pioneer y AlphaTheta, options de rekordboxAgent
antes de master.db estándar. JSON máximo 64KiB, profundidad 32 y 16 candidatos;
Unicode/escapes estrictos. Se desduplican identidades Windows. Ambigüedad exige
ruta explícita; relocation ausente o JSON inválido no caen a biblioteca antigua.
La seleccionada se conserva hasta faltar: reiniciar sesión para releer opciones
si cambia la ubicación con una biblioteca anterior todavía accesible.

Busy/read/dependencia/metadata/timeout conservan tema. Missing/format/schema
lo limpian y permiten recuperación posterior. Enums de issues se entregan al host.
4096 bytes/campo; strings con NUL no se truncan. SQLite limita LENGTH a 1MiB.
Progress handler corta SQL al stop o presupuesto nominal 50ms; este límite no
cubre loader, cifrado, filesystem, ni callback discovery personalizado bloqueado.
Stop cancela I/O compatible y hace join seguro antes de liberar recursos; sin
SLA certificado para red/almacenamiento averiado. No hay medición Idle del producto.

CTest final incluye 79 checks del reader real y 52 de dependencia. Verifica
WAL, busy/format/schema, metadata inválida, recovery, discovery, sharing/aislamiento,
paridad SQL con Lua, bytes del archivo principal y 40 ciclos sin crecimiento de
handles en el fixture. Son pruebas de contrato, no profiling de producción.

Probe final: `tests/run-rekordbox-obs-smoke.ps1 -PortableTests <carpeta-obs32-bajo-work>
-BuildRoot <build-bajo-work>`. Requiere runtime y helpers originales previamente
copiados a work. La prueba usa --only-bundled-plugins (con scripting), configura
solo el portable y verifica writer externo vivo/WAL sin checkpoint y 23 fuentes.
No se certifica convivencia con terceros: un ensayo con todos los plugins no
cerró. Resultados fallidos están separados del final Exit=0. El DLL FFI y writer
de fixture continúan fuera del producto/instalador.

Este directorio contiene el núcleo y la API de providers de OBS Playing Next. El
prototipo Lua 0.5.9 continúa intacto y es la referencia funcional. La biblioteca
no está incorporada al instalador ni registra una fuente OBS todavía.

## Contrato

`PlayingCore` recibe observaciones ya elegidas por el lector, valida el límite
de los metadatos y publica un snapshot inmutable. No decide MASTER, faders,
versión DJ, historial ni `show_existing`. Tampoco gestiona presets, animación,
expiración por fuente, selección de perfiles o visibilidad de una escena.

Estados de entrada: Waiting, Playing, Paused y Unavailable. Playing exige un
título no vacío; el artista puede faltar. Paused/Unavailable/Waiting no llevan
metadatos. La pausa limpia el texto sin detener el Core, para que la futura
fuente pueda conservar el artwork del preset. Un error transitorio de un lector
que deba mantener el tema no debe enviarse como Unavailable: el adaptador futuro
debe aplicar la política correspondiente antes de publicar.

Una observación idéntica devuelve Unchanged y conserva puntero/revisión. No hay
timers, polling, threads propios, lectura de disco, red, callbacks por frame ni
dependencias de OBS/Qt/DJ. El propietario muta el estado en su hilo; varios
consumidores pueden leer el mismo snapshot desde otros hilos. La publicación
usa operaciones atómicas sobre shared_ptr; no se promete que sean lock-free.
Retener snapshots conserva memoria hasta que el consumidor los suelta.

Las cadenas se conservan como UTF-8, con máximo 4096 bytes por campo. Se rechazan
UTF-8 inválido, controles C0/C1/DEL y combinaciones de estado/metadatos inválidas.
Los rechazos dejan intacto el último estado. No se recortan strings, ni se aplica
normalización de Unicode, ni se confunden títulos iguales con identidad estable.

Track State incorpora identidad opcional, artwork y origen en fase 06. La obtención
real de datos ausentes depende del contrato verificado de cada provider; protocolo
de red pertenece a fases 07–08. Dos
canciones distintas con igual título/artista no se distinguen aún.

## Compilación y pruebas

Con CMake 3.28+ y MSVC x64:

```powershell
cmake -S playing-next -B work/core-phase03-build -G "Visual Studio 17 2022" -A x64 -DPLAYING_BUILD_OBS_PROBE=ON
cmake --build work/core-phase03-build --config RelWithDebInfo --parallel 2
ctest --test-dir work/core-phase03-build -C RelWithDebInfo --output-on-failure
```

Target de producto: `Playing::Core`, biblioteca estática C++17. Se rechaza la
configuración Windows de 32 bits. No se copiaron motores o código de Twitch.
El motor visual real y la localización NEXUS se integrarán en sus fases.

`playing-core-probe.dll` es exclusivamente una prueba: expone un único entero
C para que `tests/obs-core-probe.lua` ejecute un ciclo del Core dentro de un OBS
portable aislado. Copiar juntos DLL y script a una carpeta de trabajo y registrar
ese Lua solo en la colección temporal. No instalarlo en obs-plugins ni en la
configuración habitual. Esa prueba no equivale a cargar un plugin OBS nativo.

Pruebas de contrato: ciclo start/stop/restart, pausa/reconexión, artista ausente,
cambios de metadatos, 10000 duplicados sin publicación, UTF-8 y límites,
snapshots retenidos tras cambios/destrucción, rechazo de escritores en otro
hilo y cuatro lectores concurrentes con 1000 actualizaciones del propietario.
Los resultados de rendimiento finales se medirán en fases posteriores.

## Providers — fase 04

`Playing::Providers` contiene `IDjProvider` y `ProviderRegistry`, sin dependencia
de OBS/Qt. La API y el registro no crean timers, threads ni discovery. El registro empieza vacío:
solo aparecen descriptores con una factory explícita. Todavía no hay adaptadores
nativos por defecto; los fixtures de tests no se distribuyen como providers del
producto. El primer adapter real, VirtualDJ History, se registra explícitamente
con `registerVirtualDjHistory()` (checkpoint 05-A).

Una factory crea un objeto inerte. `start()` prepara sus recursos y puede fallar;
`sample()` hace trabajo acotado/no bloqueante y devuelve NoChange, Observation,
TemporaryFailure, Unavailable o Pending; `stop()` es noexcept, idempotente y limpia también
un start parcial. El provider no debe reentrar en el registro. Selección de deck,
MASTER, faders, historial y normalización pertenecen al adaptador de fase 05.

El registro comparte una instancia de provider y Core por Request exacto:
ID canónico, Automatic/History, ruta y codificación del historial. Automatic exige ruta vacía
y codificación Automatic;
History admite ruta vacía para discovery automático o una ruta elegida. El host
debe canonicalizar rutas antes de pedir una sesión: aliases, distinta caja y
rutas relativas no se unifican automáticamente. Distintos modos y rutas mantienen
estados independientes. Los únicos modos definidos vienen de los lectores reales;
Serato no dispone de historial en el prototipo auditado.

`acquire()` entrega un token y snapshot inicial; no lee el programa DJ. Leer
snapshots tampoco llama al provider. El host central llama `refresh(request)`
solo cuando recibe un evento o vence un sondeo necesario, compartido y acotado.
No hay refresh por consumidor. Los renders no hacen refresh. `release()` cierra
el provider solo al salir el último consumidor. `clear()` y la destrucción
liberan todos los readers; clear conserva las factories. Los tokens no pueden
reutilizarse después de release/clear ni cruzar registros.

TemporaryFailure o excepción de sample conservan el último estado. Unavailable
lo vacía y publica desconexión; la misma instancia puede recibir reconexión en
otro refresh. Samples contradictorios o metadatos inválidos no alteran el Core.
Los resultados de error son enums, no mensajes de UI ni una nueva capa de idiomas.
Pending conserva el estado mientras el adapter completa I/O en su worker. Los
issues tipados permiten distinguir historial ausente, múltiple, lectura fallida
o sin registros, sin pasar mensajes de UI por el Core.

Todas las llamadas y la destrucción del registro se hacen en el hilo propietario.
Los consumidores pueden compartir después el snapshot const con otros hilos.
Límites iniciales: 16 factories, 32 sesiones, 256 consumidores, ID de 64 bytes,
nombre de 128 bytes y ruta de 4096 bytes. Llegar al límite no inicia otro reader.
Los límites se documentan como contrato inicial, no como certificación de cargas
máximas ni de rendimiento. No existe aún un host OBS que enlace estas sesiones
con fuentes reales: esa integración permanece fuera del producto instalado.

## VirtualDJ History nativo — checkpoint 05-A

Adapter Windows x64 real en `src/providers/virtualdj-history.cpp`, derivado de
`obs-playing/core.lua` y `windows.lua`. Este adapter solo ofrece History;
`registerVirtualDjHistory()` declara
esa disponibilidad exacta; registerVirtualDj ofrece también Automatic descrito
arriba. No registra providers por efecto lateral.

Lectura de solo lectura con share read/write/delete, nunca CREATE. Cola de un
trabajo y un resultado por sesión; un worker espera en condition_variable cuando
no hay trabajo. `sample()` solicita I/O o entrega el resultado listo sin esperar
al disco. No hay polling/timers en el adapter. El host futuro debe gestionar
Pending y programar nuevas lecturas solo cuando sean necesarias. Los tests y el
probe usan una espera finita de completado; ese timer de test no es el scheduler
del producto.

Máximo 64 KiB de cola del archivo, alineación UTF-16, descarte del primer registro
parcial y consumo solo de registros terminados en newline. BOM UTF-8/UTF-16
prevalece sobre la elección manual; Automatic usa UTF-8 y respaldo ANSI CP_ACP,
Ansi lo fuerza y Utf16LE admite archivos sin BOM. Se conserva título/artista con
Unicode, guiones y artista ausente, dentro del límite de Metadata del Core.

Discovery usa las cinco rutas del Windows adapter existente (LOCALAPPDATA,
APPDATA, USERPROFILE/Documents, OneDrive/Documents, OneDriveConsumer/Documents).
No elige arbitrariamente si aparecen dos historiales. Desduplica rutas absolutas
equivalentes ignorando caja; no resuelve identidad de hardlinks/symlinks. Una ruta
descubierta se conserva y, si desaparece, se vuelve a buscar.

Historial sin nuevos registros mantiene el último tema, sin inventar pausa o
MASTER. Ausencia del archivo limpia el Core; si reaparece, vuelve a publicar el
último registro completo, corrigiendo la recuperación del estado previo a la
desconexión. Error de lectura temporal conserva estado. El adapter publica la
última entrada inicial: `show_existing` por consumidor y expiración/visibilidad
pertenecen a la futura integración de fuentes. El Lua instalado conserva esos
ajustes y comportamientos actuales.

Stop despierta al worker, solicita CancelSynchronousIo si hay una operación y
espera su terminación antes de liberar memoria. Todavía no se ha certificado su
latencia bajo fallos de red/almacenamiento o discovery personalizado bloqueado.
No se midieron CPU/RAM/Idle del producto integrado.

Probe OBS: carga de scripts y timers no compartieron hilo en el primer intento.
La prueba crea, consulta y libera el registry desde el mismo timer callback,
manteniendo el contrato de propietario. Para la futura fuente nativa se debe
elegir explícitamente el hilo host y encaminar allí todos los eventos; no asumir
que cualquier callback de OBS corre en el hilo de construcción.
Tras preparar la copia portable, ejecutar
`tests/configure-obs-smoke.ps1 -PortableRoot <ruta-bajo-work>` para desactivar
ConfirmOnExit solo en esa copia. El helper rechaza rutas fuera de work. La
confirmación interactiva predeterminada impedía algunos cierres automáticos;
no confundir ese timeout del harness con una prueba aprobada ni con un fallo
atribuido al Core.


### Recursos PRO y compilador anfitrión

El catálogo PRO integrado puede superar la memoria del compilador anfitrión de
32 bits al compilar qrc_pro_assets.cpp (C1060). En el build MSVC de desarrollo,
usar herramientas anfitrionas x64 y una compilación a la vez:

    cmake --build work/core-phase08h-build --config RelWithDebInfo --target obs-playing-next playing-track-visual-tests playing-source-host-tests -- /p:PreferredToolArchitecture=x64 /m:1

Esto conserva el objetivo Windows x64 y no cambia recursos ni temporizadores del
producto. Las pruebas instrumentadas siguen requiriendo su opción de CMake.

### Fase 17 cerrada en desarrollo — 12 de octubre de 2026

El usuario acepta visualmente los 14 PRO (9 horizontales + 5 verticales).
Incluye dos diseños adicionales de solo texto y conserva los 23 NORMAL/aliases.
Regresión final44/44PASS; banco OBS32.2.2 con92comprobaciones/95capturas/Exit0;
68controles gráficosPASS. DLL final sin hooks. Informe autoritativo:
[outputs/OBS-Playing-Next-fase-17-cierre.md](../outputs/OBS-Playing-Next-fase-17-cierre.md).

Siguiente fase18: Preset Manager. Mantener IDs y ajustes existentes, reutilizar
catálogo/renderer y gestionar My Presets, Imported y Favorites. La matriz OBS
completa, pruebas físicas pendientes, profiling y distribución mantienen su
alcance posterior; este cierre no los certifica.

### Checkpoint 18-A — gestor funcional, cierre de fase pendiente

Fase19 iniciada: auditoría de personalización y habilitación de tipografías/colores
explícitos en BASIC/manual. Fase abierta, Clásicos y efectos de texto pendientes.
[Informe19-A](../outputs/OBS-Playing-Next-fase-19a-personalizacion.md).

19-B1: preparación de overrides opcionales para texto de los 23 Clásicos;
2244 comprobaciones CPU PASS. La conexión al host/caché/UI sigue pendiente.
[Informe19-B1](../outputs/OBS-Playing-Next-fase-19b1-clasicos.md).

Estado actual: fase19 COMPLETADA en desarrollo. Clásicos/PRO/manual/solo texto
con tipografía/color y contorno/sombra opcionales, persistencia y Reset.
Regresión45/45, visual247 checks, matriz GPU cuatroOBS32.x con reset exacto.
HooksOFF. PLAYING-18-001 sigue pendiente, sin certificación de estabilidad.
[Cierre19](../outputs/OBS-Playing-Next-fase-19-cierre.md).
Siguiente fase20: editor Basic/Advanced/Layout.

Fase20 COMPLETADA en desarrollo: editor integrado en propiedades con vistas
Basic/Advanced/Layout y Guardar como directo. Layout muestra solo la geometría
compatible con el estilo, manteniendo el marco aprobado. Matriz de vistas y
lifecycle cuatroOBS32.x PASS, Save As real30checks, hooksOFF.
[Cierre20](../outputs/OBS-Playing-Next-fase-20-cierre.md).
Siguiente fase21: import/export .obsplayingpreset.

Fase21 COMPLETADA en desarrollo: Importar/Exportar seleccionado en el gestor,
formato JSON .obsplayingpreset v1 acotado y referencias a estilos instalados.
Sin IDs privados, conexión ni rutas de imágenes; importados con ID local,
deduplicación y aplicación explícita. Regresión45/45; gestor163 checks final;
intercambio en cuatroOBS32.x,23originales y Exit0. HooksOFF.
[Cierre21](../outputs/OBS-Playing-Next-fase-21-cierre.md) y
[formato v1](../outputs/OBS-Playing-preset-format-v1.md).
Siguiente fase22: Eco/Balanced/Maximum; profiling sigue pendiente.

Fase22 COMPLETADA en desarrollo: Eco250/33ms, Balanced100/16ms,
Maximum50/16ms (lectura local/publicación de animación), defaultBalanced.
Demanda compartida solo activa; cambio conserva canción/frame/reloj, sin raster
adicional. Perfil por consumidor, excluido de presets. Reconstrucción completa,
45/45 suites y36checks en cada OBS32.0.0/32.0.4/32.1.2/32.2.2, capturaGPU,
23originales yExit0. HooksOFF y lifecycle final32.2.2. No son FPS/ahorros medidos.
[Informe22](../outputs/OBS-Playing-Next-fase-22-cierre.md).
Fase23 COMPLETADA en desarrollo: el tick de animación salta fuentes estáticas;
metadata/configuración/expiry conservan su ruta completa. Movimiento, barrido,
giro/temblor y luz se reúnen en una copia dinámica diferida por entrada/pase.
Fórmulas y renderer intactos. Host133checks;45/45CTest; cuatroOBS32.x con14checks,
seis capturas y cuatro gatesRGBA por runtime;23originales/Exit0.
PRO32.2.2:92checks/95capturas/68gates. NORMAL32.2.2:441checks/236capturas,
23/3aliases,26gates movimiento+26transiciones. HooksOFF/lifecycle final32.2.2.
[NEXUS](NEXUS-OPTIMIZATION.md), [cierre23](../outputs/OBS-Playing-Next-fase-23-cierre.md).
Siguiente fase24 Profiling & Certification. No porcentajes CPU/GPU ni IdleZero
certificados; PLAYING-18-001 y validación física Companion siguen pendientes.
Estado actual: fase18 completada en desarrollo con PLAYING-18-001 abierta
para compatibilidad/hardening antes de RC. Los estados de abajo son históricos.
No se declara corregido el crash ni certificada la estabilidad.
[Cierre18](../outputs/OBS-Playing-Next-fase-18-cierre.md).

Fuente nativa: botón Gestor de presets / favoritos, cinco categorías, búsqueda,
favoritos, Guardar como, actualizar, renombrar, eliminar y restablecer base.
Biblioteca local acotada/versionada con QSaveFile/QLockFile. Solo overrides
visuales explícitos; conserva provider, vinculación y rutas de imágenes.
Originales protegidos, aliases conservados, ningún renderer nuevo.
Importados admite clasificación interna; import/export de archivos sigue en 21.

Regresión45/45PASS; gestor50checks y pruebas dentro de los cuatro OBS32.x.
Un cierre anormal de 32.1.2 queda abierto pese a dos repeticiones normales y
controles PASS; no cerrar fase18 ni atribuir la causa sin pruebas. Informe:
[fase18-A](../outputs/OBS-Playing-Next-fase-18a-gestor.md).
Banco real: `run-source-lifecycle.ps1 -PresetManager`; requiere hooks ON.
Revisión editable: `open-user-preview.ps1 -PresetManager`; usar hooks OFF.

### 18-B — escena editable, portada y eliminación segura

El gestor activa portada inicialmente en los 14 PRO con marco; NORMAL/texto
manual/solo texto sin portada. Guardar como conserva una opción explícita false.
Rechaza fuentes marcadas eliminadas aunque OBS conserve referencias fuertes;
usa RAII y no lee/aplica durante el cierre.
Regresión45/45PASS, gestor91checks, cuatroOBS32.x en escena/23originales/Exit0.
Cierre con fuente activa+gestor abierto en32.1.2/32.2.2 PASS. Controles sin Playing
Next:12 nativos+12Lua PASS. El crash aislado de18-A no se reprodujo, pero su causa
sigue desconocida: fase18 continúa abierta; no se declara corregido.
[Informe18-B](../outputs/OBS-Playing-Next-fase-18b-gestor-y-cierre.md).

Checkpoint 18-C: captura propia de excepciones/minidumps para investigar el cierre
pendiente. Prueba controlada y dos cierres OBS 32.1.2 verificados; fase abierta.
[Informe18-C](../outputs/OBS-Playing-Next-fase-18c-diagnostico.md).
Banco: `-PresetManager -PresetManagerScene -PresetManagerKeepOpen`; añadir
`-PresetManagerKeepSource` para cerrar con fuente activa. Solo hooks ON.
