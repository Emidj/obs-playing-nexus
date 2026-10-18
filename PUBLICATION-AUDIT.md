# Publicación RC3.1 — 18 de octubre de 2026

## HECHOS COMPROBADOS

- NEXUS 1.0 RC3.1 es la versión oficial indicada por el autor. Este repositorio procede de su ZIP Sources RC3.1, sin importar el historial Git del proyecto original o el antiguo producto Lua 0.5.9.
- El instalador se conserva byte por byte: SHA256 `3C2688FC2CA204F55FD1A253DEA1D231D9526671060A8C11271934DAE87557A0`. La nueva ejecución aislada de su suite existente terminó con **61 comprobaciones PASS**. No se instaló el producto en el OBS habitual.
- Se analizaron archivos del repositorio y ZIP anidados mediante patrones de tokens GitHub/AWS/Slack, cabeceras de claves privadas y rutas personales. Las claves privadas encontradas dentro de los fuentes Qt son fixtures de tests públicos o cadenas del parser SSL de Qt; no son identidades del usuario. Los dos archivos Qt coinciden con los SHA256 fijados por la receta original.
- Los 14 archivos del motor compartido fijados por los dos locks coinciden con sus hashes. Se conservan fuentes, cabeceras y licencias originales del motor.
- Se sustituyeron rutas personales por rutas de ejemplo en los scripts y documentación preparados. Estas rutas son ejemplos; el repositorio no incluye SDK instalados en esas ubicaciones.
- Se conservaron LICENSE MIT, avisos originales y licencias de terceros. El alcance del binario combinado GPLv3 se explica en LICENSES.md y COPYING. Las fuentes Qt, sus recetas, parches y SBOM se suministran en el ZIP Sources de la Release.
- No se incluyen perfiles personales .pcs, bibliotecas DJ, configuraciones de OBS, volcados, credenciales de dispositivos, logs de sesiones o builds históricos. Las claves de pruebas de Qt son datos públicos de terceros y no deben utilizarse en producción.
- El ZIP Sources de publicación se genera desde el árbol revisado y no incluye .git. Difiere del ZIP de desarrollo original por las rutas de ejemplo, documentos de publicación y avisos añadidos; los binarios oficiales no se han recompilado ni modificado.

## INFERENCIAS Y LÍMITES

El escaneo por patrones reduce el riesgo de publicación accidental, pero no demuestra ausencia absoluta de secretos ni sustituye una auditoría de seguridad completa.

La documentación histórica de RC3.1 registra pruebas locales TLS/portadas, OBS portable 32.2.2 y actualización. Estas pruebas no se han repetido íntegramente en esta publicación. Los 61 checks nuevos corresponden al instalador, no certifican rendering o programas DJ.

## PENDIENTES DE VALIDACIÓN DEL PRODUCTO

- Portadas entre dos PCs físicos con ambos componentes actualizados.
- Reconstrucción y uso en Windows limpio; no se afirma reproducibilidad verificada.
- Nueva matriz completa OBS 32.x y sesiones prolongadas con todas las versiones DJ.
- Las recetas históricas contienen referencias a SDK y fixtures externos. Se deben proporcionar los SDK compatibles y adaptar las rutas; BUILD_TESTING=OFF evita pruebas históricas que requieren archivos del workspace original.

Estos límites se conservan en las notas de la Release Candidate; no se presenta RC3.1 como certificación universal.
