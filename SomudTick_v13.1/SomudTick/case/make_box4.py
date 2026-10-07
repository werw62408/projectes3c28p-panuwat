"""
SomudTick in the REAL grey ABS box (measured 6 Oct 2026), fourth layout (from layout 2; layout 3 = the clear D384 box):
  - the box is two halves that close on each other: 172 x 94, 34.5 high when closed
      front half (the FACE): 16 outside / 14 inside, 2 short screw posts at the top end
      back half (the LID):   19 outside / 16.5 inside, 2 screw posts (screws go through it) + 6 tall board posts
  - walls 2.5, face 2.0, lid 2.5
  - everything is fixed to the FRONT half; the back half only has the speaker grille
  - the small screen sits between the front screw posts, centred over the big screen's window
  - IR LED board top left, IR receiver top right (under the screw posts), the rocker switch in the left wall
  - one 18650 cell across the bottom end; the back half's posts 7 and 8 must be cut down (see POSTS)

    python make_box4.py  -> out_box4/drill_template.pdf (print at 100 %), out_box4/inside_layout.png,
                            out_box4/drill_list.txt, out_box4/box_asm.json

Face coordinates: x from the LEFT outside edge, y from the TOP outside edge (looking at the face, the screw-post end
at the top, buttons at the bottom). z = depth from the outside of the face.
"""
import json, math, os

# ============================== the box ==============================
BOX_W, BOX_H, BOX_D = 94.0, 172.0, 34.5   # outside, closed (measured)
WALL = 2.5                                  # side walls (measured)
FACE = 2.0                                  # front half: 16 outside, 14 inside
LID = 2.5                                   # back half: 19 outside, 16.5 inside
SEAM = 16.0                                 # where the halves meet, from the outside of the face (wall holes must stay above it)
IN_D = BOX_D - FACE - LID                   # inside depth
BODY_D = BOX_D - LID                        # the deepest a part may go (the inside of the back half)
POST_R = 3.5                                # posts drawn with this radius (MEASURE their diameter if a part comes close)
# the posts (measured from the outside edges): (name, x, y, height from its own floor, half)
# the back half is a mirror image when closed, but its posts are placed the same on both sides, so x stays
POSTS = [("front post 1", 19.5, 7.5, 5.5, "F"), ("front post 2", 74.5, 7.5, 5.5, "F"),
         ("back post 1", 19.5, 7.5, 6.5, "B"), ("back post 2", 74.5, 7.5, 6.5, "B"),
         ("back post 3", 12.5, 18.5, 10.5, "B"), ("back post 4", 81.5, 18.5, 10.5, "B"),
         ("back post 5", 9.0, 87.0, 10.5, "B"), ("back post 6", 85.0, 87.0, 10.5, "B"),
         ("back post 7", 12.5, 155.0, 10.5, "B"), ("back post 8", 81.5, 155.0, 10.5, "B")]
CUT_POSTS = {"back post 7": 0.0, "back post 8": 0.0}   # cut these off flush (the battery, 24 at its wire end, lies under them)
def post_z(name, h, half):
    h = CUT_POSTS.get(name, h)
    return (FACE, FACE + h) if half == "F" else (BODY_D - h, BODY_D)

# ============================== parts (measured, or the maker's usual size) ==============================
# 1.3" OLED I2C (SH1106 128x64): usual module 35.4 x 33.5, 4 x M2 holes 30.4 x 28.4, pins along the top edge
OLED = dict(w=35.5, h=33.5, t=3.7, win_w=31.0, win_h=16.5, win_dy=-1.9)   # DST-013 module, measured 35.5 x 33.5 x 3.7; from the photo the glass (34.7 x 18.4) sits 1.9 mm ABOVE the board centre, pins GND VDD SCK SDA on the top edge
SCR = dict(w=86.0, h=50.0, glass_w=69.2, glass_h=50.0, vis_w=59.45, vis_h=45.2, vis_off=3.07,
           pcb_front=4.5, pcb_t=1.5, back=10.0, standoff=5.0, hole_dx=78.0, hole_dy=42.0, usb_out=3.0, usb_z=7.4)
