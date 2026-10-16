"""Build the SomudTick DS-B case (clamshell, big screen on top, small screen + controls below).

Printed parts (out_b/*.stl):
  base_shell   base walls + face (stick hole, button holes, OLED window)   base_floor  bottom (speaker grille)
  lid_shell    lid walls + screen face + stick pocket                      lid_back    back plate (logo)
  btn_cap x4   caps that stand on the coloured buttons                      small_cap x2  plungers for E F
Also out_b/somudtick_ds.glb (all parts + electronics, for the 3D page), meta.json and a clash check.
Coordinates as in layout_b.py; mirrored y -> -y at export like build_ds.py.
"""
import os, json
import numpy as np
from manifold3d import Manifold
from cad import box, cyl, rbox, crbox4, xcyl, ycyl, slot_x, slot_y, ring, rim, union, pixel_text, tm
from layout_b import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_b"); os.makedirs(OUT, exist_ok=True)
W = WALL
R4 = (R_CORNER,) * 4
def crbox(x0, y0, x1, y1, z0, z1, r, ch): return crbox4(x0, y0, x1, y1, z0, z1, (r, r, r, r), ch)
def knuckle_ranges(a, b):
    L = (b - a - 2 * HINGE_GAP) / 3
    return (a, a + L), (a + L + HINGE_GAP, b - L - HINGE_GAP), (b - L, b)
def na(p): return (NA_X0 + p[0], NA_D0 + p[1])

STICK = na(JOY)
BTNS = {k: na(p) for k, p in BTN.items()}
SMALLS = {k: na(p) for k, p in SMALL.items()}
HOLES = [na(p) for p in NA_HOLES]
NA_BOT_Z = NA_TOP_Z - NA_T
KNOB_TOP = NA_TOP_Z + JOY_TOTAL
CAP_TOP = NA_TOP_Z + BTN_TOP
SMALL_TOP_Z = NA_TOP_Z + SMALL_TOP
CLUSTER = (sum(p[0] for p in BTNS.values()) / 4, sum(p[1] for p in BTNS.values()) / 4)
ox0, od0, ox1, od1 = OLED
ocx = (ox0 + ox1) / 2
win_cd = od0 + OLED_WIN[2]
WIN = (ocx - OLED_WIN[0] / 2, win_cd - OLED_WIN[1] / 2, ocx + OLED_WIN[0] / 2, win_cd + OLED_WIN[1] / 2)
OLED_Z = (FACE_BOT - OLED_T, FACE_BOT)

# ================================================================== BASE
base_body = crbox(0, 0, OW, OD, 0, BH, R_CORNER, CHAMFER)
base_cav = rbox(W, W, OW - W, OD - W, PLATE, FACE_BOT, 1.5)   # small inner corners: the board and the battery reach them
below = box(-5, -20, -5, OW + 5, OD + 5, PLATE)
shell = base_body - base_cav - below
shell = shell + (rbox(CHAMFER, CHAMFER, OW - CHAMFER, OD - CHAMFER, BH - 0.01, BH + RIM_H, R_CORNER - CHAMFER)
                 - rbox(CHAMFER + 2.2, CHAMFER + 2.2, OW - CHAMFER - 2.2, OD - CHAMFER - 2.2, BH - 1, BH + 2, R_CORNER - CHAMFER - 2.2))
for bx, bd in BOSSES:
    shell = shell + cyl(bx, bd, PLATE, FACE_BOT + 0.5, BOSS_R) - cyl(bx, bd, PLATE - 1, FACE_BOT - 2, PILOT_D / 2, 24)
# posts from the face down to the button board (an M3 from under the floor goes through the board into them)
for x, d in HOLES:
    shell = shell + cyl(x, d, NA_TOP_Z + 0.05, FACE_BOT + 0.5, 3.0) - cyl(x, d, NA_TOP_Z - 1, NA_TOP_Z + 12, 1.25, 24)
for mx, md in MAGNETS:
    shell = shell + cyl(mx, md, BH - MAG_H - 1.4, FACE_BOT + 0.5, 4.5) - cyl(mx, md, BH - MAG_H, BH + 1, MAG_D / 2, 32)
