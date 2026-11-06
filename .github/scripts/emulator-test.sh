#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::Test failed at line $LINENO: $BASH_COMMAND"' ERR
report_failure() {
  status=$?
  if [ "$status" -ne 0 ]; then
    echo "::error::Emulator test failed (exit $status)"
    adb logcat -d -v brief -t 3000 > atlas-system-log.txt || true
    adb logcat -d -s ATLAS:I "*:S" > atlas-log.txt || true
    if [ -f atlas-log.txt ]; then
      summary=$(tail -n 35 atlas-log.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
      echo "::notice title=ATLAS log::$summary"
    fi
    { grep -iE 'fatal|crash|signal|atlas|vulkan|shader|error|exception|tombstone' atlas-system-log.txt || true; } | tail -n 55 | while IFS= read -r line; do echo "::notice::$line"; done
    echo "::notice::App PID: $(adb shell pidof com.atlas.engine || true)"
  fi
}
trap report_failure EXIT
package=com.atlas.engine
check_fps_overlay() {
  local label="$1"
  adb exec-out screencap -p > "fps-${label}.png"
  adb logcat -d -s ATLAS:I ATLAS_TEST_UI:I ATLAS_TEST_STATS:I '*:S' > "fps-${label}-log.txt"
  if ! python3 .github/scripts/check-fps-overlay.py "fps-${label}.png" "fps-${label}-log.txt" > fps-check-error.txt 2>&1; then
    detail=$(cat fps-check-error.txt | tail -c 8000 | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
    echo "::error title=FPS overlay check::$detail"
    return 1
  fi
  cat fps-check-error.txt
}
check_screen() {
  for attempt in 1 2 3 4 5; do
    adb shell screencap -p /sdcard/atlas.png
    adb pull /sdcard/atlas.png atlas-screen.png
    if python3 .github/scripts/check-screen.py atlas-screen.png "${1:-current}" > screen-check.log 2>&1; then
      cat screen-check.log
      if [ "${1:-current}" != current ]; then cp atlas-screen.png "$1.png"; fi
      return 0
    fi
    sleep 2
  done
  python3 - <<'PYIMAGE'
from PIL import Image
import base64, io
image = Image.open('atlas-screen.png').convert('RGB')
image.thumbnail((320, 320))
buffer = io.BytesIO()
image.save(buffer, format='JPEG', quality=30, optimize=True)
encoded = base64.b64encode(buffer.getvalue()).decode()
for i in range(0, len(encoded), 1500):
    print(f'::notice title=screen-part-{i // 1500}::{encoded[i:i+1500]}')
PYIMAGE
  detail=$(tail -n 5 screen-check.log | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
  echo "::error title=Image check::$detail"
  return 1
}
# Install the previous release first to verify a real in-place upgrade.
adb install -r reference-0.12/ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 4
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
for i in $(seq 1 30); do
  if adb logcat -d -s ATLAS_TEST:I '*:S' | grep -q 'update angle='; then break; fi
  sleep 2
done
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
grep -q 'ATLAS 0.4b: запуск C++ ядра' atlas-log.txt
adb logcat -d -s ATLAS_TEST:I '*:S' | grep -q 'update angle='
grep -q 'Vulkan: рендерер готов' atlas-log.txt
grep -q 'Vulkan: выбран GPU' atlas-log.txt
grep -q 'Шейдер загружен: shaders/triangle.vert.spv' atlas-log.txt
grep -q 'Шейдер загружен: shaders/triangle.frag.spv' atlas-log.txt
grep -q 'Vulkan: графический конвейер создан' atlas-log.txt
grep -q 'Vulkan: буфер вершин создан и заполнен' atlas-log.txt
grep -q 'Vulkan: буфер глубины создан, формат' atlas-log.txt
grep -q 'Vulkan: currentTransform=' atlas-log.txt
check_screen portrait
sleep 4
adb logcat -d -v epoch -s ATLAS_TEST:I '*:S' > atlas-frame-rate.txt
python3 - <<'PYRATE'
import re
from pathlib import Path
times = []
for line in Path('atlas-frame-rate.txt').read_text().splitlines():
    if 'update angle=' in line:
        match = re.match(r'\s*(\d+\.\d+)', line)
        if match:
            times.append(float(match.group(1)))
assert len(times) >= 5, f'Too few animation updates recorded: {len(times)}'
times = [t for t in times if t >= times[-1] - 3]
assert len(times) >= 5, f'Too few recent animation updates: {len(times)}'
intervals = [b - a for a, b in zip(times, times[1:])]
print(f'Animation updates: {len(times)} over {times[-1] - times[0]:.3f}s; minimum interval {min(intervals):.3f}s')
assert (times[-1] - times[0]) / len(intervals) >= 0.031, 'Animation updates exceed the expected average frame limit'
PYRATE
adb exec-out screencap -p > motion-before.png
sleep 2
adb exec-out screencap -p > motion-after.png
python3 - <<'PYMOTION'
from PIL import Image, ImageChops
before = Image.open('motion-before.png').convert('RGB')
after = Image.open('motion-after.png').convert('RGB')
assert before.size == after.size
w, h = before.size
crop = (w // 20, h // 10, w * 19 // 20, h * 9 // 10)
diff = ImageChops.difference(before.crop(crop), after.crop(crop))
fraction = sum(1 for p in diff.getdata() if max(p) > 20) / (diff.width * diff.height)
print(f'Animation: {fraction:.2%} of rendered-area pixels changed')
assert fraction > 0.005, 'The cube did not visibly move'
PYMOTION
check_fps_overlay portrait
adb shell input keyevent KEYCODE_HOME
sleep 3
# Preserve lifecycle and error logs before clearing the test trace.
adb logcat -d -s ATLAS:I '*:S' > atlas-pre-background-log.txt
grep 'ATLAS stats: FPS=' atlas-pre-background-log.txt > atlas-stats.txt
# The test-only draw trace is emitted at drawFrame entry, not in production APKs.
adb logcat -c
sleep 4
adb logcat -d -s ATLAS_TEST:I '*:S' > atlas-frame-test.txt
adb logcat -d -s ATLAS:I '*:S' > atlas-background-log.txt
if grep -q 'ATLAS stats: FPS=' atlas-background-log.txt; then
  echo '::error::FPS statistics continued in the background'
  exit 1
fi
if grep -q 'draw\|update angle=' atlas-frame-test.txt; then
  cat atlas-frame-test.txt
  echo '::error::Rendering continued while in the background'
  exit 1
fi
adb shell am start -n "$package/.AtlasActivity"
sleep 7
check_screen
adb logcat -d -s ATLAS:I '*:S' > atlas-resumed-log.txt
grep 'ATLAS stats: FPS=' atlas-resumed-log.txt >> atlas-stats.txt
adb logcat -d -s ATLAS_TEST:I '*:S' > atlas-resumed-frames.txt
grep -q 'update angle=' atlas-resumed-frames.txt
check_fps_overlay resumed
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
sleep 5
if ! check_screen landscape; then
  echo '::warning::Checking whether a fresh landscape launch changes the result'
  adb shell am force-stop "$package"
  adb shell am start -n "$package/.AtlasActivity"
  sleep 5
  if check_screen landscape-restart; then
    echo '::warning::Image check passes after a fresh landscape launch, but fails after rotation'
  else
    echo '::warning::Image check also fails after a fresh landscape launch'
  fi
  exit 1
fi
# Allow the statistics window to complete after a possible focus reset.
sleep 3
check_fps_overlay landscape
adb shell settings put system user_rotation 3
sleep 5
check_screen landscape-reverse
# Exercise several consecutive orientation changes and verify each resulting frame.
for rotation in 0 1 0 3 0; do
  adb shell settings put system user_rotation "$rotation"
  sleep 4
  check_screen
done
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
cat atlas-pre-background-log.txt atlas-log.txt > atlas-combined-log.txt
mv atlas-combined-log.txt atlas-log.txt
if grep -E 'swapchain неоптимален при получении изображения|кадр выведен, но swapchain неоптимален' atlas-log.txt; then
  echo '::error::Repeated SUBOPTIMAL messages found'
  exit 1
fi
python3 - <<'PYSTATS'
import re
from pathlib import Path
lines = Path('atlas-stats.txt').read_text().splitlines()
assert len(lines) >= 2, 'Expected FPS stats before and after resuming'
for line in lines:
    match = re.search(r'FPS=([0-9.]+), средний интервал=([0-9.]+) мс, макс\. интервал=([0-9.]+) мс, свыше 50 мс=(\d+)/(\d+), работа кадра: средняя=([0-9.]+) мс, макс\.=([0-9.]+) мс', line)
    assert match, f'Malformed statistics: {line}'
    fps, interval, maximum, slow, measured, work, max_work = match.groups()
    fps, interval, maximum, work, max_work = map(float, (fps, interval, maximum, work, max_work))
    slow, measured = int(slow), int(measured)
    assert fps > 0 and interval > 0 and measured > 0, line
    assert 0 <= slow <= measured and maximum > 0 and 0 <= work <= max_work, line
    assert abs(fps * interval - 1000) < 15, line
    print(line)
first = re.search(r'макс\. интервал=([0-9.]+) мс, свыше 50 мс=(\d+)/(\d+), работа кадра: средняя=([0-9.]+) мс, макс\.=([0-9.]+) мс', lines[0])
assert float(first[1]) > 50 and int(first[2]) >= 1 and float(first[5]) >= 70, 'Injected 80 ms delay was not recorded'
PYSTATS
if grep -E 'vkCreateBuffer: ошибка|vkAllocateMemory для вершин: ошибка|vkMapMemory для вершин: ошибка' atlas-log.txt; then
  echo 'Vertex buffer Vulkan error found' >&2
  exit 1
fi
grep -q 'приложение приостановлено' atlas-log.txt
[ "$(grep -c 'приложение активно' atlas-log.txt)" -ge 2 ]
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
check_screen
# Verify that a subsequent APK signed with the same test key installs as an update.
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/.AtlasActivity"
sleep 3
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
adb shell am force-stop "$package"
adb shell am start -n "$package/.AtlasActivity"
sleep 5
check_screen
adb shell am force-stop "$package"
# Check all engine error-level logs, including Vulkan failures.
adb logcat -d -s ATLAS:E '*:S' > atlas-errors.txt
if grep -E '(^|[[:space:]])E[[:space:]]+ATLAS[[:space:]]*:' atlas-pre-background-log.txt >> atlas-errors.txt; then :; fi
if grep -Eq '(^|[[:space:]])E[[:space:]]+ATLAS[[:space:]]*:' atlas-errors.txt; then
  summary=$(tail -n 30 atlas-errors.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
  echo "::error title=ATLAS error log::$summary"
  echo '::error::ATLAS reported error-level messages during the emulator test'
  exit 1
fi
