#!/usr/bin/env bash
set -Eeuo pipefail
trap 'status=$?; if [ "$status" -ne 0 ]; then echo "::error::Physics emulator test failed at line $LINENO: $BASH_COMMAND"; if [ -f physics-diagnostics/physics.log ]; then tail -n 40 physics-diagnostics/physics.log | while IFS= read -r line; do echo "::notice::$line"; done; fi; fi' EXIT
mkdir -p physics-diagnostics
package=com.atlas.engine
adb logcat -G 16M
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
# Verify the debug signing key and application ID support upgrading from 0.26b.
adb install -r reference-0.26b/ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 3
adb install -r "$RUNNER_TEMP/physics-normal.apk"
adb logcat -c
# The test-only build briefly holds each phase so HOME can be delivered reliably.
pause_on_marker() {
  local tag="$1" marker="$2"
  (
    set +o pipefail
    timeout 15s stdbuf -o0 adb logcat -T 1 -v brief -s "$tag:I" '*:S' | while IFS= read -r line; do
      if [[ "$line" == *"$marker"* ]]; then
        adb shell input keyevent KEYCODE_HOME
        break
      fi
    done
  ) &
  watcher=$!
  adb shell am start -n "$package/.AtlasActivity"
  wait "$watcher"
  sleep 2
}
pause_on_marker ATLAS_PHYS_TEST 'velocity=-'
pause_on_marker ATLAS_JUMP_TEST 'phase=waiting'
pause_on_marker ATLAS_JUMP_TEST 'phase=jumping'
adb shell am start -n "$package/.AtlasActivity"
sleep 5
adb exec-out screencap -p > physics-diagnostics/normal.png
adb exec-out screencap -p > physics-diagnostics/resumed.png
adb logcat -d -s ATLAS:I ATLAS_PHYS_TEST:I ATLAS_JUMP_TEST:I ATLAS_PROJ_TEST:I '*:S' > physics-diagnostics/physics.log
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
for a,b in zip(samples, samples[1:]):
    assert b[0] <= a[0] + 1e-5, (a,b)
projection = [tuple(map(float,x)) for x in re.findall(r'y=([-0-9.]+) screenY=([-0-9.]+)', '\n'.join(line for line in text.splitlines() if 'ATLAS_PROJ_TEST' in line))]
assert any(a[0]-b[0] > .15 and b[1] > a[1] for a in projection for b in projection if b[0] < a[0]), 'Falling Y must move down on screen'
focus = re.findall(r'ATLAS_PHYS_TEST: (lost|gain) y=([-0-9.]+) velocity=([-0-9.]+)', text)
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
jump_frames = [tuple(map(float, x[:3])) + (int(x[3]),) for x in re.findall(r'ATLAS_JUMP_TEST: frame y=([-0-9.]+) velocity=([-0-9.]+) timer=([-0-9.]+) jumped=(\d+)', text)]
assert any(j == 0 and abs(y + .6) < 1e-5 and .2 < timer < 1 for y, v, timer, j in jump_frames), 'No grounded wait'
assert any(j and v > 1 for y, v, timer, j in jump_frames), 'No upward jump'
assert any(j and v < -1 for y, v, timer, j in jump_frames), 'No descent'
assert sum(j and abs(y + .6) < 1e-5 and abs(v) < 1e-5 for y, v, timer, j in jump_frames) > 10, 'No final landing'
assert sum(a[3] == 0 and b[3] == 1 for a,b in zip(jump_frames, jump_frames[1:])) == 1, 'Jump was not one-shot'
focus_jump = re.findall(r'ATLAS_JUMP_TEST: (lost|gain) y=([-0-9.]+) velocity=([-0-9.]+) timer=([-0-9.]+) jumped=(\d+)', text)
pairs = [(a,b) for a,b in zip(focus_jump,focus_jump[1:]) if a[0]=='lost' and b[0]=='gain']
assert len(pairs) >= 3, pairs
for a,b in pairs:
    assert a[1:] == b[1:], (a,b)
assert any(float(a[3]) > .2 and a[4]=='0' for a,b in pairs), 'No pause during waiting'
assert any(float(a[2]) > 0 and a[4]=='1' for a,b in pairs), 'No pause during ascent'
print('Physics, one-shot jump, stack, screen direction and pauses verified')
PYCODE
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/physics-alternate.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 9
adb exec-out screencap -p > physics-diagnostics/alternate.png
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/physics-reverse-light.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 9
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
