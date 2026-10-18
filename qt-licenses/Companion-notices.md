# Dependencias del Companion opt-in

Fase10-D usa el backend Qt Schannel 6.8.3 del mismo SDK auditado, dentro de
playing-qt-plugins/tls junto al módulo de desarrollo. Su SHA256 se comprueba
antes de añadir ese directorio propio a las rutas Qt. Las pruebas no sustituyen
las DLL Qt de OBS. Este backend conserva los avisos/licencias Qt del paquete
08-J; la distribución definitiva debe incluirlos y validar compatibilidad con
el Qt de cada runtime soportado. La matriz concreta de OBS32.x no certifica
futuros runtimes ni ABI privada general entre versiones de Qt.

Fase10-C enlaza el motor y renderer GPL auditados en obs-playing-next.dll de
desarrollo. El código propio mantiene MIT; el componente combinado conserva
los requisitos GPL-2.0-or-later del motor, además de las licencias de Qt/libobs.
No se modifica el paquete Companion08-K ni se genera instalador/distribución.

Fase09 añade una integración visual opt-in separada del Companion distribuido.
Los fuentes originales de src/visual y src/adapters/obs-visual-renderer, sus
shaders y tests declaran GPL-2.0-or-later en el proyecto OBS Twitch Overlays.
Se compilan directamente sin copiar la aplicación; hashes fijados en
tests/visual-audit/sources.lock.json y renderer.lock.json. Texto GPLv2 original
conservado en licenses/shared-visual-GPL-2.0.txt. El SPDX MIT de adaptadores y
metadatos CMake propios no cambia la licencia del componente externo.
QtGui y libobs se enlazan desde los SDK auditados; la licencia/copyright de
estos componentes también permanece independiente. El SDK libobs usado es31.1.1;
se probó la misma DLL de desarrollo en cuatro runtimes OBS32.x/Windows x64.
No se añadió el motor, libobs o el probe al ZIP del Companion08-K ni al
instalador existente. Una distribución combinada futura deberá incluir las
fuentes exactas/avisos/licencias de estos componentes; véase el contrato en
include/playing/visual/README.md y el informe de fase09. No relabel del motor a MIT.

08-A compila/enlaza dinámicamente Qt Core y Qt Network 6.8.3, desde el paquete
de dependencias existente de OBS Twitch Overlays. No copia fuentes de Twitch ni
redistribuye Qt en este checkpoint. Schannel y BCrypt son APIs del sistema Windows.

08-C añade Qt Gui/Widgets del mismo paquete. Headers qimage.h/qwidget.h:
LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only.
CNG/NCrypt y DPAPI son APIs Windows. Iconos del Companion se dibujan en código
propio como aproximación visual; no son assets oficiales DJ descargados.
djemimas-logo.png solo se lee en el test desde el original conservado, como
fallback de perfil explícito. No se copia como recurso instalable nuevo.

Los headers Qt inspeccionados declaran licencias alternativas comerciales,
LGPL-3.0-only y GPL (según componente). La licencia de las dependencias no se
reemplaza por el SPDX MIT del código propio. Referencias oficiales:
[Qt licensing](https://doc.qt.io/qt-6/licensing.html) y
[Qt Network QSslSocket](https://doc.qt.io/qt-6/qsslsocket.html).

El paquete local contiene licenses/qt6/LICENSE.QT-LICENSE-AGREEMENT y LICENSE.GPL2;
estos archivos por sí solos no se presentan como conjunto completo para distribuir
el Companion. Empaquetado final deberá incorporar avisos/licencias y procedencia
de los módulos Qt/backend efectivamente incluidos, además de los avisos del proyecto
en ../THIRD-PARTY-NOTICES.txt. No se ha preparado distribución en este checkpoint.

08-J prepara un paquete portable de pruebas, con enlace dinámico de Qt6Core,
Qt6Network, Qt6Gui y Qt6Widgets; plugins qwindows, qschannelbackend, qjpeg y qwebp.
Fuentes completas qtbase/qtimageformats 6.8.3 incluidas junto con textos LICENSES,
licencias/copyright de terceros, atribuciones y SBOM del SDK. Archives originales
verificados contra los hashes de obs-deps tag 2025-07-11; receta y parches incluidos.
Se permite reemplazar las DLLs Qt por builds compatibles y depurar modificaciones.
Los módulos incluidos se usan bajo LGPLv3; las licencias de sus componentes
internos se conservan en source/ y licenses/. Los textos comerciales históricos
no conceden ni presuponen una licencia comercial del usuario.

Runtime MSVC x64 14.44.35112: cinco DLLs release sin modificar desde
VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT, código propietario Microsoft.
UCRT y Schannel los proporciona Windows, no se copian DLLs de System32.
Referencia: [lista de redistribución de Visual Studio 2022](https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution).
La preparación de este paquete no certifica el instalador final ni sustituye
la revisión de licencias de cualquier módulo que se incorpore posteriormente.
