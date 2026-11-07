"""Verify each named model was read once in every completed initialization."""
import re
import sys
from pathlib import Path

log = Path(sys.argv[1]).read_text()
expected = sys.argv[2:]
pending = []
completed = 0
for line in log.splitlines():
    match = re.search(r'Модель загружена: models/([a-z0-9_]+)\.atlasmesh, вершин: (\d+)', line)
    if match:
        pending.append((match.group(1), int(match.group(2))))
    summary = re.search(r'Vulkan: уникальных моделей: (\d+), всего вершин: (\d+)', line)
    if summary:
        wanted = [(name, 18 if name == 'pyramid' else 36) for name in expected]
        assert pending == wanted, f'Per-initialization loads {pending} != {wanted}'
        assert (int(summary[1]), int(summary[2])) == (len(wanted), sum(v for _, v in wanted)), summary.groups()
        completed += 1
        pending = []
assert completed >= 1, 'No completed model initialization found'
print(f'{completed} initialization(s) loaded {expected} once each')
