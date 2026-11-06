import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image

xml_path, screen_path, log_path = sys.argv[1:]
root = ET.parse(xml_path).getroot()
nodes = [node for node in root.iter('node')
         if node.get('content-desc') == 'ATLAS FPS']
assert len(nodes) == 1, f'Expected one FPS overlay, found {len(nodes)}'
node = nodes[0]
text = node.get('text', '')
match = re.fullmatch(r'FPS: (\d+\.\d)\s+Интервал: (\d+\.\d) мс\s+Кадров: (\d+)', text)
assert match, f'FPS overlay has not displayed measurements: {text!r}'
fps, interval, frames = match.groups()
log = Path(log_path).read_text()
expected = f'FPS={fps}, средний интервал={interval} мс, кадров={frames}'
assert expected in log, f'Overlay values do not match an ATLAS statistics record: {expected}'
bounds = re.fullmatch(r'\[(\d+),(\d+)\]\[(\d+),(\d+)\]', node.get('bounds', ''))
assert bounds, 'FPS overlay has no screen bounds'
x0, y0, x1, y1 = map(int, bounds.groups())
image = Image.open(screen_path).convert('RGB')
assert 0 <= x0 < x1 <= image.width and 0 <= y0 < y1 <= image.height, bounds.group(0)
bright = sum(1 for r, g, b in image.crop((x0, y0, x1, y1)).getdata()
             if min(r, g, b) >= 220)
assert bright >= 10, f'FPS text not visibly rendered within {bounds.group(0)}'
print(f'FPS overlay visible at {bounds.group(0)}: {text!r}')