NA = dict(w=87.0, h=53.0, pcb_t=1.5, joy=(18.5, 30.9), knob_d=20.0, joy_total=30.0, btn_top=14.0, small_top=5.0,
          btn={"A": (65.0, 18.4), "B": (77.1, 29.9), "C": (65.0, 42.0), "D": (53.1, 29.9)}, small={"E": (47.5, 41.6), "F": (34.5, 41.6)},
          holes=[(4.2, 2.5), (82.8, 17.5)], cap_d=11.5)   # button places from the maker's photo: check on the paper template
NA_SPACER = 10.0          # M3 spacers face -> NA011: the colour caps then stand 1.5 out of the face (no printed caps needed)
BATT = (72.0, 24.0, 24.0);   # 18650 + BMS + shrink: measured 18.5 thick, 24 at the end with the wires (taken all along)
HUB = (30.0, 20.0, 4.0)     # perfboard hub 30 x 20, 3V3 / GND / SDA / SCL rails
DS3231 = (38.0, 21.5, 9.0); PCF = (35.0, 19.0, 9.0)
SPK = (40.0, 28.0, 10.0)    # oval speaker (measured)
IR_TX = (18.0, 15.0)      # KY-005 IR LED board (sends: air conditioner remote)
IR_RX = (15.0, 18.5)      # KY-022 IR receiver board (learns codes from a remote), turned so its dome looks at the top wall
SW = (12.0, 8.5, 9.0)     # rocker switch body (measured): 12 along the wall, 8.5 deep in z, 9 into the box
SW_SLOT = (12.5, 9.0)     # its hole in the left wall (+0.5)

