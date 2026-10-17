"""SomudTick GB3: the smallest one-piece Game Boy style case, with raised ribs inside that hold every board.

  VARIANT=na   the NA011 button board (87 x 53) as it is
  VARIANT=diy  stick + buttons on a cut piece of green perfboard (80 x 40): 13 mm shorter, the board is also the hub

Front: the 2.8" screen at the top (USB-C left wall, SD card from the top edge), the controls below.
Top edge: power switch (like the Game Boy), IR LED + IR receiver, SD card slot.
Back: the 1.3" OLED (window), speaker grille, logo. 18650 battery across the bottom.
Coordinates (looking at the front): x right, y down from the top edge, z from the front face (0) to the back (T).
Printed parts (out_gb3_<variant>/*.stl): front_shell, back_plate, btn_cap x4, small_cap x2.
"""
import os, sys, json
import numpy as np
import trimesh
from manifold3d import Manifold
from cad import box, cyl, rbox, rbox4, crbox4, xcyl, ycyl, slot_x, slot_y, ring, rim, union, pixel_text, tm

VARIANT = os.environ.get("VARIANT", sys.argv[1] if len(sys.argv) > 1 else "na")
assert VARIANT in ("na", "diy")
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_gb3_" + VARIANT); os.makedirs(OUT, exist_ok=True)

# ---------------- shell ----------------
WALL, PLATE, CH, G = 2.0, 2.0, 1.0, 0.3          # G = gap round every board
OW = 92.0
T = 29.0
TB = T - PLATE                                    # inside of the back plate (27)
RIB = 1.2                                         # rib thickness

# ---------------- ES3C28P (no acrylic), USB-C to the LEFT wall, SD card toward the top edge ----------------
PCB = (2.3, 7.3, 88.3, 57.3)                     # 86 x 50; 5 mm above it for the battery plug
PCB_Z = (6.5, 8.0)                               # board front / back (glass front at z = PLATE)
GLASS = (10.7, 7.3, 79.9, 57.3, PLATE, PLATE + 2.3)
VIS = (48.37 - 29.73, 32.3 - 22.6, 48.37 + 29.73, 32.3 + 22.6)   # lit area 59.45 x 45.2, 3.07 away from the USB side
BACK_Z = 12.0                                    # tallest parts behind the board
SCREWS = [(6.3, 11.3), (84.3, 11.3), (6.3, 53.3), (84.3, 53.3)]   # M3 through the face into the brass pillars
USB = (32.3, 9.5); USB_HOLE = (12.0, 7.0)        # y, z (left wall)
SD = (52.3, 9.0); SD_HOLE = (14.0, 4.0)          # x, z (top wall)
PLUGS = [(39.8, 57.3, 63.3), (52.8, 57.3, 63.3), (70.3, 57.3, 63.3)]   # I2C, IO, speaker (+ wires), bottom edge
BAT_PLUG = (25.3, 2.3, 7.3)

# ---------------- controls ----------------
if VARIANT == "na":
    CB = (2.5, 58.3, 89.5, 111.3)                # NA011 87 x 53, top edge (pads, yellow header) up
    CB_T = 1.5
    def on(p): return (CB[0] + p[0], CB[1] + p[1])
    STICK = on((18.5, 30.9))
    BTNS = {"A": on((65.0, 18.4)), "B": on((77.1, 29.9)), "C": on((65.0, 42.0)), "D": on((53.1, 29.9))}
    SMALLS = {"E": on((47.5, 41.6)), "F": on((34.5, 41.6))}
    HOLES = [on((4.2, 2.5)), on((82.8, 17.5))]
    HEADER = (CB[0] + 46, CB[1] + 5.5, CB[0] + 58, CB[1] + 10.5)
else:
    CB = (6.0, 58.3, 86.0, 100.3)                # perfboard 80 x 42 (31 x 16 holes), cut from a 7 x 9 cm board
    CB_T = 1.6
    def on(p): return (CB[0] + p[0], CB[1] + p[1])
    STICK = on((16.0, 23.0))
    BTNS = {"A": on((62.0, 11.5)), "B": on((73.5, 23.0)), "C": on((62.0, 34.5)), "D": on((50.5, 23.0))}
    SMALLS = {"E": on((40.0, 36.5)), "F": on((31.0, 36.5))}
    HOLES = [on((3.0, 3.0)), on((77.0, 39.0))]  # drill 2.5 mm here
    HEADER = None
