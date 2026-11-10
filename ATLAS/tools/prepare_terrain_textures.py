#!/usr/bin/env python3
"""Prepare temporary repeatable RGBA8 textures from the saved generated PNGs.
Requires Pillow and numpy. The cosine overlap moves the original seams to zero weight.
"""
from pathlib import Path
import numpy as np
from PIL import Image

root = Path(__file__).resolve().parents[1]
n = 512
weights = ((1 - np.cos(2 * np.pi * np.arange(n) / n)) / 2).astype(np.float32)
for name in ('sand', 'ground', 'stone'):
    source = Image.open(root / 'texture-sources' / f'{name}.png').convert('RGB')
    side = min(source.size)
    left, top = (source.width - side)//2, (source.height - side)//2
    source = source.crop((left,top,left+side,top+side)).resize((n,n), Image.Resampling.LANCZOS)
    rgb = np.asarray(source, dtype=np.float32)
    wx = weights[None,:,None]
    wy = weights[:,None,None]
    rgb = rgb * wx + np.roll(rgb, n//2, axis=1) * (1-wx)
    rgb = rgb * wy + np.roll(rgb, n//2, axis=0) * (1-wy)
    rgba = np.empty((n,n,4), dtype=np.uint8)
    rgba[:,:,:3] = np.clip(np.rint(rgb), 0, 255).astype(np.uint8)
    rgba[:,:,3] = 255
    (root/'app/src/main/assets/textures'/f'{name}.rgba').write_bytes(rgba.tobytes())
