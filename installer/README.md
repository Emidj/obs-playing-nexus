# Instalador OBS Playing NEXUS 1.0.0

Un EXE con opciones Plugin OBS / Companion. Recursos runtime embebidos, fuentes separadas.
Incluye la guía ilustrada offline en `docs/companion/README.html` del Companion y
en `distribution/docs/companion/README.html` de los datos del plugin. GUIA.txt
indica la ubicación correspondiente. Portadas remotas: actualizar ambos componentes.
Companion por usuario; OBS portable escribible o permisos de administrador para Program Files.
Al faltar permisos para el plugin, el instalador solicita elevación automáticamente
y reintenta una sola vez. Windows muestra su confirmación UAC; cancelarla no instala.
Companion conserva instalación por usuario. El desinstalador del plugin también
puede solicitar elevación. Los fixtures /no-registry no abren solicitudes UAC.
No modifica escenas ni instala el Lua antiguo. Copias de versiones reemplazadas se conservan.
El arranque habilita rutas largas en .NET Framework 4.6.2 o posterior para que
las copias de actualización no fallen al superar el límite histórico de 260 caracteres.

Reproducción desde el workspace original:

```powershell
& playing-next/packaging/build-native-rc.ps1 -OutputRoot work/native-new
& playing-next/packaging/build-portable.ps1 -OutputRoot work/companion-new
& playing-next/installer/build.ps1 -OutputRoot work/setup-new -NativeRoot work/native-new -CompanionRoot work/companion-new/OBS-Playing-Companion-test-x64
```

Requiere una RC Companion recién compilada y verificada, .NET Framework csc,
SDK Qt/OBS del proyecto y fuentes/receta Qt auditadas. build.ps1 comprueba manifiestos RC antes
seleccionar archivos de ejecución. No se ha validado reconstrucción en Windows limpio.

CLI: `/install /role plugin /target "C:\OBS portable"`, `/install /role companion /target "C:\Companion"`.
Desinstalación: `/uninstall /role plugin|companion /target "ruta"`.
`/no-registry` omite registro per-user en fixtures; `/log "archivo"` evita diálogo de error CLI.
El uninstaller instalado se copia a temporal para liberar su propio ejecutable; puede quedar esa copia.

Tests: test-installer.ps1 con Windows PowerShell/.NET Framework, test-installed-obs.ps1 y
 test-installed-companion.ps1 con PowerShell 7. Solo aceptan directorios nuevos bajo work.
No ejecutar los tests contra el OBS habitual.

Distribuir instalador y paquete de fuentes correspondiente con los avisos incluidos.
El EXE es RC3 sin firma digital. La imagen interior y el icono NEXUS proceden
de los PNG aprobados por el usuario; no se incluyen fuentes Qt dentro del EXE.
El ZIP Sources adyacente contiene las fuentes y licencias correspondientes.
Ver outputs/OBS-Playing-Next-RC3.md para los resultados de esta versión.
