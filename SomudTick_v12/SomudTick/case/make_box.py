"""
SomudTick in a ready-made ABS box (177 x 94 x 35, white) instead of a printed case.

    python make_box.py   -> out_box/drill_template.pdf (print at 100 %), out_box/inside_layout.png,
                            out_box/drill_list.txt, out_box/box_asm.json (for the 3D view)

Everything is fixed to the BODY of the box (its flat bottom is the front face). The LID (the back) has nothing on it
and no wires to it: take out its screws and lift it, and all the parts are there to see.

Face coordinates are what you measure on the box: x from the LEFT outside edge, y from the TOP outside edge
(looking at the face, small screen top right, buttons at the bottom). z = depth from the outside of the face.
Numbers marked MEASURE: check them on the real box / parts first.
"""
import json, math, os

# ============================== the box ==============================
BOX_W, BOX_H, BOX_D = 94.0, 177.0, 35.0   # outside, closed (seller's size)
WALL = 2.5                                  # MEASURE: side wall
FACE = 2.5                                  # MEASURE: the body's flat bottom (= the front face)
LID = 2.5                                   # MEASURE: lid thickness
POST_R, POST_IN = 3.5, 4.5                  # MEASURE: corner screw posts inside the body (radius, centre from the inside walls)
IN_D = BOX_D - FACE - LID                   # inside depth
BODY_D = BOX_D - LID                        # the body alone (face + walls)

# ============================== parts (measured, or the maker's usual size) ==============================
# 1.3" OLED I2C (SH1106 128x64): usual module 35.4 x 33.5, 4 x M2 holes 30.4 x 28.4, pins along the top edge
OLED = dict(w=35.4, h=33.5, t=4.5, win_w=31.0, win_h=16.5, win_dy=1.5)   # MEASURE when it arrives (win_dy: window below the module centre)
SCR = dict(w=86.0, h=50.0, glass_w=69.2, glass_h=50.0, vis_w=59.45, vis_h=45.2, vis_off=3.07,
           pcb_front=4.5, pcb_t=1.5, back=10.0, standoff=5.0, hole_dx=78.0, hole_dy=42.0, usb_out=3.0, usb_z=7.4)
NA = dict(w=87.0, h=53.0, pcb_t=1.5, joy=(18.5, 30.9), knob_d=20.0, joy_total=30.0, btn_top=14.0, small_top=5.0,
          btn={"A": (65.0, 18.4), "B": (77.1, 29.9), "C": (65.0, 42.0), "D": (53.1, 29.9)}, small={"E": (47.5, 41.6), "F": (34.5, 41.6)},
          holes=[(4.2, 2.5), (82.8, 17.5)], cap_d=11.5)   # button places from the maker's photo: check on the paper template
NA_SPACER = 10.0          # M3 spacers face -> NA011: the colour caps then stand 1.5 out of the face (no printed caps needed)
BATT = (48.0, 30.0, 10.0); DS3231 = (38.0, 21.5, 9.0); PCF = (35.0, 19.0, 9.0)
SPK_D, SPK_EARS, SPK_T = 16.0, 30.0, 10.0
IR_TX = (18.0, 15.0)      # KY-005 IR LED board (sends: air conditioner remote)
IR_RX = (15.0, 18.5)      # KY-022 IR receiver board (learns codes from a remote), turned so its dome looks at the top wall
SW = (12.0, 8.0, 10.0)    # power switch body; lever slot MEASURE
SW_SLOT = (9.0, 4.0)