CB_FRONT = 17.0                                  # control board front: the stick knob stands 13 mm out
CB_BACK = CB_FRONT + CB_T
KNOB_D, JOY_TOTAL = 20.0, 30.0
BTN_TOP, CAP_D, SMALL_TOP = 14.0, 11.5, 5.0     # 12x12 buttons with round caps / 6x6 for E, F
STICK_HOLE_D, BTN_HOLE_D, EXT_D, EXT_FOOT_D, SMALL_HOLE_D, PLUNGER_D = 26.0, 10.8, 10.0, 11.4, 5.6, 4.8
CAP_OUT = 0.8

# ---------------- battery strip, top-edge parts, back parts ----------------
BATT_Y0 = CB[3] + 1.0
BATT = (2.5, BATT_Y0 + 0.5, 74.5, BATT_Y0 + 24.5, TB - 24.3, TB - 0.3)   # 18650 + BMS 72 x 24 x 24, wires to the right
OH = BATT[3] + 0.5 + WALL
RADII = (6.0, 6.0, 6.0, 20.0)                    # bottom-right: the Game Boy corner
SW_BODY = (30.0, 2.0, 42.0, 11.0, 14.0, 22.5)    # rocker 12 x 9 (into the case) x 8.5, top wall
SW_C = (36.0, 18.25); SW_HOLE = (12.5, 9.0)
KY005 = (8.0, 2.5, 26.0, 17.5, 21.0, TB)         # IR LED board, LED out of the top wall
IR_LED = (17.0, 23.5); IR_LED_D = 5.4
KY022 = (70.0, 2.5, 85.0, 21.0, 21.0, TB)        # IR receiver board, receiver at the top wall
IR_RX = (77.5, 23.5); IR_RX_WIN = (7.6, 8.0)
SPK = (47.0, 26.0, 87.0, 54.0, TB - 10.0, TB)    # 40 x 28 x 10, faces the back grille
DS3231 = (4.0, 31.5, 42.0, 53.0, TB - 9.0, TB)   # 38 x 21.5 x 9 (coin cell toward the back)
OLED = (9.5, CB[1] + 2.5, 45.0, CB[1] + 36.0, TB - 3.7, TB) if VARIANT == "na" else (10.5, CB[1] + 2.5, 46.0, CB[1] + 36.0, TB - 3.7, TB)   # 1.3" 35.5 x 33.5, window in the back plate
OLED_WIN = (31.0, 16.5, 1.9)                     # w, h, glass sits this much toward the pin edge (pins up)
PCF = (48.0, CB[1] + 2.5, 83.0, CB[1] + 21.5, TB - 5.0, TB) if VARIANT == "na" else (49.5, CB[1] + 2.5, 84.5, CB[1] + 21.5, TB - 5.0, TB)   # 35 x 19, pin headers off
HUB = (52.0, CB[1] + 24.0, 82.0, CB[1] + 44.0, TB - 4.0, TB) if VARIANT == "na" else None   # the DIY board is its own hub
BOSSES = [(4.6, 4.6), (87.4, 4.6), (80.0, OH - 9.0)]
BOSS_R, PILOT_D, SCREW_D, CSK_R = 2.4, 2.0, 2.8, 2.9

IN = (WALL, WALL, OW - WALL, OH - WALL)
inner_r = tuple(r - WALL for r in RADII)

# ================================================================== front shell
body = crbox4(0, 0, OW, OH, 0, T, RADII, CH)
cav = rbox4(IN[0], IN[1], IN[2], IN[3], PLATE, T + 1, inner_r)
front = body - cav - box(-5, -5, TB, OW + 5, OH + 5, T + 5)
for bx, by in BOSSES:
    front = front + cyl(bx, by, PLATE - 0.5, TB, BOSS_R) - cyl(bx, by, PLATE + 2, TB + 1, PILOT_D / 2, 24)
