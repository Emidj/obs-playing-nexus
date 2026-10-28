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
