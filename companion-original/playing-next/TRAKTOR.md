# Traktor Pro 3.x / 4.x — lector nativo

Windows AMD64. Disponible en OBS Playing NEXUS y Companion, incluida su detección
automática. Companion se instala únicamente en el PC DJ. No se instala código
dentro de Traktor ni hacen falta Broadcast, Icecast, Bonjour, cambios QML o MIDI.

## Contrato

Se aceptan versiones de archivo `3.x.x.x` y `4.x.x.x`. Se abren `Traktor.exe`
(3.x) o `Traktor Pro 4.exe` (4.x) únicamente
con permisos de consulta y lectura. Se validan versión, AMD64, ruta, PE, RTTI,
canales y metadatos. Descubrimiento dinámico de tablas RTTI reubicadas entre
compilaciones; estructuras no reconocidas devuelven no disponible. Cada rama
utiliza su layout validado; no se aplican los offsets de 4.x a 3.x. Si ambas
aplicaciones están abiertas al descubrir el proceso, se indica ambigüedad y no
se inventa una prioridad. Una sesión existente conserva su proceso mientras viva.

Se leen los Track Decks A–D. Tiene prioridad el deck reproduciéndose marcado
MASTER. Sin maestro válido se conserva el deck anterior mientras reproduzca;
con un único candidato se utiliza ese candidato. Varios candidatos sin maestro
ni selección previa producen ambigüedad, conservando el estado anterior. MASTER
es el maestro de tempo de Traktor, no una medición del audio audible.

Se repiten las lecturas para descartar cambios de maestro/canción durante una
muestra; no se garantiza atomicidad global ni ausencia de ABA. Pausa retira
texto activo. Cerrar/reabrir Traktor libera/recrea la sesión. No se derivan
identidades de títulos ni de punteros.

La clave de portada del mismo objeto de canción identifica el archivo Coverart.
El formato 8 verificado contiene dimensiones LE y píxeles BGRA completos; WIC lo
convierte a PNG. Se rechazan formatos o tamaños no reconocidos. Caché propia
con SHA256, máximo 64 MiB; sin portada se respeta el fallback existente al perfil.
Documentos se obtiene de Windows y la subcarpeta de la versión del proceso:
`Native Instruments/Traktor <major.minor.patch>`. Roots personalizados fuera de
esta ubicación no están certificados.

Worker compartido por sesión, lectura solo por demanda del host, descubrimiento
acotado/cancelable y liberación al último consumidor. Nada de memoria/disco/reloj
en render. Se reutilizan Track State, renderer y transporte LAN existentes.

## Compatibilidad y pruebas

- Ramas 3.x/4.x habilitadas por petición del usuario, bajo la premisa de continuidad
  de estructuras entre versiones y con validación automática al arrancar.
- Pruebas reales disponibles: **3.5.0 build 270** y **4.4.1 build 22**: título,
  artista, A→B MASTER y portada. Pausa/reanudación y reapertura reales en 4.4.1;
  pausa de 3.x cubierta con fixtures. No equivale a una matriz de toda 3.x/4.x.
- Fixtures: Unicode, maestro, campos corruptos, carreras, cancelación, PNG,
  caché y ciclo de vida compartido. Una PE ficticia 4.0.0.0 con RTTI reubicada
  valida el descubridor, no una instalación real de Traktor 4.0.
- Otras instalaciones 3.x/4.x, Remix/Live Input, stems, hardware externo, roots
  personalizados y un nuevo ensayo físico de dos PCs quedan sin prueba real.
- 3.5.0 guarda la clave de portada combinada en un campo, a diferencia de
  los dos campos de 4.4.1. Ambas producen una clave normalizada y validada.

Diagnóstico opt-in (ejecutable de pruebas fuera del payload instalable):

```text
playing-traktor-tests.exe
playing-traktor-tests.exe --live 15 C:\ruta\absoluta\cache-de-prueba
```

El segundo consulta el proceso real y muestra título/artista/estado/ruta de
portada, guardando únicamente imágenes derivadas. No modifica Traktor.

Referencia: [Traktor Overview](https://docs.native-instruments.com/ni-tech-manuals/traktor-pro-manual/en/traktor-overview).
