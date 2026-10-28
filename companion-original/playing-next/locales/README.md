# Idiomas de la interfaz

Español, inglés, italiano, francés y alemán están incluidos en ambos binarios y
en el instalador. OBS proporciona su idioma mediante `obs_module_set_locale`;
Companion usa `QLocale::system()` y el instalador `CurrentUICulture` de Windows.
Las variantes regionales se resuelven al idioma base. Idiomas no admitidos usan inglés.

`messages.tsv` es el catálogo revisable, con cinco columnas en orden es/en/it/fr/de.
Cada entrada usa el texto original como clave; `\n` y `\r` son saltos escapados.
Ejecuta `python locales/generate.py` desde `playing-next` después de editarlo.
`--check` valida que las cabeceras y el catálogo C# generado estén actualizados
y que los marcadores `%1…%9` no cambien. No se hacen traducciones por red.

Los identificadores de ajustes, rutas, credenciales, nombres de estilos,
metadatos musicales y nombres de presets del usuario no se traducen. Los presets
originales conservan su nombre almacenado y solo traducen su etiqueta al mostrarse.
La UI de las bibliotecas/Windows puede seguir sus traducciones del sistema.
Las guías ilustradas se mantienen en español e inglés.
