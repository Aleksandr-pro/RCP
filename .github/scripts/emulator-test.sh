#!/usr/bin/env bash
set -euo pipefail
package=com.atlas.engine
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb logcat -c
adb shell am start -n "$package/android.app.NativeActivity"
for i in $(seq 1 30); do
  if adb logcat -d -s ATLAS:I '*:S' | grep -q 'Диагностический кадр выведен'; then break; fi
  sleep 2
done
adb logcat -d -s ATLAS:I '*:S' > atlas-log.txt
grep -q 'ATLAS 0.2a: запуск C++ ядра' atlas-log.txt
grep -q 'Диагностический кадр выведен' atlas-log.txt
grep -q 'Начинаем проверку Vulkan' atlas-log.txt
grep -q 'Vulkan surface: начало проверки' atlas-log.txt
grep -q 'Vulkan surface: поверхность создана' atlas-log.txt
grep -q 'поддерживает графику и вывод на экран' atlas-log.txt
grep -q 'Vulkan surface: проверка завершена' atlas-log.txt
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
grep -q 'Приложение приостановлено' atlas-log.txt
[ "$(grep -c 'Приложение активно' atlas-log.txt)" -ge 2 ]
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
