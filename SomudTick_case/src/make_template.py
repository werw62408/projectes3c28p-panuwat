"""1:1 drilling template for the clear box (135 x 80 x 30, ~1.5 mm walls) -> out/template.html (print to PDF)."""
import os
from layout import *

WB = 1.5                          # clear box wall
D = 30.0                          # box depth
zb = lambda zi: WB + zi           # inside depth -> depth from the front face
RED, INK, SOFT, GUIDE = "#d6451b", "#1c2420", "#6b7770", "#9fb3bb"

def svg_open(w, h, x, y):
    return f'<svg x="{x}" y="{y}" width="{w}" height="{h}" viewBox="0 0 {w} {h}" overflow="visible">'

class Face:
    def __init__(s, w, h, map_):
        s.w, s.h, s.map, s.el = w, h, map_, []
    def P(s, a, b): return s.map(a, b)
    def rect(s, x0, y0, x1, y1, col=RED, dash=False, r=0.6, sw=0.35, fill="none"):
        (u0, v0), (u1, v1) = s.P(x0, y0), s.P(x1, y1)
        u0, u1 = sorted((u0, u1)); v0, v1 = sorted((v0, v1))
        da = ' stroke-dasharray="1.6 1.2"' if dash else ""
        s.el.append(f'<rect x="{u0:.2f}" y="{v0:.2f}" width="{u1-u0:.2f}" height="{v1-v0:.2f}" rx="{r}" fill="{fill}" stroke="{col}" stroke-width="{sw}"{da}/>')
    def hole(s, x, y, d, cross=True):
        u, v = s.P(x, y); r = d / 2
        s.el.append(f'<circle cx="{u:.2f}" cy="{v:.2f}" r="{r:.2f}" fill="none" stroke="{RED}" stroke-width="0.35"/>')
        if cross:
            c = max(r + 1.2, 2.2)
            s.el.append(f'<path d="M{u-c:.2f} {v:.2f}H{u+c:.2f}M{u:.2f} {v-c:.2f}V{v+c:.2f}" stroke="{RED}" stroke-width="0.2"/>')
    def slot(s, x, y, w, h):          # w, h in face units (already mapped orientation)
        u, v = s.P(x, y)
        s.el.append(f'<rect x="{u-w/2:.2f}" y="{v-h/2:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{min(w,h)/2:.2f}" fill="none" stroke="{RED}" stroke-width="0.35"/>')
        s.el.append(f'<path d="M{u-w/2-1.5:.2f} {v:.2f}H{u+w/2+1.5:.2f}M{u:.2f} {v-h/2-1.5:.2f}V{v+h/2+1.5:.2f}" stroke="{RED}" stroke-width="0.2"/>')
    def text(s, x, y, t, size=2.6, col=INK, anchor="middle", raw=False, weight=400):
        u, v = (x, y) if raw else s.P(x, y)
        s.el.append(f'<text x="{u:.2f}" y="{v:.2f}" font-size="{size}" fill="{col}" text-anchor="{anchor}" font-weight="{weight}">{t}</text>')
    def svg(s, x, y, title, sub):
        out = [svg_open(s.w, s.h, x, y)]
        out.append(f'<rect x="0" y="0" width="{s.w}" height="{s.h}" rx="3" fill="none" stroke="{INK}" stroke-width="0.5"/>')
        # corner ticks for cutting the paper along the outline
        out += s.el + ['</svg>']
        out.append(f'<text x="{x}" y="{y-6.5}" font-size="4" font-weight="700" fill="{INK}">{title}</text>')
        out.append(f'<text x="{x}" y="{y-2.2}" font-size="2.7" fill="{SOFT}">{sub}</text>')
        return "\n".join(out)

