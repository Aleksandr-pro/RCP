"""Convert the original JPEG to tightly packed, top-to-bottom RGBA8 (Pillow required)."""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[2] / 'ATLAS/app/src/main'
image = Image.open(root / 'texture-source/ornament.jpeg').convert('RGB').convert('RGBA')
width, height = image.size
# These are the corresponding constants in TextureLoader.h.
if (width, height) != (1024, 559):
    raise ValueError(f'Update TextureLoader.h dimensions before using {width}x{height}')
pixels = image.tobytes()
assert len(pixels) == width * height * 4 and set(pixels[3::4]) == {255}
(root / 'assets/textures/ornament.rgba').write_bytes(pixels)
print(f'{width}x{height}, {len(pixels)} bytes, RGBA8, top row first')
