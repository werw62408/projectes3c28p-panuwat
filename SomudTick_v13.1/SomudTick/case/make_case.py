"""
SomudTick case v1 (v12 hardware): OLED 0.91" on top, ES3C28P screen (landscape) in the middle, NA011 button board below.
Game Boy style: 2 main parts (front shell, back plate) + 4 big button caps + 2 small button caps.

    python make_case.py        -> out/*.stl, out/preview.png, out/template_1to1.pdf

Needs only Python 3 + numpy + matplotlib. Every size is in mm and can be changed below, then run it again.
Numbers marked MEASURE come from the makers' drawings / a product photo: check them on your parts first
(print out/template_1to1.pdf at 100 % and lay the boards on it, or print fit_test.stl: about 15 minutes).

Axes: x to the right, y up (as you hold it), z = depth from the front face (0) towards the back.
"""
import math, os, struct
from collections import Counter
import numpy as np

# ============================== sizes ==============================
T_FACE = 2.0        # front face
T_WALL = 2.2        # side walls
T_BACK = 2.0        # back plate
R_CORNER = 7.0      # outside corner radius
CL = 0.4            # room around the boards

# --- ES3C28P (maker drawing ES3C28P_Size.pdf, measured from its vector lines) ---
SCR_L, SCR_H = 86.0, 50.0             # board, landscape (long side across)
SCR_HOLE_DX, SCR_HOLE_DY = 78.0, 42.0  # M3 holes 4 mm in from the edges
GLASS_L, GLASS_H, GLASS_T = 69.2, 50.0, 2.3
VIS_L, VIS_H = 59.45, 45.2            # visible area
VIS_OFF = 3.07                        # visible area centre: this far from the board centre, away from the USB end
USB_SIDE = -1                         # -1: USB-C on the left (screen setting "Wide"); +1: right ("Wide (flip)")
USB_OUT = 3.0                         # the USB-C receptacle sticks out of the board edge this much
SCR_PCB_FRONT = T_FACE + 4.5          # measured: glass front to the front of the PCB 4.5   [z]
SCR_PCB_T = 1.5
STANDOFF = 5.0                        # measured: brass pillars on the back of the board
SCR_BACK_PARTS = 10.0                 # measured: glass front to the tallest part on the back of the board
USB_Z = T_FACE + 9.0 - 1.6            # measured: glass front to the far edge of the USB-C 9.0 (receptacle 3.2 tall)
PLUG_W, PLUG_T = 12.5, 7.5            # opening for the cable plug

# --- OLED 0.91" SSD1306 128x32 module ---
OLED_L, OLED_H, OLED_T = 38.0, 12.0, 4.0     # MEASURE (with the pins cut short / wires soldered on)
OLED_WIN_L, OLED_WIN_H = 23.5, 6.8           # window (active area 22.4 x 5.6)
OLED_WIN_DX = 2.5                            # MEASURE: window centre right of the module centre (pins on the left)

# --- NA011 = Keyestudio/Funduino JoyStick Shield V1.A (measured 87 x 53, PCB 1.5) ---
NA_L, NA_H, NA_PCB_T = 87.0, 53.0, 1.5
# from the board's LEFT and TOP edge (read off the maker's photo): MEASURE
NA_JOY = (18.5, 30.9); JOY_KNOB_D = 20.0     # knob measured 20 across
JOY_HOLE_D = 28.0                            # MEASURE: knob + 2 x how far its edge moves when pushed all the way
NA_BTN = {"A": (65.0, 18.4), "B": (77.1, 29.9), "C": (65.0, 42.0), "D": (53.1, 29.9)}
BTN_CAP_D = 11.5                             # measured: colour caps 11.5 across
NA_SMALL = {"E": (47.5, 41.6), "F": (34.5, 41.6)}                                    # 6 x 6 tact buttons
NA_HOLES = [(4.2, 2.5), (82.8, 17.5)]        # MEASURE: mounting holes (3 mm, measured), their places from the photo
NA_HDR_TOP = 8.0                             # measured: the yellow 2x6 pin block stands 8 above the PCB
JOY_TOTAL = NA_PCB_T + 30.0                  # measured: top of the PCB to the top of the knob 30 (ASK: or from under the PCB?)
JOY_OUT = 8.0                                # knob stands out of the face this much
BTN_TOP = 14.0                               # measured: top of the PCB to the top of the colour caps
SMALL_TOP = 5.0                              # measured: top of the PCB to the top of the E/F stems
PINS_BELOW = 2.0                             # room under the NA011 (the long Arduino pins are taken off)
CAP_OUT = 1.8                                # printed caps stand out of the face this much

