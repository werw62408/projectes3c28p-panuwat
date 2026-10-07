"""Layout 4 with the owner's real parts: each part is cut out of the owner's photo (parts_photo/*.png), scaled to its
measured size and put where make_box4.py places it. Seen from the FRONT, as if the face were glass.

    python make_real_layout4.py <owner photo of the parts>   -> parts_photo/*.png (cut outs), out_box4/real_layout.png

The photo is only cut once (the cut outs are kept); without an argument the kept cut outs are used.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont
import make_box4 as M

HERE = os.path.dirname(os.path.abspath(__file__))
PH = os.path.join(HERE, "parts_photo")
# where each part is in the owner's photo (6 Oct 2026, 1932 x 2576), and how to turn it: (box, degrees anticlockwise)
CUTS = {"oled": ((882, 612, 1250, 920), 0), "screen": ((470, 915, 1250, 1410), 0), "shield": ((440, 1405, 1260, 1925), 0),
        "battery": ((400, 1940, 1170, 2135), 0), "ds3231": ((1280, 1535, 1525, 1870), 90), "speaker": ((440, 620, 840, 920), 0),
        "ir_tx": ((715, 315, 850, 615), 0), "ir_rx": ((560, 445, 715, 610), 0), "switch": ((1130, 480, 1335, 610), 0),
        "perf": ((1240, 935, 1600, 1270), 0)}
S = 6.0                     # pixels a mm in the picture
FONT = "/usr/share/fonts/opentype/tlwg/Loma.otf"   # (Thai letters)
if not os.path.exists(FONT): FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

def cut(photo):
    im = Image.open(photo).convert("RGB"); os.makedirs(PH, exist_ok=True)
    for k, (b, rot) in CUTS.items(): im.crop(b).rotate(rot, expand=True).save(os.path.join(PH, k + ".png"))

def picture(path):
    W, H = int(M.BOX_W * S), int(M.BOX_H * S)
    pad = 40
    img = Image.new("RGB", (W + 2 * pad + 330, H + 2 * pad), (236, 238, 240)); d = ImageDraw.Draw(img, "RGBA")
    f = ImageFont.truetype(FONT, 15); fb = ImageFont.truetype(FONT, 17)
    X = lambda x: pad + x * S; Y = lambda y: pad + y * S
    d.rounded_rectangle([X(0), Y(0), X(M.BOX_W), Y(M.BOX_H)], radius=20, fill=(200, 203, 206), outline=(90, 90, 90), width=3)
    d.rectangle([X(M.IX0), Y(M.IY0), X(M.IX1), Y(M.IY1)], fill=(222, 224, 226), outline=(150, 150, 150))
    B = {b[0]: b for b in M.boxes()}
    def put(key, name, alpha=255, w=None, h=None):
        n, cx, cy, bw, bh = B[name][:5]; bw = w or bw; bh = h or bh
        p = Image.open(os.path.join(PH, key + ".png")).resize((int(bw * S), int(bh * S)))
        if alpha < 255: p.putalpha(alpha)
        img.paste(p, (int(X(cx - bw / 2)), int(Y(cy - bh / 2))), p if alpha < 255 else None)
        d.rectangle([X(cx - bw / 2), Y(cy - bh / 2), X(cx + bw / 2), Y(cy + bh / 2)], outline=(255, 140, 0) if alpha < 255 else (40, 40, 40), width=2)
        return cx, cy, bw, bh
    # layer 1: on the face (seen through it)
    marks = []
    for key, name, num, *th in (("screen", "screen board", 2), ("oled", 'OLED 1.3"', 1), ("shield", "NA011 button board", 4), ("battery", "18650 battery", 6, 18.5),
                           ("ir_tx", "IR LED board", 10), ("ir_rx", "IR receiver board", 11), ("switch", "switch", 12)):
        cx, cy, bw, bh = put(key, name, h=th[0] if th else None); marks.append((num, cx, cy))
    # layer 2: behind (half see-through, orange edge)
    for key, name, num in (("perf", "hub board", 7), ("ds3231", "DS3231 clock", 8), ("speaker", "speaker", 13)):
        cx, cy, bw, bh = put(key, name, 150); marks.append((num, cx, cy))
    n, cx, cy, bw, bh = B["PCF8574"][:5]
    d.rectangle([X(cx - bw / 2), Y(cy - bh / 2), X(cx + bw / 2), Y(cy + bh / 2)], fill=(122, 79, 176, 150), outline=(255, 140, 0), width=2); marks.append((9, cx, cy))
    d.text((X(cx), Y(cy) + 18), "PCF8574", fill="white", font=f, anchor="mm")
    for pn, x, y, h, half in M.POSTS:
        if half == "F" or h > 0 and pn not in M.CUT_POSTS:
            r = M.POST_R * S; d.ellipse([X(x) - r, Y(y) - r, X(x) + r, Y(y) + r], outline=(60, 60, 60, 200), width=2, fill=(255, 255, 255, 70) if half == "F" else None)
    for num, x, y in marks:
        d.ellipse([X(x) - 15, Y(y) - 15, X(x) + 15, Y(y) + 15], fill=(27, 33, 38), outline="white", width=2); d.text((X(x), Y(y)), str(num), fill="white", font=fb, anchor="mm")
    # side notes
    tx = W + 2 * pad + 6
    lines = ["มองจากด้านหน้ากล่อง", "(เหมือนหน้ากล่องเป็นกระจก)", "", "กรอบดำ = ติดกับหน้ากล่อง", "กรอบส้ม = อยู่ชั้นหลัง", "วงกลม = เสา", "", "ด้านบน = หัวกล่อง",
             "(ด้านที่มีเสาน็อต 2 ต้น)", "ผนังซ้าย = USB-C", "ผนังล่าง มุมซ้าย = สวิตช์", "", "1 จอเล็ก", "2 จอใหญ่", "4 บอร์ดปุ่ม", "6 แบต 18650", "7 แผงรวมสาย", "8 DS3231",
             "9 PCF8574", "10 หลอดส่ง IR", "11 ตัวรับ IR", "12 สวิตช์", "13 ลำโพง"]
    for i, t in enumerate(lines): d.text((tx, pad + i * 24), t, fill=(30, 30, 30), font=f)
    img.save(path)

if __name__ == "__main__":
    if len(sys.argv) > 1: cut(sys.argv[1])
    picture(os.path.join(HERE, "out_box4", "real_layout.png"))
    print("written out_box4/real_layout.png")
