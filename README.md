# OBS Playing NEXUS 1.0 RC3.7

Plugin nativo para OBS Studio y Companion para Windows x64 que muestran información de reproducción DJ. El Companion permite enviar texto y portadas al PC de streaming mediante TLS. **RC3.7 es la versión oficial indicada por el autor; sigue siendo una Release Candidate.**

Incluye 23 diseños originales. La compatibilidad depende de la versión y del estado de VirtualDJ, rekordbox o Serato; no implica medición del audio que sale de una mesa externa.

## Novedades de RC3.7

- Anuncios opcionales de título y artista en el chat de Twitch, desactivados por defecto.
- Plantilla personalizable y espera de estabilización de 1–30 segundos; mínimo 10 segundos entre intentos.
- Un coordinador evita duplicados entre fuentes. Solo anuncia canciones reales con título/artista, fuente activa y emisión Twitch activa; nunca el tema ficticio.
- Autorización de chat independiente con el permiso `user:write:chat`, completada por el usuario en Twitch. Grant local protegido por DPAPI; no se publica ni se guarda en escenas o presets.
- Panel único y cinco idiomas conservados; guías español/inglés actualizadas.

En Shared Chat, Twitch puede propagar mensajes a canales participantes. Los envíos sin confirmación no se reintentan automáticamente. Companion no envía mensajes ni necesita configuración adicional.

## Preview

![OBS Playing NEXUS promotional preview](docs/images/obs-playing-nexus-preview.png)

*Promotional illustration of a now-playing overlay in OBS.*

<img src="docs/images/companion-demo.png" alt="OBS Playing Companion with demonstration track and local connection" width="440">

*Companion screenshot with demonstration data and a local loopback connection.*

## Instalación

Descarga el [instalador oficial RC3.7](https://github.com/Emidj/obs-playing-nexus/releases/tag/v1.0.0-rc3.7). Cierra OBS y el Companion antes de actualizar. Selecciona Plugin OBS en el PC de streaming y Companion en el PC DJ. El plugin puede solicitar permisos de administrador para Program Files. Para portadas entre PCs, actualiza ambos componentes a RC3.7.

Lee la [guía ilustrada en español](playing-next/docs/companion/README.md) o la [English guide](playing-next/docs/companion/README.en.md). El instalador incluye desinstalación y conserva backups de versiones reemplazadas.

El instalador no tiene firma digital. Compara SHA256 con el manifiesto de la Release.

## Código y compilación

Consulta [REBUILD.txt](REBUILD.txt), [documentación técnica](playing-next/README.md) y [estado de preparación](PUBLICATION-AUDIT.md). Se requieren Windows x64, Visual Studio 2022 C++, CMake 3.28 o superior y SDK Qt/OBS compatibles. Las recetas existentes contienen rutas del entorno original; todavía no está verificada una compilación desde Windows limpio.

Las fuentes correspondientes de RC3.7 incluyen el motor compartido en `shared/`, fuentes Qt, recetas, parches y SBOM. Los archivos Qt originales se distribuyen junto al instalador en el ZIP Sources; no deben omitirse de la Release por estar excluidos del historial Git.

## Licencias

Este es un proyecto con varias licencias. [LICENSE](LICENSE) conserva la licencia MIT del código propio; **no licencia todo el plugin combinado como MIT**. El plugin combinado se distribuye bajo GPLv3, eligiendo la opción de versión posterior del motor GPL-2.0-or-later. Qt y otros terceros conservan sus licencias. Lee [LICENSES.md](LICENSES.md) y [COPYING](COPYING) antes de redistribuir binarios.

## Estado de validación

El informe de desarrollo RC3.7 registra 48 suites de regresión superadas tras renovar un fixture TLS caducado, 61 checks de chat local, 138 del host, 899 de propiedades y 108 de idiomas en OBS portable 32.2.2, 60 del paquete Companion y 67 del instalador. Son resultados de desarrollo, no nuevos ensayos de publicación. Se verifican los hashes, el catálogo de 420 mensajes y las fuentes/licencias. No se han enviado mensajes reales a Twitch: quedan pendientes autorización humana y prueba durante una emisión, matriz OBS completa, Windows limpio y dos PCs físicos. No se declara resuelto el caso de portada Serato.

a plugin from [DjEmiMas](https://discord.gg/dhcqZPDYh)
