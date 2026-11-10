"""Test-only: force identical camera in baseline and candidate benchmark builds."""
from pathlib import Path
import sys
p=Path(sys.argv[1]) / 'ATLAS/app/src/main/cpp/main.cpp'
s=p.read_text()
needle='''    bool syncWorldScene(EngineState* state)
    {
'''
assert s.count(needle)==1
s=s.replace(needle, needle + '''        // Not shipped: compare exactly the same camera after each injected gesture.
        state->angle = 0.55f;
        state->tiltAngle = -0.35f;
        state->cameraDistance = 2.0f;
''')
p.write_text(s)
