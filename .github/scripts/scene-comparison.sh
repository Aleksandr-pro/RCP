#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p scene-comparison
package=com.atlas.engine
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb install -r reference-0.13/ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 8
adb exec-out screencap -p > scene-comparison/013-portrait.png
python3 .github/scripts/check-grid.py scene-comparison/013-portrait.png
adb shell settings put system user_rotation 1
sleep 8
adb exec-out screencap -p > scene-comparison/013-landscape.png
python3 .github/scripts/check-grid.py scene-comparison/013-landscape.png
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/atlas-014-normal.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 8
adb exec-out screencap -p > scene-comparison/014-landscape.png
python3 .github/scripts/check-grid.py scene-comparison/014-landscape.png
adb shell settings put system user_rotation 0
sleep 8
adb exec-out screencap -p > scene-comparison/014-portrait.png
python3 .github/scripts/check-grid.py scene-comparison/014-portrait.png
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/atlas-014-larger.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 8
adb exec-out screencap -p > scene-comparison/014-larger-portrait.png
python3 - <<'PYCODE'
from PIL import Image, ImageChops

for orientation in ('portrait', 'landscape'):
    before = Image.open(f'scene-comparison/013-{orientation}.png').convert('RGB')
    after = Image.open(f'scene-comparison/014-{orientation}.png').convert('RGB')
    assert before.size == after.size
    w, h = before.size
    side = min(w, h)
    # Compare the central scene, excluding the FPS popup and system bars.
    crop = (w // 2 - side * 7 // 20, h // 2 - side * 7 // 20,
            w // 2 + side * 7 // 20, h // 2 + side * 7 // 20)
    diff = ImageChops.difference(before.crop(crop), after.crop(crop))
    changed = sum(1 for pixel in diff.getdata() if max(pixel) > 20)
    fraction = changed / (diff.width * diff.height)
    print(f'{orientation}: {fraction:.3%} substantially different scene pixels')
    assert fraction < 0.005, f'Scene changed unexpectedly: {orientation}, {fraction:.3%}'

normal = Image.open('scene-comparison/014-portrait.png').convert('RGB')
larger = Image.open('scene-comparison/014-larger-portrait.png').convert('RGB')
assert normal.size == larger.size
w, h = normal.size
radius = min(w, h) * 11 // 100
crop = (w // 2 - radius, h // 2 - radius, w // 2 + radius, h // 2 + radius)
def colored(image):
    return sum(1 for r, g, b in image.crop(crop).getdata()
               if max(r, g, b) >= 100 and max(r, g, b) - min(r, g, b) >= 45)
normal_area = colored(normal)
larger_area = colored(larger)
print(f'Center cube colored pixels: normal={normal_area}, larger={larger_area}')
assert normal_area > 0 and larger_area > normal_area * 1.15, 'Changing scene scale did not visibly enlarge the center cube'
PYCODE

# The comparison build freezes automatic rotation and adds touch tracing only
# for this test. This isolates the gesture response from normal animation.
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/atlas-014-normal.apk"
adb shell settings put system user_rotation 0
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 6
adb exec-out screencap -p > scene-comparison/touch-portrait-before.png
adb shell input swipe 60 350 220 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-right.png
adb shell input swipe 220 350 60 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-left.png
adb shell settings put system user_rotation 1
sleep 5
adb exec-out screencap -p > scene-comparison/touch-landscape-before.png
adb shell input swipe 140 180 460 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-right.png
adb shell input swipe 460 180 140 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-left.png
# Leave a finger down while the app loses focus, then resume and swipe again.
adb shell input swipe 120 180 480 180 3000 &
swipe_pid=$!
sleep 1
adb shell input keyevent KEYCODE_HOME
wait "$swipe_pid"
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 5
adb shell input swipe 140 180 460 180 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' > scene-comparison/touch-log.txt
python3 .github/scripts/check-touch.py
