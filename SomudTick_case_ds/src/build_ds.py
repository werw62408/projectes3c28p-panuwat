"""Build the SomudTick DS case (opens like a Nintendo DS).

Printed parts (out/*.stl):
  base_shell   base walls + button face          base_floor  bottom plate (speaker grille, posts)
  lid_shell    lid walls + screen face            lid_back    back plate (OLED window, logo)
  btn_cap x4   caps over the A B C D buttons      small_cap x2  plungers for the E F buttons
Also: out/somudtick_ds.glb (every part + the electronics, for the 3D page) and a clash check.

Everything is built in (x, d, z): x right, d = distance from the hinge edge, z up (see layout_ds.py).
At the end the geometry is mirrored y -> -y, so that looking down at the open case from the
front, x runs to the right and d runs toward you (the frame in which layout_ds.py was drawn).
"""
import os, math
import numpy as np
import trimesh
from manifold3d import Manifold
from layout_ds import *

OUT = os.path.join(os.path.dirname(__file__), "out"); os.makedirs(OUT, exist_ok=True)
SEG = 64
W = WALL

# ------------------------------------------------------------------ helpers
def box(x0, y0, z0, x1, y1, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])

def cyl(x, y, z0, z1, r, seg=SEG, r2=None):
    return Manifold.cylinder(z1 - z0, r, r if r2 is None else r2, seg).translate([x, y, z0])

def rbox(x0, y0, x1, y1, z0, z1, r):
    if r <= 0: return box(x0, y0, z0, x1, y1, z1)
    return Manifold.batch_hull([cyl(x, y, z0, z1, r) for x in (x0 + r, x1 - r) for y in (y0 + r, y1 - r)])

def crbox(x0, y0, x1, y1, z0, z1, r, ch):
    """rounded box with a 45 degree chamfer on the top and bottom edges"""
    parts = []
    for x in (x0 + r, x1 - r):
        for y in (y0 + r, y1 - r):
            parts += [cyl(x, y, z0, z0 + ch, r - ch, SEG, r), cyl(x, y, z0 + ch, z1 - ch, r), cyl(x, y, z1 - ch, z1, r, SEG, r - ch)]
    return Manifold.batch_hull(parts)

def xcyl(x0, x1, y, z, r, seg=SEG):   # cylinder along x
    return Manifold.cylinder(x1 - x0, r, r, seg).rotate([0, 90, 0]).translate([x0, y, z])

def ycyl(y0, y1, x, z, r, seg=SEG):   # cylinder along y
    return Manifold.cylinder(y1 - y0, r, r, seg).rotate([-90, 0, 0]).translate([x, y0, z])

def slot_x(x0, x1, y, z, w, h):       # rounded hole through a wall facing x (w along y, h along z)
    r = min(w, h) / 2
    ys = [y - w / 2 + r, y + w / 2 - r]; zs = [z - h / 2 + r, z + h / 2 - r]
    return Manifold.batch_hull([xcyl(x0, x1, yy, zz, r, 32) for yy in ys for zz in zs])

def slot_y(y0, y1, x, z, w, h):       # through a wall facing y (w along x, h along z)
    r = min(w, h) / 2
    xs = [x - w / 2 + r, x + w / 2 - r]; zs = [z - h / 2 + r, z + h / 2 - r]
    return Manifold.batch_hull([ycyl(y0, y1, xx, zz, r, 32) for xx in xs for zz in zs])

def ring(x, y, z0, z1, r0, r1):
    return cyl(x, y, z0, z1, r1) - cyl(x, y, z0 - 1, z1 + 1, r0)

def rim(x0, y0, x1, y1, z0, z1, t=1.2, gap=0.4):
    return box(x0 - gap - t, y0 - gap - t, z0, x1 + gap + t, y1 + gap + t, z1) - box(x0 - gap, y0 - gap, z0 - 1, x1 + gap, y1 + gap, z1 + 1)

def union(ms):
    ms = [m for m in ms if m is not None]
    return Manifold.batch_boolean(ms, trimesh.boolean.__dict__.get("OpType", None) or __import__("manifold3d").OpType.Add) if len(ms) > 1 else ms[0]

