import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert("RGB")
pixels = list(image.getdata())
# Vulkan may output linear or sRGB values depending on the surface format.
green = sum(1 for r, g, b in pixels if g > 35 and g > r * 1.5 and g > b * 1.15 and b > r * 1.2 and g < 180)
assert green > len(pixels) * 0.45, f"Green Vulkan frame not found: {green} pixels"
print(f"Green diagnostic frame found: {green} pixels")
