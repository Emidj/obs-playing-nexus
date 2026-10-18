# Imagen del instalador

Imagen elegida explícitamente por el usuario el 14 de octubre de 2026:
`nexus-installer.png`, copia exacta de `C:/NEXUS-Dependencies/OBS NEXUS/Logo NEXUS entre ondas neón.png`.

Usar esta imagen para el instalador de OBS Playing Next en fase 28.
Conservar sus proporciones y mostrar completos el símbolo y la palabra NEXUS.
La imagen interior del instalador conserva estas proporciones.

## Icono RC3

`nexus-icon-source.png` es copia exacta del PNG transparente elegido por el usuario
el 15 de octubre de 2026: `C:/NEXUS-Dependencies/OBS NEXUS/Imagen de ChatGPT 9 oct 2026, 08_28_06.png`.
`build-icon.py` recorta únicamente margen transparente, centra el símbolo con
margen uniforme y genera `nexus-app.ico` (16, 24, 32, 48, 64, 128 y 256 px).
El instalador y Companion incorporan el mismo ICO en sus recursos Windows.
El acceso directo del Companion utiliza el icono de su ejecutable.
