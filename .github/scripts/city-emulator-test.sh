#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p city-diagnostics
package=com.atlas.engine
trap 'adb logcat -d -s ATLAS:I ATLAS_CITY_TEST:I "*:S" > city-diagnostics/failure.log || true; echo "::error::City test failed at line $LINENO"' ERR
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb logcat -G 16M
adb logcat -c
adb shell am force-stop "$package"
adb shell am start -n "$package/.AtlasActivity"
sleep 7
adb push "$RUNNER_TEMP/city-input.jar" /data/local/tmp/city.jar >/dev/null
read -r width height < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1 | tr -d '\r')
switch() { adb shell CLASSPATH=/data/local/tmp/city.jar app_process / TerrainInput switch "$width" "$height"; }
pinch() { adb shell CLASSPATH=/data/local/tmp/city.jar app_process / PinchInput out "$width" "$height"; }
switch; sleep 5
pinch; sleep 2
switch; sleep 7
adb exec-out screencap -p > city-diagnostics/city.png
adb shell dumpsys meminfo "$package" > city-diagnostics/city-memory.txt
adb shell log -p i -t ATLAS_CITY_TEST city_measure
sleep 12
adb shell input swipe $((width/4)) $((height/2)) $((width*3/4)) $((height/2)) 400
sleep 3
adb exec-out screencap -p > city-diagnostics/city-rotated.png
pinch; sleep 2
adb shell input keyevent KEYCODE_HOME
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 5
switch; sleep 5
switch; sleep 5
switch; sleep 5
adb logcat -d -s ATLAS:I ATLAS_CITY_TEST:I '*:S' > city-diagnostics/log.txt
python3 - <<'PYCODE'
import base64,io,re
from pathlib import Path
from PIL import Image,ImageChops
s=Path('city-diagnostics/log.txt').read_text()
assert 'городская сцена' in s and 'сцена мира' in s and 'физическая сцена' in s
v=[(int(w),int(c),float(d),float(a),float(t)) for w,c,d,a,t in re.findall(r'scene world=(\d+) city=(\d+) distance=([\d.]+) angle=([-\d.]+) tilt=([-\d.]+)',s)]
assert [(w,c) for w,c,*_ in v]==[(1,0),(0,1),(0,0),(1,0),(0,1)],v
assert abs(v[1][2]-8.5)<1e-5 and abs(v[2][2]-4)<1e-5,v
assert v[3][2]<v[0][2] and v[4][2]<v[1][2],v
assert abs(v[4][3]-v[1][3])>1.0, v
assert 'уникальных моделей: 9, всего вершин: 73500' in s
fps=re.findall(r'FPS=([\d.]+), средний интервал=([\d.]+)',s.split('city_measure',1)[1])
assert fps, 'no frames after city loaded'
a=Image.open('city-diagnostics/city.png').convert('RGB')
b=Image.open('city-diagnostics/city-rotated.png').convert('RGB')
assert a.size==b.size
crop=a.crop((0,a.height//4,a.width,a.height*3//4))
assert len(crop.getcolors(crop.width*crop.height) or [])>50,'city image blank or single-color'
assert ImageChops.difference(a,b).getbbox(), 'orbit did not change image'
print(f'::notice title=Scene cycle and camera::{v}')
print(f'::notice title=City FPS::samples {fps}; 9 draws, 24500 triangles')
small=a.copy();small.thumbnail((100,200)); o=io.BytesIO();small.save(o,format='JPEG',quality=48)
print('::notice title=City screenshot::'+base64.b64encode(o.getvalue()).decode())
PYCODE
