> Release status: the production bridge described below is included in official 1.0.1. Earlier development notes below describe the pre-release state; 1.0.0 remains unchanged. See RELEASE-1.0.1.md.

# Now Playing pÃºblico para OBS News Ticker NEXUS

IncorporaciÃ³n de desarrollo: 2026-10-25. No publicada en el instalador estable.

Cada fuente `obs_playing_next_development` registra el procedimiento OBS
`void nexus_now_playing_v1(out string json)`. Devuelve una copia JSON propia con
esquema `nexus.now-playing.v1`, fecha UTC `updated_at`, estado y, mientras reproduce,
artista/tÃ­tulo del snapshot inmutable. No expone credenciales, portadas, rutas ni
perfiles. No arranca detectores nuevos ni modifica Core o sus providers.

Estados: playing, paused, stopped, waiting. Una fuente inactiva o sin publicaciÃ³n
responde waiting. Pausa/stop no anuncian texto activo. Artista <=256 bytes y tÃ­tulo
<=512 bytes, truncados por frontera UTF-8 con `truncated=true` cuando corresponde.
La consulta usa la publicaciÃ³n compartida, es de solo lectura y no hace I/O.

News Ticker 0.7.1 aÃ±ade el modo OBS Playing automÃ¡tico, consulta a 4 Hz fuera del
renderizador y reutiliza su validaciÃ³n, caducidad, ancho fijo y fundido. Requiere
una Ãºnica fuente nativa OBS Playing activa; ante varias no elige silenciosamente.
La entrada JSON manual continÃºa disponible. Versiones antiguas sin el procedimiento
se indican como incompatibles hasta actualizar su mÃ³dulo.

CÃ³digo: src/obs/nexus-now-playing-publisher.hpp, adjunto en source.cpp al crear
cada fuente. No requiere PLAYING_SOURCE_TEST_HOOKS ni funciones privadas.
Prueba CTest: `playing-now-playing-public-snapshot` (serializaciÃ³n, ownership,
pausa, stop, ausencia de pista y lÃ­mites UTF-8). La prueba usa tipos reales de Core
con publicaciÃ³n controlada; no certifica por sÃ­ sola una aplicaciÃ³n DJ real.
ValidaciÃ³n con ambos mÃ³dulos reales en OBS portable se registra en el proyecto
OBS News Ticker NEXUS; los datos demo se identifican como ficticios.

La nueva DLL debe entregarse en una actualizaciÃ³n de Playing cuando se prepare
el siguiente paquete. No se sustituye el mÃ³dulo del OBS habitual automÃ¡ticamente.

Validación 2026-10-25: compilación de desarrollo correcta, CTest del publicador pasado y 7 comprobaciones con ambas DLL reales en OBS portable, 0 fallos y cierre sin fugas declaradas. Se usaron canciones demo ficticias; programa DJ real pendiente.

## Verificación de Playing — 25 de octubre de 2026

DLL de producción compilada con PLAYING_SOURCE_TEST_HOOKS=OFF.
El test público contiene 11 comprobaciones, incluidos escaping JSON y límites
UTF-8 de caracteres de cuatro bytes. Para ejecutarlo con las DLL de OBS:

```powershell
./playing-next/tests/run-now-playing-tests.ps1 -ObsRuntimeRoot ./tests/obs32/32.2.2
```

En OBS portable 32.2.2 se han cargado ambos módulos reales y un probe de prueba:
creación, lectura inicial, cambio de tema sin alterar noticias inferiores,
desactivación, reactivación y eliminación de la fuente: 7/7 comprobaciones.
Datos ficticios Bridge QA, no una sesión DJ real. Cierre normal de OBS, Exit=0.
El primer intento completó las comprobaciones pero agotó el cierre por el aviso
de confirmación; la repetición desactivó ese aviso solo en la copia portable.
Evidencia: outputs/news-ticker-compat-20261025-obs-final.txt.

Paquete de actualización independiente en outputs/OBS-Playing-NEXUS-News-Ticker-compat-20261025.zip.
No modifica el instalador estable 1.0.0 ni instala automáticamente en OBS habitual.
En News Ticker elegir el modo OBS Playing automático y mantener una sola fuente
Playing activa. Varias fuentes requieren resolver la selección en News Ticker.
