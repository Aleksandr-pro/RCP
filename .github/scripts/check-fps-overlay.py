import re
import sys
from pathlib import Path

from PIL import Image

screen_path, log_path = sys.argv[1:]
log = Path(log_path).read_text()
# The emulator-only build logs the actual TextView content after each UI update.
updates = re.findall(
    r'ATLAS_TEST_UI\s*: FPS: (\d+\.\d) \| Интервал: (\d+\.\d) мс \| Кадров: (\d+)',
    log,
)
assert updates, 'FPS view has not received numeric statistics'
fps, interval, frames = updates[-1]
expected = f'FPS={fps}, interval={interval}, frames={frames}'
assert f'ATLAS_TEST_STATS: {expected}' in log, f'Overlay values do not match the values passed from the engine: {expected}'
assert f'FPS={fps}, средний интервал={interval} мс,' in log, 'Overlay FPS and interval do not match the engine log'
# NativeActivity renders Vulkan on the main window surface; verify the separate
# FPS popup is actually visible in the top-left screen area, not just in the UI tree.
image = Image.open(screen_path).convert('RGB')
w, h = image.size
region = image.crop((4, 25, w // 2, min(h // 3, 180)))
bright = sum(1 for r, g, b in region.getdata() if min(r, g, b) >= 220)
assert bright >= 20, f'FPS text is not visible in the top-left corner ({bright} bright pixels)'
print(f'FPS overlay visible; values match ATLAS statistics: {expected}')