# ---------- front face (outside view = normal front view) ----------
F = Face(80, 135, lambda x, y: (x, y))
F.rect(*ACR, col=GUIDE, dash=True, r=2)
gx0, gy0, gx1, gy1 = GLASS
F.rect(gx0, gy0, gx1, gy1, r=2)
F.text(42.5, 42, "ตัดหน้าต่างจอ", 3.4, RED, weight=700)
F.text(42.5, 46.5, "49 × 71 มม.", 3, RED)
F.text(42.5, 51, "ทาบขอบกระจกจริงก่อนตัด", 2.4, SOFT)
for sx, sy in SCREWS: F.hole(sx, sy, 3.4)
F.text(42.5, 6.4, "รูน็อต M3 Ø3.4 (4 รู)", 2.3, RED)
F.hole(*MIC, 2.0); F.text(MIC[0] + 3, MIC[1] + 1, "ไมค์ Ø2", 2.2, RED, anchor="start")
F.hole(*JOY_STICK, JOY_HOLE_D); F.text(JOY_STICK[0], JOY_STICK[1] + 17.5, "รูจอย Ø26", 2.8, RED, weight=700)
F.text(40, 131.5, "▼ ขอบล่าง (ฝั่งจอย)", 2.4, SOFT)
F.text(40, 88.5 + 3, "", 1)

# ---------- back face (outside view: left/right swap) ----------
B = Face(80, 135, lambda x, y: (80 - x, y))
B.rect(*BATT[:4], col=GUIDE, dash=True, r=2); B.text(42, 36, "แบต (อยู่ข้างใน)", 2.3, GUIDE)
B.rect(*SPK[:4], col=GUIDE, dash=True, r=6)
for bx, by in (BOOT, RESET): B.hole(bx, by, 3.0)
B.text(BOOT[0], BOOT[1] + 5, "BOOT", 2.4, RED, weight=700); B.text(RESET[0], RESET[1] + 5, "RESET", 2.4, RED, weight=700)
for i in range(-3, 4):
    for j in range(-2, 3):
        B.hole(SPK_C[0] + i * 4.2, SPK_C[1] + j * 4.2, 2.0, cross=False)
B.text(42, 77.5, "รูลำโพง Ø2 ห่างกัน 4.2 มม. (35 รู)", 2.3, RED)
jx0, jy0, jx1, jy1 = JOY
B.rect(jx0, jy0, jx1, jy1, col=GUIDE, dash=True); B.text(40, 106, "จอยติดตรงนี้", 2.3, GUIDE); B.text(40, 110, "(ยกสูง 6.5 มม.)", 2.1, GUIDE)
B.rect(*DS[:4], col=GUIDE, dash=True); B.text((DS[0] + DS[2]) / 2, 108, "DS3231", 2.3, GUIDE)

# ---------- walls ----------
# top wall: looking down on it, screen side toward you (bottom of the drawing); u = x, v = D - z
T = Face(80, D, lambda x, z: (x, D - z))
T.slot(USB_C[0], zb(USB_C[1]), USB_HOLE[0], USB_HOLE[1])
T.text(USB_C[0], D - zb(USB_C[1]) - 5.5, "USB-C 12 × 7", 2.6, RED, raw=True, weight=700)
T.text(3, D - 1.5, "ซ้าย", 2.2, SOFT, anchor="start", raw=True); T.text(77, D - 1.5, "ขวา", 2.2, SOFT, anchor="end", raw=True)
T.text(40, D - 1.2, "ขอบด้านจอ", 2.2, SOFT, raw=True); T.text(40, 3, "ขอบด้านหลัง", 2.2, SOFT, raw=True)

# right wall: seen from the right, screen side on the left; u = z, v = y
R = Face(D, 135, lambda z, y: (z, y))
R.hole(zb(LED[1]), LED[0], LED_D); R.text(zb(LED[1]) + 4.5, LED[0] + 1, "IR Ø5.3", 2.3, RED, anchor="start", weight=700)
R.slot(zb(SD[1]), SD[0], SD_HOLE[1], SD_HOLE[0]); R.text(zb(SD[1]) + 4, SD[0] + 1, "SD 4×14", 2.3, RED, anchor="start", weight=700)
R.text(2, 131, "ด้านจอ", 2.2, SOFT, anchor="start", raw=True)
R.text(15, 5.5, "▲ บน (USB-C)", 2.2, SOFT, raw=True)

# left wall: seen from the left, screen side on the right; u = D - z, v = y
Lw = Face(D, 135, lambda z, y: (D - z, y))
Lw.slot(zb(SW[1]), SW[0], SW_HOLE[1], SW_HOLE[0])
Lw.text(D - zb(SW[1]), SW[0] - 9, "สวิตช์ 8.5×13", 2.3, RED, raw=True, weight=700)
Lw.text(D - 2, 131, "ด้านจอ", 2.2, SOFT, anchor="end", raw=True)
Lw.text(15, 5.5, "▲ บน (USB-C)", 2.2, SOFT, raw=True)