# stick hole (the knob tilts into it), button holes, E/F plunger sleeves
r = JOY_HOLE_D / 2
shell = shell - cyl(*STICK, FACE_BOT - 1, BH + 1, r, 96) - cyl(*STICK, BH - 1.5, BH + 0.01, r, 96, r + 1.5)
for p in BTNS.values():
    shell = shell - cyl(*p, FACE_BOT - 1, BH + 1, BTN_HOLE_D / 2, 48) - cyl(*p, BH - 0.6, BH + 0.01, BTN_HOLE_D / 2, 48, BTN_HOLE_D / 2 + 0.6)
for p in SMALLS.values():
    shell = shell + cyl(*p, SLEEVE_Z0, FACE_BOT + 0.5, 4.0, 32) - cyl(*p, SLEEVE_Z0 - 1, BH + 1, SMALL_HOLE_D / 2, 32)
# OLED: window (chamfered outward) + two side rails that hold the module under the face
shell = shell - Manifold.batch_hull([box(WIN[0], WIN[1], FACE_BOT - 1, WIN[2], WIN[3], BH - 0.8),
                                     box(WIN[0] - 1.2, WIN[1] - 1.2, BH - 0.01, WIN[2] + 1.2, WIN[3] + 1.2, BH + 0.01)])
for x0, x1 in ((ox0 - 1.6, ox0 - 0.3), (ox1 + 0.3, ox1 + 1.6)):
    shell = shell + box(x0, od0 + 3, FACE_BOT - OLED_T - 0.4, x1, od1 - 3, FACE_BOT + 0.2)
# engraved ring round the buttons and a frame round the small screen (paint them)
shell = shell - ring(*CLUSTER, BH - 0.5, BH + 1, 21.0, 21.8)
fr = 2.2
shell = shell - (rbox(WIN[0] - fr - 0.8, WIN[1] - fr - 0.8, WIN[2] + fr + 0.8, WIN[3] + fr + 0.8, BH - 0.5, BH + 1, 2.5)
                 - rbox(WIN[0] - fr, WIN[1] - fr, WIN[2] + fr, WIN[3] + fr, BH - 1, BH + 2, 1.8))
# walls: IR LED + IR receiver + wires (hinge wall), switch (front wall)
shell = shell - ycyl(-1, W + 1, IR_LED[0], IR_LED[1], IR_LED_D / 2, 32)
shell = shell - slot_y(-1, W + 1, IR_RX[0], IR_RX[1], *IR_RX_WIN)
shell = shell - slot_y(-1, W + 1, (WIRE_X[0] + WIRE_X[1]) / 2, WIRE_Z[0], WIRE_X[1] - WIRE_X[0], 6)
shell = shell - box(SW_C[0] - SW_HOLE[0] / 2, OD - W - 1, SW_C[1] - SW_HOLE[1] / 2, SW_C[0] + SW_HOLE[0] / 2, OD + 1, SW_C[1] + SW_HOLE[1] / 2)
# hinge: base knuckles (axis outside the hinge wall at face height)
AX_D, AX_Z = -HINGE_OFF, BH
lid_kn = []
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    for (x0, x1) in (k1, k3):
        shell = shell + xcyl(x0, x1, AX_D, AX_Z, HINGE_R) + box(x0, AX_D, AX_Z - HINGE_R, x1, 0.8, AX_Z)
    lid_kn.append(k2)
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    shell = shell - xcyl(k2[0] - HINGE_GAP, k2[1] + HINGE_GAP, AX_D, AX_Z, 6.4)
    shell = shell - xcyl(a - 1, b + 1, AX_D, AX_Z, PIN_D / 2, 24)
    shell = shell - xcyl(a - 1, a + 2.2, AX_D, AX_Z, 2.9, 32)
    shell = shell - xcyl(b - 2.6, b + 1, AX_D, AX_Z, 3.2, 6)
base_shell = shell

