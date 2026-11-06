#!/usr/bin/env bash
set -euo pipefail
package=com.atlas.engine
mkdir -p orientation-diagnostics
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb shell am force-stop "$package"
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 10
adb shell screencap -p /sdcard/atlas-orientation.png
adb pull /sdcard/atlas-orientation.png orientation-diagnostics/portrait.png
adb shell settings put system user_rotation 1
sleep 10
adb shell screencap -p /sdcard/atlas-orientation.png
adb pull /sdcard/atlas-orientation.png orientation-diagnostics/landscape-after-rotation.png
adb shell am force-stop "$package"
adb shell am start -n "$package/.AtlasActivity"
sleep 10
adb shell screencap -p /sdcard/atlas-orientation.png
adb pull /sdcard/atlas-orientation.png orientation-diagnostics/landscape-fresh.png
adb shell settings put system user_rotation 0
sleep 10
adb shell screencap -p /sdcard/atlas-orientation.png
adb pull /sdcard/atlas-orientation.png orientation-diagnostics/portrait-after-rotation.png
adb logcat -d -s ATLAS:I '*:S' > orientation-diagnostics/atlas-log.txt
grep -E 'native window=|currentTransform=|aspect буфера|swapchain |выбран GPU' orientation-diagnostics/atlas-log.txt > orientation-diagnostics/summary.txt
summary=$(tail -n 60 orientation-diagnostics/summary.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
echo "::notice title=orientation-log::$summary"
python3 - <<'PY'
from PIL import Image
import base64, io
for label, filename in [('portrait', 'portrait.png'), ('landscape', 'landscape-after-rotation.png')]:
    image = Image.open('orientation-diagnostics/' + filename).convert('RGB')
    image.thumbnail((240, 240))
    buffer = io.BytesIO()
    image.save(buffer, format='JPEG', quality=20, optimize=True)
    encoded = base64.b64encode(buffer.getvalue()).decode()
    for offset in range(0, len(encoded), 1500):
        print(f'::notice title={label}-part-{offset // 1500}::{encoded[offset:offset+1500]}')
    print(f'::notice title={label}-bytes::{len(buffer.getvalue())}')
PY
