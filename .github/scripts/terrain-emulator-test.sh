#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p terrain-diagnostics
package=com.atlas.engine
trap 'adb logcat -d -s ATLAS:I "*:S" > terrain-diagnostics/log.txt || true; tail -80 terrain-diagnostics/log.txt; echo "::error::Terrain test failed at line $LINENO"' ERR
adb install -r "$RUNNER_TEMP/terrain-normal.apk"
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
# Switch HOME as soon as the first simulation step is observed, not after
# waiting for the entire injected gesture (which can take several seconds).
( set +o pipefail; timeout 25s stdbuf -oL adb logcat -s ATLAS_TERRAIN_BODY_TEST:I '*:S' |
  while IFS= read -r line; do
    if [[ "$line" == *'velocity=-'* ]]; then adb shell input keyevent KEYCODE_HOME; break; fi
  done ) &
focus_trigger=$!
touch_test switch || true
wait "$focus_trigger"
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 8
adb logcat -d -s ATLAS_TERRAIN_BODY_TEST:I '*:S' > terrain-diagnostics/body-fall.txt
adb exec-out screencap -p > terrain-diagnostics/world-before.png
adb shell dumpsys meminfo "$package" > terrain-diagnostics/memory-before.txt
adb shell input swipe 70 350 150 350 300
adb shell CLASSPATH=/data/local/tmp/terrain.jar app_process / PinchInput out "$width" "$height"
sleep 2
adb exec-out screencap -p > terrain-diagnostics/grass-close.png
python3 - <<'PYCODE'
from PIL import Image
im=Image.open('terrain-diagnostics/grass-close.png').convert('RGB')
# Production shader output is an unlit muted green gradient.
pixels=sum(1 for r,g,b in im.getdata() if 16<=r<=42 and 58<=g<=103 and 12<=b<=28 and g>r*2.2 and g>b*2.8)
print(f'::notice title=Grass rasterization::green grass-colored pixels={pixels}')
assert pixels>3, 'no visible grass-colored fragments in close view'
PYCODE
adb shell log -p i -t ATLAS_TERRAIN_MARK move_start
touch_test move 5
adb shell log -p i -t ATLAS_TERRAIN_MARK move_end
sleep 7
adb shell dumpsys meminfo "$package" > terrain-diagnostics/memory-after.txt
adb exec-out screencap -p > terrain-diagnostics/world-return.png
adb shell input keyevent KEYCODE_HOME
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 6
adb shell log -p i -t ATLAS_TERRAIN_MARK resumed
adb shell settings put system user_rotation 1
for attempt in $(seq 1 15); do
    sleep 2
    adb exec-out screencap -p > terrain-diagnostics/world-rotated.png
    if python3 -c 'from PIL import Image; im=Image.open("terrain-diagnostics/world-rotated.png"); assert im.width>im.height'; then break; fi
done
python3 -c 'from PIL import Image; im=Image.open("terrain-diagnostics/world-rotated.png"); assert im.width>im.height'
adb shell settings put system user_rotation 0
for attempt in $(seq 1 15); do
    sleep 2
    adb exec-out screencap -p > terrain-diagnostics/world-portrait-again.png
    if python3 -c 'from PIL import Image; im=Image.open("terrain-diagnostics/world-portrait-again.png"); assert im.width<im.height'; then break; fi
done
python3 -c 'from PIL import Image; im=Image.open("terrain-diagnostics/world-portrait-again.png"); assert im.width<im.height'
touch_test switch
sleep 5
adb logcat -d -s ATLAS:I ATLAS_TERRAIN_MARK:I ATLAS_TERRAIN_BODY_TEST:I ATLAS_TERRAIN_FOCUS:I ATLAS_TERRAIN_GESTURE:I ATLAS_TEXTURE_TEST:I '*:S' > terrain-diagnostics/log.txt
grep -q 'материал рельефа x=4.000 z=4.000 surface=stone restitution=0.70' terrain-diagnostics/log.txt
[ "$(grep -c 'материал рельефа' terrain-diagnostics/log.txt)" -eq 1 ]
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
print(f'::notice title=Terrain screenshots::{a.width}x{a.height} rotated={b.width}x{b.height}')
assert (a.width<a.height) and (b.width>b.height), 'rotation did not take effect'
log=Path('terrain-diagnostics/log.txt').read_text()
grass=[tuple(map(int,x)) for x in re.findall(r'трава записей=(\d+) видимых=(\d+) draw calls мира=(\d+)',log)]
assert grass and all(n<=324 and v<=n and d>=v+2 for n,v,d in grass) and any(v>0 for n,v,d in grass), grass
print(f'::notice title=Grass::records, visible, draw calls: {grass}')
counts=[tuple(map(int,x)) for x in re.findall(r'камешков=(\d+) базовых draw calls мира=(\d+)',log)]
assert len(counts)>=10 and all(0 <= stones <= 144 and draws == stones+2 for stones,draws in counts), counts
print(f'::notice title=Stone draw calls::per active area stones and total scene draws: {counts}')
assert 'move_start' in log and 'move_end' in log
movement=log.split('move_start',1)[1].split('move_end',1)[0]
assert 'Текстура загружена:' not in movement, 'textures reloaded on chunk transitions'
for material in ('sand', 'ground', 'stone'):
    assert ('textures/'+material+'.rgba') in log, 'missing texture load: '+material