# --- things inside, glued / taped (measured, w x h x thickness) ---
BATT = (48.0, 30.0, 10.0)                    # LiPo in a bay on the back plate
SPK_D, SPK_EARS, SPK_T = 16.0, 30.0, 10.0    # round speaker 16 across with ears (30 end to end), 10 thick
DS3231 = (38.0, 21.5, 9.0)                   # clock module (pins taken off)
PCF = (35.0, 19.0, 9.0)                      # PCF8574 module with its right-angle pin rows taken off (41.5 with them)
IRB = (18.0, 15.0, 1.5)                      # IR board (LED about 5 high, bent to look out of the top wall)
SW_BODY = (12.0, 8.0, 10.0)                  # power switch body (12 long, 8 wide, 10 tall)

# --- extra openings ---
IR_X, IR_D = -20.0, 5.4                      # KY-005 IR LED (air conditioner remote) through the top wall
SW_SIDE = "L"                                # the small screen took the top right: the switch goes in the top of the LEFT wall
SW_Y, SW_L, SW_T = None, 9.0, 4.0            # power switch: slot for its lever (MEASURE the lever) in the wall opposite USB
                                             # None = just under the top corner screw

# layout gaps, top to bottom (the 7 between the small and the big screen makes room for the clock and PCF8574 inside)
GAP_TOP, GAP_OS, GAP_SN, GAP_BOT = 3.0, 7.0, 4.0, 3.0

# ============================== derived ==============================
NA_PCB_BACK = JOY_TOTAL - JOY_OUT
NA_PCB_FRONT = NA_PCB_BACK - NA_PCB_T
DEPTH = NA_PCB_BACK + PINS_BELOW + T_BACK
SHELL_D = DEPTH - T_BACK
IN_W = max(SCR_L + USB_OUT, NA_L, OLED_L) + 2 * CL
IN_H = GAP_TOP + OLED_H + GAP_OS + SCR_H + GAP_SN + NA_H + GAP_BOT + 2 * CL
OUT_W, OUT_H = IN_W + 2 * T_WALL, IN_H + 2 * T_WALL
OLED_CY = IN_H / 2 - CL - GAP_TOP - OLED_H / 2
SCR_CY = OLED_CY - OLED_H / 2 - GAP_OS - SCR_H / 2
NA_TOP = SCR_CY - SCR_H / 2 - GAP_SN
SCR_CX = -USB_SIDE * USB_OUT / 2
VIS_CX = SCR_CX - USB_SIDE * VIS_OFF
OLED_CX = IN_W / 2 - 6.9 - (OLED_L / 2 + CL + 1.2)   # small screen in the top RIGHT corner, just left of the corner screw post
NA_LEFT = -NA_L / 2
if SW_Y is None: SW_Y = IN_H / 2 - 7.2 - SW_BODY[0] / 2   # below the top corner screw post
def na(p): return (NA_LEFT + p[0], NA_TOP - p[1])
def scr_holes(): return [(SCR_CX + sx * SCR_HOLE_DX / 2, SCR_CY + sy * SCR_HOLE_DY / 2) for sx in (-1, 1) for sy in (-1, 1)]
BATT_POS = (SCR_CX - 8.0, SCR_CY)            # battery bay, behind the screen
SPK_POS = (SCR_CX + 28.0, SCR_CY)            # speaker, behind the screen on the side away from the USB
def corner_screws():
    ix, iy = IN_W / 2 - 3.4, IN_H / 2 - 3.4
    return [(-ix, iy), (ix, iy), (-ix, -iy), (ix, -iy)]

# ============================== a small 2.5D modeller ==============================
def circle(cx, cy, r, n=48): return [(cx + r * math.cos(2 * math.pi * i / n), cy + r * math.sin(2 * math.pi * i / n)) for i in range(n)]
def rect(cx, cy, w, h): return [(cx - w / 2, cy - h / 2), (cx + w / 2, cy - h / 2), (cx + w / 2, cy + h / 2), (cx - w / 2, cy + h / 2)]
def rrect(cx, cy, w, h, r, n=12):
    r = min(r, w / 2 - 0.01, h / 2 - 0.01); pts = []
    for qx, qy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(n + 1):
            a = math.radians(a0 + 90 * i / n); pts.append((cx + qx + r * math.cos(a), cy + qy + r * math.sin(a)))
    return pts
def area(p): return 0.5 * sum(p[i][0] * p[(i + 1) % len(p)][1] - p[(i + 1) % len(p)][0] * p[i][1] for i in range(len(p)))
def ccw(p): return p if area(p) > 0 else p[::-1]
def cw(p): return p if area(p) < 0 else p[::-1]
def _cross(o, a, b): return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
def _in_tri(p, a, b, c):
    d1, d2, d3 = _cross(a, b, p), _cross(b, c, p), _cross(c, a, p)
    return d1 >= -1e-9 and d2 >= -1e-9 and d3 >= -1e-9
