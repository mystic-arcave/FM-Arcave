#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""A contact sheet of the host run's screens: tools/sheet.py OUT.png a.ppm b.ppm ... (each shown at 2x, labelled)."""
import sys
from pathlib import Path
from PIL import Image, ImageDraw
out, files = sys.argv[1], sys.argv[2:]
files = sorted(files, key=lambda f: int(Path(f).stem.rsplit("-", 1)[1]))
cols = min(6, len(files)); rows = (len(files) + cols - 1) // cols
S = 1 if len(files) > 12 else 2
sheet = Image.new("RGB", (cols * (240 * S + 8) + 8, rows * (240 * S + 22) + 8), (40, 40, 48))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    x, y = 8 + (i % cols) * (240 * S + 8), 8 + (i // cols) * (240 * S + 22)
    sheet.paste(Image.open(f).convert("RGB").resize((240 * S, 240 * S), Image.NEAREST), (x, y + 14))
    d.text((x, y), Path(f).stem, fill=(220, 220, 220))
sheet.save(out)
