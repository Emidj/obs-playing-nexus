# Profiling local y alcance de certificación — fase24

## Banco reproducible

Windowsx64, OBS32.2.2 limpio portable bajo work,1280x720/60fps, D3D11,
SDR/NV12/709/Partial. No cámaras, juego, captura de escritorio, streaming,
encoder ni plugins de terceros. La escena privada del banco sustituye SOLO
su salida propia y restaura su referencia antes del cierre. No usa OBS habitual.

La DLL de recursos es exactamente la normal/hooksoff de fase23:
25BC31A78FF1FFEC1209D55BC562A23DE547C1FA416F95E70274F85A441EE589.
Un observador nativo externo de pruebas conecta un consumidor RAW de vídeo:
esto mantiene la composición activa aunque la ventana no sea visible. Incluye
conversión/readback RAW, común a todos los escenarios, y NO representa NVENC.
No hay callback Lua en el hilo de vídeo ni consulta GPU de timestamps que espere
al resultado. Los binarios del observador no forman parte del producto instalable.

Se registran hardware/controladores/OS/plan energético/SHA en environment.json.
El sistema conserva su configuración y actividad normal; no se cambian relojes,
energía, GPU, prioridades o servicios. La variabilidad entre rondas es visible.

## Recursos

Controles: salida vacía sin DLL, DLL con fuente inactiva, recuperación tras
retirar fuentes. Tres procesos OBS nuevos, tres rondas, orden de perfiles
alternado. Banco principal:5s warmup+15s medida/escenario, sin portada explícita.
Cobertura con portada/perfil fallback, History esperando y History compartido:
5s warmup+10s medida/escenario, tres rondas. Escena de seis fuentes: estilos
Neon/Vinyl/Glass/Waveform/Particles/VerticalLiquid, escala0.4 en rejilla.
Una fuente aislada usa tamaño nativo. Iluminación80, pulsoOFF, speed100.

Muestreo externo aproximadamente900ms:

- CPUcore%=100*delta de TotalProcessorTime/delta de tiempo real.100%=un procesador lógico.
- CPUmachine%=CPUcore%/16 en esta máquina; no son porcentajes exclusivos del plugin.
- PrivateMiB, WorkingMiB, handles y threads del proceso OBS.
- GPU=max de las instancias de GPU Engine del PID propio, con PdhAddEnglishCounterW.
  Valores faltantes son unavailable, no0. Son utilización por proceso/motor,
  no vatios, VRAM total ni tiempo de GPU exclusivo del plugin.
- AverageFrameMs es el promedio de1s del trabajo del hilo gráfico que publicaOBS;
  incluye render/displays y esperas/readback. No es timestamp de GPU ni coste
  aislado del plugin. Su p95 es de esos promedios, no de cada frame individual.
- OutputFrames y sus timestamps del observador RAW; ActiveFPS y LaggedFrames deOBS.

CPU/GPU sampled peak/p95 son resúmenes de ventanas, no picos instantáneos.
JSON conserva números; CSV normaliza decimales a invariant culture. No atribuir
como ahorro una diferencia de medias cuyas rondas se solapan ampliamente.

Definición de motor GPU y API de contadores:
[Microsoft: GPUs in Task Manager](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/),
[PdhAddEnglishCounterW](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw).

## Latencias

DLL instrumentada separada. profileTrace es opt-in, limitado128observaciones por
fuente y protegido por mutex. Código/exports completamente ausentes con hooksOFF.
Registra después de drawTrack el primer título nuevo con alpha de transición>=5%,
con os_gettime_ns y obs_get_video_frame_time. El observador asocia EXACTAMENTE ese
timestamp con la entrega del frame RAW. Falla si no hay correlación o el orden
de relojes no es coherente. No infiere visibilidad leyendo solo el Core/GUI.

20cambios por perfil History y20órdenes TLS loopback; cuatro inicializaciones
separadas como sample0. Historias sintéticas pequeñas reescritas y cerradas antes
de tomar inputNs. Siguiente cambio2s después de recibir el anterior: patrón
controlado, no distribución aleatoria de llegadas. Fade200ms por fase según
configuración. PortadaOFF para aislar cambio de texto. Incluye polling/worker,
cola GUI, transición y ruta RAW; excluye encoder/red de streaming/monitor.
TLS se mide desde la orden al emisor fixture hastaRAW, no desde un DJ real.
No representa disco cargado, bases grandes, Serato/rekordbox vivos o LAN física.

Nearest-rank p95 sobre20cambios por caso. Es una muestra pequeña: máximos
observados/deadlines del banco no son límites garantizados o SLA del producto.

## Sesión sostenida

Tres minutos, seis fuentes, cambio simultáneo de texto cada2s; memoria, handles,
FPS y recuperación posterior. Sirve para calentamiento/plateau corto. No prueba
estabilidad de horas, ausencia absoluta de leaks o desempeño de un stream real.
Imágenes de referencia static const permanecen durante la vida del módulo;
retención tras ocultar/liberar no implica automáticamente crecimiento sin límite.
No desactivar/free esas referencias como parte de una medición sin nueva validación.

## Gates locales y pendientes

Gates de banco: fuente activa correcta, píxeles no negros en salida visible,
GPUcounter válido, muestras independientes, RAW>=54fps en configuración60,
metadata→render→RAW correlacionados y cierre normal. Registrar lag por ventana.
Estos son gates operativos del fixture, NO requisitos de hardware universales.

Certificación local acotada a equipo/versiones/escenarios/SHA documentados.
La matriz funcional de estilos pertenece a fase23. OBS32.x completo/terceros/
HDR/OpenGL→fase25; estrés de horas y PLAYING-18-001→26; RC→27;
instalación→28; benchmark final de paquete→29. Hardware más débil y dos PCs
físicos permanecen pendientes, sin presentarlos como probados.
