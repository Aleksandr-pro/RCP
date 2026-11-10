#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p terrain-variation-comparison
trap 'echo "::error::Comparison failed at line $LINENO"' ERR
package=com.atlas.engine
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb push "$RUNNER_TEMP/compare.jar" /data/local/tmp/compare.jar >/dev/null
read -r width height < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1 | tr -d '\r')
send() { adb shell CLASSPATH=/data/local/tmp/compare.jar app_process / "$@"; }
for mode in reference varied; do
    adb shell am force-stop "$package" || true
    adb install -r "$RUNNER_TEMP/terrain-$mode.apk"
    adb logcat -c
    adb shell am start -n "$package/.AtlasActivity"
    sleep 7
    send TerrainInput switch "$width" "$height"
    sleep 8
    # Repeat the same camera gestures in both builds, with the same observer.
    adb shell input swipe 160 400 160 240 400
    sleep 2
    adb exec-out screencap -p > "terrain-variation-comparison/$mode-before.png"
    send PinchInput out "$width" "$height"
    send PinchInput out "$width" "$height"
    send PinchInput out "$width" "$height"
    sleep 3
    adb exec-out screencap -p > "terrain-variation-comparison/$mode-max.png"
    adb shell log -p i -t ATLAS_COMPARE measuring
    sleep 12
    send TerrainInput switch "$width" "$height"
    sleep 2
    adb logcat -d -s ATLAS:I ATLAS_COMPARE:I ATLAS_ZOOM_TEST:I '*:S' > "terrain-variation-comparison/$mode.log"
    adb shell dumpsys meminfo "$package" > "terrain-variation-comparison/$mode-memory.txt"
done
python3 - <<'PYCODE'
from pathlib import Path
import re
from PIL import Image

def metrics(mode):
    text=Path(f'terrain-variation-comparison/{mode}.log').read_text().split('measuring')[-1]
    values=[tuple(map(float,x)) for x in re.findall(r'FPS=([0-9.]+), средний интервал=([0-9.]+)',text)]
    assert len(values)>=1, (mode,'missing FPS stats')
    return tuple(sum(v[i] for v in values)/len(values) for i in (0,1))

def screenshot(mode):
    image=Image.open(f'terrain-variation-comparison/{mode}-max.png').convert('RGB')
    return image
r,v=screenshot('reference'),screenshot('varied')
assert r.size==v.size
width,height=r.size
# Exclude the FPS overlay and system bars. Clear colour is saturated green.
box=(0,height//4,width,height*7//8)
a,b=r.crop(box),v.crop(box)
def is_ground(pixel):
    r,g,b=pixel
    return not (g > r * 1.2 and g > b * 1.2 and g > 20)
reference=list(a.getdata()); varied=list(b.getdata())
mask=[i for i,(p,q) in enumerate(zip(reference,varied)) if is_ground(p) and is_ground(q)]
assert len(mask)>width*height*.015, f'terrain not visible after world zoom: {len(mask)} pixels'
diff=sum(abs(reference[i][c]-varied[i][c]) for i in mask for c in range(3))/(3*len(mask))
assert diff>1.0, f'new shader output indistinguishable: {diff}'
def detail(im):
    w,h=im.size; pix=im.load(); total=0; count=0
    for y in range(1,h):
        for x in range(1,w):
            p=pix[x,y]; q=pix[x-1,y]; t=pix[x,y-1]
            if is_ground(p) and is_ground(q) and is_ground(t):
                total+=sum(abs(p[c]-q[c])+abs(p[c]-t[c]) for c in range(3)); count+=6
    return total/count if count else 0
for mode in ('reference','varied'):
    text=Path(f'terrain-variation-comparison/{mode}.log').read_text()
    z=[float(x) for x in re.findall(r'zoom world=1 distance=([0-9.]+)',text)]
    assert z and min(z)<0.71, (mode,'world maximum zoom not reached',z)
    restored=[float(x) for x in re.findall(r'switch world=0 distance=([0-9.]+)',text)]
    assert restored and abs(restored[-1]-4.0)<1e-5, (mode,'physics zoom changed',restored)
    before=Image.open(f'terrain-variation-comparison/{mode}-before.png').convert('RGB').crop(box)
    after=screenshot(mode).crop(box)
    n0=sum(is_ground(p) for p in before.getdata()); n1=sum(is_ground(p) for p in after.getdata())
    print(f'::notice title=World zoom {mode}::before={n0} after={n1} non-background pixels; closest distance={min(z):.2f}, physics restored={restored[-1]:.2f}')
    assert n1>n0*1.1, (mode,'world zoom did not enlarge visible surface',n0,n1)
rf,ri=metrics('reference'); vf,vi=metrics('varied')
print(f'::notice title=Comparison same scene::reference FPS={rf:.1f} interval={ri:.1f}ms; varied FPS={vf:.1f} interval={vi:.1f}ms; image difference={diff:.2f}; detail reference={detail(a):.2f} varied={detail(b):.2f} (arbitrary units)')
PYCODE
