# OBS Playing NEXUS 1.1.0

28 de octubre de 2026 · Windows x64 · OBS Studio 31.x, 32.x y 33.x

## Español

Un mismo instalador detecta la versión del ejecutable de OBS en la carpeta
seleccionada y utiliza la estructura correspondiente:

- OBS 31/32: obs-plugins/64bit y data/obs-plugins/obs-playing-next.
- OBS 33 normal: ProgramData/obs-studio/plugins/obs-playing-next, con DLL y
  dependencias en la raíz y recursos en data.
- OBS 33 portable (portable_mode.txt o configuración local existente): plugins/obs-playing-next dentro de su
  instalación. No escribe en el ProgramData compartido.

La interfaz muestra la versión y el destino detectados. Si OBS está en otra
carpeta, se selecciona con Buscar. Se mantienen permisos automáticos de Windows,
validación AMD64 y bloqueo mientras OBS está abierto. Versiones anteriores a 31
o posteriores a 33 se rechazan hasta validarlas.

La migración a OBS 33 guarda copias verificadas antes de retirar la DLL anterior.
La entrada legacy de desinstalación se sustituye por la moderna; los metadatos
y el desinstalador anterior se conservan para permitir volver a OBS 31/32.
Las versiones previas siguen en las copias de seguridad. Las dependencias legacy
compartidas no se eliminan por su nombre. Archivos registrados modificados por el
usuario bloquean la migración. Un fallo revierte los archivos afectados.
La desinstalación moderna conserva las copias anteriores, pero no vuelve a activar
una DLL legacy. No modifica escenas, presets personales, perfiles ni equipos.

Incluye el panel de ajustes en ocho grupos y la consulta pública para News Ticker.
El Companion se conserva en su binario estable 1.0.0; solo es necesario en el PC DJ.

La DLL utiliza Qt 6.6 como base de compilación para admitir OBS 31.0. El instalador
selecciona el backend Schannel 6.6 o 6.8 según el Qt de OBS, sin reemplazar sus DLL.
Los fuentes correspondientes se entregan separados del instalador.

### Límites

La detección de la versión y la ubicación no garantiza cada configuración posible.
Las pruebas usan runtimes seleccionados de las ramas 31/32/33, fixtures de datos y
TLS local. No sustituyen una emisión prolongada, todos los drivers, cada build de
OBS o todas las versiones de los programas DJ. El alcance detallado y los hashes
se entregan en outputs/OBS-Playing-NEXUS-1.1.0.md y su manifiesto.

En OBS 31.0.0 se ha comprobado la carga de la DLL con sus bibliotecas originales
y la disponibilidad de sus símbolos, además del transporte TLS con Qt 6.6.3.
La aplicación completa OBS 31.0.0 no finalizó el arranque en este equipo, antes
de cargar plugins; no se considera una prueba de render superada. La carga
automática y el dibujo completos se verificaron en 31.1.2, 32.2.2 y 33.0.0.

## English

One Windows x64 installer detects the selected OBS executable and routes the plugin
according to its version. OBS 31/32 keep their legacy layout. OBS 33 uses the new
ProgramData plugin layout, or the installation-local plugins folder in portable
mode. Portable installations do not write into the shared ProgramData directory.

Migration retains verified backups and refuses to overwrite modified registered
files. Failed installations restore affected files. Modern uninstall preserves
backups without reactivating an older legacy DLL. Scenes, profiles, presets and
paired devices are preserved. The grouped settings panel and News Ticker bridge
are included; the stable Companion binary remains unchanged.
