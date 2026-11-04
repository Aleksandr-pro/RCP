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
check_screen() {
  for attempt in 1 2 3 4 5; do
    adb shell screencap -p /sdcard/atlas.png
    adb pull /sdcard/atlas.png atlas-screen.png
    if python3 .github/scripts/check-screen.py atlas-screen.png "${1:-current}" > screen-check.log 2>&1; then
      cat screen-check.log
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
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -n "$package/android.app.NativeActivity"
for i in $(seq 1 30); do
  if adb logcat -d -s ATLAS:I '*:S' | grep -q 'Vulkan: кадр успешно отправлен на экран'; then break; fi
  sleep 2
done
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
grep -q 'ATLAS 0.4b: запуск C++ ядра' atlas-log.txt
grep -q 'Vulkan: кадр успешно отправлен на экран' atlas-log.txt
grep -q 'Vulkan: рендерер готов' atlas-log.txt
grep -q 'Vulkan: выбран GPU' atlas-log.txt
grep -q 'Шейдер загружен: shaders/triangle.vert.spv' atlas-log.txt
grep -q 'Шейдер загружен: shaders/triangle.frag.spv' atlas-log.txt
grep -q 'Vulkan: графический конвейер создан' atlas-log.txt
grep -q 'Vulkan: буфер вершин создан и заполнен' atlas-log.txt
check_screen portrait
adb shell input keyevent KEYCODE_HOME
sleep 3
adb shell am start -n "$package/android.app.NativeActivity"
sleep 5
check_screen
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
sleep 5
if ! check_screen landscape; then
  echo '::warning::Checking whether a fresh landscape launch changes the result'
  adb shell am force-stop "$package"
  adb shell am start -n "$package/android.app.NativeActivity"
  sleep 5
  if check_screen landscape-restart; then
    echo '::warning::Image check passes after a fresh landscape launch, but fails after rotation'
  else
    echo '::warning::Image check also fails after a fresh landscape launch'
  fi
  exit 1
fi
python3 .github/scripts/check-screen.py --compare
adb shell settings put system user_rotation 0
sleep 5
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
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
adb shell am start -n "$package/android.app.NativeActivity"
sleep 3
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
adb shell am force-stop "$package"
adb shell am start -n "$package/android.app.NativeActivity"
sleep 5
check_screen
adb shell am force-stop "$package"