# ============================== the layout (face coordinates) ==============================
IX0, IX1, IY0, IY1 = WALL, BOX_W - WALL, WALL, BOX_H - WALL          # inside, seen through the face
MIDX = BOX_W / 2
posts = [(IX0 + POST_IN, IY0 + POST_IN), (IX1 - POST_IN, IY0 + POST_IN), (IX0 + POST_IN, IY1 - POST_IN), (IX1 - POST_IN, IY1 - POST_IN)]
# big screen: USB-C on the LEFT, its receptacle sits in a slot in the left wall
scr_left = IX0 + SCR["usb_out"] - 1.5
OLED_TOP = IY0 + 2.0
scr_c = (scr_left + SCR["w"] / 2, OLED_TOP + OLED["h"] + 6.0 + SCR["h"] / 2)
vis_c = (scr_c[0] + SCR["vis_off"], scr_c[1])
# small screen: top right, its window's right edge in line with the big screen window's right edge
oled_c = (vis_c[0] + (SCR["vis_w"] + 0.6) / 2 - OLED["win_w"] / 2, OLED_TOP + OLED["h"] / 2)
# button board below
na_c = (MIDX, scr_c[1] + SCR["h"] / 2 + 7.0 + NA["h"] / 2)
na_left, na_top = na_c[0] - NA["w"] / 2, na_c[1] - NA["h"] / 2
def na(p): return (na_left + p[0], na_top + p[1])
NA_FRONT = FACE + NA_SPACER; NA_BACK = NA_FRONT + NA["pcb_t"]
# speaker bottom right (Game Boy style grille), IR boards top left, switch in the left wall
spk_c = (68.0, na_top + NA["h"] + 1.0 + SPK_D / 2 + 1.0)
irtx_c = (IX0 + 10.0 + IR_TX[0] / 2, IY0 + 0.8 + IR_TX[1] / 2)
irrx_c = (irtx_c[0] + IR_TX[0] / 2 + 1.0 + IR_RX[0] / 2, IY0 + 0.8 + IR_RX[1] / 2)
sw_y = irtx_c[1] + IR_TX[1] / 2 + 3.0 + SW[0] / 2
# behind the button board: battery, clock, PCF8574 on double-sided foam (over a sheet of insulating tape)
batt_c = (na_left + 0.5 + BATT[0] / 2, na_top + 1.0 + BATT[1] / 2)
# (the clock starts 12 mm down: the NA011's yellow 2x6 pin block, where its wires are soldered, stays free above it)
HDR = (74.0, 6.0)          # MEASURE: middle of the yellow 2x6 block, from the NA011's left and top edge
ds_c = (batt_c[0] + BATT[0] / 2 + 1.0 + DS3231[0] / 2, na_top + 12.0 + DS3231[1] / 2)
pcf_c = (ds_c[0], ds_c[1] + DS3231[1] / 2 + 2.0 + PCF[1] / 2)
Z_BEHIND = NA_BACK + 2.5   # pins + foam

def boxes():
    """(name, cx, cy, w, h, z0, z1, colour) in face coordinates"""
    P = []
    P.append(("OLED 1.3\"", *oled_c, OLED["w"], OLED["h"], FACE, FACE + OLED["t"], "#1a3a7a"))
    P.append(("screen board", *scr_c, SCR["w"], SCR["h"], FACE + SCR["pcb_front"], FACE + SCR["pcb_front"] + SCR["pcb_t"], "#1f5f3a"))
    P.append(("screen glass", *scr_c, SCR["glass_w"], SCR["glass_h"], FACE, FACE + 2.3, "#10161c"))
    P.append(("screen back parts", *scr_c, SCR["w"] - 16, SCR["h"] - 14, FACE + SCR["pcb_front"] + SCR["pcb_t"], FACE + SCR["back"], "#2c3a33"))
    P.append(("USB-C", scr_left - SCR["usb_out"] / 2 + 2.0, scr_c[1], SCR["usb_out"] + 4.0, 9.0, FACE + SCR["usb_z"] - 1.6, FACE + SCR["usb_z"] + 1.6, "#b8bec4"))
    P.append(("NA011 button board", *na_c, NA["w"], NA["h"], NA_FRONT, NA_BACK, "#b3262c"))
    P.append(("battery", *batt_c, BATT[0], BATT[1], Z_BEHIND, Z_BEHIND + BATT[2], "#e07b39"))
    P.append(("DS3231 clock", *ds_c, DS3231[0], DS3231[1], Z_BEHIND, Z_BEHIND + DS3231[2], "#3b8f5e"))
    P.append(("PCF8574", *pcf_c, PCF[0], PCF[1], Z_BEHIND, Z_BEHIND + PCF[2], "#7a4fb0"))
    P.append(("IR LED board", *irtx_c, IR_TX[0], IR_TX[1], FACE, FACE + 1.5, "#c94c4c"))
    P.append(("IR receiver board", *irrx_c, IR_RX[0], IR_RX[1], FACE, FACE + 1.5, "#d98a3a"))
    P.append(("switch", IX0 + SW[2] / 2, sw_y, SW[2], SW[0], 9.0 - SW[1] / 2, 9.0 + SW[1] / 2, "#e0c040"))
    P.append(("speaker ears", *spk_c, SPK_EARS, 5.0, FACE, FACE + 1.2, "#2f5f94"))
    return P
