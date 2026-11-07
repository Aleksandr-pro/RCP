"""Add trace logging only to the x86 emulator test build."""
from pathlib import Path
p = Path('ATLAS/app/src/main/cpp/main.cpp')
s = p.read_text()
s = s.replace('#include <chrono>', '#include <chrono>\n#include <thread>')
needle = '        if (!syncPhysicsScene(state))'
assert s.count(needle) == 1
s = s.replace(needle, '        __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "frame y=%.9f velocity=%.9f rest=%.9f", state->boxes[0].y, state->boxes[0].velocityY, (state->platform.y + state->platform.halfY + state->boxes[0].halfY));\n        for (size_t i=0; i<state->boxes.size(); ++i) __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "box=%zu y=%.9f velocity=%.9f", i, state->boxes[i].y, state->boxes[i].velocityY);\n        static bool testDelayed = false;\n        if (!testDelayed && state->boxes[0].velocityY < 0.0) { testDelayed = true; std::this_thread::sleep_for(std::chrono::seconds(2)); }\n' + needle)
for event, needle in [('lost', '            case APP_CMD_LOST_FOCUS:\n'), ('gain', '            case APP_CMD_GAINED_FOCUS:\n')]:
    assert s.count(needle) == 1
    s = s.replace(needle, needle + f'                __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "{event} y=%.9f velocity=%.9f", state->boxes[0].y, state->boxes[0].velocityY);\n')
p.write_text(s)
p = Path('ATLAS/app/src/main/cpp/renderer/VulkanRenderer.cpp')
s = p.read_text()
needle = '            // Направление к источнику света в координатах сцены.'
assert s.count(needle) == 1
s = s.replace(needle, '            if (object.scale < 1.0f && object.x < 0.0f && object.y < 0.5f) __android_log_print(ANDROID_LOG_INFO, "ATLAS_PROJ_TEST", "y=%.9f screenY=%.9f", object.y, mvp.values[13] / mvp.values[15]);\n\n' + needle)
p.write_text(s)
