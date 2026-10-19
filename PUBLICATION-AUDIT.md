# Publicación RC3.2 — 19 de octubre de 2026

## HECHOS COMPROBADOS

- Actualización sobre la publicación y galerías RC3.1, conservando el historial público y la Release anterior.
- El ZIP de desarrollo RC3.2 difiere del ZIP original RC3.1 en 14 archivos: Companion, guías/captura, pruebas de UI y metadatos del instalador. Sin archivos añadidos ni eliminados en ese paquete de desarrollo.
- Instalador oficial sin modificar: SHA256 `80085EE7052136C2D63622DA82D128BE03C928FFA94A8725D60AAE04BE59BB12`.
- Companion incluido: SHA256 `B8BFA9519BF1944DC2CB04574C15F6DA02ADA91F224B5D90C3EEE5329FA5CC3F`, comprobado directamente dentro del EXE del instalador.
- Plugin OBS incluido: SHA256 `AF54873E3D55A130D96210495F6E6C7DB2E7DB87DA8C65B5AC5CCA3BBA9F948A`, idéntico al de RC3.1.
- El código del motor del instalador solo cambia sus metadatos de versión. La nueva ejecución de su suite se detuvo al detectar OBS abierto. No se completó ni se afirma un nuevo PASS de instalación/desinstalación RC3.2; la sesión OBS habitual permanece abierta.
- El informe de desarrollo RC3.2 registra 93 checks UI/perfil/preview a escalas 100 % y 200 %, 87 checks del diseño compacto al 150 % antes del aviso final Serato, 56 checks del ejecutable real y 60 del paquete con dependencias locales. Son resultados previos de desarrollo, no nuevos ensayos de esta publicación.
- Se conservan licencias MIT/GPL y de terceros según LICENSES.md y COPYING. Los archivos fuente Qt y la receta de dependencias coinciden con los hashes de la publicación anterior; no se han actualizado dependencias.
- Se revisa el árbol público preparado mediante patrones de credenciales, claves privadas y rutas personales. Las claves de prueba incluidas dentro de los archivos Qt originales son fixtures públicos de terceros, no identidades del usuario.
- Fuentes y guía de publicación se generan desde los archivos revisados; se excluyen .git, perfiles personales, bibliotecas DJ, logs de pruebas, builds y medios de cámara. El ZIP Sources conserva las fuentes Qt completas, recetas, licencias y SBOM. La captura del README muestra datos de demostración con una conexión loopback.

## LÍMITES

El escaneo por patrones no demuestra ausencia absoluta de secretos. La publicación no equivale a una auditoría de seguridad completa ni a nuevas pruebas con programas DJ reales.

Continúan pendientes las portadas entre dos PCs físicos actualizados, Windows limpio, una nueva matriz completa OBS 32.x y sesiones prolongadas. No se afirma compilación reproducible verificada. Las recetas históricas requieren SDK externos y algunos tests requieren fixtures del workspace original.

La ausencia de Bonjour en otro equipo puede impedir el descubrimiento de Serato. La publicación no certifica que se haya resuelto una conexión real en ese equipo ni instala Bonjour automáticamente.
