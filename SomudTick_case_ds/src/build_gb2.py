"""SomudTick GB2: one-piece Game Boy style case with TWO screens on the front.

The small 0.91" OLED sits above the big screen (it will show the top bar and the menu),
the big 2.8" screen (turned sideways) below it, the red Joystick Shield at the bottom.
Its plugs on the board's top edge sit behind the OLED (different depth), so the case grows only 7 mm.

    STYLE=classic | pocket | cyber  python3 build_gb2.py   -> out_gb2_<style>/

Coordinates (looking at the front): x right, y down from the top edge, z from the front face (0) to the back (T).
"""
import os, json, math
import numpy as np
import trimesh
from manifold3d import Manifold
from cad import *

STYLE = os.environ.get("STYLE", "classic")
OUT = os.path.join(os.path.dirname(__file__), "out_gb2_" + STYLE); os.makedirs(OUT, exist_ok=True)

OW, OH, T = 96.0, 135.0, 24.0
CH, WALL, PLATE = 1.2, 2.2, 2.0
TB = T - PLATE

# ---------------- outline: circle centres + radius per corner (the hull of them is the case) ----------------
if STYLE == "classic":      # original Game Boy: small corners, one big corner bottom-right
    CORNERS = [(6, 6, 6), (OW - 6, 6, 6), (6, OH - 6, 6), (OW - 22, OH - 22, 22)]
    NAME, SHELL, LENS = "SomudTick GB Classic", 0xb9b8b0, True
elif STYLE == "pocket":     # Game Boy Pocket: even round corners, black
    CORNERS = [(10, 10, 10), (OW - 10, 10, 10), (10, OH - 10, 10), (OW - 10, OH - 10, 10)]
    NAME, SHELL, LENS = "SomudTick GB Pocket", 0x1b1e24, True
else:                       # cyber: cut corners, neon lines
    R = 3.0
    CORNERS = [(12, R, R), (R, 12, R), (OW - 12, R, R), (OW - R, 12, R), (4.0, OH - 4.0, 4.0),
               (OW - R, OH - 20, R), (OW - 20, OH - R, R)]
    NAME, SHELL, LENS = "SomudTick GB Cyber", 0x1d222b, False

def outline(z0, z1, grow=0.0, ch=0.0):
    parts = []
    for x, y, r in CORNERS:
        rr = max(0.3, r + grow)
        if ch > 0:
            parts += [cyl(x, y, z0, z0 + ch, max(0.3, rr - ch), SEG, rr), cyl(x, y, z0 + ch, z1 - ch, rr), cyl(x, y, z1 - ch, z1, rr, SEG, max(0.3, rr - ch))]
        else:
            parts.append(cyl(x, y, z0, z1, rr))
    return Manifold.batch_hull(parts)

# ---------------- parts (EST = measure the real one) ----------------
OLED = (26.0, 4.5, 64.0, 16.5)            # 0.91" 38 x 12, glass against the face plate, pins toward the left
OLED_T, OLED_DX = 4.0, 3.0
OLED_WIN, OLED_GLASS = (24.0, 7.0), (30.0, 11.5)
PCB = (5.0, 18.0, 91.0, 68.0)             # ES3C28P sideways (no acrylic), plugs on its top edge
PCB_BACK, STAND = 8.0, 13.0
GLASS = (12.5, 19.0, 83.5, 67.0)
SCREWS = [(9.0, 22.0), (87.0, 22.0), (9.0, 64.0), (87.0, 64.0)]
MIC = (9.5, 28.5)
USB_C = (42.9, 9.5); USB_HOLE = (12.0, 7.0)
BOOT, RESET = (86.5, 30.9), (86.5, 53.7)
PLUGS = [(53.5, 12.0, 18.0), (40.5, 12.0, 18.0), (23.0, 12.0, 18.0), (68.0, 68.0, 73.0)]
SD_PORT = (41.0, 56.0, 68.0)
SPK = (3.0, 25.0, 43.0, 53.0, TB - 11.6, TB - 1.6)
BATT = (43.5, 25.0, 91.5, 55.0, TB - 10.7, TB - 0.7)
PCF = (5.0, 55.0, 41.0, 75.0, TB - 5.0, TB)
SHIELD = (8.0, 77.0, 69.0, 53.3)
SH_Z, SH_T = 18.3, 1.6
STICK_ON, STICK_H, STICK_CAP_D = (13.2, 32.0), 33.0, 25.0
BTN_ON = {"A": (51.0, 14.8), "D": (41.0, 29.0), "B": (60.5, 29.0), "C": (51.0, 43.0)}
SMALL_ON = {"E": (37.8, 41.3), "F": (27.9, 41.3)}
BTN_H, SMALL_H = 15.0, 5.0
STICK_HOLE_D, BTN_HOLE_D, BTN_CAP_D, SMALL_HOLE_D, SMALL_CAP_D = 27.0, 10.8, 10.0, 5.8, 4.8
CAP_OUT = 1.5
SW = (89.0, 11.0); SW_HOLE = (13.0, 8.5)
SW_BODY = (78.8, 82.5, 93.8, 95.5, 6.5, 15.5)
BOSSES = [(9.0, 9.0), (87.0, 9.0), (89.0, 73.0), (85.0, 113.0)] if STYLE == "cyber" else [(7.0, 7.0), (89.0, 7.0), (89.0, 73.0), (85.0, 113.0)]
BOSS_R, PILOT_D, SCREW_D, CSK_R = 2.5, 2.0, 2.8, 2.9