# floor: lip, posts under the board, battery and speaker rims, grille, screws
floor = base_body - box(-5, -20, PLATE, OW + 5, OD + 5, BH + 5)
lip = rbox(W + 0.35, W + 0.35, OW - W - 0.35, OD - W - 0.35, PLATE, PLATE + 1.6, 1.2) - rbox(W + 1.6, W + 1.6, OW - W - 1.6, OD - W - 1.6, PLATE - 1, PLATE + 3, 0.5)
for bx, bd in BOSSES: lip = lip - cyl(bx, bd, PLATE - 1, PLATE + 3, BOSS_R + 0.5)
for t in (BATT, SPK, KY005, KY022): lip = lip - box(t[0] - 1.5, t[1] - 3, PLATE - 1, t[2] + 1.5, t[3] + 3, PLATE + 3)   # parts sit against the walls here
floor = floor + lip
for x, d in [(NA_X0 + 3, NA_D0 + 3), (NA_X0 + NA_W - 3, NA_D0 + 3), (NA_X0 + 3, NA_D0 + NA_H - 3), (NA_X0 + NA_W - 3, NA_D0 + NA_H - 3), (NA_X0 + 40, NA_D0 + NA_H - 3)]:
    floor = floor + box(x - 2, d - 2, PLATE, x + 2, d + 2, NA_BOT_Z)
for x, d in HOLES:
    floor = floor + cyl(x, d, PLATE, NA_BOT_Z, 3.0) - cyl(x, d, -1, NA_BOT_Z + 1, 1.7, 24) - cyl(x, d, -0.01, 1.8, 3.1, 32, 1.7)
b = BATT; floor = floor + rim(b[0], b[1], b[2], b[3], PLATE, PLATE + 3, 1.0, 0.3)
s = SPK; floor = floor + rim(s[0], s[1], s[2], s[3], PLATE, PLATE + 2.5, 1.0, 0.3)
for i in range(8):
    x = s[0] + 6 + i * 4.0
    floor = floor - rbox(x - 1.0, s[1] + 5, x + 1.0, s[3] - 5, -1, PLATE + 1, 0.95)
for bx, bd in BOSSES:
    floor = floor - cyl(bx, bd, -1, PLATE + 3, SCREW_D / 2, 24) - cyl(bx, bd, -0.01, 1.7, CSK_R, 32, SCREW_D / 2)
base_floor = floor

# printed caps: a foot that sits on the coloured cap, a stem through the face
cap_h = BH + CAP_UP - CAP_TOP - 0.1
btn_cap = cyl(0, 0, 0, 1.2, EXT_FOOT_D / 2) + cyl(0, 0, 1.2, cap_h - 0.4, EXT_D / 2) + cyl(0, 0, cap_h - 0.4, cap_h, EXT_D / 2, 64, EXT_D / 2 - 0.4)
pl_h = BH + CAP_UP - SMALL_TOP_Z - 0.1
small_cap = cyl(0, 0, 0, 1.0, 3.6, 32) + cyl(0, 0, 1.0, pl_h, PLUNGER_D / 2, 32)

# ================================================================== LID (local: z 0 = screen face, LH = back)
lid_body = crbox(0, 0, OW, OD, 0, LH, R_CORNER, CHAMFER)
lid_cav = rbox(W, W, OW - W, OD - W, PLATE, LB, R_CORNER - W)
above = box(-5, -20, LB, OW + 5, OD + 5, LH + 5)
lid = lid_body - lid_cav - above
for bx, bd in LID_BOSSES:
    lid = lid + cyl(bx, bd, PLATE - 0.5, LB, BOSS_R) - cyl(bx, bd, PLATE + 2, LB + 1, PILOT_D / 2, 24)
vx0, vd0, vx1, vd1 = VIS
lid = lid - Manifold.batch_hull([box(vx0 - 0.3, vd0 - 0.3, 0.8, vx1 + 0.3, vd1 + 0.3, PLATE + 0.5), box(vx0 - 1.8, vd0 - 1.8, -0.5, vx1 + 1.8, vd1 + 1.8, 0.01)])
fr = 2.6
lid = lid - (rbox(vx0 - fr - 0.8, vd0 - fr - 0.8, vx1 + fr + 0.8, vd1 + fr + 0.8, -1, 0.5, 3) - rbox(vx0 - fr, vd0 - fr, vx1 + fr, vd1 + fr, -2, 2, 2.2))
lid = lid - cyl(*MIC, -1, PLATE + 1, 1.0, 16)
for x, d in SCREWS:   # spacer posts face -> board, M3 countersunk from the front
    lid = lid + cyl(x, d, PLATE - 0.5, PCB_Z[0], SPACER_R) - cyl(x, d, -1, PCB_Z[0] + 1, 1.7, 24) - cyl(x, d, -0.01, 1.6, 3.2, 32, 1.6)
