#!/usr/bin/env bash
set -Eeuo pipefail
trap 'status=$?; if [ "$status" -ne 0 ]; then echo "::error::Physics emulator test failed at line $LINENO: $BASH_COMMAND"; if [ -f physics-diagnostics/physics.log ]; then tail -n 40 physics-diagnostics/physics.log | while IFS= read -r line; do echo "::notice::$line"; done; fi; fi' EXIT
mkdir -p physics-diagnostics
package=com.atlas.engine
adb logcat -G 16M
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
# Verify the debug signing key and application ID support upgrading from 0.26c.
adb install -r reference-0.26c/ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 3
adb install -r "$RUNNER_TEMP/physics-normal.apk"
adb logcat -c
# As soon as the first simulation frame is logged, background the app while
# the cube is still falling. The reader starts before the activity.
(
  # logcat can remain blocked after the reader exits; its timeout is expected.
  set +o pipefail
  timeout 15s stdbuf -o0 adb logcat -v brief -s ATLAS_PHYS_TEST:I '*:S' | while IFS= read -r line; do
    if [[ "$line" == *"frame y="* && "$line" == *"velocity=-"* ]]; then
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
adb logcat -d -s ATLAS:I ATLAS_PHYS_TEST:I ATLAS_BOUNCE_TEST:I ATLAS_PROJ_TEST:I '*:S' > physics-diagnostics/physics.log
python3 - <<'PYCODE' 2> physics-diagnostics/analysis-error.txt || { cat physics-diagnostics/analysis-error.txt; echo "::error::$(tail -n 3 physics-diagnostics/analysis-error.txt | tr '\n' ' ')"; exit 1; }
from pathlib import Path
import re
text = Path('physics-diagnostics/physics.log').read_text()
assert 'Сцена загружена: scenes/physics.atlasscene, объектов: 4' in text
assert 'Vulkan: рендерер готов' in text
assert 'Текстура загружена: textures/ornament.rgba' in text
samples = [tuple(map(float, x)) for x in re.findall(r'frame y=([-0-9.]+) velocity=([-0-9.]+) rest=([-0-9.]+)', text)]
assert len(samples) > 15, len(samples)
rest = samples[0][2]
assert all(y >= rest - 1e-6 for y, v, r in samples)
for index, target in enumerate((-0.6, 0.0, -0.6)):
    states = [tuple(map(float, v)) for v in re.findall(rf'box={index} y=([-0-9.]+) velocity=([-0-9.]+)', text)]
    assert len(states) > 15 and any(v < -0.1 for y, v in states), f'Box {index} did not fall'
    assert sum(abs(y-target) < 1e-5 and abs(v) < 1e-5 for y,v in states) > 10, f'Box {index} did not settle'
assert any(v < -0.1 for y, v, r in samples), 'No fall recorded'
assert sum(abs(y-rest) < 1e-6 and abs(v) < 1e-6 for y, v, r in samples) > 10, 'No stable landing'
bounce = [tuple(map(float, v)) for v in re.findall(r'ATLAS_BOUNCE_TEST: y=([-0-9.]+) velocity=([-0-9.]+)', text)]
assert len(bounce) > 100
assert all(y >= rest - 1e-6 for y,v in bounce), 'Bounce crossed the platform'
impacts = [b[1] for a,b in zip(bounce,bounce[1:]) if a[1] < 0 and b[1] > 0 and abs(b[0]-rest) < 1e-5]
assert len(impacts) >= 2 and all(b < a for a,b in zip(impacts,impacts[1:])), f'No diminishing rebounds: {impacts}'
assert sum(abs(y-rest) < 1e-6 and abs(v) < 1e-6 for y,v in bounce) > 30, 'Bounce did not settle'
print('Bounce impact speeds:', impacts)
projection = [tuple(map(float,x)) for x in re.findall(r'y=([-0-9.]+) screenY=([-0-9.]+)', '\n'.join(line for line in text.splitlines() if 'ATLAS_PROJ_TEST' in line))]
assert any(a[0]-b[0] > .15 and b[1] > a[1] for a in projection for b in projection if b[0] < a[0]), 'Falling Y must move down on screen'
focus = re.findall(r'(lost|gain) y=([-0-9.]+) velocity=([-0-9.]+)', text)
for first, second in zip(focus, focus[1:]):
    if first[0] == 'lost' and second[0] == 'gain':
        assert float(first[1]) > rest + 0.01 and float(first[2]) < 0, f'Pause did not occur during fall: {first}'
        assert abs(float(first[1])-float(second[1])) < 1e-6 and abs(float(first[2])-float(second[2])) < 1e-6
        gain_marker = f'gain y={second[1]} velocity={second[2]}'
        after_gain = text.split(gain_marker, 1)[1]
        resumed = re.search(r'frame y=([-0-9.]+) velocity=([-0-9.]+)', after_gain)
        assert resumed, 'No frame after focus gain'
        assert abs(float(resumed[1])-float(second[1])) < 1e-6, 'Simulation jumped on first resumed frame'
        assert abs(float(resumed[2])-float(second[2])) < 1e-6, 'Velocity changed on first resumed frame'
        break
else: raise AssertionError('No focus loss/resume pair')
print('Physics, landing, screen direction, focus suspension verified')
PYCODE
# Separate test-only builds use the same scene and initial height, changing only the pair.
for pair in sand wood; do
  adb shell am force-stop "$package"
  adb install -r "$RUNNER_TEMP/physics-$pair.apk"
  adb logcat -c
  adb shell am start -n "$package/.AtlasActivity"
  sleep 6
  adb logcat -d -s ATLAS:I ATLAS_BOUNCE_TEST:I '*:S' > "physics-diagnostics/material-$pair.log"
done
python3 - <<'PYMATERIAL' 2> physics-diagnostics/material-error.txt || { cat physics-diagnostics/material-error.txt; echo "::error::$(tail -n 4 physics-diagnostics/material-error.txt | tr '\n' ' ')"; exit 1; }
from pathlib import Path
import re

def first_peak(path):
    text = Path(path).read_text()
    data = [tuple(map(float,v)) for v in re.findall(r'ATLAS_BOUNCE_TEST: y=([-0-9.]+) velocity=([-0-9.]+)', text)]
    assert len(data) > 60, f'No simulation trace in {path}'
    assert abs(data[0][0] - .4) < .05, f'Unexpected initial height in {path}: {data[0]}'
    floor = -.6
    impact = next((i for i in range(1,len(data)) if data[i-1][1] < 0 and data[i][1] > 0 and abs(data[i][0]-floor)<1e-5), None)
    assert impact is not None, f'No bounce in {path}'
    end = next((i for i in range(impact+1,len(data)) if data[i][1] <= 0), len(data))
    peak = max(y for y,v in data[impact:end]) - floor
    assert all(y >= floor-1e-5 for y,v in data)
    assert sum(abs(y-floor)<1e-5 and abs(v)<1e-5 for y,v in data)>10, f'No rest in {path}'
    return peak
stone=first_peak('physics-diagnostics/physics.log')
sand=first_peak('physics-diagnostics/material-sand.log')
wood=first_peak('physics-diagnostics/material-wood.log')
print(f'First rebound: rubber/stone={stone}, rubber/sand={sand}, wood/stone={wood}')
assert stone > sand and stone > wood
PYMATERIAL

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