# screen window (chamfered), engraved frame, screws + spacer posts, rib corners round the board
vx0, vy0, vx1, vy1 = VIS
front = front - Manifold.batch_hull([box(vx0 - 0.3, vy0 - 0.3, 0.8, vx1 + 0.3, vy1 + 0.3, PLATE + 0.5), box(vx0 - 1.6, vy0 - 1.6, -0.5, vx1 + 1.6, vy1 + 1.6, 0.01)])
fr = 1.6
FRAME = rbox(vx0 - fr - 0.8, vy0 - fr - 0.8, vx1 + fr + 0.8, vy1 + fr + 0.8, -1, 0.5, 3) - rbox(vx0 - fr, vy0 - fr, vx1 + fr, vy1 + fr, -2, 2, 2.2)
front = front - FRAME
for x, y in SCREWS:
    front = front + cyl(x, y, PLATE - 0.5, PCB_Z[0], 3.0) - cyl(x, y, -1, PCB_Z[0] + 1, 1.7, 24) - cyl(x, y, -0.01, 1.6, 3.2, 32, 1.6)
def corner_ribs(b, z0, z1, L=7.0, skip=()):
    """L-shaped ribs that hug the 4 corners of a board (b = x0, y0, x1, y1) with the gap G"""
    x0, y0, x1, y1 = b[0] - G, b[1] - G, b[2] + G, b[3] + G
    out = []
    for cx, cy, sx, sy, name in ((x0, y0, 1, 1, "tl"), (x1, y0, -1, 1, "tr"), (x0, y1, 1, -1, "bl"), (x1, y1, -1, -1, "br")):
        if name in skip: continue
        ex = cx - sx * RIB
        ey = cy - sy * RIB
        out.append(box(min(ex, cx + sx * L), min(ey, cy), z0, max(ex, cx + sx * L), max(ey, cy), z1))   # along x
        out.append(box(min(ex, cx), min(ey, cy + sy * L), z0, max(ex, cx), max(ey, cy + sy * L), z1))   # along y
    return union(out)
front = front + (corner_ribs(PCB, PLATE - 0.5, PCB_Z[1] + 1.0, skip=("tl", "bl")) - box(IN[0] - 1, -5, -1, IN[0] + 0.5, OH + 5, T + 5))   # left corners: the wall holds it
# controls: holes, plunger sleeves, front posts at the board's holes, ribs round the board, a rib between screen and controls
front = front - cyl(*STICK, -1, PLATE + 1, STICK_HOLE_D / 2, 96) - cyl(*STICK, -0.01, 1.2, STICK_HOLE_D / 2 + 1.2, 96, STICK_HOLE_D / 2)
for p in BTNS.values():
    front = front - cyl(*p, -1, PLATE + 1, BTN_HOLE_D / 2, 48) - cyl(*p, -0.01, 0.6, BTN_HOLE_D / 2 + 0.6, 48, BTN_HOLE_D / 2)
for p in SMALLS.values():
    front = front + cyl(*p, PLATE - 0.5, 8.0, 3.9, 32) - cyl(*p, -1, 9.0, SMALL_HOLE_D / 2, 32)
for x, y in HOLES:
    front = front + cyl(x, y, PLATE - 0.5, CB_FRONT, 2.8, 32) - cyl(x, y, CB_FRONT - 8, CB_FRONT + 1, 1.0, 16)   # M2.5 self-tapping from behind
front = front + corner_ribs(CB, PLATE - 0.5, CB_BACK + 1.0, L=6.0, skip=("tl", "tr"))
front = front + corner_ribs(CB, PCB_Z[1] + 0.6, CB_BACK + 1.0, L=6.0, skip=("bl", "br"))   # top corners sit behind the screen board's edge (they hang from the side walls)
GLOW = union([ring(*p, -1, 0.5, 6.6, 7.4) for p in BTNS.values()] + [ring(*p, -1, 0.5, 3.6, 4.4) for p in SMALLS.values()])
front = front - GLOW
PXF = 0.75
LOGO_F = pixel_text("SOMUDTICK", OW / 2 - 9 * 6 * PXF / 2, vy1 + 2.6, PXF, -1, 0.5)   # under the screen, like "GAME BOY"
front = front - LOGO_F
# walls: USB-C (left), SD + IR + switch (top)
front = front - slot_x(-1, IN[0] + 1, USB[0], USB[1], *USB_HOLE)
front = front - slot_y(-1, IN[1] + 1, SD[0], SD[1], *SD_HOLE)
front = front - ycyl(-1, IN[1] + 1, IR_LED[0], IR_LED[1], IR_LED_D / 2, 32)
front = front - slot_y(-1, IN[1] + 1, IR_RX[0], IR_RX[1], *IR_RX_WIN)
front = front - box(SW_C[0] - SW_HOLE[0] / 2, -1, SW_C[1] - SW_HOLE[1] / 2, SW_C[0] + SW_HOLE[0] / 2, IN[1] + 1, SW_C[1] + SW_HOLE[1] / 2)
front_shell = front

