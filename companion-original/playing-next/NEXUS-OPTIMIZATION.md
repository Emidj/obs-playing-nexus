# Optimización NEXUS — fase 23

## Contrato del scheduler

SessionHost sigue siendo propietario GUI del Core, readers, timelines, imágenes
y publicación. Render consume snapshots const; no se añade reloj, I/O, shaping,
Core refresh ni rasterizado en el hilo gráfico. El renderer compartido y sus
presupuestos actuales permanecen intactos.

Cada Entry conserva needsAnimation calculado tras su última actualización.
El tick de animación solo desarrolla las entradas activas con esa demanda.
Las estáticas requieren únicamente la comprobación de la bandera durante el
recorrido del mapa; no se actualizan su timeline, snapshot o artwork por ese tick.
No es una lista O(1) ni ausencia de todo trabajo al existir otra fuente animada.

Los eventos de metadata, cambios de configuración y expiry realizan un pase
completo. El timer de expiry independiente no se vuelve a armar/cancelar desde
un pase exclusivo de animación: un texto estático puede expirar mientras otro
estilo anima. Ocultar libera demanda; cuando no queda animación el timer para.
Los perfiles22 y el polling necesario de providers locales siguen vigentes.

## Muestra dinámica coalescida

Layer motion, borderSweep, recordAngle, recordShimmer y lightGain de una entrada
usan una misma lectura inmutable y una copia diferida, como máximo una, que se
publica al terminar. Si ningún campo cambia no se asigna esa copia. Se conservan
las fórmulas/periodos existentes de los14PRO, el reloj compartido por entrada y
la rotación/temblor del vinilo. No se restauran Float/Breathe en la interfaz.

El límite corresponde a ESTA muestra dinámica, no al total de allocations por
frame: preparación de metadata, timeline/OUT/IN y actualización de artwork tienen
sus rutas existentes. No se cambian geometría, tipografías, assets, alpha, tono,
intensidad, presets/aliases, protocolos o cachés para atribuir un ahorro ficticio.

## Contadores

SchedulingStats, leído solo por el propietario GUI:

- animationPasses: callbacks de publicación exclusiva de animación.
- animationVisits: entradas activas realmente desarrolladas en esos callbacks.
- staticSkips: entradas activas sin demanda de animación saltadas.
- dynamicCopies: copias diferidas de la muestra dinámica, incluidos pases de eventos.

Los contadores son acumulativos por host y no miden tiempo ni recursos. Se
exponen a OBS únicamente con PLAYING_SOURCE_TEST_HOOKS. Las comparaciones por
ventana deben seguir consultando mientras esperan, y esperar los comandos GUI.

## Validación y límites

source-host-tests comprueba estática+vinilo+programa multicapa+pulso, coherencia
matemática, expiración estática con otra animación, metadata/transition y parada.
obs-optimization.lua prueba dos fuentes nativas reales, seis capturas GPU y
contadores; check-optimization-captures.cjs exige RGBA estática idéntica, vinilo
moviendo, metadata actualizada y RGBA final idéntica tras ocultar la animada.

OBS mantiene su render a FPS global y puede dibujar repetidamente el mismo frame.
No hay nueva caché de composición GPU ni un porcentaje CPU/GPU certificado.
La medición de ahorro, picos, latencias y estabilidad prolongada es fase24.
