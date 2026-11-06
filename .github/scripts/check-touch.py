import math
import re
from pathlib import Path
from PIL import Image, ImageChops

log = Path('scene-comparison/touch-log.txt').read_text()
pattern = r'move dx=([-0-9.]+) width=(\d+) before=([-0-9.]+) after=([-0-9.]+)'
events = [(float(dx), int(width), float(before), float(after))
          for dx, width, before, after in re.findall(pattern, log)]
assert events, 'No touch movement was received'
print('Captured widths:', sorted(set(w for _, w, _, _ in events)))
for name in ('portrait', 'landscape'):
    print(name, 'screenshot size:', Image.open(f'scene-comparison/touch-{name}-before.png').size)
for dx, width, before, after in events:
    assert width > 0
    expected = math.fmod(before + dx / width * 6.2831853, 6.2831853)
    assert abs(expected - after) < 0.001, (dx, width, before, after, expected)
for width in (320, 640):
    movements = [dx for dx, w, _, _ in events if w == width]
    assert any(dx > 0 for dx in movements), f'No right swipe at width {width}'
    assert any(dx < 0 for dx in movements), f'No left swipe at width {width}'
assert ('lost focus active=1' in log or 'end action=3 active=1' in log), 'Neither focus loss nor Android cancellation occurred during an active touch'
vertical_pattern = r'move2 dx=([-0-9.]+) dy=([-0-9.]+) width=(\d+) height=(\d+) before=([-0-9.]+) after=([-0-9.]+)'
vertical = [(float(dx), float(dy), int(w), int(h), float(before), float(after))
            for dx, dy, w, h, before, after in re.findall(vertical_pattern, log)]
assert vertical, 'No vertical movement events received'
for dx, dy, width, height, before, after in vertical:
    assert height > 0
    expected = math.fmod(before + dy / height * 3.14159265, 6.2831853)
    assert abs(expected - after) < 0.001, (dx, dy, width, height, before, after, expected)
for width in (320, 640):
    movements = [(dx, dy) for dx, dy, w, _, _, _ in vertical if w == width]
    assert any(dy > 0 for dx, dy in movements), f'No down swipe at width {width}'
    assert any(dy < 0 for dx, dy in movements), f'No up swipe at width {width}'
    assert any(abs(dx) > 0.1 and abs(dy) > 0.1 for dx, dy in movements), f'No diagonal gesture at width {width}'
assert 'focus reset active=0 id=-1' in log, 'Active touch was not reset on focus loss'
after_focus = log.rsplit('focus reset active=0 id=-1', 1)[1]
assert re.search(pattern, after_focus), 'No touch movement after resuming'

for orientation in ('portrait', 'landscape'):
    before = Image.open(f'scene-comparison/touch-{orientation}-before.png').convert('RGB')
    right = Image.open(f'scene-comparison/touch-{orientation}-right.png').convert('RGB')
    left = Image.open(f'scene-comparison/touch-{orientation}-left.png').convert('RGB')
    assert before.size == right.size == left.size
    w, h = before.size
    if orientation == 'portrait':
        assert w < h, f'Portrait screenshot is not portrait: {w}x{h}'
    else:
        assert w > h, f'Landscape screenshot is not landscape: {w}x{h}'
    side = min(w, h)
    crop = (w // 2 - side * 7 // 20, h // 2 - side * 7 // 20,
            w // 2 + side * 7 // 20, h // 2 + side * 7 // 20)
    def changed(other):
        diff = ImageChops.difference(before.crop(crop), other.crop(crop))
        return sum(1 for pixel in diff.getdata() if max(pixel) > 20) / (diff.width * diff.height)
    right_diff, left_diff = changed(right), changed(left)
    print(f'{orientation}: right swipe difference={right_diff:.2%}, opposite swipe remaining difference={left_diff:.2%}')
    assert right_diff > 0.005, f'Right swipe did not visibly rotate the scene: {orientation}'
    assert left_diff < 0.02, f'Opposite swipe did not restore the angle: {orientation}'
    for direction in ('down', 'diagonal'):
        image = Image.open(f'scene-comparison/touch-{orientation}-{direction}.png').convert('RGB')
        difference = changed(image)
        print(f'{orientation}: {direction} swipe difference={difference:.2%}')
        assert difference > 0.005, f'{direction} swipe did not visibly rotate the scene: {orientation}'
print(f'Touch test: {len(events)} move events; focus reset and resumed gesture passed')