# ================================================================== back plate (with the ribs that hold the back parts)
back = body - box(-5, -5, -5, OW + 5, OH + 5, TB)
lip = rbox4(IN[0] + 0.35, IN[1] + 0.35, IN[2] - 0.35, IN[3] - 0.35, TB - 1.6, TB, tuple(r - 0.35 for r in inner_r)) \
    - rbox4(IN[0] + 1.6, IN[1] + 1.6, IN[2] - 1.6, IN[3] - 1.6, TB - 3, TB + 1, tuple(max(0.5, r - 1.6) for r in inner_r))
for bx, by in BOSSES: lip = lip - cyl(bx, by, TB - 3, TB + 1, BOSS_R + 0.5)
for t in (KY005, KY022, BATT, (SW_BODY[0], SW_BODY[1], SW_BODY[2], SW_BODY[3])):
    lip = lip - box(t[0] - 1.5, t[1] - 3, TB - 3, t[2] + 1.5, t[3] + 3, TB + 1)
back = back + lip
# posts that press the control board from behind (buttons don't push it back)
if VARIANT == "na":
    cb_posts = [(CB[0] + 4, CB[1] + 4), (CB[2] - 4, CB[1] + 4), (CB[0] + 4, CB[3] - 4), (CB[2] - 4, CB[3] - 4), ((CB[0] + CB[2]) / 2, CB[3] - 4)]
else:
    cb_posts = [(8.0, CB[1] + 3), (47.75, CB[1] + 4), (8.0, CB[3] - 3), (47.75, CB[3] - 3), (84.0, CB[3] - 3)]
for x, y in cb_posts:
    back = back + box(x - 1.5, y - 1.5, CB_BACK + 2.5, x + 1.5, y + 1.5, TB)
# battery cradle: two saddles shaped to the cell, end stops
bcx, bcz = None, None
by0, by1 = BATT[1], BATT[3]
bz_c = BATT[5] - 9.25                                      # the round cell (18.5) lies against the back plate
for x in (BATT[0] + 12, BATT[2] - 16):
    back = back + (box(x - 1.5, by0 - 0.5, TB - 8, x + 1.5, by1 + 0.5, TB) - xcyl(x - 3, x + 3, (by0 + by1) / 2, bz_c, 9.6, 48))
for x in (BATT[0] - 1.5, BATT[2] + 0.3):
    back = back + box(x, by0 + 4, TB - 6, x + 1.2, by1 - 4, TB)
# rims for the other parts
for t in (SPK, DS3231, PCF, KY005, KY022) + ((HUB,) if HUB else ()):
    back = back + rim(t[0], t[1], t[2], t[3], TB - 2.5, TB, RIB, G)
back = back + rim(OLED[0], OLED[1], OLED[2], OLED[3], TB - 2.5, TB, RIB, G)
ocx, ocy = (OLED[0] + OLED[2]) / 2, (OLED[1] + OLED[3]) / 2 - OLED_WIN[2]
ww, wh = OLED_WIN[0], OLED_WIN[1]
back = back - Manifold.batch_hull([box(ocx - ww / 2, ocy - wh / 2, TB - 1, ocx + ww / 2, ocy + wh / 2, T - 0.6),
                                   box(ocx - ww / 2 - 1.2, ocy - wh / 2 - 1.2, T - 0.01, ocx + ww / 2 + 1.2, ocy + wh / 2 + 1.2, T + 1)])
s = SPK
for i in range(6):                                          # speaker grille, slanted like the Game Boy
    y = s[1] + 5 + i * 3.4
    back = back - Manifold.batch_hull([cyl(s[0] + 7 + i * 1.2, y, TB - 1, T + 1, 0.9, 16), cyl(s[2] - 12 + i * 1.2, y + 3.0, TB - 1, T + 1, 0.9, 16)])
