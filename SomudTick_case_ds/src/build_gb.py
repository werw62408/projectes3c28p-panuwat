"""SomudTick GB: a tall one-piece case like the original Game Boy (no hinge).

Front: the 2.8" screen turned sideways at the top (Game Boy games fill it), the red Joystick Shield below.
Back: the 0.91" OLED, the speaker grille, the SOMUDTICK logo.
Inside: ES3C28P without its acrylic, shield (whole), battery, speaker, PCF8574, OLED, power switch.

Coordinates (looking at the front): x right, y down from the top edge, z from the front face (0) to the back (T).
Printed parts (out_gb/*.stl): front_shell, back_plate, btn_cap x4, small_cap x2.
"""
import os, json
import numpy as np
import trimesh
from manifold3d import Manifold
from cad import *

OUT = os.path.join(os.path.dirname(__file__), "out_gb"); os.makedirs(OUT, exist_ok=True)

# ---------------- sizes (mm) ----------------
OW, OH, T = 96.0, 128.0, 24.0
RADII = (6.0, 6.0, 6.0, 22.0)          # corners: top-left, top-right, bottom-left, bottom-right (the Game Boy corner)
CH, WALL, PLATE = 1.2, 2.2, 2.0
TB = T - PLATE                          # inside of the back plate
IN = (WALL, WALL, OW - WALL, OH - WALL)

# ES3C28P without acrylic, turned so USB-C is at the right wall and its plugs face the top
PCB = (5.0, 11.0, 91.0, 61.0)
PCB_BACK = 8.0                          # glass front at z = PLATE, board back 6 mm behind it
STAND = 13.0                            # back of the brass standoffs
GLASS = (12.5, 12.0, 83.5, 60.0)
SCREWS = [(9.0, 15.0), (87.0, 15.0), (9.0, 57.0), (87.0, 57.0)]
MIC = (9.5, 21.5)
USB_C = (35.9, 9.5)                     # y, z ; right wall
USB_HOLE = (12.0, 7.0)
BOOT, RESET = (86.5, 23.9), (86.5, 46.7)
PLUGS = [(53.5, 5.0, 11.0), (40.5, 5.0, 11.0), (23.0, 5.0, 11.0), (68.0, 61.0, 66.0)]   # x, y0, y1 (last: battery)
SD_PORT = (41.0, 49.0, 61.0)            # x, y0, y1 (SD card: reach it by opening the back)

# behind the board (against the back plate)
SPK = (3.0, 18.0, 43.0, 46.0, TB - 11.6, TB - 1.6)  # 40 x 28 x 10, faces the back grille (clear of the lip)
BATT = (43.5, 18.0, 91.5, 48.0, TB - 10.7, TB - 0.7)   # above the USB-C socket
OLED = (46.0, 50.5, 84.0, 62.5, TB - 4.0, TB)       # 0.91" 38 x 12, pins toward the left (seen from the front)
OLED_WIN, OLED_DX, OLED_GLASS = (24.0, 7.0), 3.0, (30.0, 11.5)
PCF = (5.0, 47.0, 41.0, 67.0, TB - 5.0, TB)

# Joystick Shield (whole), its board front at z = SH_Z; big buttons stand toward the front
SHIELD = (8.0, 70.0, 69.0, 53.3)
SH_Z, SH_T = 18.3, 1.6
STICK_ON, STICK_H, STICK_CAP_D = (13.2, 32.0), 33.0, 25.0      # (EST: measure)
BTN_ON = {"A": (51.0, 14.8), "D": (41.0, 29.0), "B": (60.5, 29.0), "C": (51.0, 43.0)}
SMALL_ON = {"E": (37.8, 41.3), "F": (27.9, 41.3)}
BTN_H, SMALL_H = 15.0, 5.0                                     # (EST: measure)
STICK_HOLE_D, BTN_HOLE_D, BTN_CAP_D, SMALL_HOLE_D, SMALL_CAP_D = 27.0, 10.8, 10.0, 5.8, 4.8
CAP_OUT = 1.5                                                  # caps stand out of the front

SW = (82.0, 11.0)                       # y, z of the switch hole in the right wall (KCD11)
SW_HOLE = (13.0, 8.5)
SW_BODY = (78.8, 75.5, 93.8, 88.5, 6.5, 15.5)
BOSSES = [(5.0, 5.0), (91.0, 5.0), (5.0, 123.0), (89.0, 66.0)]
BOSS_R, PILOT_D, SCREW_D, CSK_R = 2.5, 2.0, 2.8, 2.9

sx0, sy0, sw, sh = SHIELD
STICK = (sx0 + STICK_ON[0], sy0 + STICK_ON[1])
BTNS = {k: (sx0 + v[0], sy0 + v[1]) for k, v in BTN_ON.items()}
SMALLS = {k: (sx0 + v[0], sy0 + v[1]) for k, v in SMALL_ON.items()}
SH_BACK = SH_Z + SH_T