# 5x7 pixel letters, engraved (retro screen look)
FONT = {
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
}
def pixel_text(text, x0, d0, px, z0, z1):
    """rows go toward +d (down the page when you look at it from the front)"""
    cells = []
    for i, ch in enumerate(text):
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == "1":
                    x = x0 + (i * 6 + c) * px; d = d0 + r * px
                    cells.append(box(x + 0.08, d + 0.08, z0, x + px - 0.08, d + px - 0.08, z1))
    return union(cells)

def knuckle_ranges(a, b):
    L = (b - a - 2 * HINGE_GAP) / 3
    return (a, a + L), (a + L + HINGE_GAP, b - L - HINGE_GAP), (b - L, b)

# derived positions
sx0, sd0, sw, sh = SHIELD
STICK = (sx0 + STICK_ON[0], sd0 + STICK_ON[1])
BTNS = {k: (sx0 + v[0], sd0 + v[1]) for k, v in BTN_ON.items()}
SMALLS = {k: (sx0 + v[0], sd0 + v[1]) for k, v in SMALL_ON.items()}
CAP_UP = 0.6                                   # caps stand this much above the base face (the rim is 1.0)
PLATE_BOT = BH - PLATE                         # underside of the base face plate
CLUSTER = (sum(p[0] for p in BTNS.values()) / 4, sum(p[1] for p in BTNS.values()) / 4)

# ================================================================== BASE
base_body = crbox(0, 0, OW, OD, 0, BH, R_CORNER, CHAMFER)
base_cav = rbox(W, W, OW - W, OD - W, PLATE, PLATE_BOT, R_CORNER - W)
below = box(-5, -20, -5, OW + 5, OD + 5, PLATE)

shell = base_body - base_cav - below
# raised rim round the face: the closed lid sits on it, so it never touches the buttons
shell = shell + (rbox(CHAMFER, CHAMFER, OW - CHAMFER, OD - CHAMFER, BH - 0.01, BH + RIM_H, R_CORNER - CHAMFER) - rbox(CHAMFER + 2.2, CHAMFER + 2.2, OW - CHAMFER - 2.2, OD - CHAMFER - 2.2, BH - 1, BH + 2, R_CORNER - CHAMFER - 2.2))
# screw bosses (the floor screws into them)
for bx, bd in BOSSES:
    shell = shell + cyl(bx, bd, PLATE, PLATE_BOT + 0.5, BOSS_R) - cyl(bx, bd, PLATE - 1, PLATE_BOT - 2, PILOT_D / 2, 24)
# magnet bosses under the face
for mx, md in MAGNETS:
    shell = shell + cyl(mx, md, BH - MAG_H - 1.4, PLATE_BOT + 0.5, 4.5) - cyl(mx, md, BH - MAG_H, BH + 1, MAG_D / 2, 32)
# stick hole with a chamfer, button holes
shell = shell - cyl(*STICK, PLATE_BOT - 1, BH + 1, STICK_HOLE_D / 2, 96) - cyl(*STICK, BH - 1.0, BH + 0.01, STICK_HOLE_D / 2, 96, STICK_HOLE_D / 2 + 1.0)
for p in BTNS.values():
    shell = shell - cyl(*p, PLATE_BOT - 1, BH + 1, BTN_HOLE_D / 2, 48) - cyl(*p, BH - 0.6, BH + 0.01, BTN_HOLE_D / 2, 48, BTN_HOLE_D / 2 + 0.6)
for p in SMALLS.values():
    shell = shell - cyl(*p, PLATE_BOT - 1, BH + 1, SMALL_HOLE_D / 2, 32)
# engraved ring around the button cluster (fill with paint for the neon look)
shell = shell - ring(*CLUSTER, BH - 0.5, BH + 1, 22.5, 23.3)
# walls: switch (left), IR LED + wires (hinge side)
shell = shell - box(-1, SW[0] - SW_HOLE[0] / 2, SW[1] - SW_HOLE[1] / 2, W + 1, SW[0] + SW_HOLE[0] / 2, SW[1] + SW_HOLE[1] / 2)
shell = shell - ycyl(-1, W + 1, IR_LED[0], IR_LED[1], IR_D / 2, 32)
shell = shell - slot_y(-1, W + 1, (WIRE_X[0] + WIRE_X[1]) / 2, 16.5, WIRE_X[1] - WIRE_X[0], 7)