for bx, by in BOSSES:
    back = back - cyl(bx, by, TB - 3, T + 1, SCREW_D / 2, 24) - cyl(bx, by, T - 1.7, T + 0.01, SCREW_D / 2, 32, CSK_R)
PXB = 1.0
tw = 9 * 6 * PXB
lx0 = OW / 2 - tw / 2
LOGO_B = pixel_text("SOMUDTICK", lx0, BATT[1] + 9.0, PXB, T - 0.6, T + 1).mirror([1, 0, 0]).translate([2 * lx0 + tw, 0, 0])
back = back - LOGO_B
back_plate = back

# ================================================================== caps (printed): foot on the coloured cap, stem through the face
cap_h = CB_FRONT - BTN_TOP - 0.1 + CAP_OUT          # from the coloured cap top (z = 3) out to -0.8
btn_cap = cyl(0, 0, 0, 0.7, EXT_FOOT_D / 2) + cyl(0, 0, 0.7, cap_h - 0.4, EXT_D / 2) + cyl(0, 0, cap_h - 0.4, cap_h, EXT_D / 2, 64, EXT_D / 2 - 0.4)
pl_h = CB_FRONT - SMALL_TOP - 0.1 + CAP_OUT
small_cap = cyl(0, 0, 0, 1.0, 3.5, 32) + cyl(0, 0, 1.0, pl_h, PLUNGER_D / 2, 32)

PRINT = {"front_shell": front_shell, "back_plate": back_plate, "btn_cap": btn_cap, "small_cap": small_cap}
for n, m in PRINT.items():
    t = tm(m); assert t.is_watertight, n
    t.export(os.path.join(OUT, n + ".stl"))
    print("%-11s watertight vol %6.1f cm3  size %s" % (n, t.volume / 1000, np.round(t.extents, 1).tolist()))

# ================================================================== electronics
parts = {}
def add(name, m): parts[name] = parts[name] + m if name in parts else m
def pbox(name, t, r=0.8): add(name, rbox(t[0], t[1], t[2], t[3], t[4], t[5], r))
px0, py0, px1, py1 = PCB
add("glass", box(GLASS[0] + 0.3, GLASS[1] + 0.3, GLASS[4] + 0.02, GLASS[2] - 0.3, GLASS[3] - 0.3, GLASS[5]))
add("pcb", box(px0, py0, PCB_Z[0], px1, py1, PCB_Z[1]))
add("pcb", box(px0 + 9, py0 + 7, PCB_Z[1], px1 - 8, py1 - 7, BACK_Z))
for x, y in SCREWS: add("brass", cyl(x, y, PCB_Z[1], PCB_Z[1] + 5.0, 2.3, 6))
add("port", box(px0 - 3.0, USB[0] - 4.5, USB[1] - 1.6, px0 + 7.5, USB[0] + 4.5, USB[1] + 1.6))
add("port", box(SD[0] - 7, py0 + 0.5, PCB_Z[1], SD[0] + 7, py0 + 12, PCB_Z[1] + 1.9))
for x, a, b in PLUGS: add("plug", box(x - 3.5, a, PCB_Z[1], x + 3.5, b, PCB_Z[1] + 5))
add("plug", box(BAT_PLUG[0] - 3.5, BAT_PLUG[1], PCB_Z[1], BAT_PLUG[0] + 3.5, BAT_PLUG[2], PCB_Z[1] + 5))
add("board" if VARIANT == "diy" else "shield", box(CB[0], CB[1], CB_FRONT, CB[2], CB[3], CB_BACK))
sx, sy = STICK
add("stick_body", box(sx - 8, sy - 8, CB_FRONT - 12, sx + 8, sy + 8, CB_FRONT))
add("stick_body", Manifold.sphere(7.0, 32).translate([sx, sy, CB_FRONT - 12]))
add("stick_body", cyl(sx, sy, CB_FRONT - JOY_TOTAL + 5.5, CB_FRONT - 12, 2.5, 24))
add("stick_cap", cyl(sx, sy, CB_FRONT - JOY_TOTAL, CB_FRONT - JOY_TOTAL + 6, KNOB_D / 2, 64))
COLS = {"A": "b", "B": "y", "C": "r", "D": "g"}
for k, p in BTNS.items():
    add("tact", box(p[0] - 6, p[1] - 6, CB_FRONT - 5, p[0] + 6, p[1] + 6, CB_FRONT))
    add("cap_" + COLS[k], cyl(*p, CB_FRONT - BTN_TOP, CB_FRONT - 5, CAP_D / 2, 32))
    add("print_cap_" + COLS[k], btn_cap.mirror([0, 0, 1]).translate([p[0], p[1], CB_FRONT - BTN_TOP - 0.1]))