# ================================================================== front shell
body = crbox4(0, 0, OW, OH, 0, T, RADII, CH)
inner_r = tuple(r - WALL for r in RADII)
cav = rbox4(IN[0], IN[1], IN[2], IN[3], PLATE, T + 1, inner_r)
front = body - cav - box(-5, -5, TB, OW + 5, OH + 5, T + 5)
for bx, by in BOSSES:
    front = front + cyl(bx, by, PLATE - 0.5, TB, BOSS_R) - cyl(bx, by, PLATE + 2, TB + 1, PILOT_D / 2, 24)
gx0, gy0, gx1, gy1 = GLASS
front = front - Manifold.batch_hull([box(gx0, gy0, 0.8, gx1, gy1, PLATE + 0.5), box(gx0 - 1.6, gy0 - 1.6, -0.5, gx1 + 1.6, gy1 + 1.6, 0.01)])
fr = 1.4                                                                    # engraved frame round the screen
FRAME = rbox(gx0 - fr - 0.8, gy0 - fr - 0.8, gx1 + fr + 0.8, gy1 + fr + 0.8, -1, 0.5, 3) - rbox(gx0 - fr, gy0 - fr, gx1 + fr, gy1 + fr, -2, 2, 2.2)
front = front - FRAME
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
LOGO_F = pixel_text("SOMUDTICK", 48.0 - 9 * 6 * PXF / 2, 63.6, PXF, -1, 0.5)          # under the screen, like "GAME BOY"
front = front - LOGO_F
front = front - slot_x(IN[2] - 1, OW + 1, USB_C[0], USB_C[1], *USB_HOLE)
front = front - box(IN[2] - 1, SW[0] - SW_HOLE[0] / 2, SW[1] - SW_HOLE[1] / 2, OW + 1, SW[0] + SW_HOLE[0] / 2, SW[1] + SW_HOLE[1] / 2)
front_shell = front

# ================================================================== back plate
back = body - box(-5, -5, -5, OW + 5, OH + 5, TB)
lip = rbox4(IN[0] + 0.35, IN[1] + 0.35, IN[2] - 0.35, IN[3] - 0.35, TB - 1.6, TB, tuple(r - 0.35 for r in inner_r)) \
    - rbox4(IN[0] + 1.6, IN[1] + 1.6, IN[2] - 1.6, IN[3] - 1.6, TB - 3, TB + 1, tuple(max(0.5, r - 1.6) for r in inner_r))
for bx, by in BOSSES: lip = lip - cyl(bx, by, TB - 3, TB + 1, BOSS_R + 0.5)
lip = lip - box(IN[2] - 18, SW[0] - 9, TB - 3, OW + 1, SW[0] + 9, TB + 1)
back = back + lip
for x, y in ((sx0 + 4, sy0 + 4), (sx0 + sw - 4, sy0 + 4), (sx0 + 4, sy0 + sh - 4), (sx0 + sw - 4, sy0 + sh - 4)):
    back = back + box(x - 2, y - 2, SH_BACK + 0.1, x + 2, y + 2, TB)                  # hold the shield when buttons are pressed
back = back + rim(OLED[0], OLED[1], OLED[2], OLED[3], TB - 2.0, TB)
ocx, ocy = (OLED[0] + OLED[2]) / 2 + OLED_DX, (OLED[1] + OLED[3]) / 2
ww, wh = OLED_WIN
back = back - Manifold.batch_hull([box(ocx - ww / 2, ocy - wh / 2, TB - 1, ocx + ww / 2, ocy + wh / 2, T - 0.6),
                                   box(ocx - ww / 2 - 1.2, ocy - wh / 2 - 1.2, T - 0.01, ocx + ww / 2 + 1.2, ocy + wh / 2 + 1.2, T + 1)])
for x, y in (BOOT, RESET): back = back - cyl(x, y, TB - 1, T + 1, 1.5, 16)
s = SPK
for i in range(7):                                                                        # speaker grille: slanted like the Game Boy
    y = s[1] + 4 + i * 3.4
    back = back - Manifold.batch_hull([cyl(s[0] + 6 + i * 1.2, y, TB - 1, T + 1, 0.9, 16), cyl(s[2] - 10 + i * 1.2, y + 3.0, TB - 1, T + 1, 0.9, 16)])
for bx, by in BOSSES:
    back = back - cyl(bx, by, TB - 3, T + 1, SCREW_D / 2, 24) - cyl(bx, by, T - 1.7, T + 0.01, SCREW_D / 2, 32, CSK_R)
PXB = 1.2
tw = 9 * 6 * PXB
lx0 = 48.0 - tw / 2
LOGO_B = pixel_text("SOMUDTICK", lx0, 92.0, PXB, T - 0.6, T + 1).mirror([1, 0, 0]).translate([2 * lx0 + tw, 0, 0])   # reads right from behind
LINES_B = union([box(10.0, y - 0.4, T - 0.5, ocx - ww / 2 - 3, y + 0.4, T + 1) + cyl(10.0, y, T - 0.5, T + 1, 1.3, 24) for y in (ocy - 3, ocy + 3)])
back = back - LOGO_B - LINES_B
back_plate = back

