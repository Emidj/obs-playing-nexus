# Ajustes agrupados y estudio OBS 31/33

28 de octubre de 2026 · Windows x64 · Direct3D 11 / SDR

## Ajustes

Un único editor con ocho bloques: conexión/programa DJ; diseño/tamaño;
portada/perfil; texto/posición; color/iluminación; animación/transiciones;
chat de Twitch; pruebas. Los grupos sin controles aplicables se ocultan.
Los botones de prueba quedan al final y el chat mantiene su activación opcional.
Se conservan los nombres y valores guardados, presets, aliases y geometrías.
Los encabezados están traducidos a español, inglés, italiano, francés y alemán.
Los callbacks resuelven la raíz de propiedades cuando vienen de un grupo.

## HECHOS: OBS 31.1.2 y 33.0.0

Se descargaron los ZIP Windows x64 oficiales de GitHub y se verificó SHA256:

- 31.1.2: 9513cd402936593a6a5500da5d2bd49e6cd04dce05509c7b7f48dd25c391d2d8
- 33.0.0: 3369a3ecd67b5669a8a08d77357f87cc786d1fba42fbbd1054d841a69f6271a7

En ambas versiones se ha cargado realmente la DLL y probado el panel, la
conservación de ajustes y los selectores de 16 estilos PRO. Las 23 fuentes Lua
originales coexistieron y OBS cerró normalmente. Esto verifica los selectores;
no representa capturas comparativas completas de los 16 diseños.

Una DLL instrumentada, fuera de distribución, pasó 68 comprobaciones por versión:
render a textura no vacío, Unicode, cambio de canción, caché compartida entre dos
fuentes, cambios de geometría/overflow sin reiniciar la sesión, inactividad,
20 fuentes transitorias, receptor/emisor TLS loopback, pausa, reanudación,
revocación y liberación. SHA instrumentado:
95F20B372A73CDF9DA13FA502509C316D84CC1419A4A2DD5F0FF29A9E03FCB8D.
No se usaron dos PCs físicos ni una aplicación DJ real: el historial y TLS son
fixtures controlados. No se realizó una emisión ni una prueba prolongada.
Al terminar se restauró PLAYING_SOURCE_TEST_HOOKS=OFF.

## HECHOS: diferencias de plataforma

OBS 33 cambia la carga de plugins y sus ubicaciones. Su documentación mantiene
las ubicaciones antiguas hasta OBS 34. También elimina soporte de compilación
para Qt 6.4–6.9 y cambia gs_image_file por una actualización de libnsgif.
Fuentes oficiales consultadas el 28 de octubre de 2026:

- https://github.com/obsproject/obs-studio/releases/tag/33.0.0
- https://obsproject.com/kb/legacy-plugin-locations
- https://github.com/obsproject/obs-studio/releases/tag/31.1.2

Playing utiliza Qt y su backend Schannel. Las pruebas de OBS 33 han ejercitado la
carga del mismo binario y una conexión TLS local. No se reemplazaron las DLL de
Qt de OBS. La búsqueda en Playing y en el renderer compartido no encontró uso de
gs_image_file; las imágenes se preparan con Qt y se suben como texturas propias.

El instalador oficial 1.0.1 continúa limitado a OBS 32.x. Este estudio no cambia
esa condición ni modifica el OBS instalado. Companion conserva su independencia
respecto a la versión de OBS del PC receptor.

## INFERENCIA y límites

La misma arquitectura parece viable en 31.1.2 y 33.0.0 sin reconstruir los lectores
DJ o el renderer. Las pruebas no autorizan afirmar que toda versión 31.x o futura
33.x sea compatible. OBS 31.0.x no se ha ejecutado en esta matriz.

Antes de ampliar el instalador oficial: completar capturas de todo el catálogo,
portadas/canales, gestor con guardado/recarga, descubrimiento y vinculación desde
las propiedades, instalación/actualización/desinstalación por versión, y verificar
la estructura nueva de plugins de OBS 33. La estructura antigua puede servir de
transición en 33, pero no debe darse por válida para 34.

## Incidencias del ensayo

Los intentos iniciales del test histórico de dibujo asumían que un frame ya
contenía título al crearse/reactivarse y que la pausa eliminaba todos los frames.
La versión actual conserva el diseño durante la pausa. Se preservan los logs de
esos intentos; se ajustó el test para esperar metadatos y consultar el estado real
de reproducción. No se cambió la conducta del producto para satisfacer el test.
Los resultados finales están en outputs/obs-study-render-final-31.1.2.txt y
outputs/obs-study-render-final-33.0.0.txt; los logs incluyen cierres normales.
Los contadores globales finales de OBS no se presentan como ausencia de fugas ni
se atribuyen a un módulo sin un control comparable.

## Resultado final del panel

DLL normal (hooks OFF, exports diagnósticos ausentes):
B119571FF96B080488C0C4688365F2E1E8AEAD06C7EC2927D5D6BD4F25D66D99.
928 comprobaciones del panel por cada runtime: 31.1.2, 32.2.2 y 33.0.0.
Incluyen orden/pertenencia de grupos, ocultación de grupos vacíos, estilos,
valores heredados, posiciones guardadas y activación opcional del chat.
Cuatro suites relacionadas (host, presets, cinco idiomas y consulta pública)
PASS. Evidencias en outputs/grouped-settings-final-*.txt y
outputs/grouped-settings-regression-20261028.txt.

Se entrega una actualización de desarrollo de la DLL para una instalación
existente. El instalador oficial 1.0.1 y el OBS habitual permanecen intactos.

Localización final en el módulo real: 113 comprobaciones PASS en es/en/it/fr/de,
con valores guardados idénticos. Evidencia: outputs/grouped-settings-localization-final-20261028.txt.