for mx, md in MAGNETS:
    lid = lid + cyl(mx, md, PLATE - 0.5, MAG_H + 1.4, 4.5) - cyl(mx, md, -1, MAG_H, MAG_D / 2, 32)
lid = lid + ring(*STICK, PLATE - 0.5, LB, POCKET_R, POCKET_R + POCKET_WALL) - cyl(*STICK, -1, LB, POCKET_R, 96)
lid = lid - slot_x(OW - W - 1, OW + 1, USB[0], USB[1], *USB_HOLE)
lid = lid - slot_y(-1, W + 1, SD[0], SD[1], *SD_HOLE)
lid = lid - slot_y(-1, W + 1, (WIRE_X[0] + WIRE_X[1]) / 2, WIRE_Z[1], WIRE_X[1] - WIRE_X[0], 6)
lid = lid - xcyl(NOTCH_X - 9, NOTCH_X + 9, OD + 1.0, 0.0, 3.2) - ycyl(OD - 6, OD + 2, NOTCH_X, -1.0, 4.5)
for (x0, x1) in lid_kn:
    lid = lid + xcyl(x0, x1, AX_D, -RIM_H, HINGE_R) + box(x0, AX_D, -RIM_H, x1, 0.8, HINGE_R - RIM_H)
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    for (x0, x1) in (k1, k3): lid = lid - xcyl(x0 - HINGE_GAP, x1 + HINGE_GAP, AX_D, -RIM_H, 6.4)
    lid = lid - xcyl(a - 1, b + 1, AX_D, -RIM_H, PIN_D / 2, 24)
lid_shell = lid

back = lid_body - box(-5, -20, -5, OW + 5, OD + 5, LB)
blip = rbox(W + 0.35, W + 0.35, OW - W - 0.35, OD - W - 0.35, LB - 1.6, LB, R_CORNER - W - 0.35) \
    - rbox(W + 1.6, W + 1.6, OW - W - 1.6, OD - W - 1.6, LB - 3, LB + 1, R_CORNER - W - 1.6)
for bx, bd in LID_BOSSES: blip = blip - cyl(bx, bd, LB - 3, LB + 1, BOSS_R + 0.5)
back = back + blip
for x, d in (BOOT, RESET): back = back - cyl(x, d, LB - 1, LH + 1, 1.5, 16)
for bx, bd in LID_BOSSES:
    back = back - cyl(bx, bd, LB - 3, LH + 1, SCREW_D / 2, 24) - cyl(bx, bd, LH - 1.7, LH + 0.01, SCREW_D / 2, 32, CSK_R)
TX, TD, PXS = TEXT_POS
back = back - pixel_text("SOMUDTICK", TX, TD, PXS, LH - 0.6, LH + 1)
back = back - box(TX, TD + 7 * PXS + 2.0, LH - 0.5, TX + 53 * PXS, TD + 7 * PXS + 2.8, LH + 1)
lid_back = back

# ================================================================== export
def mirror(m): return m.mirror([0, 1, 0])
PRINT = {"base_shell": base_shell, "base_floor": base_floor, "lid_shell": lid_shell, "lid_back": lid_back, "btn_cap": btn_cap, "small_cap": small_cap}
for n, m in PRINT.items():
    t = tm(mirror(m))
    assert t.is_watertight, n
    t.export(os.path.join(OUT, n + ".stl"))
    print("%-11s watertight vol %6.1f cm3  size %s" % (n, t.volume / 1000, np.round(t.extents, 1).tolist()))