sx0, sy0, sw, sh = SHIELD
STICK = (sx0 + STICK_ON[0], sy0 + STICK_ON[1])
BTNS = {k: (sx0 + v[0], sy0 + v[1]) for k, v in BTN_ON.items()}
SMALLS = {k: (sx0 + v[0], sy0 + v[1]) for k, v in SMALL_ON.items()}
SH_BACK = SH_Z + SH_T
ocx, ocy = (OLED[0] + OLED[2]) / 2 + OLED_DX, (OLED[1] + OLED[3]) / 2
gx0, gy0, gx1, gy1 = GLASS
ww, wh = OLED_WIN
def surf(z0, z1): return box(-5, -5, z0, OW + 5, OH + 5, z1)

# ================================================================== front shell
body = outline(0, T, 0, CH)
front = body - outline(PLATE, T + 1, -WALL) - surf(TB, T + 5)
for bx, by in BOSSES:
    front = front + cyl(bx, by, PLATE - 0.5, TB, BOSS_R) - cyl(bx, by, PLATE + 2, TB + 1, PILOT_D / 2, 24)
win = lambda x0, y0, x1, y1: Manifold.batch_hull([box(x0, y0, 0.8, x1, y1, PLATE + 0.5), box(x0 - 1.4, y0 - 1.4, -0.5, x1 + 1.4, y1 + 1.4, 0.01)])
front = front - win(gx0, gy0, gx1, gy1) - win(ocx - ww / 2, ocy - wh / 2, ocx + ww / 2, ocy + wh / 2)
front = front + rim(OLED[0], OLED[1], OLED[2], OLED[3], PLATE, PLATE + 2.0)              # holds the OLED in place
GLOW = []
LENS_P = None
if LENS:   # one dark "lens" panel round both screens (0.8 mm recess: paint it or glue a dark plate)
    LENS_P = rbox(8.0, 3.0, 88.0, 76.5, -1, 0.8, 4.0)
    front = front - LENS_P
else:      # cyber: engraved frames round both screens + lines along the cut corners
    GLOW.append(rbox(gx0 - 2.2, gy0 - 2.2, gx1 + 2.2, gy1 + 2.2, -1, 0.5, 3) - rbox(gx0 - 1.4, gy0 - 1.4, gx1 + 1.4, gy1 + 1.4, -2, 2, 2.2))
    GLOW.append(rbox(ocx - ww / 2 - 2.2, ocy - wh / 2 - 2.2, ocx + ww / 2 + 2.2, ocy + wh / 2 + 2.2, -1, 0.5, 2) - rbox(ocx - ww / 2 - 1.4, ocy - wh / 2 - 1.4, ocx + ww / 2 + 1.4, ocy + wh / 2 + 1.4, -2, 2, 1.4))
    def diag(x0, y0, x1, y1, w=0.8):
        L = math.hypot(x1 - x0, y1 - y0); a = math.degrees(math.atan2(y1 - y0, x1 - x0))
        return box(0, -w / 2, -1, L, w / 2, 0.5).rotate([0, 0, a]).translate([x0, y0, 0])
    GLOW.append(diag(OW - 7.0, OH - 25.0, OW - 25.0, OH - 7.0))                           # along the big cut corner
    GLOW.append(diag(OW - 11.0, OH - 27.0, OW - 27.0, OH - 11.0))