def _x(a, b, c, d): return _cross(a, b, c) * _cross(a, b, d) < -1e-12 and _cross(c, d, a) * _cross(c, d, b) < -1e-12
def triangulate(outer, holes):
    """ear clipping; each hole is first joined to the outline by a bridge the other edges don't cross"""
    poly = list(ccw(outer)); holes = [cw(h) for h in holes]
    rings = [poly] + holes
    def solid_at(q):   # even-odd over the original outline and holes: True = q is in the material
        c = False
        for g in rings:
            for i in range(len(g)):
                a, b = g[i], g[(i + 1) % len(g)]
                if (a[1] > q[1]) != (b[1] > q[1]) and q[0] < a[0] + (q[1] - a[1]) * (b[0] - a[0]) / (b[1] - a[1]): c = not c
        return c
    for h in sorted(holes, key=lambda h: -max(p[0] for p in h)):
        hi = max(range(len(h)), key=lambda i: h[i][0]); hp = h[hi]
        edges = [(poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly))] + \
                [(g[i], g[(i + 1) % len(g)]) for g in holes for i in range(len(g))]
        used = Counter(poly)
        for i, p in sorted(enumerate(poly), key=lambda t: (t[1][0] - hp[0]) ** 2 + (t[1][1] - hp[1]) ** 2):
            if p == hp or used[p] > 1 or any(_x(hp, p, a, b) for a, b in edges): continue   # (not the end of an earlier join)
            if not all(solid_at((hp[0] + (p[0] - hp[0]) * s, hp[1] + (p[1] - hp[1]) * s)) for s in (0.001, 0.25, 0.5, 0.75, 0.999)): continue
            best = i; break
        else: raise RuntimeError("a hole can't be joined")
        poly = poly[:best + 1] + h[hi:] + h[:hi + 1] + poly[best:]
    def strictly_in(p, a, b, c):
        return _cross(a, b, p) > 1e-9 and _cross(b, c, p) > 1e-9 and _cross(c, a, p) > 1e-9
    tris, idx = [], list(range(len(poly)))
    while len(idx) > 3:
        n = len(idx); done = False
        for test in (_in_tri, strictly_in):   # 1) no point inside or on the ear  2) no point strictly inside
            for k in range(n):
                ia, ib, ic = idx[k - 1], idx[k], idx[(k + 1) % n]; a, b, c = poly[ia], poly[ib], poly[ic]
                if _cross(a, b, c) <= 1e-12: continue
                if any(test(poly[j], a, b, c) for j in idx if poly[j] != a and poly[j] != b and poly[j] != c): continue
                tris.append((a, b, c)); idx.pop(k); done = True; break
            if done: break
        if not done:   # only flat (collinear) corners left: drop one, it adds no area
            k = min(range(n), key=lambda k: abs(_cross(poly[idx[k - 1]], poly[idx[k]], poly[idx[(k + 1) % n]])))
            if abs(_cross(poly[idx[k - 1]], poly[idx[k]], poly[idx[(k + 1) % n]])) > 1e-6: raise RuntimeError("triangulation stuck")
            idx.pop(k)
    tris.append(tuple(poly[i] for i in idx))
    return tris

class Mesh:
    def __init__(self): self.t = []; self.solids = 0
    def extrude(self, outer, holes, z0, z1):
        """one closed solid: a 2D shape (outline + holes) from depth z0 to z1"""
        z0, z1 = min(z0, z1), max(z0, z1)
        if z1 - z0 < 1e-6: return self
        self.solids += 1
        for a, b, c in triangulate(outer, holes):
            if abs(_cross(a, b, c)) < 1e-12: continue
            self.t.append(((a[0], a[1], z1), (b[0], b[1], z1), (c[0], c[1], z1)))
            self.t.append(((a[0], a[1], z0), (c[0], c[1], z0), (b[0], b[1], z0)))
        for ring in [ccw(outer)] + [cw(h) for h in holes]:
            for i in range(len(ring)):
                a, b = ring[i], ring[(i + 1) % len(ring)]
                self.t.append(((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1)))
                self.t.append(((a[0], a[1], z0), (b[0], b[1], z1), (a[0], a[1], z1)))
        return self
    def cyl(self, cx, cy, r, z0, z1, r_in=0.0, n=48):
        return self.extrude(circle(cx, cy, r, n), [circle(cx, cy, r_in, n)] if r_in > 0 else [], z0, z1)
    def add(self, o): self.t += o.t; self.solids += o.solids; return self
    def save(self, path):
        with open(path, "wb") as f:
            f.write(b"SomudTick case".ljust(80, b" ")); f.write(struct.pack("<I", len(self.t)))
            for a, b, c in self.t:
                nr = np.cross(np.subtract(b, a), np.subtract(c, a)); L = np.linalg.norm(nr); nr = nr / L if L else nr
                f.write(struct.pack("<12fH", *nr, *a, *b, *c, 0))

