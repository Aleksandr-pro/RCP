#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::Scene comparison failed at line $LINENO: $BASH_COMMAND"' ERR
mkdir -p scene-comparison
package=com.atlas.engine
adb logcat -G 32M
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

wait_orientation() {
  local expected="$1" screenshot="scene-comparison/orientation-wait.png"
  for attempt in $(seq 1 12); do
    adb exec-out screencap -p > "$screenshot"
    if python3 - "$screenshot" "$expected" <<'PYORIENTATION'
import sys
from PIL import Image
w, h = Image.open(sys.argv[1]).size
assert (w < h) if sys.argv[2] == 'portrait' else (w > h), (w, h)
PYORIENTATION
    then return 0; fi
    sleep 2
  done
  echo "::error::Display did not reach $expected orientation"
  return 1
}

# The comparison build freezes automatic rotation and adds touch tracing only
# for this test. This isolates the gesture response from normal animation.
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/atlas-014-normal.apk"
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 6
wait_orientation portrait
adb exec-out screencap -p > scene-comparison/touch-portrait-before.png
adb shell input swipe 60 350 220 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-right.png
adb shell input swipe 220 350 60 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-left.png
adb shell input swipe 160 220 160 420 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-down.png
adb shell input swipe 160 420 160 220 700
sleep 2
adb shell input swipe 60 230 210 410 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-diagonal.png
adb shell input swipe 210 410 60 230 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' > scene-comparison/touch-log.txt
adb logcat -c
# Start a fresh landscape activity, so this gesture check is independent of
# the preceding portrait swipe. Rotation during an active app is tested above.
adb shell am force-stop "$package"
adb shell input keyevent KEYCODE_HOME
adb shell settings put system user_rotation 1
sleep 3
adb shell am start -n "$package/.AtlasActivity"
sleep 8
wait_orientation landscape
adb exec-out screencap -p > scene-comparison/touch-landscape-before.png
adb shell input swipe 140 180 460 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-right.png
adb shell input swipe 460 180 140 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-left.png
adb shell input swipe 320 80 320 240 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-down.png
adb shell input swipe 320 240 320 80 700
sleep 2
adb shell input swipe 160 80 440 240 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-diagonal.png
adb shell input swipe 440 240 160 80 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' >> scene-comparison/touch-log.txt
adb logcat -c
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
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' >> scene-comparison/touch-log.txt
if ! python3 .github/scripts/check-touch.py > scene-comparison/touch-check.txt 2>&1; then
  detail=$(cat scene-comparison/touch-check.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
  echo "::error title=Touch test::$detail"
  grep 'ATLAS_TOUCH_TEST' scene-comparison/touch-log.txt | tail -n 25 | while IFS= read -r line; do echo "::notice::$line"; done
  exit 1
fi
cat scene-comparison/touch-check.txt