# ============================== the layout (face coordinates) ==============================
IX0, IX1, IY0, IY1 = WALL, BOX_W - WALL, WALL, BOX_H - WALL          # inside, seen through the face
MIDX = BOX_W / 2
# big screen: USB-C on the LEFT, its receptacle sits in a slot in the left wall
scr_left = IX0 + SCR["usb_out"] - 1.5
OLED_TOP = IY0 + 0.5
scr_c = (scr_left + SCR["w"] / 2, OLED_TOP + OLED["h"] + 2.0 + SCR["h"] / 2)
vis_c = (scr_c[0] + SCR["vis_off"], scr_c[1])
# small screen: between the two front screw posts, centred over the big screen's window
oled_c = (vis_c[0], OLED_TOP + OLED["h"] / 2)
# button board below
na_c = (MIDX, scr_c[1] + SCR["h"] / 2 + 2.0 + NA["h"] / 2)
na_left, na_top = na_c[0] - NA["w"] / 2, na_c[1] - NA["h"] / 2
def na(p): return (na_left + p[0], na_top + p[1])
NA_FRONT = FACE + NA_SPACER; NA_BACK = NA_FRONT + NA["pcb_t"]
# speaker bottom right (Game Boy style grille), IR boards top left, switch in the left wall
spk_c = (scr_c[0] + 8.0, scr_c[1] + 4.0)               # behind the big screen, under the grille in the back half
# the IR boards lie flat just under the screw posts (posts end 11 mm from the top): LED board left, receiver right
IR_Y0 = 12.0
irtx_c = (IX0 + 1.5 + IR_TX[0] / 2, IR_Y0 + IR_TX[1] / 2)
irrx_c = (IX1 - 1.5 - IR_RX[0] / 2, IR_Y0 + IR_RX[1] / 2)
IR_LED_X = 9.0                                         # the LED is bent up to the top wall, left of front post 1
# rocker switch in the BOTTOM wall, left corner (the side walls have latch tabs at 38-49 and 143-155 from the top)
sw_x = IX0 + 0.5 + SW_SLOT[0] / 2
sw_y = IY1 - SW[2] / 2
# behind the button board: battery, clock, PCF8574 on double-sided foam (over a sheet of insulating tape)
batt_c = (sw_x + SW_SLOT[0] / 2 + 1.0 + BATT[0] / 2, na_top + NA["h"] + 1.0 + BATT[1] / 2)   # across the bottom strip, right of the switch; its wire end on the LEFT (next to the switch)
# (the clock starts 12 mm down: the NA011's yellow 2x6 pin block, where its wires are soldered, stays free above it)
HDR = (52.0, 8.0)          # the solder pads V G A B C D E F K 3 X Y (2 x 6, left of the yellow block; owner's photo 7 Oct), from the NA011's left and top edge
ds_c = (na_left + 1.0 + DS3231[0] / 2, na_top + 12.0 + DS3231[1] / 2)
pcf_c = (ds_c[0] + DS3231[0] / 2 + 2.0 + PCF[0] / 2, na_top + 12.0 + PCF[1] / 2)
hub_c = (scr_c[0] - 20.0, scr_c[1] + 6.0)                 # behind the big screen, left of the speaker (seen from the front)
Z_SCR_BACK = FACE + SCR["back"]                            # back of the big screen's parts
SPK_Z0 = BODY_D - 0.5 - SPK[2]                              # speaker on a foam pad, its cone 0.5 mm under the lid
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
    P.append(("18650 battery", *batt_c, BATT[0], BATT[1], FACE + 0.5, FACE + 0.5 + BATT[2], "#e07b39"))
    P.append(("hub board", *hub_c, HUB[0], HUB[1], Z_SCR_BACK + 0.5, Z_SCR_BACK + 0.5 + HUB[2], "#9e8a5a"))
    P.append(("DS3231 clock", *ds_c, DS3231[0], DS3231[1], Z_BEHIND, Z_BEHIND + DS3231[2], "#3b8f5e"))
    P.append(("PCF8574", *pcf_c, PCF[0], PCF[1], Z_BEHIND, Z_BEHIND + PCF[2], "#7a4fb0"))
    P.append(("IR LED board", *irtx_c, IR_TX[0], IR_TX[1], FACE, FACE + 1.5, "#c94c4c"))
    P.append(("IR receiver board", *irrx_c, IR_RX[0], IR_RX[1], FACE, FACE + 1.5, "#d98a3a"))
    P.append(("switch", sw_x, sw_y, SW[0], SW[2], 9.0 - SW[1] / 2, 9.0 + SW[1] / 2, "#e0c040"))
    P.append(("speaker", *spk_c, SPK[0], SPK[1], SPK_Z0, SPK_Z0 + SPK[2], "#4a90d9"))
    P.append(("speaker foam pad", *spk_c, 18.0, 18.0, Z_SCR_BACK + 0.5, SPK_Z0, "#d8d0b8"))
    return P
def cylinders():
    C = []
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
    for n, x, y, h, half in POSTS: C.append(("box post", x, y, POST_R, *post_z(n, h, half), "#dddddd"))
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
    """speaker grille in the LID: the speaker sits on a foam pad behind the big screen, its cone right under these holes.
    (cx, cy) in face coordinates (as seen from the FRONT); the template flips it to how you see the lid from the back."""
    return [("circle", "speaker", spk_c[0] + i * 5.0, spk_c[1] + j * 5.0, 2.5, 0) for i in range(-2, 3) for j in range(-1, 2)]