# hinge: base knuckles
AX_D, AX_Z = -HINGE_OFF, BH
base_kn, lid_kn = [], []
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    for (x0, x1) in (k1, k3):
        base_kn.append(xcyl(x0, x1, AX_D, AX_Z, HINGE_R) + box(x0, AX_D, AX_Z - HINGE_R, x1, 0.8, AX_Z))
    lid_kn.append(k2)
for k in base_kn: shell = shell + k
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    shell = shell - xcyl(k2[0] - HINGE_GAP, k2[1] + HINGE_GAP, AX_D, AX_Z, 6.4)      # room for the turning lid knuckle
    shell = shell - xcyl(a - 1, b + 1, AX_D, AX_Z, PIN_D / 2, 24)                     # pin
    shell = shell - xcyl(a - 1, a + 2.2, AX_D, AX_Z, 2.9, 32)                         # bolt head
    shell = shell - xcyl(b - 2.6, b + 1, AX_D, AX_Z, 3.2, 6)                          # nut (hex)
base_shell = shell

# floor plate
floor = base_body - box(-5, -20, PLATE, OW + 5, OD + 5, BH + 5)
lip = rbox(W + 0.35, W + 0.35, OW - W - 0.35, OD - W - 0.35, PLATE, PLATE + 1.6, R_CORNER - W - 0.35) \
    - rbox(W + 1.6, W + 1.6, OW - W - 1.6, OD - W - 1.6, PLATE - 1, PLATE + 3, R_CORNER - W - 1.6)
for bx, bd in BOSSES: lip = lip - cyl(bx, bd, PLATE - 1, PLATE + 3, BOSS_R + 0.5)
lip = lip - box(-1, SW[0] - 9, PLATE - 1, 20, SW[0] + 9, PLATE + 3)                # switch body
floor = floor + lip
for x, d in ((sx0 + 3, sd0 + 3), (sx0 + sw - 3, sd0 + 3), (sx0 + 3, sd0 + sh - 3), (sx0 + sw - 3, sd0 + sh - 3)):
    floor = floor + box(x - 2, d - 2, PLATE, x + 2, d + 2, SHIELD_Z)                    # posts under the shield
b = BATT; floor = floor + rim(b[0], b[1], b[2], b[3], PLATE, PLATE + 3, 1.0, 0.3)
s = SPK; floor = floor + rim(s[0], s[1], s[2], s[3], PLATE, PLATE + 2.5, 1.0, 0.3)
for i in range(8):                                                                   # speaker grille
    x = s[0] + 5 + i * 4.3
    floor = floor - rbox(x - 1.1, s[1] + 5, x + 1.1, s[3] - 5, -1, PLATE + 1, 1.05)
for bx, bd in BOSSES:
    floor = floor - cyl(bx, bd, -1, PLATE + 3, SCREW_D / 2, 24) - cyl(bx, bd, -0.01, 1.7, 3.3, 32, 1.6)
base_floor = floor

# caps (printed separately, stand upright on the bed)
cap_z0 = SHIELD_Z + SHIELD_T + BTN_H + 0.1
btn_cap = cyl(0, 0, 0, 1.0, 6.75) + cyl(0, 0, 1.0, BH + CAP_UP - cap_z0 - 0.4, BTN_CAP_D / 2) \
    + cyl(0, 0, BH + CAP_UP - cap_z0 - 0.4, BH + CAP_UP - cap_z0, BTN_CAP_D / 2, SEG, BTN_CAP_D / 2 - 0.4)
small_z0 = SHIELD_Z + SHIELD_T + SMALL_H + 0.1
small_cap = cyl(0, 0, 0, BH + CAP_UP - small_z0, SMALL_CAP_D / 2, 32) + cyl(0, 0, PLATE_BOT - 1.3 - small_z0, PLATE_BOT - 0.3 - small_z0, 4.0, 32)