for p in SMALLS.values():
    add("tact", box(p[0] - 3, p[1] - 3, CB_FRONT - SMALL_TOP, p[0] + 3, p[1] + 3, CB_FRONT))
    add("print_small", small_cap.mirror([0, 0, 1]).translate([p[0], p[1], CB_FRONT - SMALL_TOP - 0.1]))
if HEADER: add("header", box(HEADER[0], HEADER[1], CB_FRONT - 8.5, HEADER[2], HEADER[3], CB_FRONT))
add("joints", box(CB[0] + 1, CB[1] + 1, CB_BACK, CB[2] - 1, CB[3] - 1, CB_BACK + 2.0))   # solder joints (pins cut)
CELL_R = 9.25
cell_y, cell_z = (BATT[1] + BATT[3]) / 2, BATT[5] - CELL_R
add("battery", xcyl(BATT[0], BATT[2] - 12, cell_y, cell_z, CELL_R, 48))
pbox("battery", (BATT[2] - 12, BATT[1], BATT[2], BATT[3], BATT[4], BATT[5]), 2)      # BMS end with the wires (24 x 24)
pbox("speaker", SPK, 3)
pbox("ds3231", (DS3231[0], DS3231[1], DS3231[2], DS3231[3], DS3231[5] - 1.6, DS3231[5]))
add("coin", cyl((DS3231[0] + DS3231[2]) / 2 + 6, (DS3231[1] + DS3231[3]) / 2, DS3231[4], DS3231[5] - 1.6, 10.2))
add("oled_pcb", box(OLED[0], OLED[1], OLED[5] - 1.2, OLED[2], OLED[3], OLED[5]))
add("oled_glass", box(ocx - 17.3, ocy - 9.2, OLED[4], ocx + 17.3, ocy + 9.2, OLED[5] - 1.2))
pbox("pcf8574", (PCF[0], PCF[1], PCF[2], PCF[3], PCF[5] - 1.6, PCF[5]))
add("pcf8574_chip", box(PCF[0] + 10, PCF[1] + 6, PCF[4], PCF[0] + 22, PCF[1] + 14, PCF[5] - 1.6))
if HUB: pbox("hub", HUB, 0.4)
pbox("ky005", (KY005[0], KY005[1], KY005[2], KY005[3], KY005[5] - 1.6, KY005[5]))
add("led", ycyl(0.6, KY005[1] + 3, IR_LED[0], IR_LED[1], 2.5, 24))
pbox("ky022", (KY022[0], KY022[1], KY022[2], KY022[3], KY022[5] - 1.6, KY022[5]))
add("ir_rx", box(IR_RX[0] - 3.5, KY022[1] + 0.2, IR_RX[1] - 3.5, IR_RX[0] + 3.5, KY022[1] + 6, KY022[5] - 1.6))
add("switch", box(SW_BODY[0], SW_BODY[1], SW_BODY[4], SW_BODY[2], SW_BODY[3], SW_BODY[5]))
add("switch_rocker", box(SW_C[0] - 5.5, -1.4, SW_C[1] - 3.2, SW_C[0] + 5.5, 0.2, SW_C[1] + 3.2))
add("paint_cyan", FRAME ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_cyan", LOGO_F ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_pink", GLOW ^ box(-5, -5, 0.03, OW + 5, OH + 5, 0.45))
add("paint_cyan", LOGO_B ^ box(-5, -5, T - 0.55, OW + 5, OH + 5, T - 0.03))

# ================================================================== clash check
clash = 0
SKIP = ("switch_rocker", "led", "ir_rx", "port")
for name, m in parts.items():
    if name.startswith(("paint_", "print_")) or name in SKIP: continue
    for sn, s_ in (("front_shell", front_shell), ("back_plate", back_plate)):
        v = (m ^ s_).volume()
        if v > 0.5:
            bb = (m ^ s_).bounding_box()
            print("  CLASH %-12s with %s: %.1f mm3 near %s" % (name, sn, v, [round(c, 1) for c in bb])); clash += 1
