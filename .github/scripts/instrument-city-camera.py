"""Diagnostics for emulator-only scene cycle tests; not included in APK."""
from pathlib import Path
p=Path('ATLAS/app/src/main/cpp/main.cpp')
s=p.read_text()
needle='''            state->worldGesture = false;
            state->consumeGesture = true;'''
assert s.count(needle)==1
s=s.replace(needle,'''            __android_log_print(ANDROID_LOG_INFO, "ATLAS_CITY_TEST", "scene world=%d city=%d distance=%.6f angle=%.6f tilt=%.6f", state->worldScene, state->cityScene, state->cameraDistance, state->angle, state->tiltAngle);
'''+needle)
p.write_text(s)
