"""Build the offline illustrated guide from README.md; standard library only."""
from html import escape
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
IMAGES = ROOT / "imagenes"
IMAGES.mkdir(exist_ok=True)

def svg(name, title, inner, height=600):
    (IMAGES / name).write_text(f'''<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="{height}" viewBox="0 0 1100 {height}">
<rect width="1100" height="{height}" rx="24" fill="#071320"/>
<defs><linearGradient id="neon"><stop stop-color="#02baff"/><stop offset="1" stop-color="#ab43ff"/></linearGradient></defs>
<rect x="30" y="28" width="9" height="40" rx="4" fill="url(#neon)"/>
<g font-family="Segoe UI,Arial,sans-serif" fill="#f1f6ff"><text x="55" y="58" font-size="29" font-weight="700">{escape(title)}</text>
{inner}<text x="34" y="{height-22}" font-size="15" fill="#95adc5">OBS PLAYING NEXUS · Esquema de configuración · Las IP y canciones son ejemplos</text></g></svg>''', encoding="utf-8")

def card(x,y,w,h,title):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" fill="#102236" stroke="#28516d" stroke-width="2"/><text x="{x+24}" y="{y+42}" font-size="24" font-weight="700" fill="#63d0ff">{escape(title)}</text>'

def text(x,y,value,size=21,color="#e2edfa"):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}">{escape(value)}</text>'

def button(x,y,w,label):
    return f'<rect x="{x}" y="{y}" width="{w}" height="48" rx="10" fill="#075a83" stroke="#03bdff"/>'+text(x+17,y+31,label,20)

def check(x,y,label):
    return f'<rect x="{x}" y="{y-19}" width="24" height="24" rx="5" fill="#08c892"/><path d="M{x+5} {y-8}l5 5 10-13" fill="none" stroke="#051321" stroke-width="3"/>'+text(x+38,y,label,21)

svg("01-dos-pcs.svg", "01 · Un componente para cada PC",
    card(45,115,415,325,"PC DJ")+text(73,205,"rekordbox / VirtualDJ / Serato",24)
    +text(73,249,"OBS Playing Companion",26)+text(73,301,"Ejemplo IP: 192.168.1.20",21)
    +check(73,361,"Permitir red privada")
    +card(640,115,415,325,"PC DE STREAMING")+text(668,205,"OBS Studio 32.x x64",26)
    +text(668,249,"Plugin OBS Playing NEXUS",24)+text(668,301,"Ejemplo IP: 192.168.1.10",21)
    +text(668,361,"Fuente: Companion — PC DJ",21)
    +'<path d="M480 255h135m-20-16 20 16-20 16" stroke="#15c9ff" stroke-width="5" fill="none"/>'
    +text(485,214,"Título",18)+text(485,303,"Artista",18)+text(485,336,"Portada",18)
    +text(205,505,"Mismo router · Red doméstica · Vinculación confirmada en ambos PCs",23))

svg("02-ajustes-dj.svg", "02 · Configuración del Companion en el PC DJ",
    card(45,100,1010,435,"⚙  PC DJ")+text(75,194,"Programa DJ",22)
    +'<rect x="300" y="160" width="700" height="49" rx="8" fill="#21394f"/>'
    +text(320,193,"rekordbox",24)+text(965,193,"▾",24)
    +check(75,260,"Ver canción en este PC")
    +check(75,316,"Permitir conexión desde otro PC de mi red")
    +check(75,372,"Iniciar envío automáticamente al abrir")
    +button(75,424,220,"Iniciar envío")
    +text(340,454,"Debe aparecer el título antes de probar OBS",22))

svg("03-red-firewall.svg", "03 · Permiso de Windows en el PC DJ",
    card(45,110,470,360,"CONEXIÓN DE CASA")+text(73,206,"Configuración → Red e Internet",22)
    +text(73,250,"Propiedades de Ethernet o Wi-Fi",21)
    +check(73,314,"Red privada")
    +text(73,383,"Solo tu red doméstica de confianza",19,"#a9bdd4")
    +card(555,110,500,360,"FIREWALL")+text(583,206,"Permitir una aplicación",23)
    +text(583,250,"obs-playing-companion.exe",22)
    +check(583,314,"Privada")
    +button(583,363,385,"Permitir otra aplicación…")
    +text(55,517,"Si no aparece en la lista, selecciona el ejecutable instalado del Companion.",22))

svg("04-vincular.svg", "04 · Buscar y confirmar en los dos equipos",
    card(45,110,470,368,"PC DJ · COMPANION")
    +button(75,185,408,"Pair New Device")
    +check(75,288,"Vinculación abierta: 2 minutos")
    +text(75,343,"IP: 192.168.1.20 · Puerto: 9000",22)
    +text(75,409,"Confirma la solicitud del PC streaming",20)
    +card(555,110,500,368,"PC STREAMING · FUENTE OBS")
    +button(583,185,440,"Vincular / cambiar PC DJ")
    +button(583,254,440,"Buscar PC DJ")
    +text(583,338,"DJ-PC — 192.168.1.20",23)
    +button(583,368,440,"Vincular equipo seleccionado")
    +text(110,530,"Compara la huella de identidad y acepta la vinculación en ambos PCs.",24))

