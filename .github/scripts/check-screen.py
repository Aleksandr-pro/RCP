import json
import sys
from pathlib import Path
from PIL import Image

if sys.argv[1] == "--compare":
    portrait = json.loads(Path("portrait.json").read_text())
    landscape = json.loads(Path(f"{sys.argv[2] if len(sys.argv) > 2 else 'landscape'}.json").read_text())
    assert portrait["screen_width"] < portrait["screen_height"], portrait
    assert landscape["screen_width"] > landscape["screen_height"], landscape
    difference = abs(portrait["ratio"] - landscape["ratio"]) / max(portrait["ratio"], landscape["ratio"])
    assert difference < 0.3, f"Figure proportions differ after rotation: {portrait}, {landscape}"
    print(f"Proportions match in both orientations (difference {difference:.1%})")
    sys.exit()

image = Image.open(sys.argv[1]).convert("RGB")
width, height = image.size
# Ignore system bars. The dark background is less bright than the cube faces.
points = []
colors = set()
for y in range(height // 10, height * 9 // 10):
    for x in range(width // 20, width * 19 // 20):
        r, g, b = image.getpixel((x, y))
        # Exclude the white/gray FPS overlay and system text, retaining colored cube faces.
        if max(r, g, b) - min(r, g, b) > 30 and (r > 140 or g > 150 or b > 150):
            points.append((x, y))
            colors.add((r // 32, g // 32, b // 32))
assert len(points) > width * height * 0.005, f"Colored figure not found: {len(points)} pixels"
assert len(colors) >= 2, f"Expected multiple face colors, got {colors}"
x0 = min(p[0] for p in points)
x1 = max(p[0] for p in points)
y0 = min(p[1] for p in points)
y1 = max(p[1] for p in points)
ratio = (x1 - x0 + 1) / (y1 - y0 + 1)
assert 0.4 < ratio < 2.5, f"Unexpected figure proportions: {ratio}, bounds={(x0, y0, x1, y1)}, image={(width, height)}, pixels={len(points)}, colors={colors}"
metrics = {"screen_width": width, "screen_height": height, "ratio": ratio}
Path(f"{sys.argv[2]}.json").write_text(json.dumps(metrics))
print(f"Colored figure found: {len(points)} pixels, width/height={ratio:.3f}")
