"""Add body traces to the emulator-only build, never to the shipped APK."""
from pathlib import Path
p = Path('ATLAS/app/src/main/cpp/main.cpp')
s = p.read_text()
needle = '                state->physicsAccumulator = state->physicsAccumulator - fixedStep;'
assert s.count(needle) == 1
s = s.replace(needle, '''                double testGround = 0.0;
                atlas::world::sampleTerrainMeshHeight(state->terrainBody.x, state->terrainBody.z, testGround);
                __android_log_print(ANDROID_LOG_INFO, "ATLAS_TERRAIN_BODY_TEST", "y=%.9f velocity=%.9f ground=%.9f half=%.9f", state->terrainBody.y, state->terrainBody.velocityY, testGround, state->terrainBody.halfHeight);
''' + needle)
p.write_text(s)