svg("05-comprobar.svg", "05 · Verificar el envío real",
    card(45,110,1010,375,"CONNECTED · 1 PC CONECTADO")
    +'<rect x="78" y="190" width="170" height="170" rx="15" fill="url(#neon)"/>'
    +text(105,285,"PORTADA",24)
    +text(288,228,"Summer Lights",35)+text(288,277,"Luna Coast",27,"#65d7ff")
    +check(288,338,"El texto cambia con la canción")
    +check(288,391,"La carátula pertenece al tema actual")
    +text(75,445,"En OBS: estilo PRO + «Mostrar portada / imagen de perfil»",23)
    +text(75,530,"Actualiza plugin y Companion a RC3.1 o posterior para las portadas remotas.",23))

# Translate diagram text without changing coordinates, colours or artwork.
english = {
    "01 · Un componente para cada PC": "01 · One component for each PC",
    "PC DJ": "DJ PC", "PC DE STREAMING": "STREAMING PC",
    "Ejemplo IP: 192.168.1.20": "Example IP: 192.168.1.20",
    "Ejemplo IP: 192.168.1.10": "Example IP: 192.168.1.10",
    "Permitir red privada": "Allow private network",
    "Plugin OBS Playing NEXUS": "OBS Playing NEXUS plugin",
    "Fuente: Companion — PC DJ": "Source: Companion — PC DJ",
    "Título": "Title", "Artista": "Artist", "Portada": "Artwork",
    "Mismo router · Red doméstica · Vinculación confirmada en ambos PCs": "Same router · Home network · Pairing confirmed on both PCs",
    "02 · Configuración del Companion en el PC DJ": "02 · Companion settings on the DJ PC",
    "⚙  PC DJ": "⚙  DJ PC", "Programa DJ": "DJ application",
    "Ver canción en este PC": "Show track on this PC",
    "Permitir conexión desde otro PC de mi red": "Allow another PC on my network",
    "Iniciar envío automáticamente al abrir": "Start sending automatically when opened",
    "Iniciar envío": "Start sending",
    "Debe aparecer el título antes de probar OBS": "Check the title before testing OBS",
    "03 · Permiso de Windows en el PC DJ": "03 · Windows permission on the DJ PC",
    "CONEXIÓN DE CASA": "HOME CONNECTION",
    "Configuración → Red e Internet": "Settings → Network & Internet",
    "Propiedades de Ethernet o Wi-Fi": "Ethernet or Wi-Fi properties",
    "Red privada": "Private network",
    "Solo tu red doméstica de confianza": "Only your trusted home network",
    "Permitir una aplicación": "Allow an app through firewall",
    "Privada": "Private", "Permitir otra aplicación…": "Allow another app…",
    "Si no aparece en la lista, selecciona el ejecutable instalado del Companion.": "If it is missing from the list, select the installed Companion executable.",
    "04 · Buscar y confirmar en los dos equipos": "04 · Search and confirm on both computers",
    "PC DJ · COMPANION": "DJ PC · COMPANION",
    "Vinculación abierta: 2 minutos": "Pairing open: 2 minutes",
    "IP: 192.168.1.20 · Puerto: 9000": "IP: 192.168.1.20 · Port: 9000",
    "Confirma la solicitud del PC streaming": "Confirm the streaming PC's request",
    "PC STREAMING · FUENTE OBS": "STREAMING PC · OBS SOURCE",
    "Vincular / cambiar PC DJ": "Pair / change DJ PC",
    "Buscar PC DJ": "Search for DJ PC",
    "Vincular equipo seleccionado": "Pair selected computer",
    "Compara la huella de identidad y acepta la vinculación en ambos PCs.": "Compare the identity fingerprint and confirm pairing on both PCs.",
    "05 · Verificar el envío real": "05 · Check the actual transmission",
    "CONNECTED · 1 PC CONECTADO": "CONNECTED · 1 CONNECTED PC",
    "PORTADA": "ARTWORK",
    "El texto cambia con la canción": "Text updates when the track changes",
    "La carátula pertenece al tema actual": "Artwork belongs to the current track",
    "En OBS: estilo PRO + «Mostrar portada / imagen de perfil»": "In OBS: PRO style + show cover / profile image",
    "Actualiza plugin y Companion a RC3.1 o posterior para las portadas remotas.": "Update plugin and Companion to RC3.1 or later for remote artwork.",
    "OBS PLAYING NEXUS · Esquema de configuración · Las IP y canciones son ejemplos": "OBS PLAYING NEXUS · Setup diagram · IP addresses and tracks are examples",
}
ET.register_namespace("", "http://www.w3.org/2000/svg")
for source in IMAGES.glob("*.svg"):
    if source.name.endswith(".en.svg"):
        continue
    tree = ET.parse(source)
    for node in tree.iter():
        if node.text in english:
            node.text = english[node.text]
    tree.write(source.with_suffix(".en.svg"), encoding="utf-8", xml_declaration=False)

