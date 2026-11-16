#!/usr/bin/env python3
"""Erzeugt Test-JPEGs fuer test_jpeg_header_check (Pillow noetig).

Aufruf: python3 make_samples.py <Zielordner>
Dateinamen mit "bad_" muessen von der Pruefung abgelehnt werden, alle anderen
angenommen.

Zusaetzlich sinnvoll (ffmpeg): ein MJPEG-Bild, wie es Kameras liefern, und eines
mit der Abtastung 1x2, die der IDF-Treiber nicht kennt:
  ffmpeg -f lavfi -i testsrc=size=1280x720 -frames:v 1 -q:v 3 -pix_fmt yuvj420p <Ziel>/ok_ffmpeg_420.jpg
  ffmpeg -f lavfi -i testsrc=size=640x480 -frames:v 1 -q:v 5 <Ziel>/bad_ffmpeg_sampling12.jpg
"""
import os
import sys
from PIL import Image

out = sys.argv[1] if len(sys.argv) > 1 else "samples"
os.makedirs(out, exist_ok=True)


def picture(w, h):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = ((x * 7) & 255, (y * 5) & 255, ((x + y) * 3) & 255)
    return img


n = 0
for (w, h) in [(320, 240), (640, 480), (1280, 720)]:
    img = picture(w, h)
    for q in (30, 75, 95):
        for sub in (0, 1, 2):                      # 4:4:4, 4:2:2, 4:2:0
            img.save(f"{out}/ok_{w}x{h}_q{q}_s{sub}.jpg", quality=q, subsampling=sub)
            n += 1
    img.save(f"{out}/ok_{w}x{h}_optimize.jpg", quality=75, optimize=True)
    img.save(f"{out}/ok_{w}x{h}_comment.jpg", quality=75, comment=b"Lela Test")
    img.save(f"{out}/ok_{w}x{h}_restart.jpg", quality=75, restart_marker_rows=2)
    img.convert("L").save(f"{out}/ok_{w}x{h}_gray.jpg", quality=75)
    img.save(f"{out}/bad_{w}x{h}_progressive.jpg", quality=75, progressive=True)
    n += 5
print(f"{n} Dateien in {out}")
