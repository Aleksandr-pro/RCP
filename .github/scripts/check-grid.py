"""Verify that the rendered area contains nine distinct cubes arranged in 3x3."""
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
assert len(components) == 9, f'Expected nine distinct cubes, found {len(components)}: {components}'
components.sort(key=lambda item: item[1])
for row in range(3):
    group = sorted(components[row * 3:row * 3 + 3])
    assert group[0][0] < group[1][0] < group[2][0], group
    if row:
        previous = components[(row - 1) * 3:row * 3]
        assert min(item[1] for item in group) > max(item[1] for item in previous), components
print(f'Nine separate cubes detected in a 3x3 grid: {components}')
