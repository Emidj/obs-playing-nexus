# Mapa de licencias de RC3.4

No se ha cambiado la licencia de los archivos originales.

| Componente | Evidencia local | Tratamiento |
|---|---|---|
| Código propio Playing/Companion/instalador | LICENSE, cabeceras SPDX | MIT donde lo declara el titular; conservar avisos |
| Motor y renderer compartidos | shared/LICENSE, cabeceras GPL-2.0-or-later, locks de visual-audit | GPL-2.0-or-later; incluir fuentes exactas y recetas del componente combinado |
| libobs y frontend API | enlaces en playing-next/CMakeLists.txt | OBS usa GPL; no anunciar la DLL combinada como MIT |
| Qt 6.8.3 Core/Network/Gui/Widgets y plugins | qt-licenses/, sbom/, archivos fuente Qt adyacentes | LGPLv3 según el empaquetado existente, con licencias adicionales de terceros; permitir sustitución de bibliotecas y depuración |
| SQLite3 Multiple Ciphers 2.5.1 | avisos originales del producto y hash fijo de la DLL | MIT y licencias de los componentes internos; verificar correspondencia final |
| Runtime MSVC | REBUILD.txt y documentación del paquete | Redistribuible propietario Microsoft; no es código abierto |
| Windows Schannel/DPAPI/BCrypt | llamadas al sistema | Componentes del sistema; no copiar DLL de Windows |
| Referencias Serato/rekordbox | avisos originales | Conservar atribuciones; confirmar procedencia de cualquier código adaptado |
| Marca, logos y guía | recursos del paquete | Confirmar titularidad y permiso de redistribución del autor |

**Distribución del plugin combinado:** se utiliza la opción GPL versión 3 de los componentes GPL-2.0-or-later, compatible con la utilización de Qt bajo LGPLv3. El texto GPLv3 se incluye en [COPYING](COPYING). Las cabeceras MIT del código propio y los avisos originales de cada tercero se conservan. Esta elección para el conjunto no convierte el código GPL en MIT, ni convierte el runtime propietario de Microsoft en código abierto.

El Companion y el instalador conservan los términos MIT de su código propio y los términos de sus dependencias. Qt se enlaza dinámicamente; se permite reemplazar las bibliotecas por versiones modificadas compatibles y realizar ingeniería inversa para depurar esas modificaciones. Las fuentes completas Qt 6.8.3, las recetas, parches y SBOM se entregan en el ZIP Sources de la misma Release. Las referencias originales a GPL-2.0-or-later siguen permitiendo elegir GPLv3 para el conjunto.

Los gráficos NEXUS incluidos son los recursos elegidos por el autor en el proyecto. Las marcas OBS, VirtualDJ, rekordbox y Serato pertenecen a sus titulares; no se afirma afiliación ni patrocinio. Los permisos sobre las marcas no se conceden por esta licencia de software.

El ZIP Sources original conserva archivos Qt y textos de terceros. La existencia de estos archivos no equivale a una auditoría jurídica completa o una reconstrucción reproducible verificada.

Fuentes oficiales consultadas el 18-10-2026:

- https://github.com/obsproject/obs-studio/blob/master/COPYING
- https://www.qt.io/development/open-source-lgpl-obligations
- https://github.com/utelle/SQLite3MultipleCiphers/blob/main/LICENSE
- https://github.com/chrisle/serato-connect