def wall_cuts():
    """(wall, name, along, z, a, b): wall 'L' (along = y from the top edge) or 'T' (along = x from the left edge);
    z = from the FACE side edge of the wall; rect a (along) x b (z)"""
    # wall 'B' = bottom wall, along = x from the left edge
    return [("L", "USB-C plug", scr_c[1], FACE + SCR["usb_z"], 12.5, 7.5),
            ("B", "rocker switch", sw_x, 9.0, *SW_SLOT),
            ("T", "IR LED (round 5.4)", IR_LED_X, FACE + 1.5 + 2.7, 5.4, 5.4),
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
            if c[0] != "box post": continue
            dx = max(abs(c[1] - a[1]) - a[3] / 2, 0); dy = max(abs(c[2] - a[2]) - a[4] / 2, 0)
            if dx * dx + dy * dy < c[3] ** 2 - 0.05 and zo(a[5], a[6], c[4], c[5]): bad.append(f"{a[0]} hits the {c[0]}")
    for w, n, along, z, a, b in wall_cuts():
        if z + b / 2 > SEAM - 1.0: bad.append(f"wall hole {n} reaches the seam of the halves")
    for k, n, cx, cy, a, b in face_cuts():
        for pn, x, y, h, half in POSTS:
            if half != "F": continue
            r = (a / 2 if k == "circle" else 0); dx = max(abs(x - cx) - (a / 2 if k == "rect" else 0), 0) - r; dy = max(abs(y - cy) - (b / 2 if k == "rect" else 0), 0) - (r if k == "circle" else 0)
            if max(dx, 0) ** 2 + max(dy, 0) ** 2 < (POST_R + 1.0) ** 2: bad.append(f"face hole {n} cuts into {pn}")
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
        for pn, x, y, h, half in POSTS:
            if half == "F": ax.add_patch(Circle(T(x, y), POST_R, fill=False, lw=0.6, color="gray")); X, Y = T(x, y); ax.text(X, Y - POST_R - 2.5, "post", ha="center", fontsize=5, color="gray")
        for kind, n, cx, cy, a, b in face_cuts():
            X, Y = T(cx, cy)
            if kind == "rect": ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue"))
            else: ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue" if a > 3.5 else "tab:red"))
            cross(ax, X, Y, 1.5 if a < 5 else 2.5)
        for n, lab in (("small screen window", "small screen"), ("big screen window", "big screen"), ("joystick", "joystick 28")):
            k = [c for c in face_cuts() if c[1] == n][0]; X, Y = T(k[2], k[3]); ax.text(X, Y + (4 if n != "joystick" else 0), lab, ha="center", va="center", fontsize=6, color="tab:blue")
        for k, p in {**NA["btn"], **NA["small"]}.items(): X, Y = T(*na(p)); ax.text(X + 3.5, Y + 3.5, k, fontsize=6)
        X, Y = T(batt_c[0] - BATT[0] / 2, batt_c[1] + BATT[1] / 2); ax.add_patch(Rectangle((X, Y), BATT[0], BATT[1], fill=False, lw=0.5, ls="--", color="gray"))
        X, Y = T(*batt_c); ax.text(X, Y, "18650 battery lies here inside (no holes)", ha="center", va="center", fontsize=5)
        ax.text(105, 297 - 12, "SomudTick - drilling template: FRONT FACE (outside of the half with the 2 short posts)", ha="center", fontsize=9, weight="bold")
        ax.text(105, 297 - 18, "print at 100 % (Actual size, no 'fit to page'). The line at the bottom must measure 100 mm.", ha="center", fontsize=7)
        ax.text(105, 297 - 23, "Cut out along the outer outline, tape it on the face, lay the boards on it to check, then mark the centres (+) and drill.", ha="center", fontsize=6.5)
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        ax.text(105, 22, "blue = openings   red = screw holes (3.2)   gray dashed = where the boards sit behind   gray circles = the 2 posts inside (no holes there)", ha="center", fontsize=6)
        fig.savefig(path.replace(".pdf", "_face.png"), dpi=90); pdf.savefig(fig); plt.close(fig)
        # page 2: the walls
        fig, ax = page()
        ax.text(105, 297 - 12, "SomudTick - drilling template: WALLS (1:1)", ha="center", fontsize=9, weight="bold")
        # left wall of the FRONT half: BOX_H long, SEAM high, the face side at the left
        lx, ly = 16, 297 - 40 - BOX_H
        ax.add_patch(Rectangle((lx, ly), SEAM, BOX_H, fill=False, lw=1.0))
        ax.text(lx + SEAM / 2, ly + BOX_H + 4, "LEFT wall (front half)", ha="center", fontsize=8, weight="bold")
        ax.text(lx - 3, ly + BOX_H / 2, "face side", rotation=90, ha="center", va="center", fontsize=6.5)
        ax.text(lx + SEAM / 2, ly - 5, "bottom end (button board, switch)", ha="center", fontsize=6)
        ax.text(lx + SEAM / 2, ly + BOX_H + 9, "top end (screw posts)", ha="center", fontsize=6)
        for w, n, along, z, a, b in wall_cuts():
            if w != "L": continue
            X, Y = lx + z, ly + BOX_H - along
            ax.add_patch(Rectangle((X - b / 2, Y - a / 2), b, a, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y)
            ax.text(X + b / 2 + 2, Y, f"{n}\n{a:g} x {b:g}\n{along:.1f} from top, {z:.1f} from face", fontsize=5.5, va="center")
        # top wall: 94 long
        tx, ty = 100, 297 - 60
        ax.add_patch(Rectangle((tx, ty), BOX_W, SEAM, fill=False, lw=1.0))
        ax.text(tx + BOX_W / 2, ty + SEAM + 4, "TOP wall of the front half (seen from above, face side down)", ha="center", fontsize=8, weight="bold")
        ax.text(tx + BOX_W / 2, ty - 4, "face side", ha="center", fontsize=6.5)
        for i, (w, n, along, z, a, b) in enumerate([c for c in wall_cuts() if c[0] == "T"]):
            X, Y = tx + along, ty + z
            if "round" in n: ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue"))
            else: ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue"))
            ly2 = ty + SEAM + 12 + i * 7                     # labels one under the other, with a leader line
            cross(ax, X, Y); ax.plot([X, X], [Y + b / 2, ly2 - 1.5], color="gray", lw=0.4)
            ax.text(X + 1.5, ly2, f"{n}: {along:.1f} from left, {z:.1f} from face", fontsize=5.5, va="center")
        # bottom wall of the front half (seen from below, face side down)
        bx, by = 100, 297 - 120
        ax.add_patch(Rectangle((bx, by), BOX_W, SEAM, fill=False, lw=1.0))
        ax.text(bx + BOX_W / 2, by + SEAM + 4, "BOTTOM wall of the front half (seen from below, face side down)", ha="center", fontsize=8, weight="bold")
        ax.text(bx + BOX_W / 2, by - 4, "face side   (left edge of the drawing = left edge of the face)", ha="center", fontsize=6.5)
        for w, n, along, z, a, b in wall_cuts():
            if w != "B": continue
            X, Y = bx + along, by + z
            ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y)
            ax.text(X + a / 2 + 2, Y, f"{n}: {a:g} x {b:g}, centre {along:.1f} from left, {z:.1f} from face", fontsize=5.5, va="center")
        ax.text(tx, 160, f"Box as measured: {BOX_W:g} x {BOX_H:g}, closed {BOX_D:g}.\nFront half 16 outside / 14 inside, back half 19 / 16.5.\n"
                "All wall holes are in the FRONT half (under 16 mm).\n\nBack half: cut posts 7 and 8 (the two at the\nbottom end) down to 7 mm high: the battery lies under them.", fontsize=7, va="top")
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        fig.savefig(path.replace(".pdf", "_walls.png"), dpi=90); pdf.savefig(fig); plt.close(fig)
        # page 3: the lid, seen from the BACK (outside of the lid): left and right are swapped compared with the face
        fig, ax = page(); ox, oy = (210 - BOX_W) / 2, 297 - 40 - BOX_H
        L = lambda x, y: (ox + (BOX_W - x), oy + BOX_H - y)
        ax.add_patch(FancyBboxPatch(L(BOX_W, BOX_H), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.0))
        X, Y = L(*spk_c); ax.add_patch(Rectangle((X - SPK[0] / 2, Y - SPK[1] / 2), SPK[0], SPK[1], fill=False, lw=0.5, ls="--", color="gray"))
        for pn, x, y, h, half in POSTS:
            if half == "B": X2, Y2 = L(x, y); ax.add_patch(Circle((X2, Y2), POST_R, fill=False, lw=0.4, ls=":", color="gray"))
        for kind, n, cx, cy, a, b in lid_cuts():
            X, Y = L(cx, cy); ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y, 1.5)
        X, Y = L(*spk_c); ax.text(X, Y - 17, f"speaker grille: 15 holes 2.5 mm\ncentre {BOX_W - spk_c[0]:.1f} from the left edge, {spk_c[1]:.1f} from the top\n(as you look at the BACK)", ha="center", fontsize=6, va="top")
        ax.text(105, 297 - 12, "SomudTick - drilling template: BACK HALF (seen from the BACK, outside)", ha="center", fontsize=9, weight="bold")
        ax.text(105, 297 - 18, "Top of the page = the end with the 2 screw holes. Only the speaker grille; the speaker sits under it on a foam pad.", ha="center", fontsize=7)
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.text(105, 10, "100 mm", ha="center", fontsize=7)
        fig.savefig(path.replace(".pdf", "_lid.png"), dpi=90); pdf.savefig(fig); plt.close(fig)

