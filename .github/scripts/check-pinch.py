import re
from pathlib import Path
from PIL import Image

pattern = re.compile(r'pinch old=([0-9.]+) new=([0-9.]+) oldgap=([0-9.]+) gap=([0-9.]+) angle=([-0-9.]+) tilt=([-0-9.]+)')
for orientation in ('portrait', 'landscape'):
    log = Path(f'scene-comparison/pinch-{orientation}-log.txt').read_text()
    samples = [tuple(map(float, m)) for m in pattern.findall(log)]
    assert samples, f'No two-pointer zoom events in {orientation}'
    for old, new, oldgap, gap, angle, tilt in samples:
        expected = max(2.5, min(10.0, old * oldgap / gap))
        assert abs(expected - new) < 0.001, (old, new, oldgap, gap)
        assert 2.5 <= new <= 10.0
    assert any(new < old - 0.001 for old, new, *_ in samples), f'No zoom in: {orientation}'
    assert any(new > old + 0.001 for old, new, *_ in samples), f'No zoom out: {orientation}'
    assert any(abs(new - 2.5) < 0.001 for _, new, *_ in samples), f'Near limit not reached: {orientation}'
    assert any(abs(new - 10.0) < 0.001 for _, new, *_ in samples), f'Far limit not reached: {orientation}'
    event_pattern = re.compile(r'event=(\d+) count=(\d+) touch=(\d+) pinch=(\d+) angle=([-0-9.]+) tilt=([-0-9.]+) camera=([0-9.]+)')
    events = [tuple(map(float, m)) for m in event_pattern.findall(log)]
    for i, event in enumerate(events):
        action, count, touch, pinch, angle, tilt, camera = event
        if action == 6 and count == 2 and pinch == 1:
            tail = events[i + 1:]
            later = next((e for e in tail if e[0] == 2 and e[1] == 1), None)
            assert later is not None, 'No remaining-finger move observed'
            assert later[2] == 0 and later[3] == 0, f'Gesture was not deactivated: {later}'
            assert abs(later[4] - angle) < 1e-5 and abs(later[5] - tilt) < 1e-5, 'View jumped after lifting a finger'
            break
    else:
        raise AssertionError('No active pinch ended by lifting a finger')
    if orientation == 'landscape':
        assert ('lost focus pinch=1' in log or re.search(r'event=3 count=\d+ touch=0 pinch=1', log)), 'No active pinch was cancelled by backgrounding'
        assert 'focus reset pinch=0 id1=-1 id2=-1 gap=0.00' in log, 'Pinch state not reset on focus loss'
        after = log.rsplit('focus reset pinch=0 id1=-1 id2=-1 gap=0.00', 1)[1]
        assert pattern.search(after), 'Pinch did not work after return from background'
    print(f'{orientation}: {len(samples)} pinch updates, limits and transition verified')

near = Image.open('scene-comparison/pinch-near.png').convert('RGB')
far = Image.open('scene-comparison/pinch-far.png').convert('RGB')
assert near.size == far.size
w, h = near.size
side = min(w, h)
crop = (w // 2 - side * 7 // 20, h // 2 - side * 7 // 20,
        w // 2 + side * 7 // 20, h // 2 + side * 7 // 20)
def color_count(image):
    return sum(1 for r, g, b in image.crop(crop).getdata()
               if max(r, g, b) >= 100 and max(r, g, b) - min(r, g, b) >= 45)
a, b = color_count(near), color_count(far)
print(f'Visible colored scene pixels near={a}, far={b}')
assert b > 0 and a > b * 2, 'Pinch distance did not visibly change scene size'
