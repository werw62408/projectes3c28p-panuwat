"""Shape helpers shared by the case builds (manifold3d)."""
import numpy as np
import trimesh
from manifold3d import Manifold, OpType

SEG = 64

def box(x0, y0, z0, x1, y1, z1):
    return Manifold.cube([x1 - x0, y1 - y0, z1 - z0]).translate([x0, y0, z0])

def cyl(x, y, z0, z1, r, seg=SEG, r2=None):
    return Manifold.cylinder(z1 - z0, r, r if r2 is None else r2, seg).translate([x, y, z0])

def rbox(x0, y0, x1, y1, z0, z1, r):
    if r <= 0: return box(x0, y0, z0, x1, y1, z1)
    return Manifold.batch_hull([cyl(x, y, z0, z1, r) for x in (x0 + r, x1 - r) for y in (y0 + r, y1 - r)])

def rbox4(x0, y0, x1, y1, z0, z1, radii):
    """rounded box with its own radius per corner: radii = (top-left, top-right, bottom-left, bottom-right)"""
    tl, tr, bl, br = radii
    return Manifold.batch_hull([cyl(x0 + tl, y0 + tl, z0, z1, tl), cyl(x1 - tr, y0 + tr, z0, z1, tr),
                                cyl(x0 + bl, y1 - bl, z0, z1, bl), cyl(x1 - br, y1 - br, z0, z1, br)])

def crbox4(x0, y0, x1, y1, z0, z1, radii, ch):
    """rbox4 with a 45 degree chamfer on the front and back edges"""
    tl, tr, bl, br = radii
    parts = []
    for (x, y, r) in ((x0 + tl, y0 + tl, tl), (x1 - tr, y0 + tr, tr), (x0 + bl, y1 - bl, bl), (x1 - br, y1 - br, br)):
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
    return Manifold.batch_boolean(ms, OpType.Add) if len(ms) > 1 else ms[0]

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
    """rows go toward +d (down the page when you look at it)"""
    cells = []
    for i, ch in enumerate(text):
        for r, row in enumerate(FONT[ch]):
            for c, bit in enumerate(row):
                if bit == "1":
                    x = x0 + (i * 6 + c) * px; d = d0 + r * px
                    cells.append(box(x + 0.08, d + 0.08, z0, x + px - 0.08, d + px - 0.08, z1))
    return union(cells)

def tm(m):
    mesh = m.to_mesh()
    return trimesh.Trimesh(vertices=np.array(mesh.vert_properties)[:, :3], faces=np.array(mesh.tri_verts), process=False)
