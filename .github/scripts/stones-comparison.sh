#!/usr/bin/env bash
set -Eeuo pipefail
mkdir -p stones-comparison
package=com.atlas.engine
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb logcat -G 16M
adb push "$RUNNER_TEMP/stone-compare.jar" /data/local/tmp/stone-compare.jar >/dev/null
read -r width height < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1 | tr -d '\r')
touch_test() { adb shell CLASSPATH=/data/local/tmp/stone-compare.jar app_process / TerrainInput "$1" "$width" "$height" "${2:-1}"; }
for mode in baseline stones; do
    adb shell am force-stop "$package" || true
    adb install -r "$RUNNER_TEMP/$mode.apk"
    adb logcat -c
    adb shell am start -n "$package/.AtlasActivity"
    sleep 7
    touch_test switch
    sleep 8
    adb shell input swipe 160 400 160 240 400
    adb shell CLASSPATH=/data/local/tmp/stone-compare.jar app_process / PinchInput out "$width" "$height"
    sleep 3
    adb exec-out screencap -p > "stones-comparison/$mode.png"
    adb shell log -p i -t ATLAS_STONE_COMPARE measure_static
    sleep 15
    adb shell dumpsys meminfo "$package" > "stones-comparison/$mode-before.txt"
    adb shell log -p i -t ATLAS_STONE_COMPARE measure_move
    touch_test move 8
    adb shell log -p i -t ATLAS_STONE_COMPARE move_end
    sleep 7
    adb shell dumpsys meminfo "$package" > "stones-comparison/$mode-after.txt"
    adb logcat -d -s ATLAS:I ATLAS_STONE_COMPARE:I '*:S' > "stones-comparison/$mode.log"
done
python3 - <<'PYCODE'
from pathlib import Path
import re
for mode in ('baseline','stones'):
    text=Path(f'stones-comparison/{mode}.log').read_text()
    assert 'measure_static' in text and 'move_end' in text
    for label,segment in [('static',text.split('measure_static',1)[1].split('measure_move',1)[0]),('moving',text.split('measure_move',1)[1].split('move_end',1)[0])]:
        vals=re.findall(r'FPS=([0-9.]+), средний интервал=([0-9.]+)',segment)
        assert vals, (mode,label,'missing stats')
        print(f'::notice title={mode} {label} FPS and interval::{vals}')
    def pss(phase):
        t=Path(f'stones-comparison/{mode}-{phase}.txt').read_text()
        m=re.search(r'TOTAL PSS:\s*(\d+)',t) or re.search(r'^\s*TOTAL\s+(\d+)',t,re.M)
        assert m; return m.group(1)
    print(f'::notice title={mode} memory::PSS before/after={pss("before")}/{pss("after")} kB')
    if mode=='stones':
        counts=[tuple(map(int,x)) for x in re.findall(r'камешков=(\d+) базовых draw calls мира=(\d+)',text)]
        assert len(counts)>=10 and all(n<=144 and d==n+2 for n,d in counts),counts
        print(f'::notice title=Stones and base draw calls::{counts}')
        grass=[tuple(map(int,x)) for x in re.findall(r'трава записей=(\d+) видимых=(\d+) draw calls мира=(\d+)',text)]
        assert grass and all(n<=324 and v<=n and d>=v+2 for n,v,d in grass), grass
        assert any(v>0 for n,v,d in grass), grass
        print(f'::notice title=Grass visibility and total draw calls::{grass}')
PYCODE
