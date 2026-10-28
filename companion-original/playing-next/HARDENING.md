# Fase 26 — estrés y seguimiento de estabilidad

El usuario ha pedido bajar la prioridad de PLAYING-18-001 porque no se ha
reproducido. Se conserva como incidencia histórica no reproducida; no se afirma
una causa o reparación. No ampliar los ensayos centrados exclusivamente en ese
incidente sin evidencia nueva. Tres cierres del gestor/fuente activos en 32.1.2,
bajo el capturador propio, terminaron con Exit=0/timeout=0/exceptions=0.

Banco sostenido: DLL normal final de fase 25, SHA
`7E287C980AC125B309113FEEB123BA8EE522E4F4ECCBBF2D5D48421AB217CFFD`.
OBS 32.2.2, SDR NV12 1280×720/60 fps, seis estilos con portada local, cambios
de tema cada dos segundos, sin codificar/emitir. Plan inicial: una hora de actividad entre
dos ventanas de idle. El observador de vídeo es independiente del plugin.

Se recogen intervalos reales de muestreo, CPU del proceso, memoria privada,
working set, handles, threads, GPU por motor y salida RAW. Para el banco largo
se exige al menos 0,6 muestras/s, además de muestras de vídeo independientes y
los gates de salida visible/FPS/cierre. Los lotes breves de fase 24 conservan
su gate anterior. La media de CPU del ensayo largo debe ponderarse por duración
del intervalo, evitando sesgo si la consulta de contadores tarda más.

Reproducción con un directorio nuevo bajo work:

```powershell
& playing-next/tests/run-obs-profile.ps1 -WorkRoot work/phase26-soak-rerun `
  -Rounds 1 -Soak -SoakSeconds 1800 -SoakArtwork -MeasureSeconds 20 `
  -ProductionDllPath work/phase25-final-off-32.2.2/tests/obs32/obs-playing-next.dll
```

Compilar antes el observador opcional `playing-obs-profile-probe`; no incorporarlo
al instalador. La DLL medida se fija explícitamente: no usar la copia antigua
de profiling de fase 24 para probar el producto corregido. Directorios, perfiles
y procesos del banco son propios; no se modifica el OBS habitual.

La estabilidad durante esta sesión de 30 minutos no demuestra ausencia universal de fugas/crashes,
estabilidad de días, hardware más débil ni transmisión real entre dos PCs.
Se informan tendencias por intervalos, calentamiento de cachés, recursos retenidos
tras liberar las fuentes y cualquier fallo. Timeouts/kills nunca son PASS.
La sesión sostenida debe terminar normalmente antes de declarar su resultado.

El usuario redujo la sesión activa en curso a 30 minutos totales. El fixture
original ya había cargado 3600 segundos: se registra la abreviación y se solicita
cierre normal de su ventana al llegar a 1800 segundos. El marcador manual declara
expresamente que no es PASS del fixture original; permite conservar sus muestras
antes de que los gates de la duración antigua rechacen el plan incompleto.
Se evalúan datos reales y Exit=0 por separado. No hubo ventana final de recovery;
no se atribuye un resultado a esa parte ausente del plan original.
