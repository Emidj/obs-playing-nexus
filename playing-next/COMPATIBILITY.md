# Instalador adaptativo 1.1.0 — 28 de octubre de 2026

La 1.1.0 admite las ramas OBS 31.x, 32.x y 33.x Windows x64. Detecta la versión y
elige estructura legacy (31/32), ProgramData moderno (33 normal) o plugins local
(33 portable). Ver [1.1.0](RELEASE-1.1.0.md). Las notas siguientes conservan el
estudio previo; la restricción a OBS 32 del instalador 1.0.1 es histórica.

La DLL 1.1.0 se compila con Qt 6.6.3 y selecciona el backend TLS apropiado.
OBS 31.0.0: carga de DLL/imports y TLS Qt 6.6 verificados; ejecución completa
de OBS pendiente por un bloqueo de arranque previo a los plugins en este equipo.
OBS 31.1.2, 32.2.2 y 33.0.0: instalación, detección automática, dibujo y cierre
verificados en copias portables aisladas. No certifica todos los builds de cada rama.

# Estudio adicional OBS 31 / 33 — 28 de octubre de 2026

[Ajustes y pruebas en OBS 31.1.2 y 33.0.0](OBS-31-33-STUDY.md).
Carga/panel y un ensayo de dibujo/TLS loopback verificados en esas versiones.
No equivale a certificar todas las ramas 31.x/33.x. El instalador oficial 1.0.1
mantiene por ahora su validación de OBS 32.x. La matriz siguiente es histórica.

# Compatibilidad OBS — alcance de fase 25

El lector Traktor Pro 3.x/4.x es posterior a esta matriz histórica. Su alcance y
evidencia actual están en [TRAKTOR.md](TRAKTOR.md): pruebas reales 3.5.0 build 270 y 4.4.1 build 22,
con el resto de las ramas 3.x/4.x sujeto a validación automática de estructuras.

Producto objetivo: OBS Studio 32.x, Windows x64. El SDK de compilación 31.1.1
no sustituye las pruebas con runtimes reales. Cada matriz fija el SHA de la DLL.

Banco principal: 32.0.0, 32.0.4, 32.1.2 y 32.2.2, Direct3D 11/SDR, configuración
portable nueva por caso. Comprueba 23 NORMAL y tres aliases, 14 PRO y dos estilos
solo texto, animación, cambio de tema, Unicode, portadas, alpha, selección y
persistencia de presets. El gestor se cierra abierto con una fuente pública
activa. Las 23 fuentes Lua originales coexisten en todos los casos.

Las capturas del renderer se verifican por píxeles: scroll de texto, movimiento
por estilo, identidad/alpha en transiciones, hue, iluminación y conservación de
la portada. La matriz no comprueba cada combinación de fuente, idioma y tamaño.

Banco gráfico adicional en 32.2.2: OpenGL SDR y Direct3D 11 P010 Rec.2100 PQ/HLG.
Debe confirmar backend/formato en el log, dibujo no vacío, fuentes estáticas y
animadas y cierre normal. Sus capturas de fuente no miden luminancia del monitor
ni fidelidad del frame final HDR; no bastan para certificar mastering HDR.
La fuente no declara un espacio HDR propio: OBS aplica su contrato sRGB por
defecto. No añadir un flag HDR para aparentar compatibilidad.

Los ajustes compartidos entre dos fuentes repiten el callback de defaults. La
fuente conserva los objetos de tipografía por defecto que ya existen, evitando
reemplazar objetos default-only en los runtimes afectados. El control específico
comprueba dos fuentes BASIC, texto visible y cierre; además verifica la identidad
y valores del objeto de tipografía en las ejecuciones posteriores a su ampliación.
Los crashes anteriores y el cambio provisional descartado quedan documentados.

Los módulos incluidos de OBS y scripts originales forman parte del banco.
No se certifican módulos externos arbitrarios, Windows 10, otros controladores,
AMD/Intel como GPU de render, codificación, dos PCs físicos ni estabilidad de
horas. PLAYING-18-001 permanece abierto hasta investigar su causa; un cierre
normal o una matriz verde no demuestran que ese fallo histórico esté corregido.

Reproducción desde la raíz del proyecto, con la DLL instrumentada compilada:

```powershell
& playing-next/tests/run-obs-compatibility.ps1 -EvidencePrefix phase25-rerun
& playing-next/tests/run-obs-graphics-compatibility.ps1 -EvidencePrefix phase25-graphics-rerun
```

Las rutas deben ser nuevas bajo `work/`. El helper rechaza timeouts, cierres
anormales, resultados antiguos y errores de callbacks Lua. La DLL normal se
restaura y verifica sin exports de prueba al terminar. No instalar estos bancos
ni modificar la configuración del OBS habitual.

Evidencias autoritativas: `outputs/phase25-compatibility.csv`,
`outputs/phase25-graphics-final.csv` e informe de cierre de fase 25. Las tablas
gráficas iniciales conservan los intentos fallidos y no son el resultado final.
