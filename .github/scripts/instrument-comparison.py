"""Add camera diagnostics to emulator comparison builds only."""
from pathlib import Path
p=Path('ATLAS/app/src/main/cpp/main.cpp')
s=p.read_text()
needle='            std::swap(state->cameraDistance, state->otherSceneCameraDistance);'
assert s.count(needle)==1
s=s.replace(needle,needle+'\n            __android_log_print(ANDROID_LOG_INFO, "ATLAS_ZOOM_TEST", "switch world=%d distance=%.6f", state->worldScene, state->cameraDistance);')
needle='                        state->lastPinchDistance = distance;'
assert s.count(needle)==1
s=s.replace(needle,needle+'\n                        __android_log_print(ANDROID_LOG_INFO, "ATLAS_ZOOM_TEST", "zoom world=%d distance=%.6f", state->worldScene, state->cameraDistance);')
p.write_text(s)
