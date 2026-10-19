"""Build the offline illustrated guide from README.md; standard library only."""
from html import escape
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
IMAGES = ROOT / "imagenes"
IMAGES.mkdir(exist_ok=True)

def svg(name, title, inner, height=600):
    caption = "Setup diagram" if name.endswith("-en.svg") else "Esquema de configuración"
    (IMAGES / name).write_text(f'''<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="{height}" viewBox="0 0 1100 {height}">
<rect width="1100" height="{height}" rx="24" fill="#071320"/>
<defs><linearGradient id="neon"><stop stop-color="#02baff"/><stop offset="1" stop-color="#ab43ff"/></linearGradient></defs>
<rect x="30" y="28" width="9" height="40" rx="4" fill="url(#neon)"/>
<g font-family="Segoe UI,Arial,sans-serif" fill="#f1f6ff"><text x="55" y="58" font-size="29" font-weight="700">{escape(title)}</text>
{inner}<text x="34" y="{height-22}" font-size="15" fill="#95adc5">OBS PLAYING NEXUS · {caption}</text></g></svg>''', encoding="utf-8")

def card(x,y,w,h,title):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="18" fill="#102236" stroke="#28516d" stroke-width="2"/><text x="{x+24}" y="{y+42}" font-size="24" font-weight="700" fill="#63d0ff">{escape(title)}</text>'

def text(x,y,value,size=21,color="#e2edfa"):
    return f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}">{escape(value)}</text>'

def button(x,y,w,label):
    return f'<rect x="{x}" y="{y}" width="{w}" height="48" rx="10" fill="#075a83" stroke="#03bdff"/>'+text(x+17,y+31,label,20)

def check(x,y,label):
    return f'<rect x="{x}" y="{y-19}" width="24" height="24" rx="5" fill="#08c892"/><path d="M{x+5} {y-8}l5 5 10-13" fill="none" stroke="#051321" stroke-width="3"/>'+text(x+38,y,label,21)

# Current simplified setup diagrams; old image files remain for archived guides.
for lang in ("es", "en"):
    es=lang=="es"
    svg("quick-01-"+lang+".svg", "01 · Un componente por PC" if es else "01 · One component per PC",
        card(45,120,470,330,"PC DJ" if es else "DJ PC")
        +text(75,220,"Companion",32)+text(75,280,"rekordbox / VirtualDJ / Serato",23)
        +text(75,350,"Abrir y conectar" if es else "Open and connect",24)
        +card(555,120,500,330,"PC DE STREAMING" if es else "STREAMING PC")
        +text(585,220,"OBS Studio",32)+text(585,280,"Plugin OBS Playing NEXUS",23)
        +text(585,350,"Sin Companion" if es else "No Companion needed",24)
        +text(90,515,"Los dos en la misma red" if es else "Both on the same network",25))
    svg("quick-02-"+lang+".svg", "02 · Conectar una vez" if es else "02 · Connect once",
        card(45,120,470,330,"PC DJ · COMPANION" if es else "DJ PC · COMPANION")
        +button(75,195,410,"Conectar con OBS")
        +text(75,300,"Lectura y envío automáticos" if es else "Automatic reading and sending",22)
        +text(75,360,"Aceptar solicitud" if es else "Accept request",24)
        +card(555,120,500,330,"OBS")
        +button(585,195,440,"Conectar Companion")
        +text(585,300,"Búsqueda automática" if es else "Automatic discovery",24)
        +text(585,360,"Confirmar identidad" if es else "Confirm identity",24)
        +text(90,515,"Sin IP, puertos ni códigos" if es else "No IP addresses, ports or codes",25))
    svg("quick-03-"+lang+".svg", "03 · Comprobar el resultado" if es else "03 · Check the result",
        card(45,120,1010,350,"OBS · ESTADO REAL" if es else "OBS · ACTUAL STATUS")
        +check(80,230,"Conexión: conectada" if es else "Connection: connected")
        +check(80,305,"Canción: recibida" if es else "Track: received")
        +check(80,380,"Imagen en OBS: disponible" if es else "Image in OBS: available")
        +text(80,530,"La imagen depende de la portada o del perfil" if es else "Image availability depends on artwork or profile",24))

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
print("Built Spanish and English offline guides with three current assistant diagrams each")
