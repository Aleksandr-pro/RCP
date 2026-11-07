#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p physics-diagnostics
package=com.atlas.engine
adb logcat -G 16M
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
# Verify the debug signing key and application ID support upgrading from 0.25e.
adb install -r reference-0.25e/ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 3
adb install -r "$RUNNER_TEMP/physics-normal.apk"
adb logcat -c
# As soon as the first simulation frame is logged, background the app while
# the cube is still falling. The reader starts before the activity.
(
  timeout 30s adb logcat -v brief -s ATLAS_PHYS_TEST:I '*:S' | while IFS= read -r line; do
    if [[ "$line" == *"frame y="* ]]; then
      adb shell input keyevent KEYCODE_HOME
      break
    fi
  done
) &
watcher=$!
adb shell am start -n "$package/.AtlasActivity"
wait "$watcher"
sleep 3
adb shell am start -n "$package/.AtlasActivity"
sleep 4
adb exec-out screencap -p > physics-diagnostics/normal.png
adb exec-out screencap -p > physics-diagnostics/resumed.png
adb logcat -d -s ATLAS:I ATLAS_PHYS_TEST:I ATLAS_PROJ_TEST:I '*:S' > physics-diagnostics/physics.log
python3 - <<'PYCODE'
from pathlib import Path
import re
text = Path('physics-diagnostics/physics.log').read_text()
assert 'Сцена загружена: scenes/physics.atlasscene, объектов: 2' in text
assert 'Vulkan: рендерер готов' in text
assert 'Текстура загружена: textures/ornament.rgba' in text
samples = [tuple(map(float, x)) for x in re.findall(r'frame y=([-0-9.]+) velocity=([-0-9.]+) rest=([-0-9.]+)', text)]
assert len(samples) > 15, len(samples)
rest = samples[0][2]
assert all(y >= rest - 1e-6 for y, v, r in samples)
assert any(v < -0.1 for y, v, r in samples), 'No fall recorded'
assert sum(abs(y-rest) < 1e-6 and abs(v) < 1e-6 for y, v, r in samples) > 10, 'No stable landing'
for a,b in zip(samples, samples[1:]):
    assert b[0] <= a[0] + 1e-5, (a,b)
projection = [tuple(map(float,x)) for x in re.findall(r'y=([-0-9.]+) screenY=([-0-9.]+)', '\n'.join(line for line in text.splitlines() if 'ATLAS_PROJ_TEST' in line))]
assert any(a[0]-b[0] > .15 and b[1] > a[1] for a in projection for b in projection if b[0] < a[0]), 'Falling Y must move down on screen'
focus = re.findall(r'(lost|gain) y=([-0-9.]+) velocity=([-0-9.]+)', text)
for first, second in zip(focus, focus[1:]):
    if first[0] == 'lost' and second[0] == 'gain':
        assert float(first[1]) > rest + 0.01 and float(first[2]) < 0, f'Pause did not occur during fall: {first}'
        assert abs(float(first[1])-float(second[1])) < 1e-6 and abs(float(first[2])-float(second[2])) < 1e-6
        break
else: raise AssertionError('No focus loss/resume pair')
print('Physics, landing, screen direction, focus suspension verified')
PYCODE
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/physics-alternate.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 4
adb exec-out screencap -p > physics-diagnostics/alternate.png
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/physics-reverse-light.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 4
adb exec-out screencap -p > physics-diagnostics/reverse-light.png
python3 - <<'PYCODE'
from PIL import Image, ImageChops
normal = Image.open('physics-diagnostics/normal.png').convert('RGB')
w, h = normal.size
box = (w//5, h//5, 4*w//5, 4*h//5)
for name in ('alternate', 'reverse-light'):
    img = Image.open(f'physics-diagnostics/{name}.png').convert('RGB')
    assert img.size == normal.size
    diff = ImageChops.difference(normal.crop(box), img.crop(box))
    count = sum(max(pixel) > 30 for pixel in diff.getdata())
    print(name, 'changed pixels', count)
    assert count > 150, name
PYCODE
