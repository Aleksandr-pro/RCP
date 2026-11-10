#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p terrain-diagnostics
package=com.atlas.engine
trap 'adb logcat -d -s ATLAS:I "*:S" > terrain-diagnostics/log.txt || true; tail -80 terrain-diagnostics/log.txt; echo "::error::Terrain test failed at line $LINENO"' ERR
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb shell am force-stop "$package"
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 7
adb push "$RUNNER_TEMP/terrain.jar" /data/local/tmp/terrain.jar >/dev/null
read -r width height < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1 | tr -d '\r')
touch_test() { adb shell CLASSPATH=/data/local/tmp/terrain.jar app_process / TerrainInput "$1" "$width" "$height" "${2:-1}"; }
touch_test switch
sleep 8
adb exec-out screencap -p > terrain-diagnostics/world-before.png
adb shell dumpsys meminfo "$package" > terrain-diagnostics/memory-before.txt
touch_test move 5
sleep 7
adb shell dumpsys meminfo "$package" > terrain-diagnostics/memory-after.txt
adb exec-out screencap -p > terrain-diagnostics/world-return.png
adb shell input keyevent KEYCODE_HOME
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 6
adb shell log -p i -t ATLAS_TERRAIN_MARK resumed
adb shell settings put system user_rotation 1
sleep 6
adb exec-out screencap -p > terrain-diagnostics/world-rotated.png
adb shell settings put system user_rotation 0
sleep 5
touch_test switch
sleep 5
adb logcat -d -s ATLAS:I ATLAS_TERRAIN_MARK:I '*:S' > terrain-diagnostics/log.txt
grep -q 'сцена мира' terrain-diagnostics/log.txt
grep -q 'физическая сцена' terrain-diagnostics/log.txt
grep -q 'Участок: x=-1 z=0, активных=9' terrain-diagnostics/log.txt
grep -q 'Участок: x=0 z=0, активных=9' terrain-diagnostics/log.txt
[ "$(grep -c 'Участок:' terrain-diagnostics/log.txt)" -ge 10 ]
! grep 'Участок:' terrain-diagnostics/log.txt | grep -v 'активных=9'
grep 'ATLAS stats: FPS=' terrain-diagnostics/log.txt > terrain-diagnostics/fps.txt
cat terrain-diagnostics/fps.txt
printf '\nMemory before and after movement (kB):\n'
grep -E 'TOTAL PSS|TOTAL ' terrain-diagnostics/memory-before.txt | head -3
grep -E 'TOTAL PSS|TOTAL ' terrain-diagnostics/memory-after.txt | head -3

python3 - <<'PYCODE'
from pathlib import Path
import re
from PIL import Image
for name in ('world-before', 'world-return', 'world-rotated'):
    image=Image.open('terrain-diagnostics/'+name+'.png').convert('RGB')
    pixels=image.crop((image.width//4,image.height//4,image.width*3//4,image.height*3//4))
    assert len(pixels.getcolors(pixels.width*pixels.height)) > 50, f'{name} looks blank'
    print(f'{name}: {image.width}x{image.height}, nonuniform center')
a=Image.open('terrain-diagnostics/world-before.png')
b=Image.open('terrain-diagnostics/world-rotated.png')
assert (a.width<a.height) and (b.width>b.height), 'rotation did not take effect'
log=Path('terrain-diagnostics/log.txt').read_text()
assert 'ATLAS stats: FPS=' in log.split('resumed',1)[1], 'no frames after resume and rotation'
fps=re.findall(r'ATLAS stats: FPS=([0-9.]+)', log)
def pss(name):
    text=Path('terrain-diagnostics/memory-'+name+'.txt').read_text()
    m=re.search(r'TOTAL PSS:\s*(\d+)',text)
    if not m: m=re.search(r'^\s*TOTAL\s+(\d+)', text, re.M)
    assert m, 'missing PSS'
    return int(m.group(1))
print('::notice title=Terrain emulator metrics::FPS samples '+', '.join(fps)+'; total PSS before/after movement '+str(pss('before'))+'/'+str(pss('after'))+' kB')
PYCODE