GLOW_F = union(GLOW) if GLOW else None
if GLOW_F: front = front - GLOW_F
front = front - cyl(*MIC, -1, PLATE + 1, 1.0, 16)
for x, y in SCREWS:
    front = front - cyl(x, y, -1, PLATE + 1, 1.7, 24) - cyl(x, y, -0.01, 1.6, 3.2, 32, 1.6)
front = front - cyl(*STICK, -1, PLATE + 1, STICK_HOLE_D / 2, 96) - cyl(*STICK, -0.01, 1.0, STICK_HOLE_D / 2 + 1.0, 96, STICK_HOLE_D / 2)
for p in BTNS.values():
    front = front - cyl(*p, -1, PLATE + 1, BTN_HOLE_D / 2, 48) - cyl(*p, -0.01, 0.6, BTN_HOLE_D / 2 + 0.6, 48, BTN_HOLE_D / 2)
for p in SMALLS.values():
    front = front - cyl(*p, -1, PLATE + 1, SMALL_HOLE_D / 2, 32)
GLOW_BTN = union([ring(*p, -1, 0.5, 6.8, 7.6) for p in BTNS.values()] + [ring(*p, -1, 0.5, 3.8, 4.6) for p in SMALLS.values()])
front = front - GLOW_BTN
PXF = 0.9
LOGO_F = pixel_text("SOMUDTICK", 48.0 - 9 * 6 * PXF / 2, 69.6, PXF, -1, 0.5)
front = front - LOGO_F
front = front - slot_x(OW - WALL - 1, OW + 1, USB_C[0], USB_C[1], *USB_HOLE)
front = front - box(OW - WALL - 1, SW[0] - SW_HOLE[0] / 2, SW[1] - SW_HOLE[1] / 2, OW + 1, SW[0] + SW_HOLE[0] / 2, SW[1] + SW_HOLE[1] / 2)
front_shell = front

# ================================================================== back plate
back = body - surf(-5, TB)
lip = outline(TB - 1.6, TB, -WALL - 0.35) - outline(TB - 3, TB + 1, -WALL - 1.6)
for bx, by in BOSSES: lip = lip - cyl(bx, by, TB - 3, TB + 1, BOSS_R + 0.5)
lip = lip - box(OW - WALL - 18, SW[0] - 9, TB - 3, OW + 1, SW[0] + 9, TB + 1)
back = back + lip
for x, y in ((sx0 + 4, sy0 + 4), (sx0 + sw - 4, sy0 + 4), (sx0 + 4, sy0 + sh - 4), (sx0 + sw - 4, sy0 + sh - 4)):
    back = back + box(x - 2, y - 2, SH_BACK + 0.1, x + 2, y + 2, TB)
for x, y in (BOOT, RESET): back = back - cyl(x, y, TB - 1, T + 1, 1.5, 16)
s = SPK
for i in range(7):
    y = s[1] + 4 + i * 3.4
    back = back - Manifold.batch_hull([cyl(s[0] + 6 + i * 1.2, y, TB - 1, T + 1, 0.9, 16), cyl(s[2] - 10 + i * 1.2, y + 3.0, TB - 1, T + 1, 0.9, 16)])
for bx, by in BOSSES:
    back = back - cyl(bx, by, TB - 3, T + 1, SCREW_D / 2, 24) - cyl(bx, by, T - 1.7, T + 0.01, SCREW_D / 2, 32, CSK_R)
PXB = 1.2; tw = 9 * 6 * PXB; lx0 = 48.0 - tw / 2
LOGO_B = pixel_text("SOMUDTICK", lx0, 98.0, PXB, T - 0.6, T + 1).mirror([1, 0, 0]).translate([2 * lx0 + tw, 0, 0])
back = back - LOGO_B
back_plate = back

# ================================================================== caps
cap = cyl(0, 0, 0, 1.0, 6.75) + cyl(0, 0, 1.0, 1.0 + PLATE + 0.2 + CAP_OUT - 0.4, BTN_CAP_D / 2) \
    + cyl(0, 0, 1.0 + PLATE + 0.2 + CAP_OUT - 0.4, 1.0 + PLATE + 0.2 + CAP_OUT, BTN_CAP_D / 2, SEG, BTN_CAP_D / 2 - 0.4)