for i in range(3):
    created=len(re.findall(fr'ATLAS_TEXTURE_TEST.*create index={i}', log))
    destroyed=len(re.findall(fr'ATLAS_TEXTURE_TEST.*destroy index={i}', log))
    assert created >= 2 and created-destroyed == 1, (i,created,destroyed)
print('::notice title=Texture lifecycle::three shared texture resources released on each tested renderer recreation')
# A compact screenshot is embedded in CI annotations for visual review.
import base64, io
shot=Image.open('terrain-diagnostics/world-before.png').convert('RGB')
shot.thumbnail((100,200))
out=io.BytesIO(); shot.save(out, format='JPEG', quality=48)
print('::notice title=Terrain screenshot::'+base64.b64encode(out.getvalue()).decode())
body=[tuple(map(float,m)) for m in re.findall(r'y=(-?[0-9.]+) velocity=(-?[0-9.]+) ground=(-?[0-9.]+) half=(-?[0-9.]+)', Path('terrain-diagnostics/body-fall.txt').read_text())]
print(f'::notice title=Terrain body sample::count={len(body)} minVelocity={min((v for y,v,g,h in body),default=0):.3f} maxVelocity={max((v for y,v,g,h in body),default=0):.3f} final={body[-1] if body else None}')
assert len(body)>100, f'missing body traces: {len(body)}'
assert all(y + 1e-7 >= ground + half for y, velocity, ground, half in body), 'body penetrated at center'
assert min(v for y,v,g,h in body)<-1 and max(v for y,v,g,h in body)>0.1, 'no fall or rebound'
assert any(abs(v)<1e-7 and abs(y-g-h)<1e-6 for y,v,g,h in body[-100:]), 'body did not settle'
print('::notice title=Terrain body test::observed fall, rebound, and rest without penetration at center')
print(f"::notice title=World gestures::orbit={'orbit world=1' in log} pinch={'pinch world=1' in log}")
assert 'orbit world=1' in log and 'pinch world=1' in log, 'world camera gestures not detected'
focus=re.findall(r'(lost|gain) world=1 y=(-?[0-9.]+) velocity=(-?[0-9.]+)',log)
print(f'::notice title=Focus traces::{focus}')
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

# Test-only palette demonstrates that the actual GPU shader samples and mixes
# all three descriptor bindings, independently of photographic texture colors.
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/terrain-palette.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 5
touch_test switch
sleep 3
adb shell input swipe 160 400 160 240 400
sleep 3
adb exec-out screencap -p > terrain-diagnostics/palette.png
python3 - <<'PYCODE'
from PIL import Image
im=Image.open('terrain-diagnostics/palette.png').convert('RGB')
colors=list(im.getdata())
red=sum(1 for r,g,b in colors if r>50 and r>b*3 and r>g*3)
magenta=sum(1 for r,g,b in colors if r>35 and b>35 and min(r,b)>g*3 and max(r,b)<min(r,b)*2)
blue=sum(1 for r,g,b in colors if b>50 and b>r*3 and b>g*3)
print(f'::notice title=GPU material palette::sand={red} ground={magenta} stone={blue} pixels')
assert min(red,magenta,blue)>10, 'one or more material textures were not visible'
PYCODE

# An initialization error after two successful uploads must free both images,
# including allocations created before any descriptor views exist.
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/terrain-missing.apk"
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 6
adb logcat -d -s ATLAS:E ATLAS_TEXTURE_TEST:I '*:S' > terrain-diagnostics/missing.log
python3 - <<'PYCODE'
from pathlib import Path
import re
s=Path('terrain-diagnostics/missing.log').read_text()
assert 'Текстура не найдена: textures/stone.rgba' in s
for i in (0,1):
    creates=len(re.findall(fr'ATLAS_TEXTURE_TEST.*create index={i}',s))
    destroys=len(re.findall(fr'ATLAS_TEXTURE_TEST.*destroy index={i}',s))
    assert creates>0 and creates==destroys, (i,creates,destroys)
assert not re.search(r'ATLAS_TEXTURE_TEST.*create index=2',s)
print('::notice title=Texture error cleanup::missing stone texture: previous sand and ground GPU images released')
PYCODE

# Diagnostic material proves individual stone triangles reach the screen.
adb shell am force-stop "$package"
adb install -r "$RUNNER_TEMP/terrain-stone-diagnostic.apk"
adb shell am start -n "$package/.AtlasActivity"
sleep 5
touch_test switch
sleep 4
adb shell input swipe 160 400 160 240 400
sleep 2
adb shell CLASSPATH=/data/local/tmp/terrain.jar app_process / PinchInput out "$width" "$height"
sleep 2
adb exec-out screencap -p > terrain-diagnostics/stone-diagnostic.png
python3 - <<'PYCODE'
from PIL import Image
im=Image.open('terrain-diagnostics/stone-diagnostic.png').convert('RGB')
pixels=sum(1 for r,g,b in im.getdata() if r>40 and b>40 and g<5 and abs(r-b)<5)
print(f'::notice title=Stone rasterization::diagnostic magenta pixels={pixels}')
assert pixels>5, 'no diagnostic stone fragments visible'
PYCODE