def cylinders():
    C = [("speaker", *spk_c, SPK_D / 2, FACE + 1.0, FACE + 1.0 + SPK_T, "#4a90d9")]   # back taped to the face, cone to the lid
    jx, jy = na(NA["joy"]); knob_top = NA_BACK - NA["pcb_t"] - NA["joy_total"]
    C.append(("joystick knob", jx, jy, NA["knob_d"] / 2, knob_top, knob_top + 6, "#222222"))
    C.append(("joystick stick", jx, jy, 2.5, knob_top + 6, NA_FRONT - 11, "#555555"))
    C.append(("joystick base", jx, jy, 8.0, NA_FRONT - 11, NA_FRONT, "#2b2b2b"))
    for (k, p), c in zip(NA["btn"].items(), ["#2d7fd6", "#e8c21e", "#d93b3b", "#3fae4f"]):
        x, y = na(p); C.append(("button " + k, x, y, NA["cap_d"] / 2, NA_FRONT - NA["btn_top"], NA_FRONT, c))
    for k, p in NA["small"].items():
        x, y = na(p); C.append(("stem " + k, x, y, 1.75, NA_FRONT - NA["small_top"], NA_FRONT, "#111111"))
        C.append(("stub " + k, x, y, 1.7, FACE - 0.8, NA_FRONT - NA["small_top"], "#f2f2f2"))     # a 3.5 mm plastic rod glued on
    for dx in (-1, 1):
        for dy in (-1, 1):
            C.append(("brass pillar", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 2.5, FACE + SCR["pcb_front"] + SCR["pcb_t"], FACE + SCR["pcb_front"] + SCR["pcb_t"] + SCR["standoff"], "#c9a44a"))
            C.append(("spacer 4.5", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 2.5, FACE, FACE + SCR["pcb_front"], "#c9a44a"))
    for p in NA["holes"]: x, y = na(p); C.append(("spacer 10", x, y, 2.5, FACE, NA_FRONT, "#c9a44a"))
    for x, y in posts: C.append(("box post", x, y, POST_R, FACE, BODY_D, "#dddddd"))
    return C

# ============================== the holes to make ==============================
def face_cuts():
    """(kind, name, cx, cy, a, b): kind 'rect' (a x b) or 'circle' (a = diameter), in face coordinates"""
    H = [("rect", "small screen window", oled_c[0], oled_c[1] + OLED["win_dy"], OLED["win_w"], OLED["win_h"]),
         ("rect", "big screen window", vis_c[0], vis_c[1], SCR["vis_w"] + 0.6, SCR["vis_h"] + 0.6)]
    jx, jy = na(NA["joy"]); H.append(("circle", "joystick", jx, jy, 28.0, 0))
    for k, p in NA["btn"].items(): H.append(("circle", "button " + k, *na(p), NA["cap_d"] + 1.0, 0))
    for k, p in NA["small"].items(): H.append(("circle", "button " + k, *na(p), 4.2, 0))
    for dx in (-1, 1):
        for dy in (-1, 1): H.append(("circle", "M3 screen", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 3.2, 0))
    for p in NA["holes"]: H.append(("circle", "M3 buttons", *na(p), 3.2, 0))
    return H
