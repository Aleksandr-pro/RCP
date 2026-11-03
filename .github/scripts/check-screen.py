import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
pixels = list(image.getdata())
# Allow for color conversion, but require substantial areas of both colors.
blue = sum(1 for r, g, b in pixels if 35 <= r <= 75 and 145 <= g <= 190 and 195 <= b <= 240)
dark = sum(1 for r, g, b in pixels if r <= 25 and 10 <= g <= 35 and 25 <= b <= 50)
assert blue > len(pixels) * 0.03, f"Blue diagnostic band not found: {blue} pixels"
assert dark > len(pixels) * 0.25, f"Dark background not found: {dark} pixels"
print(f"Diagnostic image found: {blue} blue and {dark} dark pixels")