cap_z_back = SH_Z - BTN_H - 0.1
small_len = (SH_Z - SMALL_H - 0.1) + CAP_OUT
small = cyl(0, 0, 0, small_len, SMALL_CAP_D / 2, 32) + cyl(0, 0, small_len - CAP_OUT - PLATE - 1.2, small_len - CAP_OUT - PLATE - 0.2, 4.0, 32)

PRINT = {"front_shell": front_shell, "back_plate": back_plate, "btn_cap": cap, "small_cap": small}
for n, m in PRINT.items():
    t = tm(m); assert t.is_watertight, n
    t.export(os.path.join(OUT, n + ".stl"))
    print("%-11s watertight vol %6.1f cm3  size %s" % (n, t.volume / 1000, np.round(t.extents, 1).tolist()))

# ================================================================== electronics + clash check
parts = {}
def add(name, m): parts[name] = parts[name] + m if name in parts else m
def pbox(name, t, r=1.0): add(name, rbox(t[0], t[1], t[2], t[3], t[4], t[5], r))
px0, py0, px1, py1 = PCB
add("glass", box(gx0 + 0.4, gy0 + 0.4, PLATE, gx1 - 0.4, gy1 - 0.4, PLATE + 4.4))
add("pcb", box(px0, py0, PCB_BACK - 1.6, px1, py1, PCB_BACK))
for x, y in SCREWS: add("brass", cyl(x, y, PCB_BACK, STAND, 2.3, 6))
add("port", box(px1 - 7.5, USB_C[0] - 4.5, PCB_BACK, px1, USB_C[0] + 4.5, PCB_BACK + 3.2))
add("port", box(SD_PORT[0] - 7, SD_PORT[1], PCB_BACK, SD_PORT[0] + 7, SD_PORT[2], PCB_BACK + 1.9))
for x, y0, y1 in PLUGS: add("plug", box(x - 3.5, y0, PCB_BACK, x + 3.5, y1, PCB_BACK + 5))
pbox("speaker", SPK, 3); pbox("battery", BATT, 2)
add("oled_pcb", box(OLED[0], OLED[1], PLATE + OLED_T - 1.2, OLED[2], OLED[3], PLATE + OLED_T))
add("oled_glass", box(ocx - OLED_GLASS[0] / 2, ocy - OLED_GLASS[1] / 2, PLATE + 0.05, ocx + OLED_GLASS[0] / 2, ocy + OLED_GLASS[1] / 2, PLATE + OLED_T - 1.2))
pbox("pcf8574", (PCF[0], PCF[1], PCF[2], PCF[3], PCF[5] - 1.6, PCF[5]))
add("pcf8574_chip", box(PCF[0] + 10, PCF[1] + 6, PCF[4], PCF[0] + 22, PCF[1] + 14, PCF[5] - 1.6))
add("shield", box(sx0, sy0, SH_Z, sx0 + sw, sy0 + sh, SH_BACK))
add("stick_body", box(STICK[0] - 8, STICK[1] - 8, SH_Z - 13, STICK[0] + 8, STICK[1] + 8, SH_Z))
add("stick_body", Manifold.sphere(7.5, 32).translate([STICK[0], STICK[1], SH_Z - 13]))
add("stick_body", cyl(*STICK, SH_BACK - STICK_H + 6, SH_Z - 13, 3, 24))
add("stick_cap", cyl(*STICK, SH_BACK - STICK_H, SH_BACK - STICK_H + 7, STICK_CAP_D / 2, 64))
for k, p in BTNS.items():
    add("tact", box(p[0] - 6, p[1] - 6, SH_Z - 7, p[0] + 6, p[1] + 6, SH_Z))
    add("cap_" + ("y" if k in "AC" else "b"), cyl(*p, SH_Z - BTN_H, SH_Z - 7, 5.8, 32))
    add("print_cap_" + ("y" if k in "AC" else "b"), cap.mirror([0, 0, 1]).translate([p[0], p[1], cap_z_back]))
for p in SMALLS.values():
    add("tact", box(p[0] - 3, p[1] - 3, SH_Z - SMALL_H, p[0] + 3, p[1] + 3, SH_Z))
    add("print_small", small.mirror([0, 0, 1]).translate([p[0], p[1], SH_Z - SMALL_H - 0.1]))