def drill_list(path):
    L = ["SomudTick box - holes (mm). Face: x from the LEFT edge, y from the TOP edge (looking at the face).", ""]
    for kind, n, cx, cy, a, b in face_cuts():
        L.append(f"  face  {n:22s} centre x {cx:6.1f}  y {cy:6.1f}   " + (f"{a:.1f} x {b:.1f} rect" if kind == "rect" else f"diameter {a:.1f}"))
    L.append("")
    for w, n, along, z, a, b in wall_cuts():
        L.append(f"  {dict(L='left', T='top ', B='bottom')[w]} wall {n:18s} {'y' if w == 'L' else 'x'} {along:6.1f}  from face {z:5.1f}   {a:.1f} x {b:.1f}")
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
        if c[0] == "box post": ax.add_patch(Circle((M(c[1]), BOX_H - c[2]), c[3], fc=c[6], ec="k", lw=0.5, alpha=0.8))
    ax.text(BOX_W / 2, BOX_H + 8, "inside the front half, seen from the BACK (back half off)", ha="center", fontsize=9)
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
        ('จอเล็ก OLED 1.3" (SH1106 128×64)', at('OLED 1.3"'), "บนสุด ระหว่างเสาน็อต 2 ต้น ตรงกลางจอใหญ่ · แปะเทปกาวสองหน้าด้านในหน้ากล่อง"),
        ("จอใหญ่ ES3C28P (ตัวบอร์ดหลัก)", (X(scr_c[0] - 30), Y(scr_c[1] - 17), FACE + SCR["pcb_front"]), "กลางกล่อง · น็อต M3 4 ตัวผ่านหน้ากล่อง + เสารอง 4.5 มม. เข้าเสาทองเหลืองเดิมของจอ"),
        ("ช่อง USB-C (ชาร์จ / แฟลช)", at("USB-C"), "ผนังซ้าย ตรงกลางจอใหญ่ · หัวช่องเสียบอยู่ในรูผนังพอดี"),
        ("บอร์ดปุ่ม NA011", at("NA011 button board"), "ล่าง · น็อต M3 2 ตัว + เสารอง 10 มม. · ปุ่มสี A B C D ขวา, E F กลาง"),
        ("จอย (โผล่ 17.5 มม.)", (X(jx), Y(jy), knob_z + 3), "ซ้ายล่าง ติดกับบอร์ดปุ่ม · รูหน้ากล่อง 28 มม. ให้โยกได้"),
        ("แบต 18650 3000mAh (มี BMS)", at("18650 battery"), "ท้ายกล่อง ใต้บอร์ดปุ่ม นอนขวาง ปลายสายไฟไว้ซ้าย (ติดสวิตช์) · ตีนตุ๊กแก · ตัดเสา 7, 8 ของซีกหลังออก"),
        ("แผงรวมสาย (hub)", at("hub board"), "หลังจอใหญ่ · ราง 3V3 / GND / SDA / SCL · เทปโฟม ทับเทปกันไฟฟ้า"),
        ("นาฬิกา DS3231", at("DS3231 clock"), "หลังบอร์ดปุ่ม ซ้าย (มองจากหน้า) · เทปโฟม ทับเทปกันไฟฟ้า"),
        ("PCF8574 (ตัวอ่านปุ่ม 7 ปุ่ม)", at("PCF8574"), "หลังบอร์ดปุ่ม ขวา (ใต้ขาเหลือง) · เทปโฟม ทับเทปกันไฟฟ้า"),
        ("หลอดส่ง IR KY-005 (รีโมทแอร์)", at("IR LED board"), "ซ้ายบน ใต้เสาน็อต วางราบ · งอขาหลอดให้ชี้ออกรูผนังบน (ซ้ายของเสา)"),
        ("ตัวรับ IR KY-022 (เรียนรู้รีโมท)", at("IR receiver board"), "ขวาบน ใต้เสาน็อต วางราบ · ตัวรับมองออกช่องผนังบน (ขวาของเสา)"),
        ("สวิตช์กระดก", at("switch"), "ผนังล่าง มุมซ้าย ข้างปลายแบต · ใส่ช่อง 12.5 × 9 กดล็อกเอง"),
        ("ลำโพง 40×28", at("speaker"), "หลังจอใหญ่ บนแผ่นโฟม · หน้าลำโพงชิดซีกหลัง ตรงรูเสียง 15 รู"),
    ]
    # free room left inside (for extras later): (name, cx, cy, w, h, z0, z1) in face coordinates
    free = []
    free_j = [dict(name=n, cx=X(cx), cy=Y(cy), w=w, h=h, z0=z0, z1=z1, size=f"{w:.0f} × {h:.0f} ลึก {z1 - z0:.0f} มม.") for n, cx, cy, w, h, z0, z1 in free]
    json.dump(dict(box=[BOX_W, BOX_H, BOX_D], wall=WALL, face=FACE, lid=LID,
                   labels=[dict(name=n, x=p[0], y=p[1], z=p[2], where=w) for n, p, w in labels], free=free_j,
                   posts=[[X(x), Y(y), *post_z(n, h, half)] for n, x, y, h, half in POSTS], post_r=POST_R, seam=SEAM,
                   body_rim=SEAM, tab="กล่องเทา 172", title="กล่องเทา 172 × 94 × 34.5 สองซีก (เจาะเอง)", template="case/out_box4/drill_template.pdf",
                   notes=["ของทุกชิ้นยึดกับซีกหน้า (ซีกที่มีเสาเตี้ย 2 ต้น) ซีกหลังมีแค่รูลำโพง: คลายน็อต 2 ตัวที่หัวกล่อง แล้วยกซีกหลังออก ก็เห็นทุกชิ้น",
                          "ซีกหลังมีเสา 8 ต้น: ตัดเสา 7, 8 (ท้ายกล่อง) ออกให้เรียบพื้นก่อนประกอบ ไม่อย่างนั้นค้ำแบต (หนา 24 ตรงปลายสายไฟ)"],
                   items=items, holes=holes, walls=walls,
                   lid_holes=[dict(kind=k, cx=X(cx), cy=Y(cy), a=a, b=b) for k, n, cx, cy, a, b in lid_cuts()]), open(path, "w"))

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box4"); os.makedirs(out, exist_ok=True)
    ok = check()
    template(os.path.join(out, "drill_template.pdf")); drill_list(os.path.join(out, "drill_list.txt"))
    layout(os.path.join(out, "inside_layout.png")); asm_json(os.path.join(out, "box_asm.json"))
    print("written to", out, "" if ok else "  (SOMETHING DOES NOT FIT)")