def inline(value):
    value = escape(value)
    value = re.sub(r"`([^`]+)`", r"<code>\1</code>", value)
    value = re.sub(r"\*\*([^*]+)\*\*", r"<strong>\1</strong>", value)
    value = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', value)
    return re.sub(r"(?<!\*)\*([^*]+)\*(?!\*)", r"<em>\1</em>", value)

def render(markdown):
    # The guide uses headings, paragraphs, flat lists, images, tables and fences.
    lines = markdown.splitlines(); result = []; i = 0
    def block(line):
        return not line.strip() or line.startswith(("#", "![", "```", "|")) or re.match(r"^(?:\d+\.|-) ", line)
    while i < len(lines):
        line = lines[i]
        if not line.strip(): i += 1; continue
        heading = re.match(r"^(#{1,6}) (.*)", line)
        image = re.fullmatch(r"!\[([^\]]*)\]\(([^)]+)\)", line)
        if heading:
            n=len(heading[1]); result.append(f"<h{n}>{inline(heading[2])}</h{n}>"); i+=1
        elif image:
            result.append(f'<figure><img src="{escape(image[2])}" alt="{escape(image[1])}" loading="lazy"/></figure>');i+=1
        elif line.startswith("```"):
            i+=1; code=[]
            while i<len(lines) and not lines[i].startswith("```"):code.append(lines[i]);i+=1
            result.append("<pre><code>"+escape("\n".join(code))+"</code></pre>");i+=1
        elif line.startswith("|"):
            rows=[]
            while i<len(lines) and lines[i].startswith("|"):
                cells=[c.strip() for c in lines[i].strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?",c) for c in cells):rows.append(cells)
                i+=1
            result.append('<div class="table"><table>')
            for n,row in enumerate(rows):
                tag="th" if n==0 else "td"
                result.append("<tr>"+"".join(f"<{tag}>{inline(c)}</{tag}>" for c in row)+"</tr>")
            result.append("</table></div>")
        elif re.match(r"^(?:\d+\.|-) ",line):
            ordered=bool(re.match(r"^\d+\. ",line)); pattern=r"^\d+\. " if ordered else r"^- ";tag="ol" if ordered else "ul"
            result.append(f"<{tag}>")
            while i<len(lines) and re.match(pattern,lines[i]):
                item=re.sub(pattern,"",lines[i]);i+=1
                while i<len(lines) and lines[i].startswith("  ") and not block(lines[i]):item+=" "+lines[i].strip();i+=1
                result.append("<li>"+inline(item)+"</li>")
            result.append(f"</{tag}>")
        else:
            paragraph=[line];i+=1
            while i<len(lines) and not block(lines[i]):paragraph.append(lines[i].strip());i+=1
            result.append("<p>"+inline(" ".join(paragraph))+"</p>")
    return "\n".join(result)

css='''html{color-scheme:dark}body{margin:0;background:#08111d;color:#d8e4f1;font:17px/1.65 "Segoe UI",Arial,sans-serif}main{max-width:1050px;margin:auto;padding:35px 28px 65px}h1{font-size:36px;color:#fff;line-height:1.2}h2{margin-top:55px;padding:13px 0;border-bottom:2px solid #184764;color:#7cdaff}strong{color:#fff}a{color:#72d5ff}li{margin:8px 0}figure{margin:26px 0}img{display:block;max-width:100%;height:auto;border-radius:16px}img[src*="companion-ejemplo"]{max-width:440px;margin:auto}code{font-family:Consolas,monospace;background:#17283b;padding:2px 6px;border-radius:5px}pre{overflow:auto;background:#17283b;padding:18px;border-radius:12px;font-size:14px}.table{overflow:auto}table{border-collapse:collapse;width:100%;font-size:15px}td,th{border:1px solid #29435c;padding:13px;text-align:left;vertical-align:top}th{background:#123653}em{color:#a7bcd0}footer{border-top:1px solid #29435c;margin-top:45px;padding-top:18px;color:#8aa2ba}@media print{html{color-scheme:light}body{background:white;color:#202a35;font-size:11pt}main{padding:0}h1,h2,strong{color:#10283e}h2{break-after:avoid}figure,table,li{break-inside:avoid}a{color:#08639b}code,pre{background:#edf2f8}th{background:#dfedf6}em{color:#46596a}}'''
for stem,lang,title,footer in (
    ("README","es","Configurar Companion · OBS Playing NEXUS","Guía offline de Companion"),
    ("README.en","en","Companion setup · OBS Playing NEXUS","Offline Companion setup guide"),
):
    body=render((ROOT/(stem+".md")).read_text(encoding="utf-8"))
    body=body.replace('href="README.md"','href="README.html"').replace('href="README.en.md"','href="README.en.html"')
    (ROOT/(stem+".html")).write_text('<!doctype html><html lang="'+lang+'"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>'+title+'</title><style>'+css+'</style></head><body><main>'+body+'<footer>NEXUS OBS Plugins by DjEmiMas · '+footer+'</footer></main></body></html>',encoding="utf-8")
print("Built Spanish and English offline guides with five diagrams each")
