#!/usr/bin/env bash
set -euo pipefail
trap 'status=$?; if [ "$status" -ne 0 ]; then echo "::error::Emulator test failed on line $LINENO (exit $status)"; if [ -f atlas-log.txt ]; then tail -n 30 atlas-log.txt | while IFS= read -r line; do echo "::notice::$line"; done; fi; fi' EXIT
package=com.atlas.engine
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -n "$package/android.app.NativeActivity"
for i in $(seq 1 30); do
  if adb logcat -d -s ATLAS:I '*:S' | grep -q 'Vulkan: кадр успешно отправлен на экран'; then break; fi
  sleep 2
done
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
grep -q 'ATLAS 0.3: запуск C++ ядра' atlas-log.txt
grep -q 'Vulkan: кадр успешно отправлен на экран' atlas-log.txt
grep -q 'Vulkan: рендерер готов' atlas-log.txt
grep -q 'Vulkan: выбран GPU' atlas-log.txt
adb shell input keyevent KEYCODE_HOME
sleep 3
adb shell am start -n "$package/android.app.NativeActivity"
sleep 5
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 1
sleep 5
adb shell settings put system user_rotation 0
sleep 5
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
grep -q 'приложение приостановлено' atlas-log.txt
[ "$(grep -c 'приложение активно' atlas-log.txt)" -ge 2 ]
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
adb shell screencap -p /sdcard/atlas.png
adb pull /sdcard/atlas.png atlas-screen.png
python3 .github/scripts/check-screen.py atlas-screen.png
# Verify that a subsequent APK signed with the same test key installs as an update.
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n "$package/android.app.NativeActivity"
sleep 3
[ "$(adb shell pidof "$package" | tr -d '\r' | wc -w)" -ge 1 ]
adb shell am force-stop "$package"