for pn in [n for n in parts if n.startswith("print_")]:
    for sn, s_ in (("front_shell", front_shell), ("back_plate", back_plate)):
        v = (parts[pn] ^ s_).volume()
        if v > 0.5: print("  CLASH %s with %s: %.1f mm3" % (pn, sn, v)); clash += 1
GROUPS = [{"stick_body", "stick_cap", "shield", "board", "tact", "cap_b", "cap_y", "cap_r", "cap_g", "header", "joints"},
          {"glass", "pcb", "brass", "port", "plug"}, {"pcf8574", "pcf8574_chip"}, {"oled_pcb", "oled_glass"}, {"ds3231", "coin"},
          {"switch", "switch_rocker"}, {"ky005", "led"}, {"ky022", "ir_rx"}]
names = [n for n in parts if not n.startswith(("paint_", "print_"))]
for i, a in enumerate(names):
    for b in names[i + 1:]:
        if any({a, b} <= g for g in GROUPS): continue
        v = (parts[a] ^ parts[b]).volume()
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (a, b, v)); clash += 1
print("clashes:", clash)
knob = JOY_TOTAL - CB_FRONT
print("%s: %g x %.1f x %g mm, stick %.1f mm out, caps %.1f tall, plungers %.1f long" % (VARIANT, OW, OH, T, knob, cap_h, pl_h))

# ================================================================== GLB + meta (3D page, KIND gb)
M = np.array([[1, 0, 0, -OW / 2], [0, -1, 0, OH / 2], [0, 0, -1, T / 2], [0, 0, 0, 1]], float)
scene = trimesh.Scene()
for name, m in [("front_shell", front_shell), ("back_plate", back_plate)] + list(parts.items()):
    t = tm(m); t.apply_transform(M)
    scene.add_geometry(t, node_name="B_" + name, geom_name="B_" + name)
scene.export(os.path.join(OUT, "somudtick_ds.glb"))
tot = sum(tm(m).volume for m in PRINT.values()) / 1000
name = "SomudTick GB3" + (" NA" if VARIANT == "na" else " DIY")
meta = dict(KIND="gb", NAME=name, VARIANT=VARIANT, OW=OW, OH=round(OH, 1), T=T, OD=OH, BH=0, LH=0, AX_D=0, AX_Z=0,
            GLASS=[vx0, vx1, vy0, vy1], GLASS_Z=PLATE - 0.1,
            OLED_WIN=[ocx - ww / 2, ocx + ww / 2, ocy - wh / 2, ocy + wh / 2], OLED_Z=TB + 0.02,
            SIZE="%g × %g × %g มม." % (OW, round(OH), T), HALVES="จอยโผล่ %g มม." % round(knob, 1), VOL="~%d ซม³" % round(tot, -1),
            PRINT={n: "×".join("%g" % round(v, 1) for v in tm(m).extents) for n, m in PRINT.items()},
            PCF="35×19", CLASHES=clash, CAP_H=round(cap_h, 1), PL_H=round(pl_h, 1),
            SCREEN_NOTE="จอ ES3C28P ไม่ใส่อะคริลิก: กระจกถึงหลังบอร์ด 6 ✓ วัดแล้ว",
            BUY=["น็อตเกลียวปล่อย M2.5×8 ×5 (ฝาหลัง 3 + บอร์ดปุ่ม 2)", "น็อต M3×10 หัวจม ×4 (ยึดจอเข้าเสาทองเหลืองเดิม)", "เทปโฟมสองหน้าบาง (ลำโพง, DS3231, PCF8574, OLED)"]
            + (["แผ่นปริ๊นท์อเนกประสงค์ 7×9 ซม. ตัดเหลือ 80×40", "จอยอนาล็อก PS2 (แบบเดียวกับ NA011) 1 ตัว", "ปุ่มกด 12×12 + หมวกกลม ×4, ปุ่มกด 6×6 สูง 5 มม. ×2"] if VARIANT == "diy" else []))
json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print("glb", os.path.getsize(os.path.join(OUT, "somudtick_ds.glb")) // 1024, "KB")
