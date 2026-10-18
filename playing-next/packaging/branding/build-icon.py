"""Convert the approved transparent NEXUS logo to a padded Windows icon."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent
image = Image.open(root / "nexus-icon-source.png").convert("RGBA")
bounds = image.getchannel("A").getbbox()
if not bounds:
    raise ValueError("Empty logo")
logo = image.crop(bounds)
logo.thumbnail((920, 920), Image.Resampling.LANCZOS)
canvas = Image.new("RGBA", (1024, 1024))
canvas.alpha_composite(logo, ((1024-logo.width)//2, (1024-logo.height)//2))
canvas.save(root / "nexus-app.ico", sizes=[(n,n) for n in (16,24,32,48,64,128,256)])