# ================================================================== LID (local: z 0 = screen face, LH = back)
LB = LH - PLATE                                    # underside of the back plate
lid_body = crbox(0, 0, OW, OD, 0, LH, R_CORNER, CHAMFER)
lid_cav = rbox(W, W, OW - W, OD - W, PLATE, LB, R_CORNER - W)
above = box(-5, -20, LB, OW + 5, OD + 5, LH + 5)
lid = lid_body - lid_cav - above
for bx, bd in BOSSES:
    lid = lid + cyl(bx, bd, PLATE - 0.5, LB, BOSS_R) - cyl(bx, bd, PLATE + 2, LB + 1, PILOT_D / 2, 24)
# screen window, chamfered toward the outside
gx0, gd0, gx1, gd1 = GLASS
lid = lid - Manifold.batch_hull([box(gx0, gd0, 0.8, gx1, gd1, PLATE + 0.5), box(gx0 - 1.6, gd0 - 1.6, -0.5, gx1 + 1.6, gd1 + 1.6, 0.01)])
# engraved frame around the screen (paint it: neon frame)
fr = 3.2
lid = lid - (rbox(gx0 - fr - 0.8, gd0 - fr - 0.8, gx1 + fr + 0.8, gd1 + fr + 0.8, -1, 0.5, 3) - rbox(gx0 - fr, gd0 - fr, gx1 + fr, gd1 + fr, -2, 2, 2.2))
lid = lid - cyl(*MIC, -1, PLATE + 1, 1.0, 16)
for x, d in SCREWS:
    lid = lid - cyl(x, d, -1, PLATE + 1, 1.7, 24) - cyl(x, d, -0.01, 1.6, 3.2, 32, 1.6)
for mx, md in MAGNETS:
    lid = lid + cyl(mx, md, PLATE - 0.5, MAG_H + 1.4, 4.5) - cyl(mx, md, -1, MAG_H, MAG_D / 2, 32)
# stick pocket: a tube up to the back plate
lid = lid + ring(*STICK, PLATE - 0.5, LB, POCKET_R, POCKET_R + POCKET_WALL) - cyl(*STICK, -1, LB, POCKET_R, 96)
# walls: USB-C (right), SD card + wires (hinge side)
lid = lid - slot_x(OW - W - 1, OW + 1, USB_C[0], PLATE + USB_C[1], *USB_HOLE)
lid = lid - slot_y(-1, W + 1, SD[0], PLATE + SD[1], *SD_HOLE)
lid = lid - slot_y(-1, W + 1, (WIRE_X[0] + WIRE_X[1]) / 2, 7.5, WIRE_X[1] - WIRE_X[0], 7)
# finger notch on the front edge
lid = lid - xcyl(NOTCH_X - 9, NOTCH_X + 9, OD + 1.0, 0.0, 3.2) - ycyl(OD - 6, OD + 2, NOTCH_X, -1.0, 4.5)
# hinge: lid knuckles (same axis; in lid coordinates the axis is at z = -RIM_H)
for (x0, x1) in lid_kn:
    lid = lid + xcyl(x0, x1, AX_D, -RIM_H, HINGE_R) + box(x0, AX_D, -RIM_H, x1, 0.8, HINGE_R - RIM_H)
for a, b in HINGE_SETS:
    k1, k2, k3 = knuckle_ranges(a, b)
    for (x0, x1) in (k1, k3): lid = lid - xcyl(x0 - HINGE_GAP, x1 + HINGE_GAP, AX_D, -RIM_H, 6.4)
    lid = lid - xcyl(a - 1, b + 1, AX_D, -RIM_H, PIN_D / 2, 24)
lid_shell = lid

# back plate: OLED window, logo, tech lines
back = lid_body - box(-5, -20, -5, OW + 5, OD + 5, LB)
blip = rbox(W + 0.35, W + 0.35, OW - W - 0.35, OD - W - 0.35, LB - 1.6, LB, R_CORNER - W - 0.35) \
    - rbox(W + 1.6, W + 1.6, OW - W - 1.6, OD - W - 1.6, LB - 3, LB + 1, R_CORNER - W - 1.6)