# ================================================================== electronics
base_parts, lid_parts = {}, {}
def add(dic, name, m): dic[name] = dic[name] + m if name in dic else m
def pbox(dic, name, t, r=1.0): add(dic, name, rbox(t[0], t[1], t[2], t[3], t[4], t[5], r))
add(base_parts, "shield", box(NA_X0, NA_D0, NA_BOT_Z, NA_X0 + NA_W, NA_D0 + NA_H, NA_TOP_Z))
sx, sd = STICK
add(base_parts, "stick_body", box(sx - 8, sd - 8, NA_TOP_Z, sx + 8, sd + 8, NA_TOP_Z + 12))
add(base_parts, "stick_body", Manifold.sphere(7.0, 32).translate([sx, sd, NA_TOP_Z + 12]))
add(base_parts, "stick_body", cyl(sx, sd, NA_TOP_Z + 12, KNOB_TOP - 5.5, 2.5, 24))
add(base_parts, "stick_cap", cyl(sx, sd, KNOB_TOP - 6, KNOB_TOP, KNOB_D / 2, 64))
cap_z0 = CAP_TOP + 0.1
for k, p in BTNS.items():
    add(base_parts, "tact", box(p[0] - 6, p[1] - 6, NA_TOP_Z, p[0] + 6, p[1] + 6, NA_TOP_Z + 5))
    add(base_parts, "cap_" + BTN_COL[k], cyl(*p, NA_TOP_Z + 5, CAP_TOP, CAP_D / 2, 32))
    add(base_parts, "print_cap_" + BTN_COL[k], btn_cap.translate([p[0], p[1], cap_z0]))
for p in SMALLS.values():
    add(base_parts, "tact", box(p[0] - 3, p[1] - 3, NA_TOP_Z, p[0] + 3, p[1] + 3, SMALL_TOP_Z))
    add(base_parts, "print_small", small_cap.translate([p[0], p[1], SMALL_TOP_Z + 0.1]))
add(base_parts, "header", box(NA_X0 + 46, NA_D0 + 5.5, NA_TOP_Z, NA_X0 + 58, NA_D0 + 10.5, NA_TOP_Z + 8.5))   # yellow header (EST)
add(base_parts, "oled_pcb", box(ox0, od0, OLED_Z[0], ox1, od1, OLED_Z[0] + 1.2))
gw, gh, gd = OLED_GLASS
add(base_parts, "oled_glass", box(ocx - gw / 2, od0 + gd, OLED_Z[0] + 1.2, ocx + gw / 2, od0 + gd + gh, OLED_Z[1] - 0.05))
pbox(base_parts, "battery", BATT, 3)
pbox(base_parts, "speaker", SPK, 4)
pbox(base_parts, "pcf8574", (PCF[0], PCF[1], PCF[2], PCF[3], PCF[4], PCF[4] + 1.6))
add(base_parts, "pcf8574_chip", box(PCF[0] + 10, PCF[1] + 6, PCF[4] + 1.6, PCF[0] + 22, PCF[1] + 14, PCF[5]))
pbox(base_parts, "ky005", (KY005[0], KY005[1], KY005[2], KY005[3], KY005[4], KY005[4] + 1.6))
add(base_parts, "led", ycyl(0.6, KY005[1] + 1, IR_LED[0], IR_LED[1], 2.5, 24))
pbox(base_parts, "ky022", KY022)
add(base_parts, "ir_rx", box(IR_RX[0] - 3.5, KY022[1] + 0.2, KY022[5], IR_RX[0] + 3.5, KY022[1] + 6, KY022[5] + 6.0))
pbox(base_parts, "hub", HUB, 0.5)
sw_ = SW_BODY; add(base_parts, "switch", box(sw_[0], sw_[1], sw_[4], sw_[2], sw_[3], sw_[5]))
add(base_parts, "switch_rocker", box(SW_C[0] - 5.5, OD - 0.2, SW_C[1] - 3.2, SW_C[0] + 5.5, OD + 1.4, SW_C[1] + 3.2))
for mx, md in MAGNETS: add(base_parts, "magnet", cyl(mx, md, BH - MAG_H + 0.1, BH - 0.1, 3.0, 24))
for x, d in HOLES: add(base_parts, "bolt", cyl(x, d, 0.2, NA_TOP_Z + 10, 1.4, 16))
for a, b in HINGE_SETS:
    add(base_parts, "bolt", xcyl(a - 0.6, b + 0.6, AX_D, AX_Z, 1.45, 16))