# ============================== walls with openings ==============================
def walls(z0, z1, openings):
    """the wall ring as closed pieces: 4 corner arcs + 4 straight strips. openings: (side, a, b, zlo, zhi) cut into a
    straight strip; side 'L','R' (a..b along y) or 'T','B' (a..b along x)."""
    W2, H2, w2, h2 = OUT_W / 2, OUT_H / 2, IN_W / 2, IN_H / 2
    R = R_CORNER; r = R - T_WALL; ex, ey = W2 - R, H2 - R; n = 12; m = Mesh()
    for (sx, sy), a0 in {(1, 1): 0, (-1, 1): 90, (-1, -1): 180, (1, -1): 270}.items():
        cx, cy = sx * ex, sy * ey
        oa = [(cx + R * math.cos(math.radians(a0 + 90 * i / n)), cy + R * math.sin(math.radians(a0 + 90 * i / n))) for i in range(n + 1)]
        ia = [(cx + r * math.cos(math.radians(a0 + 90 * i / n)), cy + r * math.sin(math.radians(a0 + 90 * i / n))) for i in range(n + 1)]
        m.extrude(oa + ia[::-1], [], z0, z1)
    strips = {"T": ("x", -ex, ex, h2, H2), "B": ("x", -ex, ex, -H2, -h2), "L": ("y", -ey, ey, -W2, -w2), "R": ("y", -ey, ey, w2, W2)}
    for side, (ax, lo, hi, c0, c1) in strips.items():
        cuts = sorted([o for o in openings if o[0] == side], key=lambda o: o[1])
        def box(a, b, za, zb):
            if b - a < 1e-6 or zb - za < 1e-6: return
            m.extrude(rect((a + b) / 2, (c0 + c1) / 2, b - a, c1 - c0) if ax == "x" else rect((c0 + c1) / 2, (a + b) / 2, c1 - c0, b - a), [], za, zb)
        pos = lo
        for _, a, b, zlo, zhi in cuts:
            box(pos, a, z0, z1)                          # solid wall before the opening
            box(a, b, z0, max(z0, zlo)); box(a, b, min(z1, zhi), z1)   # below and above it
            pos = b
        box(pos, hi, z0, z1)
    return m

# ============================== the parts ==============================
def face_holes():
    h = [rect(VIS_CX, SCR_CY, VIS_L + 0.6, VIS_H + 0.6), rect(OLED_CX + OLED_WIN_DX, OLED_CY, OLED_WIN_L, OLED_WIN_H)]
    jx, jy = na(NA_JOY); h.append(circle(jx, jy, JOY_HOLE_D / 2, 64))
    for p in NA_BTN.values(): h.append(circle(*na(p), 13.4 / 2))
    for p in NA_SMALL.values(): h.append(circle(*na(p), 6.6 / 2, 32))
    return h

def openings():
    usb = ("L" if USB_SIDE < 0 else "R", SCR_CY - PLUG_W / 2, SCR_CY + PLUG_W / 2, USB_Z - PLUG_T / 2, USB_Z + PLUG_T / 2)
    ir = ("T", IR_X - IR_D / 2, IR_X + IR_D / 2, 8.0 - IR_D / 2, 8.0 + IR_D / 2)
    sw = (SW_SIDE, SW_Y - SW_L / 2, SW_Y + SW_L / 2, 9.0 - SW_T / 2, 9.0 + SW_T / 2)
    return [usb, ir, sw]

def front_shell():
    m = Mesh().extrude(rrect(0, 0, OUT_W, OUT_H, R_CORNER), face_holes(), 0, T_FACE)
    m.add(walls(T_FACE, SHELL_D, openings()))
    # glass frame (keeps the screen glass in place against the face)
    m.extrude(rect(SCR_CX, SCR_CY, GLASS_L + 2 * CL + 2.4, GLASS_H + 2 * CL + 2.4), [rect(SCR_CX, SCR_CY, GLASS_L + 2 * CL, GLASS_H + 2 * CL)], T_FACE, T_FACE + GLASS_T - 0.3)
    # pads on the screen PCB at its 4 holes (the glass is shorter than the board): the back plate screws press it on them
    for x, y in scr_holes(): m.cyl(x, y, 2.8, T_FACE, SCR_PCB_FRONT)
    # small screen: a frame the module drops into (a dab of hot glue behind it)
    m.extrude(rect(OLED_CX, OLED_CY, OLED_L + 2 * CL + 2.4, OLED_H + 2 * CL + 2.4), [rect(OLED_CX, OLED_CY, OLED_L + 2 * CL, OLED_H + 2 * CL)], T_FACE, T_FACE + OLED_T)
    # NA011 posts at its holes (M2.5 self-tapping screws from the back)
    for p in NA_HOLES: m.cyl(*na(p), 3.0, T_FACE, NA_PCB_FRONT, r_in=1.1)
    # (no guide tubes for the E / F stems: the caps go in from the inside and a tube on the same axis would block
    #  their top; the socket at the stem end keeps them centred on the tact button)
    # corner bosses for the back plate (M2.5 x 8 self-tapping)
    for x, y in corner_screws(): m.cyl(x, y, 3.2, T_FACE, SHELL_D, r_in=1.1)
    return m

