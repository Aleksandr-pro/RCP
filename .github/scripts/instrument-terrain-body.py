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
for event, needle in [('lost', '            case APP_CMD_LOST_FOCUS:\n'), ('gain', '            case APP_CMD_GAINED_FOCUS:\n')]:
    assert s.count(needle)==1
    s=s.replace(needle, needle + f'                __android_log_print(ANDROID_LOG_INFO, "ATLAS_TERRAIN_FOCUS", "{event} world=%d y=%.9f velocity=%.9f", state->worldScene, state->terrainBody.y, state->terrainBody.velocityY);\n')
needle = '                    state->angle = std::fmod(state->angle, fullTurn);'
assert s.count(needle)==1
s=s.replace(needle, needle+'\n                    __android_log_print(ANDROID_LOG_INFO, "ATLAS_TERRAIN_GESTURE", "orbit world=%d", state->worldScene);')
needle = '                        state->lastPinchDistance = distance;'
assert s.count(needle)==1
s=s.replace(needle, needle+'\n                        __android_log_print(ANDROID_LOG_INFO, "ATLAS_TERRAIN_GESTURE", "pinch world=%d", state->worldScene);')
p.write_text(s)

p = Path('ATLAS/app/src/main/cpp/renderer/VulkanRenderer.cpp')
s = p.read_text()
needle = '            !uploadTexture(terrainImages_[i], atlas::TERRAIN_TEXTURE_SIZE, atlas::TERRAIN_TEXTURE_SIZE, pixels)) return false;'
assert s.count(needle) == 1
s = s.replace(needle, needle + '\n        __android_log_print(ANDROID_LOG_INFO, "ATLAS_TEXTURE_TEST", "create index=%zu", i);')
needle = '            if (terrainViews_[i]) vkDestroyImageView(device_, terrainViews_[i], nullptr);'
assert s.count(needle) == 1
s = s.replace(needle, '            if (terrainViews_[i] && terrainImages_[i] && terrainMemories_[i]) __android_log_print(ANDROID_LOG_INFO, "ATLAS_TEXTURE_TEST", "destroy index=%zu", i);\n' + needle)
p.write_text(s)