gx0, gd0, gx1, gd1 = GLASS
add(lid_parts, "glass", box(gx0 + 0.4, gd0 + 0.4, PLATE + 0.05, gx1 - 0.4, gd1 - 0.4, PLATE + 2.3))
px0, pd0, px1, pd1 = PCB
add(lid_parts, "pcb", box(px0, pd0, PCB_Z[0], px1, pd1, PCB_Z[1]))
add(lid_parts, "pcb", box(px0 + 8, pd0 + 7, PCB_Z[1], px1 - 8, pd1 - 7, BACK_Z))
for x, d in SCREWS: add(lid_parts, "brass", cyl(x, d, PCB_Z[1], PCB_Z[1] + 5.0, 2.3, 6))
add(lid_parts, "port", box(px1 - 7.5, USB[0] - 4.5, USB[1] - 1.6, px1 + 3.0, USB[0] + 4.5, USB[1] + 1.6))
add(lid_parts, "port", box(SD[0] - 7, pd0 + 0.5, PCB_Z[1], SD[0] + 7, pd0 + 15, PCB_Z[1] + 1.9))
for x, a, b_ in PLUGS: add(lid_parts, "plug", box(x - 3.5, a, PCB_Z[1], x + 3.5, b_, PCB_Z[1] + 5))
add(lid_parts, "plug", box(BAT_PLUG[0] - 3.5, BAT_PLUG[1], PCB_Z[1], BAT_PLUG[0] + 3.5, BAT_PLUG[2], PCB_Z[1] + 5))
pbox(lid_parts, "ds3231", (DS3231[0], DS3231[1], DS3231[2], DS3231[3], DS3231[4], DS3231[4] + 1.6))
add(lid_parts, "coin", cyl((DS3231[0] + DS3231[2]) / 2 + 6, (DS3231[1] + DS3231[3]) / 2, DS3231[4] + 1.6, DS3231[5], 10.2))
for mx, md in MAGNETS: add(lid_parts, "magnet", cyl(mx, md, 0.1, MAG_H - 0.1, 3.0, 24))
# glow paint (3D page only)
add(base_parts, "paint_pink", ring(*CLUSTER, BH - 0.45, BH - 0.03, 21.05, 21.75))
add(base_parts, "paint_cyan", rbox(WIN[0] - 2.2 - 0.75, WIN[1] - 2.2 - 0.75, WIN[2] + 2.2 + 0.75, WIN[3] + 2.2 + 0.75, BH - 0.45, BH - 0.03, 2.45)
    - rbox(WIN[0] - 2.25, WIN[1] - 2.25, WIN[2] + 2.25, WIN[3] + 2.25, BH - 1, BH + 1, 1.85))
add(lid_parts, "paint_cyan", rbox(vx0 - fr - 0.75, vd0 - fr - 0.75, vx1 + fr + 0.75, vd1 + fr + 0.75, 0.03, 0.45, 3) - rbox(vx0 - fr - 0.05, vd0 - fr - 0.05, vx1 + fr + 0.05, vd1 + fr + 0.05, -1, 2, 2.25))
add(lid_parts, "paint_cyan", pixel_text("SOMUDTICK", TX, TD, PXS, LH - 0.55, LH - 0.03))
add(lid_parts, "paint_pink", box(TX + 0.05, TD + 7 * PXS + 2.05, LH - 0.45, TX + 53 * PXS - 0.05, TD + 7 * PXS + 2.75, LH - 0.03))

# ================================================================== clash check
def vol(a, b_): return (a ^ b_).volume()
lid_closed = lambda m: m.translate([0, 0, BH + RIM_H])
clashes = 0
SKIP = ("switch_rocker", "led", "bolt", "ir_rx")
for name, m in base_parts.items():
    if name in SKIP or name.startswith(("print_", "paint_")): continue
    for sn, s_ in (("base_shell", base_shell), ("base_floor", base_floor), ("closed lid_shell", lid_closed(lid_shell)), ("closed lid_back", lid_closed(lid_back))):
        v = vol(m, s_)
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v)); clashes += 1
names = [n for n in base_parts if not n.startswith(("paint_", "print_")) and n not in ("bolt",)]
for i, a in enumerate(names):          # parts against each other
    for b_ in names[i + 1:]:
        if {a, b_} <= {"stick_body", "stick_cap", "shield", "tact", "cap_b", "cap_y", "cap_r", "cap_g", "header"}: continue
        if {a, b_} == {"oled_pcb", "oled_glass"} or {a, b_} == {"pcf8574", "pcf8574_chip"} or {a, b_} == {"switch", "switch_rocker"} or {a, b_} == {"ky022", "ir_rx"} or {a, b_} == {"ky005", "led"}: continue
        v = vol(base_parts[a], base_parts[b_])
        if v > 0.5: print("  CLASH %s with %s: %.1f mm3" % (a, b_, v)); clashes += 1