def back_plate():
    holes = [circle(x, y, 1.45, 24) for x, y in corner_screws()]
    holes += [circle(x, y, 2.9, 32) for x, y in scr_holes()]                  # M3 head goes in (counterbore)
    holes += [circle(*na(p), 1.45, 24) for p in NA_HOLES]
    sx, sy = SPK_POS
    for i in range(-2, 3):                                                    # speaker grille (inside the 16 mm cone)
        for j in range(-2, 3):
            if i * i + j * j <= 5: holes.append(circle(sx + i * 3.0, sy + j * 3.0, 1.0, 12))
    m = Mesh().extrude(rrect(0, 0, OUT_W, OUT_H, R_CORNER), holes, SHELL_D, DEPTH)
    # lip inside the walls: 4 straight pieces (the corner bosses are left free)
    li = 0.3; t = 1.2; w2, h2 = IN_W / 2 - li, IN_H / 2 - li; k = 8.0
    for cx, cy, w, h in ((0, h2 - t / 2, 2 * (w2 - k), t), (0, -h2 + t / 2, 2 * (w2 - k), t), (w2 - t / 2, 0, t, 2 * (h2 - k)), (-w2 + t / 2, 0, t, 2 * (h2 - k))):
        m.extrude(rect(cx, cy, w, h), [], SHELL_D - 1.6, SHELL_D)
    # screen: M3 x 8 through a 3 mm floor into the brass pillars (the head sits deep in the post)
    pillar_end = SCR_PCB_FRONT + SCR_PCB_T + STANDOFF
    for x, y in scr_holes():
        m.cyl(x, y, 4.2, pillar_end, pillar_end + 3.0, r_in=1.7)             # floor the screw pulls on
        m.cyl(x, y, 4.2, pillar_end + 3.0, SHELL_D, r_in=2.9)                # post (head room inside)
    # spacers under the NA011 at its holes
    for p in NA_HOLES: m.cyl(*na(p), 3.0, NA_PCB_BACK, SHELL_D, r_in=1.45)
    # battery bay (48 x 30 x 10 LiPo) behind the screen: a low fence, foam tape inside
    bx, by = BATT_POS; bw, bh = BATT[0] + 1.0, BATT[1] + 1.0
    m.extrude(rect(bx, by, bw + 2.4, bh + 2.4), [rect(bx, by, bw, bh)], SHELL_D - 5, SHELL_D)
    # speaker: 4 pegs around the cone (the ears lie flat along y, between the pegs) + a dab of hot glue
    for a in (45, 135, 225, 315):
        r = SPK_D / 2 + 1.5; m.cyl(sx + r * math.cos(math.radians(a)), sy + r * math.sin(math.radians(a)), 1.2, SHELL_D - 4, SHELL_D, n=16)
    return m

# ============================== what goes where inside (and a check that nothing hits) ==============================
def inside_parts():
    """boxes (name, cx, cy, w, h, z0, z1) of everything inside, as seen from the front (x right, y up, z from the face)"""
    top = IN_H / 2 - CL
    parts = [
        ("battery", *BATT_POS, BATT[0], BATT[1], SHELL_D - BATT[2], SHELL_D),
        ("speaker", *SPK_POS, SPK_D, SPK_EARS, SHELL_D - SPK_T, SHELL_D),
        ("DS3231 clock", -18.0, top - DS3231[1] / 2 - 0.15, DS3231[0], DS3231[1], SHELL_D - DS3231[2], SHELL_D),
        ("PCF8574", 20.0, top - PCF[1] / 2 - 0.4, PCF[0], PCF[1], SHELL_D - PCF[2], SHELL_D),
        ("IR board", IR_X, top - IRB[1] / 2, IRB[0], IRB[1], T_FACE + OLED_T + 0.5, T_FACE + OLED_T + 0.5 + 4.0),
        ("switch", (1 if SW_SIDE == "R" else -1) * (IN_W / 2 - SW_BODY[2] / 2), SW_Y, SW_BODY[2], SW_BODY[0], 9.0 - SW_BODY[1] / 2, 9.0 + SW_BODY[1] / 2),
    ]
    fixed = [
        ("screen board", SCR_CX, SCR_CY, SCR_L, SCR_H, T_FACE, T_FACE + SCR_BACK_PARTS),
        ("small screen", OLED_CX, OLED_CY, OLED_L + 2 * CL + 2.4, OLED_H + 2 * CL + 2.4, T_FACE, T_FACE + OLED_T),
        ("NA011", NA_LEFT + NA_L / 2, NA_TOP - NA_H / 2, NA_L, NA_H, NA_PCB_FRONT, NA_PCB_BACK + PINS_BELOW),
    ]
    posts = [("screen post", x, y, 4.2, T_FACE, SHELL_D) for x, y in scr_holes()] + [("corner post", x, y, 3.2, T_FACE, SHELL_D) for x, y in corner_screws()]
    return parts, fixed, posts