for bx, bd in BOSSES: blip = blip - cyl(bx, bd, LB - 3, LB + 1, BOSS_R + 0.5)
back = back + blip
ox0, od0, ox1, od1 = OLED
back = back + rim(ox0, od0, ox1, od1, LB - 2.0, LB)                                  # holds the OLED in place
ocx, ocd = (ox0 + ox1) / 2, (od0 + od1) / 2
ww, wh = OLED_WIN
back = back - Manifold.batch_hull([box(ocx - ww, ocd - wh / 2 - 1.5, LB - 1, ocx + ww, ocd + wh / 2 + 1.5, LB + 0.8),   # (made wide, trimmed below)
                                   box(ocx - ww, ocd - wh / 2 - 1.5, LB + 0.8, ocx + ww, ocd + wh / 2 + 1.5, LB + 0.8)]) if False else back
back = back - Manifold.batch_hull([box(ocx - 12, ocd - 6.5, LB - 1, ocx + 12, ocd + 6.5, LH - 0.6),
                                   box(ocx - 13.2, ocd - 7.7, LH - 0.01, ocx + 13.2, ocd + 7.7, LH + 1)])
for x, d in (BOOT, RESET): back = back - cyl(x, d, LB - 1, LH + 1, 1.5, 16)
for bx, bd in BOSSES:
    back = back - cyl(bx, bd, LB - 3, LH + 1, SCREW_D / 2, 24) - cyl(bx, bd, LH - 1.7, LH + 0.01, 1.6, 32, 3.3)
PX = 1.45
back = back - pixel_text("SOMUDTICK", 52.0, 70.0, PX, LH - 0.6, LH + 1)
for (x0, x1, d) in ((ocx + 16, 140.0, ocd - 3.0), (ocx + 16, 118.0, ocd + 3.0)):        # tech lines from the OLED
    back = back - box(x0, d - 0.4, LH - 0.5, x1, d + 0.4, LH + 1) - cyl(x1, d, LH - 0.5, LH + 1, 1.3, 24)
lid_back = back

# ================================================================== export printed parts
def mirror(m): return m.mirror([0, 1, 0])
def tm(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.array(mesh.vert_properties)[:, :3], faces=np.array(mesh.tri_verts), process=False)

PRINT = {"base_shell": base_shell, "base_floor": base_floor, "lid_shell": lid_shell, "lid_back": lid_back,
         "btn_cap": btn_cap, "small_cap": small_cap}
for n, m in PRINT.items():
    t = tm(mirror(m))
    assert t.is_watertight, n
    t.export(os.path.join(OUT, n + ".stl"))
    print("%-11s watertight vol %6.1f cm3  size %s" % (n, t.volume / 1000, np.round(t.extents, 1).tolist()))

# ================================================================== electronics (for the 3D page and the clash check)
base_parts, lid_parts = {}, {}
def add(dic, name, m): dic[name] = dic[name] + m if name in dic else m
pz0 = SHIELD_Z; pz1 = SHIELD_Z + SHIELD_T
add(base_parts, "shield", box(sx0, sd0, pz0, sx0 + sw, sd0 + sh, pz1))
add(base_parts, "stick_body", box(STICK[0] - 8, STICK[1] - 8, pz1, STICK[0] + 8, STICK[1] + 8, pz1 + 13))
add(base_parts, "stick_body", Manifold.sphere(7.5, 32).translate([STICK[0], STICK[1], pz1 + 13]))
add(base_parts, "stick_body", cyl(*STICK, pz1 + 13, pz0 + STICK_H - 6, 3, 24))
add(base_parts, "stick_cap", cyl(*STICK, pz0 + STICK_H - 7, pz0 + STICK_H, STICK_CAP_D / 2, 64))
for k, p in BTNS.items():
    add(base_parts, "tact", box(p[0] - 6, p[1] - 6, pz1, p[0] + 6, p[1] + 6, pz1 + 7))
    add(base_parts, "cap_" + ("y" if k in "AC" else "b"), cyl(*p, pz1 + 7, pz1 + BTN_H, 5.8, 32))
    add(base_parts, "print_cap_" + ("y" if k in "AC" else "b"), btn_cap.translate([p[0], p[1], cap_z0]))