for pn in [n for n in base_parts if n.startswith("print_")]:   # printed caps go through the face holes
    v = vol(base_parts[pn], base_shell)
    if v > 0.5: print("  CLASH %s with base_shell: %.1f mm3" % (pn, v)); clashes += 1
for name, m in lid_parts.items():
    if name.startswith("paint_"): continue
    for sn, s_ in (("lid_shell", lid_shell), ("lid_back", lid_back)):
        v = vol(m, s_)
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v)); clashes += 1
for a_n, a in (("base_shell", base_shell), ("base_floor", base_floor)):
    for b_n, b_ in (("lid_shell", lid_shell), ("lid_back", lid_back)):
        v = vol(a, lid_closed(b_))
        if v > 0.5: print("  CLASH closed %s with %s: %.1f mm3" % (b_n, a_n, v)); clashes += 1
print("clashes:", clashes)
print("stick knob %.1f mm out of the face, %.1f into the lid pocket (pocket %.1f deep)" % (KNOB_TOP - BH, KNOB_TOP - BH - RIM_H, LB))
print("printed caps %.1f tall, plungers %.1f long" % (cap_h, pl_h))
print("closed size %.0f x %.0f x %.0f mm (+ hinge %.0f mm)" % (OW, OD, BH + RIM_H + LH, HINGE_OFF + HINGE_R))

# ================================================================== GLB + meta
def to_three(m):
    t = tm(mirror(m))
    M = np.array([[1, 0, 0, -OW / 2], [0, 0, 1, 0], [0, -1, 0, -OD / 2], [0, 0, 0, 1]], float)
    t.apply_transform(M)
    return t
import trimesh
scene = trimesh.Scene()
for name, m in [("base_shell", base_shell), ("base_floor", base_floor)] + list(base_parts.items()):
    scene.add_geometry(to_three(m), node_name="B_" + name, geom_name="B_" + name)
for name, m in [("lid_shell", lid_shell), ("lid_back", lid_back)] + list(lid_parts.items()):
    scene.add_geometry(to_three(lid_closed(m)), node_name="L_" + name, geom_name="L_" + name)
scene.export(os.path.join(OUT, "somudtick_ds.glb"))
tot = sum(tm(m).volume for m in PRINT.values()) / 1000
meta = dict(KIND="b", OW=OW, OD=OD, BH=BH + RIM_H, LH=LH, AX_D=AX_D, AX_Z=AX_Z, NAME=NAME,
            GLASS=[vx0, vx1, vd0, vd1], GLASS_Y=PLATE + 0.05 - 0.1,
            OLED_WIN=[WIN[0], WIN[2], WIN[1], WIN[3]], OLED_BASE_Y=OLED_Z[1] - 0.02,
            SIZE="%g × %g × %g มม." % (OW, OD, BH + RIM_H + LH), HALVES="%g / %g มม." % (BH + RIM_H, LH), VOL="~%d ซม³" % round(tot, -1),
            PRINT={n: "×".join("%g" % round(v, 1) for v in tm(mirror(m)).extents) for n, m in PRINT.items()},
            BUY=BUY, SCREEN_NOTE="จอ ES3C28P ไม่ใส่อะคริลิก: หน้ากระจกถึงหลังบอร์ด 6 มม. ✓ วัดแล้ว", PCF="35×19",
            KNOB_OUT=round(KNOB_TOP - BH, 1), CAP_H=round(cap_h, 1), PL_H=round(pl_h, 1), CLASHES=clashes)
json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print("glb", os.path.getsize(os.path.join(OUT, "somudtick_ds.glb")) // 1024, "KB")
