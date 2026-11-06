"""Verify multiple visible scene regions even when projected cubes overlap."""
import sys
from PIL import Image

image = Image.open(sys.argv[1]).convert('RGB')
w, h = image.size
pixels = image.load()
mask = set()
# Bright, saturated face colors; the green background and gray FPS overlay
# are excluded. Restrict the search to the central rendered area.
for y in range(h // 8, 7 * h // 8):
    for x in range(w // 10, 9 * w // 10):
        r, g, b = pixels[x, y]
        if max(r, g, b) >= 100 and max(r, g, b) - min(r, g, b) >= 45:
            mask.add((x, y))

components = []
while mask:
    start = mask.pop()
    todo = [start]
    points = [start]
    while todo:
        x, y = todo.pop()
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not (dx or dy):
                    continue
                point = (x + dx, y + dy)
                if point in mask:
                    mask.remove(point)
                    todo.append(point)
                    points.append(point)
    if len(points) >= 30:
        components.append((sum(p[0] for p in points) / len(points),
                           sum(p[1] for p in points) / len(points), len(points)))
# With an oblique scene view some cubes can touch or overlap in projection;
# nine draws are independently verified by test-only command tracing.
assert len(components) >= 3, f'Expected a multi-object scene, got {len(components)} regions: {components}'
y_min = min(y for _, y, _ in components)
y_max = max(y for _, y, _ in components)
assert y_max - y_min > h * 0.05, f'Scene is vertically collapsed: {components}'
print(f'Colored scene regions detected: {components}')
