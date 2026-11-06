#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::Camera test failed at line $LINENO: $BASH_COMMAND"' ERR
mkdir -p scene-comparison
package=com.atlas.engine
adb logcat -G 32M
wait_orientation() {
  local expected="$1" screenshot="scene-comparison/orientation-wait.png"
  for attempt in $(seq 1 12); do
    adb exec-out screencap -p > "$screenshot"
    if python3 - "$screenshot" "$expected" <<'PYORIENTATION'
import sys
from PIL import Image
w, h = Image.open(sys.argv[1]).size
assert (w < h) if sys.argv[2] == 'portrait' else (w > h), (w, h)
PYORIENTATION
    then return 0; fi
    if [ "$expected" = landscape ] && [ "$attempt" = 5 ]; then
      # Some emulators start the launcher in portrait despite the rotation
      # setting. Relaunch once and keep the dimension assertion mandatory.
      adb shell am force-stop "$package"
      adb shell settings put system accelerometer_rotation 0
      adb shell settings put system user_rotation 1
      adb shell am start -n "$package/.AtlasActivity"
    fi
    sleep 2
  done
  echo "::error::Display did not reach $expected orientation"
  return 1
}

# The emulator-only build adds tracing; production has no gesture logging.
adb shell am force-stop "$package"
adb install -r ATLAS/app/build/outputs/apk/debug/app-debug.apk
adb shell settings put system accelerometer_rotation 0
adb shell settings put system user_rotation 0
adb logcat -c
adb shell am start -n "$package/.AtlasActivity"
sleep 6
wait_orientation portrait
adb exec-out screencap -p > scene-comparison/touch-portrait-before.png
adb shell input swipe 60 350 220 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-right.png
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-still.png
adb shell input swipe 220 350 60 350 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-left.png
adb shell input swipe 160 220 160 420 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-down.png
adb shell input swipe 160 420 160 220 700
sleep 2
adb shell input swipe 60 230 210 410 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-portrait-diagonal.png
adb shell input swipe 210 410 60 230 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' > scene-comparison/touch-log.txt
adb logcat -c
# Start a fresh landscape activity, so this gesture check is independent of
# the preceding portrait swipe. Rotation during an active app is tested above.
adb shell am force-stop "$package"
adb shell input keyevent KEYCODE_HOME
adb shell settings put system user_rotation 1
sleep 3
adb shell am start -n "$package/.AtlasActivity"
sleep 8
wait_orientation landscape
adb exec-out screencap -p > scene-comparison/touch-landscape-before.png
adb shell input swipe 140 180 460 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-right.png
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-still.png
adb shell input swipe 460 180 140 180 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-left.png
adb shell input swipe 320 80 320 240 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-down.png
adb shell input swipe 320 240 320 80 700
sleep 2
adb shell input swipe 160 80 440 240 700
sleep 2
adb exec-out screencap -p > scene-comparison/touch-landscape-diagonal.png
adb shell input swipe 440 240 160 80 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' >> scene-comparison/touch-log.txt
adb logcat -c
# Leave a finger down while the app loses focus, then resume and swipe again.
adb shell input swipe 120 180 480 180 3000 &
swipe_pid=$!
sleep 1
adb shell input keyevent KEYCODE_HOME
wait "$swipe_pid"
sleep 2
adb shell am start -n "$package/.AtlasActivity"
sleep 5
adb shell input swipe 140 180 460 180 700
sleep 2
adb logcat -d -s ATLAS:I ATLAS_TOUCH_TEST:I '*:S' >> scene-comparison/touch-log.txt
if ! python3 .github/scripts/check-touch.py > scene-comparison/touch-check.txt 2>&1; then
  detail=$(cat scene-comparison/touch-check.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
  echo "::error title=Touch test::$detail"
  grep 'ATLAS_TOUCH_TEST' scene-comparison/touch-log.txt | tail -n 25 | while IFS= read -r line; do echo "::notice::$line"; done
  exit 1
fi
cat scene-comparison/touch-check.txt

# Inject genuine Android multi-pointer MotionEvents as the emulator shell user.
adb push "$RUNNER_TEMP/pinch.jar" /data/local/tmp/atlas-pinch.jar >/dev/null
pinch() {
  adb shell CLASSPATH=/data/local/tmp/atlas-pinch.jar app_process /system/bin PinchInput "$1" "$2" "$3"
}
adb shell am force-stop "$package"
adb shell settings put system user_rotation 0
adb shell am start -n "$package/.AtlasActivity"
sleep 6
wait_orientation portrait
adb logcat -c
adb exec-out screencap -p > scene-comparison/pinch-before.png
pinch out 320 640
sleep 2
adb exec-out screencap -p > scene-comparison/pinch-near.png
pinch in 320 640
sleep 2
adb exec-out screencap -p > scene-comparison/pinch-far.png
adb logcat -d -s ATLAS:I ATLAS_PINCH_TEST:I '*:S' > scene-comparison/pinch-portrait-log.txt
adb shell am force-stop "$package"
adb shell input keyevent KEYCODE_HOME
adb shell settings put system user_rotation 1
sleep 3
adb shell am start -n "$package/.AtlasActivity"
sleep 8
wait_orientation landscape
adb logcat -c
pinch out 640 320
# Backgrounding may terminate the shell injector with SIGKILL; handle that
# expected status in the parent rather than emitting a misleading ERR annotation.
( trap - ERR; pinch hold 640 320 ) &
pinch_pid=$!
sleep 1
adb shell input keyevent KEYCODE_HOME
# The OS may kill the shell input helper when HOME cancels the gesture.
# Accept only that specific exit status; the logs below must still prove
# that an active pinch was cancelled and that a new pinch works on resume.
if wait "$pinch_pid"; then
  echo 'Pinch helper finished after HOME'
else
  status=$?
  if [ "$status" -ne 137 ]; then
    echo "::error::Pinch helper failed unexpectedly (exit $status)"
    exit "$status"
  fi
  echo '::notice::Pinch helper was stopped by Android while switching to HOME'
fi
sleep 3
adb shell am start -n "$package/.AtlasActivity"
sleep 6
wait_orientation landscape
pinch in 640 320
sleep 2
adb logcat -d -s ATLAS:I ATLAS_PINCH_TEST:I '*:S' > scene-comparison/pinch-landscape-log.txt
if ! python3 .github/scripts/check-pinch.py > scene-comparison/pinch-check.txt 2>&1; then
  detail=$(cat scene-comparison/pinch-check.txt | sed ':a;N;$!ba;s/%/%25/g;s/\r/%0D/g;s/\n/%0A/g')
  echo "::error title=Pinch test::$detail"
  grep 'ATLAS_PINCH_TEST' scene-comparison/pinch-landscape-log.txt | tail -n 25 | while IFS= read -r line; do echo "::notice::$line"; done
  exit 1
fi
cat scene-comparison/pinch-check.txt