def check_inside():
    parts, fixed, posts = inside_parts(); bad = []
    def zo(a0, a1, b0, b1): return min(a1, b1) - max(a0, b0) > 0.05
    def rr(a, b): return abs(a[1] - b[1]) < (a[3] + b[3]) / 2 - 0.05 and abs(a[2] - b[2]) < (a[4] + b[4]) / 2 - 0.05 and zo(a[5], a[6], b[5], b[6])
    def rc(a, c):
        dx = max(abs(c[1] - a[1]) - a[3] / 2, 0); dy = max(abs(c[2] - a[2]) - a[4] / 2, 0)
        return dx * dx + dy * dy < c[3] ** 2 - 0.05 and zo(a[5], a[6], c[4], c[5])
    for i, a in enumerate(parts):
        for b in parts[i + 1:] + fixed:
            if rr(a, b): bad.append(f"{a[0]} hits {b[0]}")
        for c in posts:
            if rc(a, c): bad.append(f"{a[0]} hits a {c[0]}")
        if abs(a[1]) + a[3] / 2 > IN_W / 2 + 0.01 or abs(a[2]) + a[4] / 2 > IN_H / 2 + 0.01: bad.append(f"{a[0]} sticks out of the case")
    print("  inside: " + ("everything fits" if not bad else "; ".join(bad)))
    return not bad

def layout(path):
    """the inside seen from the back (back plate off): left and right are swapped compared with the front"""
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon, Rectangle, Circle
    parts, fixed, posts = inside_parts()
    fig, ax = plt.subplots(figsize=(7.2, 9.6)); ax.set_aspect("equal"); ax.axis("off")
    ax.add_patch(Polygon([(-x, y) for x, y in rrect(0, 0, OUT_W, OUT_H, R_CORNER)], fill=False, lw=1.2))
    for n, x, y, w, h, z0, z1 in fixed:
        ax.add_patch(Rectangle((-x - w / 2, y - h / 2), w, h, fill=True, fc="#eeeeee", ec="#999999", ls="--", lw=0.8))
        ax.text(-x, y - h / 2 + 2, n, ha="center", fontsize=7, color="#777777")
    for n, x, y, r, z0, z1 in posts: ax.add_patch(Circle((-x, y), r, fc="#dddddd", ec="#888888", lw=0.6))
    cols = {"battery": "#f4a261", "speaker": "#90caf9", "DS3231 clock": "#a5d6a7", "PCF8574": "#ce93d8", "IR board": "#ef9a9a", "switch": "#ffe082"}
    for n, x, y, w, h, z0, z1 in parts:
        ax.add_patch(Rectangle((-x - w / 2, y - h / 2), w, h, fc=cols.get(n, "#ccc"), ec="k", lw=0.8, alpha=0.85))
        ax.text(-x, y, f"{n}\n{w:g} x {h:g}\nz {z0:.1f}-{z1:.1f}", ha="center", va="center", fontsize=6.5)
    ax.text(-USB_SIDE * (OUT_W / 2 + 4), SCR_CY, "USB-C", rotation=90, ha="center", va="center", fontsize=8)
    ax.text(0, OUT_H / 2 + 6, f"inside, seen from the BACK (back plate off)   case {OUT_W:.1f} x {OUT_H:.1f} x {DEPTH:.1f} mm", ha="center", fontsize=8.5)
    ax.text(0, -OUT_H / 2 - 7, "z = depth from the front face; the battery, speaker, clock and PCF8574 lie on the back plate side\n"
            "gray dashed = boards fixed to the front shell, gray circles = screw posts", ha="center", fontsize=7)
    ax.set_xlim(-OUT_W / 2 - 10, OUT_W / 2 + 10); ax.set_ylim(-OUT_H / 2 - 14, OUT_H / 2 + 10)
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)

def btn_cap():   # printed with the top down: plunger through the face, a wide body that can't come out, a skirt on the colour cap
    z_top, z_c = -CAP_OUT, NA_PCB_FRONT - BTN_TOP
    return (Mesh().cyl(0, 0, 6.45, z_top, T_FACE + 0.3)
            .cyl(0, 0, 7.9, T_FACE + 0.3, z_c)
            .cyl(0, 0, 7.9, z_c, z_c + 2.5, r_in=BTN_CAP_D / 2 + 0.35))

