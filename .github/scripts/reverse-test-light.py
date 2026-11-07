"""Invert the light only in a temporary CI build, leaving the shipped source intact."""
import sys
from pathlib import Path

path = Path(sys.argv[1])
text = path.read_text()
for before, after in [('lightX = 0.4f;', 'lightX = -0.4f;'),
                      ('lightY = 0.8f;', 'lightY = -0.8f;'),
                      ('lightZ = -0.6f;', 'lightZ = 0.6f;')]:
    assert text.count(before) == 1, before
    text = text.replace(before, after)
path.write_text(text)
