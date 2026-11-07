"""Add trace logging only to the x86 emulator test build."""
from pathlib import Path
p = Path('ATLAS/app/src/main/cpp/main.cpp')
s = p.read_text()
needle = '        if (!state->renderer.setFallingCubeY(static_cast<float>(state->physics.y())))'
assert s.count(needle) == 1
s = s.replace(needle, '        __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "frame y=%.9f velocity=%.9f rest=%.9f", state->physics.y(), state->physics.velocity(), state->physics.restY());\n' + needle)
for event, needle in [('lost', '            case APP_CMD_LOST_FOCUS:\n'), ('gain', '            case APP_CMD_GAINED_FOCUS:\n')]:
    assert s.count(needle) == 1
    s = s.replace(needle, needle + f'                __android_log_print(ANDROID_LOG_INFO, "ATLAS_PHYS_TEST", "{event} y=%.9f velocity=%.9f", state->physics.y(), state->physics.velocity());\n')
p.write_text(s)
p = Path('ATLAS/app/src/main/cpp/renderer/VulkanRenderer.cpp')
s = p.read_text()
needle = '            // Направление к источнику света в координатах сцены.'
assert s.count(needle) == 1
s = s.replace(needle, '            if (object.scale < 1.0f) __android_log_print(ANDROID_LOG_INFO, "ATLAS_PROJ_TEST", "y=%.9f screenY=%.9f", object.y, mvp.values[13] / mvp.values[15]);\n\n' + needle)
p.write_text(s)