for p in SMALLS.values():
    add(base_parts, "tact", box(p[0] - 3, p[1] - 3, pz1, p[0] + 3, p[1] + 3, pz1 + SMALL_H))
    add(base_parts, "print_small", small_cap.translate([p[0], p[1], small_z0]))
def pbox(dic, name, t, r=1.0): add(dic, name, rbox(t[0], t[1], t[2], t[3], t[4], t[5], r))
pbox(base_parts, "battery", BATT, 2)
pbox(base_parts, "speaker", SPK, 3)
pbox(base_parts, "ds3231", (DS3231[0], DS3231[1], DS3231[2], DS3231[3], DS3231[4], DS3231[4] + 1.6))
add(base_parts, "coin", cyl((DS3231[0] + DS3231[2]) / 2, DS3231[1] + 26, DS3231[4] + 1.6, DS3231[5], 10.5))
pbox(base_parts, "pcf8574", (PCF[0], PCF[1], PCF[2], PCF[3], PCF[4], PCF[4] + 1.6))
add(base_parts, "pcf8574_chip", box(PCF[0] + 10, PCF[1] + 6, PCF[4] + 1.6, PCF[0] + 22, PCF[1] + 14, PCF[4] + 4))
pbox(base_parts, "ky005", KY)
add(base_parts, "led", ycyl(0.6, KY[1], IR_LED[0], IR_LED[1], 2.5, 24))
sw_ = SW_BODY; add(base_parts, "switch", box(sw_[0], sw_[1], sw_[4], sw_[2], sw_[3], sw_[5]))
add(base_parts, "switch_rocker", box(-1.4, SW[0] - 5.5, SW[1] - 3.2, 0.2, SW[0] + 5.5, SW[1] + 3.2))
for mx, md in MAGNETS: add(base_parts, "magnet", cyl(mx, md, BH - MAG_H + 0.1, BH - 0.1, 3.0, 24))
for a, b in HINGE_SETS:
    add(base_parts, "bolt", xcyl(a - 0.6, b + 0.6, AX_D, AX_Z, 1.45, 16))
    add(base_parts, "bolt", xcyl(a + 0.1, a + 2.1, AX_D, AX_Z, 2.7, 24))

ax0, ad0, ax1, ad1 = ACR
add(lid_parts, "acrylic", rbox(ax0, ad0, ax1, ad1, PLATE, PLATE + ACR_T, 2))
add(lid_parts, "glass", box(gx0 + 0.4, gd0 + 0.4, PLATE + 0.3, gx1 - 0.4, gd1 - 0.4, PLATE + ACR_T + 2.5))
add(lid_parts, "pcb", box(ax0 - 0.5, 15.0, PLATE + PCB_BACK - 1.6, ax1 + 0.5, 65.0, PLATE + PCB_BACK))
for x, d in SCREWS: add(lid_parts, "brass", cyl(x, d, PLATE + PCB_BACK, PLATE + LID_IN - 0.1, 2.3, 6))
add(lid_parts, "port", box(ax1 - 7, USB_C[0] - 4.5, PLATE + PCB_BACK, ax1 + 0.5, USB_C[0] + 4.5, PLATE + PCB_BACK + 3.2))
add(lid_parts, "port", box(SD[0] - 7, 14.0, PLATE + PCB_BACK, SD[0] + 7, 28.0, PLATE + PCB_BACK + 1.9))
for x, d in PLUGS: add(lid_parts, "plug", box(x - 3.5, 65.0, PLATE + PCB_BACK, x + 3.5, 71.0, PLATE + PCB_BACK + 5))
add(lid_parts, "plug", box(BAT_PLUG[0] - 3.5, 10.0, PLATE + PCB_BACK, BAT_PLUG[0] + 3.5, 15.0, PLATE + PCB_BACK + 5))
add(lid_parts, "oled_pcb", box(ox0, od0, LB - OLED_T, ox1, od1, LB - OLED_T + 1.2))
add(lid_parts, "oled_glass", box(ocx - 13, ocd - 9.5, LB - OLED_T + 1.2, ocx + 13, ocd + 9.5, LB - 0.05))
for mx, md in MAGNETS: add(lid_parts, "magnet", cyl(mx, md, 0.1, MAG_H - 0.1, 3.0, 24))

