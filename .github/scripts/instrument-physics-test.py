"""Add trace logging and controllable timing only to the x86 emulator build."""
from pathlib import Path
p = Path('ATLAS/app/src/main/cpp/main.cpp')
s = p.read_text()
s = s.replace('#include <chrono>', '#include <chrono>\n#include <thread>')
needle = '        if (!syncPhysicsScene(state))'
assert s.count(needle) == 1
s = s.replace(needle, '''        __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "frame y=%.9f velocity=%.9f rest=%.9f", state->boxes[0].y, state->boxes[0].velocityY, (state->platform.y + state->platform.halfY + state->boxes[0].halfY));
        for (size_t i=0; i<state->boxes.size(); ++i) __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "box=%zu y=%.9f velocity=%.9f", i, state->boxes[i].y, state->boxes[i].velocityY);
        __android_log_print(ANDROID_LOG_INFO, "ATLAS_JUMP_TEST", "frame y=%.9f velocity=%.9f timer=%.9f jumped=%d", state->boxes[2].y, state->boxes[2].velocityY, state->jumpTest.groundedTime, state->jumpTest.jumped);
        static bool testDelayed = false, delayedWait = false, delayedJump = false;
        if (!testDelayed && state->boxes[0].velocityY < 0.0) { testDelayed = true; std::this_thread::sleep_for(std::chrono::seconds(2)); }
        if (!delayedWait && state->jumpTest.groundedTime > 0.2f && !state->jumpTest.jumped) { delayedWait = true; __android_log_print(ANDROID_LOG_INFO, "ATLAS_JUMP_TEST", "phase=waiting"); std::this_thread::sleep_for(std::chrono::seconds(2)); }
        if (!delayedJump && state->jumpTest.jumped && state->boxes[2].velocityY > 0.0f) { delayedJump = true; __android_log_print(ANDROID_LOG_INFO, "ATLAS_JUMP_TEST", "phase=jumping"); std::this_thread::sleep_for(std::chrono::seconds(2)); }
''' + needle)
for event, needle in [('lost', '            case APP_CMD_LOST_FOCUS:\n'), ('gain', '            case APP_CMD_GAINED_FOCUS:\n')]:
    assert s.count(needle) == 1
    s = s.replace(needle, needle + f'                __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "{event} y=%.9f velocity=%.9f", state->boxes[0].y, state->boxes[0].velocityY);\n                __android_log_print(ANDROID_LOG_INFO, "ATLAS_JUMP_TEST", "{event} y=%.9f velocity=%.9f timer=%.9f jumped=%d", state->boxes[2].y, state->boxes[2].velocityY, state->jumpTest.groundedTime, state->jumpTest.jumped);\n')
p.write_text(s)
p = Path('ATLAS/app/src/main/cpp/renderer/VulkanRenderer.cpp')
s = p.read_text()
needle = '            // Направление к источнику света в координатах сцены.'
assert s.count(needle) == 1
s = s.replace(needle, '            if (object.scale < 1.0f && object.x < 0.0f && object.y < 0.5f) __android_log_print(ANDROID_LOG_INFO, "ATLAS_PROJ_TEST", "y=%.9f screenY=%.9f", object.y, mvp.values[13] / mvp.values[15]);\n\n' + needle)
p.write_text(s)
