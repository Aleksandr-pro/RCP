#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p terrain-diagnostics
package=com.atlas.engine
trap 'adb logcat -d -s ATLAS:I "*:S" > terrain-diagnostics/log.txt || true; tail -80 terrain-diagnostics/log.txt; echo "::error::Terrain test failed at line $LINENO"' ERR
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb shell am force-stop "$package"
adb logcat -G 16M
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 7
adb push "$RUNNER_TEMP/terrain.jar" /data/local/tmp/terrain.jar >/dev/null
read -r width height < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1 | tr -d '\r')
touch_test() { adb shell CLASSPATH=/data/local/tmp/terrain.jar app_process / TerrainInput "$1" "$width" "$height" "${2:-1}"; }
touch_test switch
sleep 0.2
adb shell input keyevent KEYCODE_HOME
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 8
adb logcat -d -s ATLAS_TERRAIN_BODY_TEST:I '*:S' > terrain-diagnostics/body-fall.txt
adb exec-out screencap -p > terrain-diagnostics/world-before.png
adb shell dumpsys meminfo "$package" > terrain-diagnostics/memory-before.txt
adb shell input swipe 70 350 150 350 300
adb shell CLASSPATH=/data/local/tmp/terrain.jar app_process / PinchInput out "$width" "$height"
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
adb logcat -d -s ATLAS:I ATLAS_TERRAIN_MARK:I ATLAS_TERRAIN_BODY_TEST:I ATLAS_TERRAIN_FOCUS:I ATLAS_TERRAIN_GESTURE:I '*:S' > terrain-diagnostics/log.txt
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
body=[tuple(map(float,m)) for m in re.findall(r'y=(-?[0-9.]+) velocity=(-?[0-9.]+) ground=(-?[0-9.]+) half=(-?[0-9.]+)', Path('terrain-diagnostics/body-fall.txt').read_text())]
print(f'::notice title=Terrain body sample::count={len(body)} minVelocity={min((v for y,v,g,h in body),default=0):.3f} maxVelocity={max((v for y,v,g,h in body),default=0):.3f} final={body[-1] if body else None}')
assert len(body)>100, f'missing body traces: {len(body)}'
assert all(y + 1e-7 >= ground + half for y, velocity, ground, half in body), 'body penetrated at center'
assert min(v for y,v,g,h in body)<-1 and max(v for y,v,g,h in body)>0.1, 'no fall or rebound'
assert any(abs(v)<1e-7 and abs(y-g-h)<1e-6 for y,v,g,h in body[-100:]), 'body did not settle'
print('::notice title=Terrain body test::observed fall, rebound, and rest without penetration at center')
assert 'orbit world=1' in log and 'pinch world=1' in log, 'world camera gestures not detected'
focus=re.findall(r'(lost|gain) world=1 y=(-?[0-9.]+) velocity=(-?[0-9.]+)',log)
assert len(focus)>=2, f'missing world focus events: {focus}'
for i,(event,y,v) in enumerate(focus[:-1]):
    if event=='lost' and focus[i+1][0]=='gain' and abs(float(v)) > 0.001:
        assert abs(float(y)-float(focus[i+1][1])) < 1e-8 and abs(float(v)-float(focus[i+1][2])) < 1e-8, 'body changed while paused'
        break
else: raise AssertionError('no pause while body moving')
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