# glow paint in the engraved lines (only for the 3D page: shows the painted look)
add(base_parts, "paint_pink", ring(*CLUSTER, BH - 0.45, BH - 0.03, 22.55, 23.25))
add(lid_parts, "paint_cyan", rbox(gx0 - fr - 0.75, gd0 - fr - 0.75, gx1 + fr + 0.75, gd1 + fr + 0.75, 0.03, 0.45, 3) - rbox(gx0 - fr - 0.05, gd0 - fr - 0.05, gx1 + fr + 0.05, gd1 + fr + 0.05, -1, 2, 2.25))
add(lid_parts, "paint_cyan", pixel_text("SOMUDTICK", 52.0, 70.0, PX, LH - 0.55, LH - 0.03))
for (x0, x1, d) in ((ocx + 16, 140.0, ocd - 3.0), (ocx + 16, 118.0, ocd + 3.0)):
    add(lid_parts, "paint_pink", box(x0 + 0.05, d - 0.35, LH - 0.45, x1, d + 0.35, LH - 0.03) + cyl(x1, d, LH - 0.45, LH - 0.03, 1.25, 24))

# clash check (parts vs printed shells), and closed lid vs base
def vol(a, b): return (a ^ b).volume()
lid_closed = lambda m: m.translate([0, 0, BH + RIM_H])
for name, m in base_parts.items():
    if name in ("switch_rocker", "led", "bolt") or name.startswith(("print_", "paint_")): continue
    for sn, s_ in (("base_shell", base_shell), ("base_floor", base_floor)):
        v = vol(m, s_)
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v))
    for sn, s_ in (("lid_shell", lid_closed(lid_shell)), ("lid_back", lid_closed(lid_back))):
        v = vol(m, s_)
        if v > 0.5: print("  CLASH %-12s with closed %s: %.1f mm3" % (name, sn, v))
for name, m in lid_parts.items():
    if name.startswith("paint_"): continue
    for sn, s_ in (("lid_shell", lid_shell), ("lid_back", lid_back)):
        v = vol(m, s_)
        if v > 0.5: print("  CLASH %-12s with %s: %.1f mm3" % (name, sn, v))
for a_n, a in (("base_shell", base_shell), ("base_floor", base_floor)):
    for b_n, b_ in (("lid_shell", lid_shell), ("lid_back", lid_back)):
        v = vol(a, lid_closed(b_))
        if v > 0.5: print("  CLASH closed %s with %s: %.1f mm3" % (b_n, a_n, v))
print("stick top %.1f, lid pocket top %.1f (closed)" % (SHIELD_Z + STICK_H, BH + RIM_H + LB))
print("closed size %.0f x %.0f x %.0f mm (+ hinge %.0f mm)" % (OW, OD, BH + RIM_H + LH, HINGE_OFF + HINGE_R))

# ================================================================== GLB for the 3D page
# three.js frame: x = X - OW/2, y = Z, z = d - OD/2 (after the mirror this is a proper rotation)
def to_three(m):
    t = tm(mirror(m))
    M = np.array([[1, 0, 0, -OW / 2], [0, 0, 1, 0], [0, -1, 0, -OD / 2], [0, 0, 0, 1]], float)
    t.apply_transform(M)
    return t
scene = trimesh.Scene()
for name, m in [("base_shell", base_shell), ("base_floor", base_floor)] + list(base_parts.items()):
    scene.add_geometry(to_three(m), node_name="B_" + name, geom_name="B_" + name)
for name, m in [("lid_shell", lid_shell), ("lid_back", lid_back)] + list(lid_parts.items()):
    scene.add_geometry(to_three(lid_closed(m)), node_name="L_" + name, geom_name="L_" + name)
scene.export(os.path.join(OUT, "somudtick_ds.glb"))
meta = dict(OW=OW, OD=OD, BH=BH + RIM_H, LH=LH, AX_D=AX_D, AX_Z=AX_Z)
import json; json.dump(meta, open(os.path.join(OUT, "meta.json"), "w"))
print("glb", os.path.getsize(os.path.join(OUT, "somudtick_ds.glb")) // 1024, "KB")