def ruler(x, y):
    s = [f'<g transform="translate({x},{y})">', f'<rect x="0" y="0" width="50" height="4" fill="none" stroke="{INK}" stroke-width="0.3"/>']
    for i in range(0, 51):
        h = 4 if i % 10 == 0 else (2.5 if i % 5 == 0 else 1.4)
        s.append(f'<path d="M{i} 0V{h}" stroke="{INK}" stroke-width="0.18"/>')
    s.append(f'<text x="0" y="8" font-size="2.8" text-anchor="start" fill="{INK}">ต้องยาวพอดี 50 มม. ถ้าไม่ตรง ให้พิมพ์ใหม่แบบ "ขนาดจริง / 100%"</text></g>')
    return "\n".join(s)

def note(x, y, lines, size=3.1, gap=4.6):
    return "\n".join(f'<text x="{x}" y="{y + i * gap:.1f}" font-size="{size}" fill="{INK}">{t}</text>' for i, t in enumerate(lines))

page1 = f'''<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297" font-family="Loma, 'IBM Plex Sans Thai', sans-serif">
<text x="15" y="12" font-size="5.2" font-weight="700" fill="{INK}">SomudTick · แบบเจาะกล่องใส 135×80×30  (หน้า 1/2)</text>
{F.svg(15, 26, "ฝาหน้า", "มองจากด้านหน้า · USB-C อยู่บน")}
{B.svg(115, 26, "ฝาหลัง", "มองจากด้านหลัง · USB-C อยู่บน (ซ้าย-ขวากลับกันแล้ว)")}
{ruler(15, 172)}
{note(15, 190, [
 "วิธีใช้",
 "1. พิมพ์ขนาดจริง 100% แล้ววัดไม้บรรทัด 50 มม. ข้างบนก่อน",
 "2. ตัดกระดาษตามกรอบดำ แปะเทปกาวบนกล่องให้ขอบตรงกัน",
 "3. ใช้เหล็กแหลมจิ้มจุดกากบาทแดง แล้วเจาะเล็ก (Ø2) ก่อน ค่อยขยาย",
 "4. หน้าต่างจอ: เจาะรูมุม 4 จุด แล้วตัด/ตะไบเข้าหาเส้น",
 "5. เส้นประสีฟ้า = ตำแหน่งของข้างใน (ไม่ต้องเจาะ)",
 "",
 "สำคัญ: ทาบจอจริงก่อนเจาะรูน็อตและหน้าต่างจอเสมอ",
 "ตำแหน่งในแบบนี้วัดจากรูปถ่าย อาจคลาดได้ 1-2 มม.",
])}
</svg>'''

page2 = f'''<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297" font-family="Loma, 'IBM Plex Sans Thai', sans-serif">
<text x="15" y="12" font-size="5.2" font-weight="700" fill="{INK}">SomudTick · แบบเจาะกล่องใส: ผนังข้าง  (หน้า 2/2)</text>
{Lw.svg(15, 26, "ผนังซ้าย", "มองจากทางซ้าย")}
{R.svg(62, 26, "ผนังขวา", "มองจากทางขวา")}
{T.svg(110, 26, "ผนังบน", "มองลงจากข้างบน")}
{ruler(110, 72)}
{note(110, 92, [
 "ความลึกนับจากผิวฝาหน้า",
 "USB-C กลางรูลึก 12 มม.",
 "หลอด IR กลางรูลึก 10.5 มม.",
 "ช่อง SD กลางรูลึก 12 มม.",
 "สวิตช์ กลางรูลึก 11.5 มม.",
 "",
 "ก่อนเจาะผนัง: ประกอบจอ",
 "เข้าฝาหน้าก่อน แล้วเล็ง",
 "USB-C กับช่อง SD ของจริง",
 "ขยับรูตามของจริงได้เลย",
 "",
 "ผนังล่างไม่ต้องเจาะ",
], size=3.1, gap=5)}
</svg>'''

html = f'''<!doctype html><html><head><meta charset="utf-8"><title>SomudTick template</title>
<style>@page{{size:A4;margin:0}} html,body{{margin:0}} svg{{display:block;page-break-after:always}}</style></head>
<body>{page1}{page2}</body></html>'''
os.makedirs("out", exist_ok=True)
open("out/template.html", "w").write(html)
print("ok")
