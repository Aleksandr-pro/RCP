import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
pixels = list(image.getdata())
# Allow for linear and sRGB conversion of the supplied cube colors.
front = sum(1 for r, g, b in pixels if g > 180 and g > r * 1.35 and g > b * 1.15 and b > r * 1.05)
right = sum(1 for r, g, b in pixels if r > 100 and r > g * 1.25 and g > b * 1.35)
bottom = sum(1 for r, g, b in pixels if b > 100 and b > g * 1.25 and r > g * 1.2)
assert front > len(pixels) * 0.08, f"Green front face not found: {front} pixels"
assert right > len(pixels) * 0.005, f"Orange right face not found: {right} pixels"
assert bottom > len(pixels) * 0.005, f"Purple bottom face not found: {bottom} pixels"
print(f"Cube faces found: {front} green, {right} orange, {bottom} purple pixels")