def small_cap():  # E / F: button top down, flange, a stem with a socket over the 3.5 mm tact stem
    z_top, z_c = -CAP_OUT, NA_PCB_FRONT - SMALL_TOP
    return (Mesh().cyl(0, 0, 3.1, z_top, T_FACE + 0.3, n=32)
            .cyl(0, 0, 4.3, T_FACE + 0.3, T_FACE + 1.5, n=32)
            .cyl(0, 0, 2.0, T_FACE + 1.5, z_c, n=32)
            .cyl(0, 0, 2.6, z_c, z_c + 1.5, r_in=1.9, n=32))

def fit_test(): return Mesh().extrude(rrect(0, 0, OUT_W, OUT_H, R_CORNER), face_holes(), 0, 1.2)

JOY_TEST_D = (26.0, 32.0)   # the two joystick holes in na_test.stl
def na_test():
    """two small test faces for the button board, side by side (joystick hole 26 and 32): the same holes and height as
    the real case face. Printed face down; put it over the NA011 (corner feet on the PCB corners, L guides around its
    edges): the knob comes out 8 mm, the colour caps show through their holes. Push the stick around to feel the hole."""
    m = Mesh(); ox0, oy0 = NA_LEFT + NA_L / 2, NA_TOP - NA_H / 2   # board centre in case coordinates
    rim = CL + 1.6; pw, ph = NA_L + 2 * rim, NA_H + 2 * rim; leg = NA_PCB_FRONT - T_FACE
    for k, jd in enumerate(JOY_TEST_D):
        dx, dy = 0.0, (0.5 - k) * (ph + 8)       # one above the other (fits a small print bed)
        def loc(p): x, y = na(p); return (x - ox0 + dx, y - oy0 + dy)
        holes = [circle(*loc(NA_JOY), jd / 2, 64)]
        holes += [circle(*loc(p), (BTN_CAP_D + 1.0) / 2) for p in NA_BTN.values()]   # bare colour caps
        holes += [circle(*loc(p), 6.6 / 2, 32) for p in NA_SMALL.values()]
        m.extrude(rrect(dx, dy, pw, ph, 3.0), holes, 0, T_FACE)
        for sx in (-1, 1):
            for sy in (-1, 1):
                cx, cy = dx + sx * NA_L / 2, dy + sy * NA_H / 2     # a board corner
                m.extrude(rect(cx - sx * 1.5, cy - sy * 1.5, 2.6, 2.6), [], T_FACE, T_FACE + leg)   # foot on the PCB corner
                # L guide just outside the board edges (0.4 room), 1.2 longer than the foot so the board sits in it
                box = lambda x0, y0, x1, y1: rect((x0 + x1) / 2, (y0 + y1) / 2, abs(x1 - x0), abs(y1 - y0))
                m.extrude(box(cx - sx * 7, cy + sy * CL, cx + sx * (CL + 1.6), cy + sy * (CL + 1.6)), [], T_FACE, T_FACE + leg + 1.2)
                m.extrude(box(cx + sx * CL, cy - sy * 7, cx + sx * (CL + 1.6), cy + sy * CL), [], T_FACE, T_FACE + leg + 1.2)
    return m

# ============================== checks, pictures, template ==============================
def check(m, name):
    e = Counter()
    for a, b, c in m.t:
        for u, v in ((a, b), (b, c), (c, a)): e[(tuple(np.round(u, 5)), tuple(np.round(v, 5)))] += 1
    bad = sum(1 for (u, v), k in e.items() if e.get((v, u), 0) != k)
    mn = np.array(m.t).reshape(-1, 3).min(0); mx = np.array(m.t).reshape(-1, 3).max(0)
    print(f"  {name:12s} {len(m.t):6d} triangles, {m.solids:3d} solids, size {mx[0]-mn[0]:6.1f} x {mx[1]-mn[1]:6.1f} x {mx[2]-mn[2]:5.1f}   "
          + ("closed OK" if bad == 0 else f"{bad} OPEN EDGES"))
    return bad == 0

