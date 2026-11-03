import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
pixels = list(image.getdata())
# The Vulkan clear color can be converted differently for sRGB surfaces.
green = sum(1 for r, g, b in pixels if g > 35 and g > r * 1.5 and g > b * 1.15 and b > r * 1.2 and g < 180)
red = sum(1 for r, g, b in pixels if r > 80 and r > g * 1.3 and r > b * 1.3)
blue = sum(1 for r, g, b in pixels if b > 80 and b > r * 1.3 and b > g * 1.15)
assert green > len(pixels) * 0.35, f"Green background not found: {green} pixels"
assert red > len(pixels) * 0.005, f"Red region of triangle not found: {red} pixels"
assert blue > len(pixels) * 0.005, f"Blue region of triangle not found: {blue} pixels"
print(f"Triangle on green background found: {green} green, {red} red, {blue} blue pixels")
