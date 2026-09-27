#!/usr/bin/env python3
# make_vlw.py: turn a TTF font into a smooth (anti-aliased) VLW font for LovyanGFX, written as a C array.
#   python3 make_vlw.py out.h NAME font.ttf size [weight] [chars]
# weight: for variable fonts (e.g. 700 = bold), 0 = leave as is. chars: default = printable ASCII.
# Used for the v11.8 HUD theme (Orbitron and Chakra Petch, both SIL Open Font License).
import sys, struct
from PIL import ImageFont, Image, ImageDraw

out, name, path, size = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
weight = int(sys.argv[5]) if len(sys.argv) > 5 else 0
chars = sys.argv[6] if len(sys.argv) > 6 else ''.join(chr(c) for c in range(0x20, 0x7F))
f = ImageFont.truetype(path, size)
if weight:
    try: f.set_variation_by_axes([weight])
    except Exception as e: print('no variation:', e)
asc, desc = f.getmetrics()
glyphs = []
for ch in sorted(set(chars), key=ord):
    adv = round(f.getlength(ch))
    box = f.getbbox(ch, anchor='ls')   # left, top, right, bottom around the baseline (top < 0)
    if ch == ' ' or box[2] <= box[0] or box[3] <= box[1]:
        glyphs.append((ord(ch), 0, 0, adv, 0, 0, b'')); continue
    l, t, r, b = box
    w, h = r - l, b - t
    im = Image.new('L', (w, h), 0)
    ImageDraw.Draw(im).text((-l, -t), ch, font=f, fill=255, anchor='ls')
    glyphs.append((ord(ch), h, w, adv, -t, l, im.tobytes()))
data = struct.pack('>6i', len(glyphs), 11, size, 0, asc, desc)
for g in glyphs: data += struct.pack('>7i', g[0], g[1], g[2], g[3], g[4], g[5], 0)
for g in glyphs: data += g[6]
with open(out, 'a') as o:
    o.write('const uint8_t %s[] PROGMEM = {  // %s %dpx, %d glyphs, %d bytes\n' % (name, path.split('/')[-1], size, len(glyphs), len(data)))
    for i in range(0, len(data), 24): o.write(','.join('0x%02X' % b for b in data[i:i + 24]) + ',\n')
    o.write('};\n')
print(name, len(glyphs), 'glyphs', len(data), 'bytes')
