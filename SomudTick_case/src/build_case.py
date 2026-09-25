"""Build the printable case (front + back shell) and the parts for the 3D viewer.
Output: out/front_shell.stl, out/back_shell.stl, out/somudtick_case.glb
"""
import os, math
import numpy as np
import trimesh
from manifold3d import Manifold
from layout import *

OUT = os.path.join(os.path.dirname(__file__), "out"); os.makedirs(OUT, exist_ok=True)
W = WALL
OX0, OY0, OX1, OY1 = IX0 - W, IY0 - W, IX1 + W, IY1 + W       # outer
OZ1 = W + INNER_D + W                                          # 31
SPLIT = W + SPLIT_ZI                                           # 17
Z = lambda zi: W + zi                                          # inside depth -> absolute z
SEG = 48

def box(x0, y0, z0, x1, y1, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])

def cyl(x, y, z0, z1, r, seg=SEG):
    return Manifold.cylinder(z1 - z0, r, r, seg).translate([x, y, z0])

def rbox(x0, y0, x1, y1, z0, z1, r):
    if r <= 0: return box(x0, y0, z0, x1, y1, z1)
    cs = [cyl(x, y, z0, z1, r) for x in (x0 + r, x1 - r) for y in (y0 + r, y1 - r)]
    return Manifold.batch_hull(cs)

def xhole(x0, x1, y, z, w, h):          # stadium hole through a wall facing x (w along y, h along z)
    r = min(w, h) / 2
    ys = [y - w / 2 + r, y + w / 2 - r] if w >= h else [y, y]
    zs = [z - h / 2 + r, z + h / 2 - r] if h > w else [z, z]
    parts = [Manifold.cylinder(x1 - x0, r, r, SEG).rotate([0, 90, 0]).translate([x0, yy, zz]) for yy in ys for zz in zs]
    return Manifold.batch_hull(parts)

def yhole(y0, y1, x, z, w, h):          # through a wall facing y (w along x, h along z)
    r = min(w, h) / 2
    xs = [x - w / 2 + r, x + w / 2 - r] if w >= h else [x, x]
    zs = [z - h / 2 + r, z + h / 2 - r] if h > w else [z, z]
    parts = [Manifold.cylinder(y1 - y0, r, r, SEG).rotate([-90, 0, 0]).translate([xx, y0, zz]) for xx in xs for zz in zs]
    return Manifold.batch_hull(parts)

def rim(x0, y0, x1, y1, z0, z1, t=1.2, gap=0.3):
    return box(x0 - gap - t, y0 - gap - t, z0, x1 + gap + t, y1 + gap + t, z1) - box(x0 - gap, y0 - gap, z0 - 1, x1 + gap, y1 + gap, z1 + 1)

# ------------------------------------------------------------------ shells
outer = rbox(OX0, OY0, OX1, OY1, 0, OZ1, 5.0)
cavity = rbox(IX0, IY0, IX1, IY1, W, W + INNER_D, 3.0)
shell = outer - cavity

holes = []
gx0, gy0, gx1, gy1 = GLASS
holes.append(rbox(gx0, gy0, gx1, gy1, -1, W + 1, 2.0))                     # screen window
holes.append(cyl(*MIC, -1, W + 1, 1.0))
holes.append(cyl(*JOY_STICK, -1, W + 1, JOY_HOLE_D / 2, 96))
for sx, sy in SCREWS: holes.append(cyl(sx, sy, -1, W + 1, 1.7))
holes.append(yhole(OY0 - 1, IY0 + 1, USB_C[0], Z(USB_C[1]), *USB_HOLE))   # top wall
holes.append(xhole(IX1 - 1, OX1 + 1, SD[0], Z(SD[1]), *SD_HOLE))           # right wall
holes.append(xhole(IX1 - 1, OX1 + 1, LED[0], Z(LED[1]), LED_D, LED_D))
holes.append(xhole(OX0 - 1, IX0 + 1, SW[0], Z(SW[1]), *SW_HOLE))           # left wall
for bx, by in (BOOT, RESET): holes.append(cyl(bx, by, OZ1 - W - 1, OZ1 + 1, 1.5))   # back: pin holes
for i in range(-3, 4):
    for j in range(-2, 3):
        holes.append(cyl(SPK_C[0] + i * 4.2, SPK_C[1] + j * 4.2, OZ1 - W - 1, OZ1 + 1, 1.0, 20))

HOLES = holes[0]
for h in holes[1:]: HOLES = HOLES + h

# pillars joining the two halves (M3)
front_add, back_add, front_cut, back_cut = [], [], [], []
for px, py in PILLARS:
    front_add.append(cyl(px, py, W - 0.5, SPLIT, PILLAR_R))
    back_add.append(cyl(px, py, SPLIT, OZ1 - W + 0.5, PILLAR_R))
    front_cut.append(cyl(px, py, W + 2, SPLIT + 1, 1.35, 24))                  # 2.7 mm pilot for an M3 screw
    back_cut.append(cyl(px, py, SPLIT - 1, OZ1 + 1, 1.7, 24))                  # 3.4 mm clearance
    back_cut.append(cyl(px, py, OZ1 - 3.2, OZ1 + 1, 3.1, 32))                  # head pocket 6.2 mm