def lid_cuts():
    """speaker grille in the LID: the speaker's back (with its tape) sticks inside the face, its cone looks at the lid.
    (cx, cy) in face coordinates (as seen from the FRONT); the template flips it to how you see the lid from the back."""
    return [("circle", "speaker", spk_c[0] + i * 4.0, spk_c[1] + j * 4.0, 2.5, 0) for i in range(-1, 2) for j in range(-1, 2) if i * i + j * j <= 2]
def wall_cuts():
    """(wall, name, along, z, a, b): wall 'L' (along = y from the top edge) or 'T' (along = x from the left edge);
    z = from the FACE side edge of the wall; rect a (along) x b (z)"""
    return [("L", "USB-C plug", scr_c[1], FACE + SCR["usb_z"], 12.5, 7.5),
            ("L", "switch lever", sw_y, 9.0, *SW_SLOT),
            ("T", "IR LED (round 5.4)", irtx_c[0], FACE + 1.5 + 2.7, 5.4, 5.4),
            ("T", "IR receiver", irrx_c[0], FACE + 1.5 + 4.0, 7.0, 7.5)]

# ============================== checks ==============================
def check():
    bad = []; B = boxes(); C = cylinders()
    def zo(a0, a1, b0, b1): return min(a1, b1) - max(a0, b0) > 0.05
    keep = [b for b in B if b[0] not in ("screen glass", "screen back parts", "USB-C")]
    for i, a in enumerate(keep):
        if a[1] - a[3] / 2 < IX0 - 0.01 and a[0] not in ("switch",) or a[1] + a[3] / 2 > IX1 + 0.01 or a[2] - a[4] / 2 < IY0 - 0.01 or a[2] + a[4] / 2 > IY1 + 0.01:
            bad.append(f"{a[0]} is outside the box")
        if a[6] > BODY_D + 0.01: bad.append(f"{a[0]} is too deep ({a[6]:.1f} > {BODY_D:.1f})")
        for b in keep[i + 1:]:
            if b[0] == "screen board" and a[0] == "screen glass": continue
            if abs(a[1] - b[1]) < (a[3] + b[3]) / 2 - 0.05 and abs(a[2] - b[2]) < (a[4] + b[4]) / 2 - 0.05 and zo(a[5], a[6], b[5], b[6]): bad.append(f"{a[0]} hits {b[0]}")
        for c in C:
            if c[0] != "box post" and c[0] != "speaker": continue
            if c[0] == "speaker" and a[0] == "speaker ears": continue
            dx = max(abs(c[1] - a[1]) - a[3] / 2, 0); dy = max(abs(c[2] - a[2]) - a[4] / 2, 0)
            if dx * dx + dy * dy < c[3] ** 2 - 0.05 and zo(a[5], a[6], c[4], c[5]): bad.append(f"{a[0]} hits the {c[0]}")
    knob_out = -(NA_BACK - NA["pcb_t"] - NA["joy_total"])
    print(f"box {BOX_W} x {BOX_H} x {BOX_D}, inside {IX1 - IX0:.1f} x {IY1 - IY0:.1f} x {IN_D:.1f}")
    print(f"  joystick knob stands {knob_out:.1f} mm out of the face, colour caps {NA['btn_top'] - NA_SPACER - FACE:.1f} mm out")
    print("  inside: " + ("everything fits" if not bad else "; ".join(bad)))
    return not bad