# ================================================================== caps
cap = cyl(0, 0, 0, 1.0, 6.75) + cyl(0, 0, 1.0, 1.0 + PLATE + 0.2 + CAP_OUT - 0.4, BTN_CAP_D / 2) \
    + cyl(0, 0, 1.0 + PLATE + 0.2 + CAP_OUT - 0.4, 1.0 + PLATE + 0.2 + CAP_OUT, BTN_CAP_D / 2, SEG, BTN_CAP_D / 2 - 0.4)
cap_z_back = SH_Z - BTN_H - 0.1                 # the cap's flange sits on the old cap top (seen from the front: z grows backward)
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
add("oled_pcb", box(OLED[0], OLED[1], OLED[4], OLED[2], OLED[3], OLED[4] + 1.2))
add("oled_glass", box(ocx - OLED_GLASS[0] / 2, ocy - OLED_GLASS[1] / 2, OLED[4] + 1.2, ocx + OLED_GLASS[0] / 2, ocy + OLED_GLASS[1] / 2, OLED[5] - 0.05))
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
# glow paint in the engraved lines (3D page only)
add("paint_cyan", FRAME ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_cyan", LOGO_F ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_pink", GLOW_BTN ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_cyan", LOGO_B ^ box(-5, -5, T - 0.55, OW + 5, OH + 5, T - 0.03))
add("paint_pink", LINES_B ^ box(-5, -5, T - 0.45, OW + 5, OH + 5, T - 0.03))

clash = 0
for name, m in parts.items():
    if name.startswith(("paint_", "print_")) or name in ("switch_rocker",): continue
    for sn, s_ in (("front_shell", front_shell), ("back_plate", back_plate)):
        v = (m ^ s_).volume()
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v)); clash += 1
names = [n for n in parts if not n.startswith(("paint_", "print_")) and n not in ("switch_rocker",)]
for i, a in enumerate(names):                                     # parts against each other
    for b in names[i + 1:]:
        if {a, b} <= {"stick_body", "stick_cap", "shield", "tact", "cap_y", "cap_b"} or {a, b} <= {"glass", "pcb", "brass", "port", "plug"}: continue
        if {a, b} <= {"pcf8574", "pcf8574_chip"} or {a, b} <= {"oled_pcb", "oled_glass"}: continue
        v = (parts[a] ^ parts[b]).volume()
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (a, b, v)); clash += 1
print("clashes:", clash)
print("size %g x %g x %g mm, the stick stands %.1f mm out of the front" % (OW, OH, T, STICK_H - SH_BACK))

# ================================================================== GLB for the 3D page
# three.js frame: x = x - OW/2, y = -(y - OH/2), z = -(z - T/2)  (a 180 degree turn: the front faces the viewer)
M = np.array([[1, 0, 0, -OW / 2], [0, -1, 0, OH / 2], [0, 0, -1, T / 2], [0, 0, 0, 1]], float)
scene = trimesh.Scene()
for name, m in [("front_shell", front_shell), ("back_plate", back_plate)] + list(parts.items()):
    t = tm(m); t.apply_transform(M)
    scene.add_geometry(t, node_name="B_" + name, geom_name="B_" + name)
scene.export(os.path.join(OUT, "somudtick_ds.glb"))
tot = sum(tm(m).volume for m in PRINT.values()) / 1000
meta = dict(KIND="gb", NAME="SomudTick GB", OW=OW, OH=OH, T=T, OD=OH, BH=0, LH=0, AX_D=0, AX_Z=0,
            GLASS=[gx0, gx1, gy0, gy1], GLASS_Z=PLATE - 0.1,
            OLED_WIN=[ocx - ww / 2, ocx + ww / 2, ocy - wh / 2, ocy + wh / 2], OLED_Z=TB + 0.02,
            SIZE="%g × %g × %g มม." % (OW, OH, T), HALVES="จอยโผล่ %g มม." % round(STICK_H - SH_BACK, 1), VOL="~%d ซม³" % round(tot, -1),
            PRINT={n: "×".join("%g" % round(v, 1) for v in tm(m).extents) for n, m in PRINT.items()},
            PCF="%g×%g" % (PCF[2] - PCF[0], PCF[3] - PCF[1]),
            SCREEN_NOTE="จอ ES3C28P ไม่ใส่อะคริลิก: กระจกถึงหลังบอร์ด 6",
            BUY=["น็อตเกลียวปล่อย M2.5×10 ×4 (ฝาหลัง)", "น็อต M3×8 หัวจม ×4 (ยึดจอเข้าเสาทองเหลืองเดิม)", "เทปโฟมสองหน้า (แบต ลำโพง PCF8574 OLED)"])
json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print("glb", os.path.getsize(os.path.join(OUT, "somudtick_ds.glb")) // 1024, "KB")