front = (shell ^ box(OX0 - 1, OY0 - 1, -1, OX1 + 1, OY1 + 1, SPLIT))
for a in front_add: front = front + a
for c in front_cut: front = front - c
front = front - HOLES

back = (shell ^ box(OX0 - 1, OY0 - 1, SPLIT, OX1 + 1, OY1 + 1, OZ1 + 1))
# alignment lip: rises 2 mm into the front half, 0.25 mm clearance
lip = rbox(IX0 + 0.25, IY0 + 0.25, IX1 - 0.25, IY1 - 0.25, SPLIT - 2, SPLIT + 0.01, 2.8) - \
      rbox(IX0 + 1.45, IY0 + 1.45, IX1 - 1.45, IY1 - 1.45, SPLIT - 3, SPLIT + 1, 1.6)
for px, py in PILLARS: lip = lip - cyl(px, py, SPLIT - 3, SPLIT + 1, PILLAR_R + 0.6)
lip = lip - xhole(IX0 - 1, IX0 + 3, SW[0], Z(SW[1]), SW_HOLE[0] + 4, SW_HOLE[1] + 8)
back = back + lip
for a in back_add: back = back + a

# cradles on the inside of the back wall
bz = OZ1 - W
bx0, by0, bx1, by1, _, _ = BATT; back = back + rim(bx0, by0, bx1, by1, bz - 4, bz)
sx0, sy0, sx1, sy1, _, _ = SPK; back = back + rim(sx0, sy0, sx1, sy1, bz - 3, bz)
dx0, dy0, dx1, dy1, _, _ = DS; back = back + rim(dx0, dy0, dx1, dy1, bz - 2.5, bz)
jx0, jy0, jx1, jy1 = JOY
joy_z = bz - JOY_LIFT
back = back + (box(jx0, jy0, joy_z, jx1, jy1, bz) - box(jx0 + 2.5, jy0 + 2.5, joy_z - 1, jx1 - 2.5, jy1 - 2.5, bz + 1))   # platform frame
back = back + rim(jx0, jy0, jx1, jy1, joy_z - 2.5, bz, t=1.2, gap=0.3)                                                # side guides
for c in back_cut: back = back - c
back = back - HOLES
for bx, by in (BOOT, RESET): back = back - cyl(bx, by, bz - 10, OZ1 + 1, 1.5)
for i in range(-3, 4):
    for j in range(-2, 3):
        back = back - cyl(SPK_C[0] + i * 4.2, SPK_C[1] + j * 4.2, bz - 5, OZ1 + 1, 1.0, 20)

def tm(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.array(mesh.vert_properties)[:, :3], faces=np.array(mesh.tri_verts), process=False)

tf, tb = tm(front), tm(back)
for t, n in ((tf, "front_shell"), (tb, "back_shell")):
    assert t.is_watertight, n
    t.export(os.path.join(OUT, n + ".stl"))
    print(n, "watertight", t.is_watertight, "vol cm3 %.1f" % (t.volume / 1000), "bbox", np.round(t.bounds, 1).tolist())

# ------------------------------------------------------------------ parts for the viewer
parts = {}
def add(name, m): parts[name] = parts[name] + m if name in parts else m
ax0, ay0, ax1, ay1 = ACR
add("acrylic", rbox(ax0, ay0, ax1, ay1, Z(0), Z(ACR_T), 2))
add("glass", box(gx0 + 0.5, gy0 + 0.5, Z(0.1), gx1 - 0.5, gy1 - 0.5, Z(ACR_T + 3)))
px0, py0, px1, py1 = PCB
add("pcb", box(px0, py0, Z(PCB_BACK - 1.6), px1, py1, Z(PCB_BACK)))
for sx, sy in SCREWS: add("brass", cyl(sx, sy, Z(PCB_BACK), Z(DISP_D), 2.3, 6))
add("port", box(USB_C[0] - 4.5, IY0, Z(PCB_BACK), USB_C[0] + 4.5, IY0 + 7.5, Z(PCB_BACK + 3.2)))
add("port", box(px1 - 14, SD[0] - 7, Z(PCB_BACK), px1 + 1.5, SD[0] + 7, Z(PCB_BACK + 1.9)))
for (x, y) in (BOOT, RESET): add("button", cyl(x, y, Z(PCB_BACK), Z(PCB_BACK + 3.5), 1.6, 16))
for (x, y) in (BAT_PLUG, SPK_PLUG, I2C_PLUG, IO_PLUG):
    dx = 1 if x > 40 else -1
    add("plug", box(min(x, x + dx * 6), y - 3.5, Z(PCB_BACK), max(x, x + dx * 6), y + 3.5, Z(PCB_BACK + 5)))