def preview(parts, path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig = plt.figure(figsize=(22, 7), dpi=100)
    caps = Mesh()
    for i, nm in enumerate(["btn_cap", "small_cap"]): caps.t += [tuple((p[0] + i * 22, p[1], p[2]) for p in t) for t in parts[nm].t]
    def seen_from_front(m):   # z points into the case: turn it so the face looks at the camera (no mirror)
        o = Mesh(); o.t = [tuple((p[0], p[1], -p[2]) for p in (t[0], t[2], t[1])) for t in m.t]; return o
    fr = seen_from_front(parts["front_shell"])
    views = [("front (as you hold it)", fr, 90, -90), ("front, from the side", fr, 25, -60), ("front shell, inside", parts["front_shell"], 90, 90),
             ("back plate, inside", seen_from_front(parts["back_plate"]), -90, 90), ("caps", caps, 20, -60)]
    for i, (title, m, el, az) in enumerate(views):
        ax = fig.add_subplot(1, 5, i + 1, projection="3d")
        tr = np.array(m.t); nr = np.cross(tr[:, 1] - tr[:, 0], tr[:, 2] - tr[:, 0]); L = np.linalg.norm(nr, axis=1, keepdims=True); L[L == 0] = 1
        el_r, az_r = math.radians(el), math.radians(az)
        eye = np.array([math.cos(el_r) * math.cos(az_r), math.cos(el_r) * math.sin(az_r), math.sin(el_r)])
        sh = np.abs((nr / L) @ eye) * 0.75 + 0.2
        ax.add_collection3d(Poly3DCollection(tr, facecolors=[(0.25 + 0.6 * s * 0.6, 0.4 + 0.5 * s * 0.6, 0.55 + 0.4 * s * 0.6) for s in sh], edgecolors="none"))
        mn, mx = tr.reshape(-1, 3).min(0), tr.reshape(-1, 3).max(0); c = (mn + mx) / 2; s = (mx - mn).max() / 2
        ax.set_xlim(c[0] - s, c[0] + s); ax.set_ylim(c[1] - s, c[1] + s); ax.set_zlim(c[2] - s, c[2] + s)
        ax.view_init(el, az); ax.set_axis_off(); ax.set_title(title)
    fig.tight_layout(); fig.savefig(path); plt.close(fig)

def template(path):
    """1:1 face + boards: print at 100 % (no 'fit to page'), check the 100 mm line, lay the boards on it."""
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Polygon, Rectangle, Circle
    fig = plt.figure(figsize=(210 / 25.4, 297 / 25.4)); ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(-105, 105); ax.set_ylim(-148.5, 148.5); ax.set_aspect("equal"); ax.axis("off")
    ax.add_patch(Polygon(rrect(0, 0, OUT_W, OUT_H, R_CORNER), fill=False, lw=1.0))
    for h in face_holes(): ax.add_patch(Polygon(h, fill=False, lw=0.8, color="tab:blue"))
    for (x, y, w, h) in ((SCR_CX - SCR_L / 2, SCR_CY - SCR_H / 2, SCR_L, SCR_H), (NA_LEFT, NA_TOP - NA_H, NA_L, NA_H), (OLED_CX - OLED_L / 2, OLED_CY - OLED_H / 2, OLED_L, OLED_H)):
        ax.add_patch(Rectangle((x, y), w, h, fill=False, ls="--", lw=0.5, color="gray"))
    for x, y in scr_holes(): ax.add_patch(Circle((x, y), 1.6, fill=False, color="red", lw=0.6))
    for p in NA_HOLES: ax.add_patch(Circle(na(p), 1.3, fill=False, color="red", lw=0.6))
    for k, p in {**NA_BTN, **NA_SMALL}.items(): ax.text(*na(p), k, ha="center", va="center", fontsize=8)
    ax.text(USB_SIDE * (OUT_W / 2 + 4), SCR_CY, "USB-C", rotation=90, ha="center", va="center", fontsize=7)
    ax.text(IR_X, OUT_H / 2 + 3, "IR", ha="center", fontsize=7)
    ax.text(0, OUT_H / 2 + 12, f"SomudTick case  {OUT_W:.1f} x {OUT_H:.1f} x {DEPTH:.1f} mm  -  print at 100 %", ha="center", fontsize=9)
    ax.plot([-50, 50], [-OUT_H / 2 - 10] * 2, color="k", lw=1); ax.text(0, -OUT_H / 2 - 15, "this line must measure 100 mm", ha="center", fontsize=8)
    ax.text(0, -OUT_H / 2 - 21, "blue = holes in the face   gray dashed = boards   red = screw holes: check them on your boards", ha="center", fontsize=7)
    fig.savefig(path); fig.savefig(path.replace(".pdf", "_view.png"), dpi=110); plt.close(fig)

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out"); os.makedirs(out, exist_ok=True)
    print(f"case {OUT_W:.1f} x {OUT_H:.1f} x {DEPTH:.1f} mm  (front shell {SHELL_D:.1f} + back plate {T_BACK:.1f})")
    parts = {"front_shell": front_shell(), "back_plate": back_plate(), "btn_cap": btn_cap(), "small_cap": small_cap(), "fit_test": fit_test(), "na_test": na_test()}
    ok = all(check(m, n) for n, m in parts.items())
    ok = check_inside() and ok
    for n, m in parts.items(): m.save(os.path.join(out, n + ".stl"))
    preview(parts, os.path.join(out, "preview.png")); template(os.path.join(out, "template_1to1.pdf")); layout(os.path.join(out, "inside_layout.png"))
    print("written to", out, "" if ok else "  (SOME PARTS ARE NOT CLOSED)")