# ============================== the paper template ==============================
def template(path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    def page():
        fig = plt.figure(figsize=(210 / 25.4, 297 / 25.4)); ax = fig.add_axes([0, 0, 1, 1])
        ax.set_xlim(0, 210); ax.set_ylim(0, 297); ax.set_aspect("equal"); ax.axis("off"); return fig, ax
    def cross(ax, X, Y, s=2.0): ax.plot([X - s, X + s], [Y, Y], color="k", lw=0.4); ax.plot([X, X], [Y - s, Y + s], color="k", lw=0.4)
    with PdfPages(path) as pdf:
        # page 1: the face, 1:1
        fig, ax = page(); ox, oy = (210 - BOX_W) / 2, 297 - 40 - BOX_H
        T = lambda x, y: (ox + x, oy + BOX_H - y)
        ax.add_patch(FancyBboxPatch(T(0, BOX_H), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.0))
        ax.add_patch(Rectangle(T(IX0, IY1), IX1 - IX0, IY1 - IY0, fill=False, lw=0.4, ls=":", color="gray"))
        for n, cx, cy, w, h, z0, z1, col in boxes():
            if n in ("screen board", "NA011 button board", "OLED 1.3\""):
                ax.add_patch(Rectangle(T(cx - w / 2, cy + h / 2), w, h, fill=False, lw=0.5, ls="--", color="gray"))
        for x, y in posts: ax.add_patch(Circle(T(x, y), POST_R, fill=False, lw=0.4, ls=":", color="gray"))
        for kind, n, cx, cy, a, b in face_cuts():
            X, Y = T(cx, cy)
            if kind == "rect": ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue"))
            else: ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue" if a > 3.5 else "tab:red"))
            cross(ax, X, Y, 1.5 if a < 5 else 2.5)
        for n, lab in (("small screen window", "small screen"), ("big screen window", "big screen"), ("joystick", "joystick 28")):
            k = [c for c in face_cuts() if c[1] == n][0]; X, Y = T(k[2], k[3]); ax.text(X, Y + (4 if n != "joystick" else 0), lab, ha="center", va="center", fontsize=6, color="tab:blue")
        for k, p in {**NA["btn"], **NA["small"]}.items(): X, Y = T(*na(p)); ax.text(X + 3.5, Y + 3.5, k, fontsize=6)
        X, Y = T(*spk_c); ax.add_patch(Circle((X, Y), SPK_D / 2, fill=False, lw=0.5, ls="--", color="gray"))
        ax.text(X, Y - 11, "speaker sticks here inside (no holes: its grille is in the lid)", ha="center", fontsize=5)
        ax.text(105, 297 - 12, "SomudTick - drilling template: FRONT FACE (the flat bottom of the box body)", ha="center", fontsize=9, weight="bold")
        ax.text(105, 297 - 18, "print at 100 % (Actual size, no 'fit to page'). The line at the bottom must measure 100 mm.", ha="center", fontsize=7)
        ax.text(105, 297 - 23, "Cut out along the outer outline, tape it on the face, lay the boards on it to check, then mark the centres (+) and drill.", ha="center", fontsize=6.5)
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        ax.text(105, 22, "blue = openings   red = screw holes (3.2)   gray dashed = where the boards sit behind   dotted = inside of the box, corner posts", ha="center", fontsize=6)
        fig.savefig(path.replace(".pdf", "_face.png"), dpi=90); pdf.savefig(fig); plt.close(fig)
        # page 2: the walls
        fig, ax = page()
        ax.text(105, 297 - 12, "SomudTick - drilling template: WALLS (1:1)", ha="center", fontsize=9, weight="bold")
        # left wall: 177 long, BODY_D high, the face side at the bottom
        lx, ly = 16, 297 - 40 - 177
        ax.add_patch(Rectangle((lx, ly), BODY_D, BOX_H, fill=False, lw=1.0))
        ax.text(lx + BODY_D / 2, ly + BOX_H + 4, "LEFT wall", ha="center", fontsize=8, weight="bold")
        ax.text(lx - 3, ly + BOX_H / 2, "face side", rotation=90, ha="center", va="center", fontsize=6.5)
        ax.text(lx + BODY_D / 2, ly - 5, "bottom end (button board)", ha="center", fontsize=6)
        ax.text(lx + BODY_D / 2, ly + BOX_H + 9, "top end (small screen)", ha="center", fontsize=6)
        for w, n, along, z, a, b in wall_cuts():
            if w != "L": continue
            X, Y = lx + z, ly + BOX_H - along
            ax.add_patch(Rectangle((X - b / 2, Y - a / 2), b, a, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y)
            ax.text(X + b / 2 + 2, Y, f"{n}\n{a:g} x {b:g}\n{along:.1f} from top, {z:.1f} from face", fontsize=5.5, va="center")
        # top wall: 94 long
        tx, ty = 100, 297 - 60
        ax.add_patch(Rectangle((tx, ty), BOX_W, BODY_D, fill=False, lw=1.0))
        ax.text(tx + BOX_W / 2, ty + BODY_D + 4, "TOP wall (seen from above, face side down)", ha="center", fontsize=8, weight="bold")
        ax.text(tx + BOX_W / 2, ty - 4, "face side", ha="center", fontsize=6.5)
        for i, (w, n, along, z, a, b) in enumerate([c for c in wall_cuts() if c[0] == "T"]):
            X, Y = tx + along, ty + z
            if "round" in n: ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue"))
            else: ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue"))
            ly2 = ty + BODY_D - 5 - i * 9                     # labels one under the other, with a leader line
            cross(ax, X, Y); ax.plot([X, X], [Y + b / 2, ly2 - 1.5], color="gray", lw=0.4)
            ax.text(X + 1.5, ly2, f"{n}: {along:.1f} from left, {z:.1f} from face", fontsize=5.5, va="center")
        ax.text(tx, 150, "MEASURE the box when it comes: wall, face and lid thickness,\n"
                "and the corner posts. Tell me the numbers and the template\nis made again to fit exactly.", fontsize=7, va="top")
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        fig.savefig(path.replace(".pdf", "_walls.png"), dpi=90); pdf.savefig(fig); plt.close(fig)
        # page 3: the lid, seen from the BACK (outside of the lid): left and right are swapped compared with the face
        fig, ax = page(); ox, oy = (210 - BOX_W) / 2, 297 - 40 - BOX_H
        L = lambda x, y: (ox + (BOX_W - x), oy + BOX_H - y)
        ax.add_patch(FancyBboxPatch(L(BOX_W, BOX_H), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.0))
        X, Y = L(*spk_c); ax.add_patch(Circle((X, Y), SPK_D / 2, fill=False, lw=0.5, ls="--", color="gray"))
        for kind, n, cx, cy, a, b in lid_cuts():
            X, Y = L(cx, cy); ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y, 1.5)
        X, Y = L(*spk_c); ax.text(X, Y - 12, f"speaker grille: 7 holes 2.5 mm\ncentre {BOX_W - spk_c[0]:.1f} from the left edge, {spk_c[1]:.1f} from the top\n(as you look at the BACK)", ha="center", fontsize=6, va="top")
        ax.text(105, 297 - 12, "SomudTick - drilling template: LID (seen from the BACK, outside of the lid)", ha="center", fontsize=9, weight="bold")
        ax.text(105, 297 - 18, "Top of the page = the end with the small screen. Only the speaker grille, nothing is fixed to the lid.", ha="center", fontsize=7)
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        fig.savefig(path.replace(".pdf", "_lid.png"), dpi=90); pdf.savefig(fig); plt.close(fig)

def drill_list(path):
    L = ["SomudTick box - holes (mm). Face: x from the LEFT edge, y from the TOP edge (looking at the face).", ""]
    for kind, n, cx, cy, a, b in face_cuts():
        L.append(f"  face  {n:22s} centre x {cx:6.1f}  y {cy:6.1f}   " + (f"{a:.1f} x {b:.1f} rect" if kind == "rect" else f"diameter {a:.1f}"))
    L.append("")
    for w, n, along, z, a, b in wall_cuts():
        L.append(f"  {'left' if w == 'L' else 'top '} wall {n:18s} {'y' if w == 'L' else 'x'} {along:6.1f}  from face {z:5.1f}   {a:.1f} x {b:.1f}")
    L += ["", "  LID, looking at the BACK: x from the LEFT edge of the lid as you see it from behind, y from the top (small screen end)"]
    for kind, n, cx, cy, a, b in lid_cuts():
        L.append(f"  lid   {n:22s} centre x {BOX_W - cx:6.1f}  y {cy:6.1f}   diameter {a:.1f}")
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")

def layout(path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    fig, ax = plt.subplots(figsize=(6.6, 11)); ax.set_aspect("equal"); ax.axis("off")
    M = lambda x: BOX_W - x   # seen from the back: left and right swap
    ax.add_patch(FancyBboxPatch((0, 0), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.2))
    for n, cx, cy, w, h, z0, z1, col in boxes():
        if n in ("screen glass", "speaker ears"): continue
        ax.add_patch(Rectangle((M(cx) - w / 2, BOX_H - cy - h / 2), w, h, fc=col, ec="k", lw=0.6, alpha=0.8 if z0 > 12 else 0.45))
        if n != "screen board": ax.text(M(cx), BOX_H - cy, f"{n}\nz {z0:.0f}-{z1:.0f}", ha="center", va="center", fontsize=6, color="k")
    for c in cylinders():
        if c[0] in ("speaker", "box post"): ax.add_patch(Circle((M(c[1]), BOX_H - c[2]), c[3], fc=c[6], ec="k", lw=0.5, alpha=0.8))
    ax.text(BOX_W / 2, BOX_H + 8, "inside the box body, seen from the BACK (lid off)", ha="center", fontsize=9)
    ax.text(BOX_W / 2, -8, "z = depth from the outside of the face (mm); the lid has nothing on it", ha="center", fontsize=7)
    ax.set_xlim(-8, BOX_W + 8); ax.set_ylim(-14, BOX_H + 14)
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)

def asm_json(path):
    """for the 3D view: x right, y up (centre origin), z into the box"""
    cx0, cy0 = BOX_W / 2, BOX_H / 2
    X = lambda x: x - cx0; Y = lambda y: cy0 - y
    items = [dict(k="box", name=n, cx=X(cx), cy=Y(cy), w=w, h=h, z0=z0, z1=z1, c=col) for n, cx, cy, w, h, z0, z1, col in boxes()]
    items += [dict(k="cyl", name=n, cx=X(cx), cy=Y(cy), r=r, z0=z0, z1=z1, c=col) for n, cx, cy, r, z0, z1, col in cylinders() if n != "box post"]
    holes = [dict(kind=k, cx=X(cx), cy=Y(cy), a=a, b=b) for k, n, cx, cy, a, b in face_cuts()]
    walls = [dict(wall=w, along=(Y(al) if w == "L" else X(al)), z=z, a=a, b=b) for w, n, al, z, a, b in wall_cuts()]
    B = {b[0]: b for b in boxes()}; Cy = {c[0]: c for c in cylinders()}
    def at(name): b = B[name]; return (X(b[1]), Y(b[2]), (b[5] + b[6]) / 2)
    jx, jy = na(NA["joy"]); knob_z = NA_BACK - NA["pcb_t"] - NA["joy_total"]
    labels = [  # number markers in the 3D view: what it is, where, how it is held
        ('จอเล็ก OLED 1.3" (SH1106 128×64)', at('OLED 1.3"'), "มุมขวาบน ขอบขวาตรงกับจอใหญ่ · แปะเทปกาวสองหน้าด้านในหน้ากล่อง (หรือน็อต M2)"),
        ("จอใหญ่ ES3C28P (ตัวบอร์ดหลัก)", (X(scr_c[0] - 30), Y(scr_c[1] - 17), FACE + SCR["pcb_front"]), "กลางกล่อง · น็อต M3 4 ตัวผ่านหน้ากล่อง + เสารอง 4.5 มม. เข้าเสาทองเหลืองเดิมของจอ"),
        ("ช่อง USB-C (ชาร์จ / แฟลช)", at("USB-C"), "ผนังซ้าย ตรงกลางจอใหญ่ · หัวช่องเสียบอยู่ในรูผนังพอดี"),
        ("บอร์ดปุ่ม NA011", at("NA011 button board"), "ล่าง · น็อต M3 2 ตัว + เสารอง 10 มม. · ปุ่มสี A B C D ขวา, E F กลาง"),
        ("จอย (โผล่ 17.5 มม.)", (X(jx), Y(jy), knob_z + 3), "ซ้ายล่าง ติดกับบอร์ดปุ่ม · รูหน้ากล่อง 28 มม. ให้โยกได้"),
        ("แบต LiPo 48×30×10", at("battery"), "หลังบอร์ดปุ่ม (ฝั่งซ้ายเมื่อมองจากหน้า) · เทปโฟม ทับเทปกันไฟฟ้าก่อน"),
        ("นาฬิกา DS3231", at("DS3231 clock"), "หลังบอร์ดปุ่ม ขวาบน · เทปโฟม"),
        ("PCF8574 (ตัวอ่านปุ่ม 7 ปุ่ม)", at("PCF8574"), "หลังบอร์ดปุ่ม ขวาล่าง · เทปโฟม"),
        ("หลอดส่ง IR KY-005 (รีโมทแอร์)", at("IR LED board"), "มุมซ้ายบน วางราบกับหน้ากล่อง · หลอดงอ 90° ชี้ออกรูผนังบน"),
        ("ตัวรับ IR KY-022 (เรียนรู้รีโมท)", at("IR receiver board"), "ถัดไปทางขวา วางราบ · ตัวรับมองออกช่องผนังบน"),
        ("สวิตช์เปิดปิด", at("switch"), "ผนังซ้ายด้านบน · ก้านโผล่ช่องผนัง หยอดกาวร้อนยึด"),
        ("ลำโพง Ø16", (X(spk_c[0]), Y(spk_c[1]), Cy["speaker"][4] + 5), "ขวาล่าง ใต้บอร์ดปุ่ม · หลังลำโพงแปะกาวติดด้านในหน้ากล่อง รูเสียง 7 รูที่ฝาหลัง"),
    ]
    # free room left inside (for extras later): (name, cx, cy, w, h, z0, z1) in face coordinates
    free = [("ว่าง: หลังจอใหญ่", scr_c[0], scr_c[1], SCR["hole_dx"] - 10, SCR["h"] - 6, FACE + SCR["pcb_front"] + SCR["pcb_t"] + SCR["standoff"] + 0.5, BODY_D - 0.5),
            ("ว่าง: ซ้ายล่าง ข้างลำโพง", (IX0 + POST_IN + POST_R + 1 + spk_c[0] - SPK_EARS / 2 - 1) / 2, (na_top + NA["h"] + IY1) / 2,
             spk_c[0] - SPK_EARS / 2 - 1 - (IX0 + POST_IN + POST_R + 1), IY1 - (na_top + NA["h"]) - 1, FACE + 0.5, BODY_D - 0.5),
            ("ว่าง: ใต้บอร์ด IR", (IX0 + SW[2] + 1 + oled_c[0] - OLED["w"] / 2 - 1) / 2, (irrx_c[1] + IR_RX[1] / 2 + OLED_TOP + OLED["h"]) / 2 + 0.5,
             oled_c[0] - OLED["w"] / 2 - 1 - (IX0 + SW[2] + 1), OLED_TOP + OLED["h"] - (irrx_c[1] + IR_RX[1] / 2) - 1, FACE + 0.5, BODY_D - 0.5)]
    free_j = [dict(name=n, cx=X(cx), cy=Y(cy), w=w, h=h, z0=z0, z1=z1, size=f"{w:.0f} × {h:.0f} ลึก {z1 - z0:.0f} มม.") for n, cx, cy, w, h, z0, z1 in free]
    json.dump(dict(box=[BOX_W, BOX_H, BOX_D], wall=WALL, face=FACE, lid=LID,
                   labels=[dict(name=n, x=p[0], y=p[1], z=p[2], where=w) for n, p, w in labels], free=free_j, posts=[[X(x), Y(y)] for x, y in posts], post_r=POST_R,
                   items=items, holes=holes, walls=walls,
                   lid_holes=[dict(kind=k, cx=X(cx), cy=Y(cy), a=a, b=b) for k, n, cx, cy, a, b in lid_cuts()]), open(path, "w"))

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box"); os.makedirs(out, exist_ok=True)
    ok = check()
    template(os.path.join(out, "drill_template.pdf")); drill_list(os.path.join(out, "drill_list.txt"))
    layout(os.path.join(out, "inside_layout.png")); asm_json(os.path.join(out, "box_asm.json"))
    print("written to", out, "" if ok else "  (SOMETHING DOES NOT FIT)")