add("switch", box(SW_BODY[0], SW_BODY[1], SW_BODY[4], SW_BODY[2], SW_BODY[3], SW_BODY[5]))
add("switch_rocker", box(OW - 0.2, SW[0] - 5.5, SW[1] - 3.2, OW + 1.4, SW[0] + 5.5, SW[1] + 3.2))
skin = surf(0.03, 0.45)
if GLOW_F: add("paint_cyan", GLOW_F ^ skin)
add("paint_cyan", LOGO_F ^ (surf(0.03, 0.45) if not LENS else surf(0.03, 0.45)))
add("paint_pink", GLOW_BTN ^ skin)
add("paint_cyan", LOGO_B ^ surf(T - 0.55, T - 0.03))
if LENS:
    lens = (LENS_P ^ surf(0.05, 0.75)) - win(gx0, gy0, gx1, gy1) - win(ocx - ww / 2, ocy - wh / 2, ocx + ww / 2, ocy + wh / 2) - (LOGO_F ^ surf(-2, 2))
    add("lens", lens)

clash = 0
for name, m in parts.items():
    if name.startswith(("paint_", "print_")) or name in ("switch_rocker", "lens"): continue
    for sn, s_ in (("front_shell", front_shell), ("back_plate", back_plate)):
        v = (m ^ s_).volume()
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v)); clash += 1
names = [n for n in parts if not n.startswith(("paint_", "print_")) and n not in ("switch_rocker", "lens")]
same = [{"stick_body", "stick_cap", "shield", "tact", "cap_y", "cap_b"}, {"glass", "pcb", "brass", "port", "plug"}, {"pcf8574", "pcf8574_chip"}, {"oled_pcb", "oled_glass"}]
for i, a in enumerate(names):
    for b in names[i + 1:]:
        if any({a, b} <= g for g in same): continue
        v = (parts[a] ^ parts[b]).volume()
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (a, b, v)); clash += 1
print("clashes:", clash)
print("%s: %g x %g x %g mm, the stick stands %.1f mm out of the front" % (NAME, OW, OH, T, STICK_H - SH_BACK))

# ================================================================== GLB for the 3D page
M = np.array([[1, 0, 0, -OW / 2], [0, -1, 0, OH / 2], [0, 0, -1, T / 2], [0, 0, 0, 1]], float)
scene = trimesh.Scene()
for name, m in [("front_shell", front_shell), ("back_plate", back_plate)] + list(parts.items()):
    t = tm(m); t.apply_transform(M)
    scene.add_geometry(t, node_name="B_" + name, geom_name="B_" + name)
scene.export(os.path.join(OUT, "somudtick_ds.glb"))
tot = sum(tm(m).volume for m in PRINT.values()) / 1000
meta = dict(KIND="gb", GB2=True, NAME=NAME, SHELL=SHELL, OW=OW, OH=OH, T=T, OD=OH, BH=0, LH=0, AX_D=0, AX_Z=0,
            GLASS=[gx0, gx1, gy0, gy1], GLASS_Z=PLATE - 0.1,
            OLED_WIN=[ocx - ww / 2, ocx + ww / 2, ocy - wh / 2, ocy + wh / 2], OLED_Z=PLATE - 0.1, OLED_FRONT=True,
            SIZE="%g × %g × %g มม." % (OW, OH, T), HALVES="จอยโผล่ %g มม." % round(STICK_H - SH_BACK, 1), VOL="~%d ซม³" % round(tot, -1),
            PRINT={n: "×".join("%g" % round(v, 1) for v in tm(m).extents) for n, m in PRINT.items()},
            PCF="%g×%g" % (PCF[2] - PCF[0], PCF[3] - PCF[1]),
            SCREEN_NOTE="จอ ES3C28P ไม่ใส่อะคริลิก: กระจกถึงหลังบอร์ด 6",
            BUY=["น็อตเกลียวปล่อย M2.5×10 ×4 (ฝาหลัง)", "น็อต M3×8 หัวจม ×4 (ยึดจอเข้าเสาทองเหลืองเดิม)", "เทปโฟมสองหน้า (แบต ลำโพง PCF8574)"])
json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print("glb", os.path.getsize(os.path.join(OUT, "somudtick_ds.glb")) // 1024, "KB")
