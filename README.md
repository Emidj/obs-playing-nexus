# OBS Playing NEXUS 1.0 RC3.5

Plugin nativo para OBS Studio y Companion para Windows x64 que muestran información de reproducción DJ. El Companion permite enviar texto y portadas al PC de streaming mediante TLS. **RC3.5 es la versión oficial indicada por el autor; sigue siendo una Release Candidate.**

Incluye 23 diseños originales. La compatibilidad depende de la versión y del estado de VirtualDJ, rekordbox o Serato; no implica medición del audio que sale de una mesa externa.

## Novedades de RC3.5

- Plugin OBS, Companion e instalador disponibles en español, inglés, italiano, francés y alemán.
- 396 mensajes traducidos; variantes regionales usan el idioma base y otros idiomas usan inglés.
- El plugin sigue el idioma de OBS; Companion y el instalador siguen el idioma de interfaz de Windows.
- Se retiran las 14 etiquetas «en desarrollo» de estilos, conservando los IDs y ajustes existentes.
- Nombres comerciales, canciones y presets del usuario se conservan. Las guías ilustradas siguen disponibles en español e inglés.

No cambian los lectores DJ, la extracción de portadas Serato ni el protocolo LAN.

## Preview

![OBS Playing NEXUS promotional preview](docs/images/obs-playing-nexus-preview.png)

*Promotional illustration of a now-playing overlay in OBS.*

<img src="docs/images/companion-demo.png" alt="OBS Playing Companion with demonstration track and local connection" width="440">

*Companion screenshot with demonstration data and a local loopback connection.*

## Instalación

Descarga el [instalador oficial RC3.5](https://github.com/Emidj/obs-playing-nexus/releases/tag/v1.0.0-rc3.5). Cierra OBS y el Companion antes de actualizar. Selecciona Plugin OBS en el PC de streaming y Companion en el PC DJ. El plugin puede solicitar permisos de administrador para Program Files. Para portadas entre PCs, actualiza ambos componentes a RC3.5.

Lee la [guía ilustrada en español](playing-next/docs/companion/README.md) o la [English guide](playing-next/docs/companion/README.en.md). El instalador incluye desinstalación y conserva backups de versiones reemplazadas.

El instalador no tiene firma digital. Compara SHA256 con el manifiesto de la Release.

## Código y compilación

Consulta [REBUILD.txt](REBUILD.txt), [documentación técnica](playing-next/README.md) y [estado de preparación](PUBLICATION-AUDIT.md). Se requieren Windows x64, Visual Studio 2022 C++, CMake 3.28 o superior y SDK Qt/OBS compatibles. Las recetas existentes contienen rutas del entorno original; todavía no está verificada una compilación desde Windows limpio.

Las fuentes correspondientes de RC3.5 incluyen el motor compartido en `shared/`, fuentes Qt, recetas, parches y SBOM. Los archivos Qt originales se distribuyen junto al instalador en el ZIP Sources; no deben omitirse de la Release por estar excluidos del historial Git.

## Licencias

Este es un proyecto con varias licencias. [LICENSE](LICENSE) conserva la licencia MIT del código propio; **no licencia todo el plugin combinado como MIT**. El plugin combinado se distribuye bajo GPLv3, eligiendo la opción de versión posterior del motor GPL-2.0-or-later. Qt y otros terceros conservan sus licencias. Lee [LICENSES.md](LICENSES.md) y [COPYING](COPYING) antes de redistribuir binarios.

## Estado de validación

El informe de desarrollo RC3.5 registra cinco suites UI, 85 checks multilingües adicionales al 200 %, 103 checks de propiedades en OBS portable 32.2.2, 101 checks de idioma/render del instalador, 67 del motor de instalación y 60 del paquete Companion. Son resultados de desarrollo, no nuevos ensayos de publicación. Se verifican los hashes de esta distribución y se revisan fuentes y licencias. Los 23 diseños originales y los ajustes se conservan. Siguen pendientes dos PCs físicos, Windows limpio y el caso de portadas Serato; no se certifican todas las ventanas de terceros.

a plugin from [DjEmiMas](https://discord.gg/dhcqZPDYh)