b = BATT; add("battery", rbox(b[0], b[1], b[2], b[3], Z(b[4]), Z(b[5]), 2))
s = SPK; add("speaker", rbox(s[0], s[1], s[2], s[3], Z(s[4]), Z(s[5]), 8))
add("speaker_cone", cyl(SPK_C[0], SPK_C[1], Z(s[5]) - 0.8, Z(s[5]) - 0.2, 11))
jz1 = bz - JOY_LIFT
add("joy_board", box(jx0, jy0, jz1 - 1.6, jx1, jy1, jz1))
add("joy_body", box(JOY_STICK[0] - 8, JOY_STICK[1] - 8, jz1 - 13, JOY_STICK[0] + 8, JOY_STICK[1] + 8, jz1 - 1.6))
add("joy_body", Manifold.sphere(7.5, 32).translate([JOY_STICK[0], JOY_STICK[1], jz1 - 13]))
cap_top = jz1 - JOY_H
add("joy_cap", cyl(JOY_STICK[0], JOY_STICK[1], cap_top, cap_top + 7, 11.5, 64))
add("joy_cap", cyl(JOY_STICK[0], JOY_STICK[1], cap_top + 6, jz1 - 16, 3, 24))
d = DS; add("ds3231", box(d[0], d[1], Z(d[4]) + 4, d[2], d[3], Z(d[4]) + 5.6))
add("ds_coin", cyl((d[0] + d[2]) / 2, (d[1] + d[3]) / 2 + 5, Z(d[4]) + 5.6, Z(d[5]) - 0.2, 10))
add("ds3231", box(d[0] + 2, d[1] + 2, Z(d[4]) + 1.5, d[0] + 12, d[1] + 12, Z(d[4]) + 4))
k = KY; add("ky_board", box(k[0], k[1], Z(k[4]), k[2], k[3], Z(k[5])))
add("led", Manifold.cylinder(IX1 + 1.5 - k[2], 2.5, 2.5, 24).rotate([0, 90, 0]).translate([k[2], LED[0], Z(LED[1])]))
add("led", Manifold.sphere(2.5, 24).translate([IX1 + 1.5, LED[0], Z(LED[1])]))
sw = SW_BODY; add("switch", box(sw[0], sw[1], Z(sw[4]), sw[2] - 4, sw[3], Z(sw[5])))
add("switch", box(OX0 - 1.2, SW[0] - 7.5, Z(SW[1]) - 5, OX0, SW[0] + 7.5, Z(SW[1]) + 5))
add("switch_rocker", box(OX0 - 2.8, SW[0] - 5.5, Z(SW[1]) - 3.2, OX0 - 1.2, SW[0] + 5.5, Z(SW[1]) + 3.2))
add("switch", box(IX0, SW[0] - SW_HOLE[0] / 2 + 0.2, Z(SW[1]) - SW_HOLE[1] / 2 + 0.2, IX0 + 0.01, SW[0] + SW_HOLE[0] / 2 - 0.2, Z(SW[1]) + SW_HOLE[1] / 2 - 0.2))
add("splice", Manifold.sphere(2.2, 24).translate([SPLICE[0], SPLICE[1], Z(18)]))
for name, pts in WIRES.items():
    for (a, b2) in zip(pts, pts[1:]):
        p0 = np.array([a[0], a[1], Z(a[2])]); p1 = np.array([b2[0], b2[1], Z(b2[2])])
        v = p1 - p0; L = float(np.linalg.norm(v))
        if L < 1e-3: continue
        c = Manifold.cylinder(L, 0.7, 0.7, 10)
        dirn = v / L
        # rotate z-axis to dirn
        ang = math.degrees(math.acos(max(-1, min(1, dirn[2]))))
        axis = np.cross([0, 0, 1], dirn)
        if np.linalg.norm(axis) < 1e-6: m = c if dirn[2] > 0 else c.rotate([180, 0, 0])
        else:
            axis = axis / np.linalg.norm(axis)
            K = np.array([[0, -axis[2], axis[1]], [axis[2], 0, -axis[0]], [-axis[1], axis[0], 0]])
            th = math.radians(ang); R = np.eye(3) + math.sin(th) * K + (1 - math.cos(th)) * K @ K
            m = c.transform(np.hstack([R, np.zeros((3, 1))]).tolist())
        add("wire_" + name, m.translate(p0.tolist()))
        add("wire_" + name, Manifold.sphere(0.7, 10).translate(p1.tolist()))

# viewer frame: X right, Y up, Z toward the viewer; centre of the box at the origin
cx, cy, cz = (OX0 + OX1) / 2, (OY0 + OY1) / 2, OZ1 / 2
M = np.array([[1, 0, 0, -cx], [0, -1, 0, cy], [0, 0, -1, cz], [0, 0, 0, 1]], float)
scene = trimesh.Scene()
for name, m in [("front_shell", front), ("back_shell", back)] + list(parts.items()):
    t = tm(m); t.apply_transform(M)
    scene.add_geometry(t, node_name=name, geom_name=name)
scene.export(os.path.join(OUT, "somudtick_case.glb"))

# interference check: parts vs shells
for name, m in parts.items():
    if name.startswith("wire") or name in ("switch_rocker", "led", "joy_cap"): continue
    for sn, s_ in (("front", front), ("back", back)):
        v = (m ^ s_).volume()
        if v > 0.5: print("  overlap %-12s with %s shell: %.1f mm3" % (name, sn, v))
print("parts", len(parts))
