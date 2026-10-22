# OBS Playing NEXUS 1.0.0

22 de octubre de 2026 · Windows x64 · OBS Studio 32.x

## Español

Versión estable cerrada por petición del usuario a partir de RC3.9, con un
Companion más compacto. Un mismo instalador incluye el plugin OBS y Companion;
Companion solo es necesario en el PC DJ cuando se utilizan dos ordenadores.

- Lectores VirtualDJ, rekordbox, Serato y Traktor Pro 3/4, con los límites de
  versiones y estructuras documentados en [Compatibilidad](COMPATIBILITY.md)
  y [Traktor](TRAKTOR.md).
- Título, artista y portada; perfil del streamer como alternativa configurable.
- Catálogo NORMAL y PRO, animaciones propias de cada estilo, transiciones,
  editor único, tipografías, colores y presets personales.
- Vinculación guiada con Companion, lectura y envío automáticos, perfiles
  persistentes y reconexión. Logo original de Traktor en su fila.
- El panel principal muestra solo los PCs conectados actualmente. Los equipos
  vinculados se administran en configuración y permanecen guardados para futuras
  conexiones; una desconexión retira el equipo del panel sin borrar su vinculación.
- Interfaz en español, inglés, italiano, francés y alemán. Guías ilustradas
  offline en español e inglés en `docs/companion/`.
- Anuncios de canciones opcionales en Twitch, desactivados inicialmente, con
  autorización separada del chat y opción de desactivación.

### Instalación y actualización

Ejecuta `OBS-Playing-NEXUS-Setup-1.0.0.exe` y elige el componente que necesitas.
Cierra OBS para instalar el plugin y Companion para actualizar la aplicación.
El instalador solicita permisos de Windows cuando la ruta del plugin lo exige.
Las escenas, presets, dispositivos y ajustes existentes conservan sus identidades.
Las versiones anteriores del proyecto se mantienen.

El EXE contiene los componentes de ejecución. Las fuentes correspondientes,
licencias y avisos se entregan aparte en `OBS-Playing-NEXUS-1.0.0-Sources.zip`.

### Alcance de validación

La base RC3.9 tiene regresión local, instalación/desinstalación y pruebas OBS
aisladas. Traktor tiene pruebas reales en 3.5.0 build 270 y 4.4.1 build 22; no se
afirma haber probado todas las versiones 3.x/4.x. El usuario confirma completadas
la instalación en otro PC y la comprobación de compatibilidad OBS.

El cierre estable no convierte las pruebas pendientes en resultados positivos:
benchmark nuevo de esta versión, envío al chat real de Twitch y un nuevo ensayo
físico Traktor entre dos PCs quedan como seguimiento posterior. No se han
publicado estos artefactos ni actualizado el OBS habitual durante este cierre.

## English

Stable release based on RC3.9, with a more compact Companion dashboard. One
installer includes the OBS plugin and DJ-PC Companion. Existing source,
installation, profile and preset identities are retained when upgrading.

Includes VirtualDJ, rekordbox, Serato and Traktor Pro 3/4 readers, artwork and
optional streamer-image fallback, NORMAL/PRO styles, animations, transitions,
personal presets, guided pairing and optional Twitch song announcements.
The dashboard lists current connections only; saved paired devices remain
available in settings for reconnecting or revoking access.
The UI supports Spanish, English, Italian, French and German; offline illustrated
guides are supplied in Spanish and English. Companion uses the original Traktor
application logo to identify its reader.

Close OBS or Companion before updating the respective component. Run
`OBS-Playing-NEXUS-Setup-1.0.0.exe`; corresponding sources and notices are provided
separately in `OBS-Playing-NEXUS-1.0.0-Sources.zip`.

Live Traktor checks cover 3.5.0 build 270 and 4.4.1 build 22, rather than every
3.x/4.x release. The user confirmed installation on another PC and OBS
compatibility checks. A new benchmark, real Twitch chat delivery and a new
physical two-PC Traktor test remain follow-up validation; stable status does
not imply these checks were performed. No remote publication is part of this
local release closure.
