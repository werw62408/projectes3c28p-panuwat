"""
SomudTick in the REAL grey ABS box (172 x 94 x 34.5, measured 6 Oct 2026), fifth layout: GAME BOY style (17 Oct 2026).

  - big screen at the TOP (window with a Game Boy bezel drawn round it), the button board under it,
    the 18650 across the bottom, the speaker under a slanted Game Boy grille at the bottom of the face
  - the small 1.3" screen moves to the BACK half: its window sits in the label recess, turn the box over to see the time
  - the box is turned END FOR END compared with layout 4: the end with the screw posts is now at the BOTTOM.
    Why: the USB-C slot has to be level with the middle of the big screen, and at the top end that is where the
    latch tabs of the side walls are (38-49 from the post end). Turned round, the tabs are at 17-29 and 123-134 from the top.
  - IR LED + IR receiver look out of the TOP wall (point the top at the air conditioner), rocker switch in the BOTTOM wall, left

    python make_box5.py  -> out_box5/drill_template.pdf (print at 100 %), out_box5/drill_list.txt,
                            out_box5/inside_layout.png, out_box5/box_asm.json (3D view)

Face coordinates: x from the LEFT outside edge, y from the TOP outside edge (looking at the face, screen at the top,
the screw-post end at the bottom). z = depth from the outside of the face.
Every hole has a SIZE TO CUT FIRST (a bit smaller) and a FINAL size: cut the first, fit the real part, then file to the final.
"""
import json, math, os

# ============================== the box ==============================
BOX_W, BOX_H, BOX_D = 94.0, 172.0, 34.5   # outside, closed (measured)
WALL = 2.5
FACE = 2.0                                  # front half: 16 outside, 14 inside
LID = 2.5                                   # back half: 19 outside, 16.5 inside
SEAM = 16.0                                 # where the halves meet, from the outside of the face (wall holes stay 1 mm under it)
IN_D = BOX_D - FACE - LID
BODY_D = BOX_D - LID                        # the deepest a part may go
POST_R = 3.5                                # drawn a bit bigger than the real posts (about 6 across in the photo)
# the posts as measured for layout 4 (x, y from the post end), turned end for end here: (x, y) -> (W - x, H - y)
_POSTS4 = [("front post 1", 19.5, 7.5, 5.5, "F"), ("front post 2", 74.5, 7.5, 5.5, "F"),
           ("back post 1", 19.5, 7.5, 6.5, "B"), ("back post 2", 74.5, 7.5, 6.5, "B"),
           ("back post 3", 12.5, 18.5, 10.5, "B"), ("back post 4", 81.5, 18.5, 10.5, "B"),
           ("back post 5", 9.0, 87.0, 10.5, "B"), ("back post 6", 85.0, 87.0, 10.5, "B"),
           ("back post 7", 12.5, 155.0, 10.5, "B"), ("back post 8", 81.5, 155.0, 10.5, "B")]
POSTS = [(n, BOX_W - x, BOX_H - y, h, half) for n, x, y, h, half in _POSTS4]
def post_z(name, h, half): return (FACE, FACE + h) if half == "F" else (BODY_D - h, BODY_D)
# things in the walls of the FRONT half to stay clear of (measured on layout 4, turned round)
TABS_SIDE = [(17.0, 29.0), (123.0, 134.0)]  # latch tabs, left and right wall, y from the top
NOTCH_TOP = (38.0, 49.0)                    # the opening notch at the seam, top wall, x from the left
# the label recess on the OUTSIDE of the back half (read from the owner's photo, about +-2 mm): x0, y0, x1, y1
# seen from the back, y from the top of the box as it now stands (the screw holes are at the bottom)
RECESS = (10.0, 128.0, 84.0, 150.0)

# ============================== parts (measured, or the maker's usual size) ==============================
OLED = dict(w=35.5, h=33.5, t=3.7, win_w=31.0, win_h=16.5, win_dy=-1.9)   # 1.3" SH1106: glass 1.9 above the board centre (pins edge up)
SCR = dict(w=86.0, h=50.0, glass_w=69.2, glass_h=50.0, vis_w=59.45, vis_h=45.2, vis_off=3.07,
           pcb_front=4.5, pcb_t=1.5, back=10.0, standoff=5.0, hole_dx=78.0, hole_dy=42.0, usb_out=3.0, usb_z=7.4)
NA = dict(w=87.0, h=53.0, pcb_t=1.5, joy=(18.5, 30.9), knob_d=20.0, joy_total=30.0, btn_top=14.0, small_top=5.0,
          btn={"A": (65.0, 18.4), "B": (77.1, 29.9), "C": (65.0, 42.0), "D": (53.1, 29.9)}, small={"E": (47.5, 41.6), "F": (34.5, 41.6)},
          holes=[(4.2, 2.5), (82.8, 17.5)], cap_d=11.5)
NA_SPACER = 10.0          # M3 spacers face -> NA011: the colour caps stand 2 out of the face
BATT_L, BATT_CELL, BATT_END, BATT_END_L = 72.0, 18.5, 24.0, 15.0   # 18650 + BMS: cell 18.5 thick, 24 at the wire end (15 long)
HUB = (30.0, 20.0, 4.0)
DS3231 = (38.0, 21.5, 9.0); PCF = (35.0, 19.0, 9.0)
SPK = (40.0, 28.0, 10.0)  # oval speaker (measured), cone down onto the face
IR_TX = (18.0, 15.0); IR_RX = (15.0, 18.5)
SW = (12.0, 8.5, 9.0); SW_SLOT = (12.5, 9.0)
SW_TAIL = 5.0             # solder tags + wires behind the switch body

# ============================== the layout (face coordinates) ==============================
IX0, IX1, IY0, IY1 = WALL, BOX_W - WALL, WALL, BOX_H - WALL
MIDX = BOX_W / 2
# big screen at the top, USB-C in the left wall (receptacle 1.5 into the wall)
scr_left = IX0 + SCR["usb_out"] - 1.5
SCR_TOP = 12.0                                  # leaves 9.5 mm under the top wall for the IR LED and receiver
scr_c = (scr_left + SCR["w"] / 2, SCR_TOP + SCR["h"] / 2)
vis_c = (scr_c[0] + SCR["vis_off"], scr_c[1])
# button board under it
na_c = (MIDX, scr_c[1] + SCR["h"] / 2 + 1.5 + NA["h"] / 2)
na_left, na_top = na_c[0] - NA["w"] / 2, na_c[1] - NA["h"] / 2
def na(p): return (na_left + p[0], na_top + p[1])
NA_FRONT = FACE + NA_SPACER; NA_BACK = NA_FRONT + NA["pcb_t"]
NA_BOT = na_top + NA["h"]
# 18650 across under the button board, wire end (BMS, 24 thick) on the LEFT, next to the switch
BATT_Y = NA_BOT + 0.5 + BATT_END / 2            # centre line of the cell
batt_x0 = IX0 + 1.5
# speaker at the bottom, between the two front posts (they are at x 19.5 and 74.5 at this end)
spk_c = (MIDX, BATT_Y + BATT_CELL / 2 + 1.0 + SPK[1] / 2)     # the cell is 18.5; the 24 mm wire end is left of the speaker
# rocker switch in the BOTTOM wall, left corner (left of front post 1)
sw_x = IX0 + 0.5 + SW_SLOT[0] / 2
sw_y = IY1 - SW[2] / 2
SW_Z = 9.0
# behind the button board (on foam over insulating tape): clock left, PCF8574 right, low enough to miss back posts 5 and 6
BEH_Y = 93.0
ds_c = (9.0 + DS3231[0] / 2, BEH_Y + DS3231[1] / 2)
pcf_c = (ds_c[0] + DS3231[0] / 2 + 2.0 + PCF[0] / 2, BEH_Y + PCF[1] / 2)
Z_BEHIND = NA_BACK + 2.5
# hub behind the big screen, left
hub_c = (scr_c[0] - 20.0, scr_c[1] + 6.0)
Z_SCR_BACK = FACE + SCR["back"]
# IR boards on foam on the back of the big screen, near its top edge, right of the notch; LED / receiver bent to the top wall
IR_Z0 = Z_SCR_BACK + 1.5
irtx_c = (51.0 + IR_TX[0] / 2, SCR_TOP + 0.5 + IR_TX[1] / 2)
irrx_c = (71.0 + IR_RX[0] / 2, SCR_TOP + 0.5 + IR_RX[1] / 2)
IR_LED_X, IR_RX_X, IR_HOLE_Z = irtx_c[0], irrx_c[0], 10.5
# small screen on the BACK half, window in the middle of the label recess
oled_win_c = ((RECESS[0] + RECESS[2]) / 2, (RECESS[1] + RECESS[3]) / 2)
oled_c = (oled_win_c[0], oled_win_c[1] - OLED["win_dy"])
OLED_Z0 = BODY_D - OLED["t"]

def boxes():
    """(name, cx, cy, w, h, z0, z1, colour) in face coordinates"""
    P = []
    P.append(("screen board", *scr_c, SCR["w"], SCR["h"], FACE + SCR["pcb_front"], FACE + SCR["pcb_front"] + SCR["pcb_t"], "#1f5f3a"))
    P.append(("screen glass", *scr_c, SCR["glass_w"], SCR["glass_h"], FACE, FACE + 2.3, "#10161c"))
    P.append(("screen back parts", *scr_c, SCR["w"] - 16, SCR["h"] - 14, FACE + SCR["pcb_front"] + SCR["pcb_t"], FACE + SCR["back"], "#2c3a33"))
    P.append(("USB-C", scr_left - SCR["usb_out"] / 2 + 2.0, scr_c[1], SCR["usb_out"] + 4.0, 9.0, FACE + SCR["usb_z"] - 1.6, FACE + SCR["usb_z"] + 1.6, "#b8bec4"))
    P.append(("NA011 button board", *na_c, NA["w"], NA["h"], NA_FRONT, NA_BACK, "#b3262c"))
    P.append(("18650 wire end (BMS)", batt_x0 + BATT_END_L / 2, BATT_Y, BATT_END_L, BATT_END, FACE + 0.5, FACE + 0.5 + BATT_END, "#c0602c"))
    P.append(("18650 cell", batt_x0 + BATT_END_L + (BATT_L - BATT_END_L) / 2, BATT_Y, BATT_L - BATT_END_L, BATT_CELL, FACE + 0.5, FACE + 0.5 + BATT_CELL, "#e07b39"))
    P.append(("hub board", *hub_c, HUB[0], HUB[1], Z_SCR_BACK + 0.5, Z_SCR_BACK + 0.5 + HUB[2], "#9e8a5a"))
    P.append(("DS3231 clock", *ds_c, DS3231[0], DS3231[1], Z_BEHIND, Z_BEHIND + DS3231[2], "#3b8f5e"))
    P.append(("PCF8574", *pcf_c, PCF[0], PCF[1], Z_BEHIND, Z_BEHIND + PCF[2], "#7a4fb0"))
    P.append(("IR LED board", *irtx_c, IR_TX[0], IR_TX[1], IR_Z0, IR_Z0 + 1.5, "#c94c4c"))
    P.append(("IR receiver board", *irrx_c, IR_RX[0], IR_RX[1], IR_Z0, IR_Z0 + 1.5, "#d98a3a"))
    P.append(("IR LED", IR_LED_X, (IY0 + SCR_TOP) / 2, 5.0, SCR_TOP - IY0 - 1.0, IR_HOLE_Z - 2.5, IR_HOLE_Z + 2.5, "#dddddd"))
    P.append(("IR receiver", IR_RX_X, IY0 + 3.0, 6.0, 5.0, IR_HOLE_Z - 3.0, IR_HOLE_Z + 3.0, "#333333"))
    P.append(("switch", sw_x, sw_y, SW[0], SW[2], SW_Z - SW[1] / 2, SW_Z + SW[1] / 2, "#e0c040"))
    P.append(("switch tags + wires", sw_x, sw_y - SW[2] / 2 - SW_TAIL / 2, SW[0] - 2, SW_TAIL, SW_Z - SW[1] / 2, SW_Z + SW[1] / 2, "#b0a060"))
    P.append(("speaker", *spk_c, SPK[0], SPK[1], FACE, FACE + SPK[2], "#4a90d9"))
    P.append(("OLED 1.3\" (back)", *oled_c, OLED["w"], OLED["h"], OLED_Z0, BODY_D, "#1a3a7a"))
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
        C.append(("stub " + k, x, y, 1.7, FACE - 0.8, NA_FRONT - NA["small_top"], "#f2f2f2"))
    for dx in (-1, 1):
        for dy in (-1, 1):
            C.append(("brass pillar", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 2.5, FACE + SCR["pcb_front"] + SCR["pcb_t"], FACE + SCR["pcb_front"] + SCR["pcb_t"] + SCR["standoff"], "#c9a44a"))
            C.append(("spacer 4.5", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 2.5, FACE, FACE + SCR["pcb_front"], "#c9a44a"))
    for p in NA["holes"]: x, y = na(p); C.append(("spacer 10", x, y, 2.5, FACE, NA_FRONT, "#c9a44a"))
    for n, x, y, h, half in POSTS: C.append(("box post", x, y, POST_R, *post_z(n, h, half), "#dddddd"))
    return C

# ============================== the holes to make ==============================
# (kind, name, cx, cy, final a, final b, first a, first b): rect a x b, circle a = diameter (b = 0)
def grille():
    """Game Boy speaker grille: 6 slanted slots over the speaker cone, each drilled as a row of small holes"""
    S = []
    ang = math.radians(60.0)                       # slots lean right, like the Game Boy
    ux, uy = math.cos(ang), -math.sin(ang)         # along a slot (y grows downwards)
    for i in range(6):
        cx, cy = spk_c[0] - 12.5 + i * 5.0, spk_c[1]
        for j in (-2, -1, 0, 1, 2):
            S.append((cx + j * 3.0 * ux, cy + j * 3.0 * uy))   # 3.0 apart: a 0.5 web between 2.5 holes, easy to file into a slot
    return S
def face_cuts():
    H = [("rect", "big screen window", vis_c[0], vis_c[1], SCR["vis_w"] + 0.6, SCR["vis_h"] + 0.6, SCR["vis_w"] - 1.4, SCR["vis_h"] - 1.4)]
    jx, jy = na(NA["joy"]); H.append(("circle", "joystick", jx, jy, 28.0, 0, 24.0, 0))
    for k, p in NA["btn"].items(): H.append(("circle", "button " + k, *na(p), 13.0, 0, 12.0, 0))
    for k, p in NA["small"].items(): H.append(("circle", "button " + k, *na(p), 4.5, 0, 4.0, 0))
    for dx in (-1, 1):
        for dy in (-1, 1): H.append(("circle", "M3 screen", scr_c[0] + dx * SCR["hole_dx"] / 2, scr_c[1] + dy * SCR["hole_dy"] / 2, 3.2, 0, 2.5, 0))
    for p in NA["holes"]: H.append(("circle", "M3 buttons", *na(p), 3.2, 0, 2.5, 0))
    for x, y in grille(): H.append(("circle", "speaker", x, y, 2.5, 0, 2.5, 0))
    return H
def lid_cuts():
    """the back half: the small screen window. (cx, cy) in face coordinates (as seen from the FRONT); templates flip it."""
    return [("rect", "small screen window", oled_win_c[0], oled_win_c[1], OLED["win_w"], OLED["win_h"], OLED["win_w"] - 2.0, OLED["win_h"] - 2.0)]
def wall_cuts():
    """(wall, name, along, z, a, b): wall 'L' (along = y from the top), 'T' / 'B' (along = x from the left edge, seen from the front);
    z = from the outside of the face; a (along) x b (z)"""
    return [("L", "USB-C", scr_c[1], FACE + SCR["usb_z"], 12.5, 7.5),
            ("T", "IR LED (round 5.4)", IR_LED_X, IR_HOLE_Z, 5.4, 5.4),
            ("T", "IR receiver", IR_RX_X, IR_HOLE_Z, 7.0, 7.5),
            ("B", "rocker switch", sw_x, SW_Z, *SW_SLOT)]
# Game Boy look: drawn on the template only (paint or sticker, no cutting)
BEZEL = (5.0, 5.0, 89.0, 63.5)          # x0, y0, x1, y1: dark bezel round the screen window (big round corner bottom right)
LOGO = (8.0, 71.5)                      # "SOMUDTICK" under the bezel, left aligned like "Nintendo GAME BOY"

# ============================== checks ==============================
def check():
    bad = []; B = boxes(); C = cylinders()
    def zo(a0, a1, b0, b1): return min(a1, b1) - max(a0, b0) > 0.05
    skip_pairs = {("screen board", "screen glass"), ("screen board", "screen back parts"), ("screen glass", "screen back parts"),
                  ("IR LED board", "IR LED"), ("IR receiver board", "IR receiver"), ("switch", "switch tags + wires"),
                  ("18650 wire end (BMS)", "18650 cell"), ("screen board", "USB-C")}
    for i, a in enumerate(B):
        if a[0] not in ("USB-C", "switch") and (a[1] - a[3] / 2 < IX0 - 0.01 or a[1] + a[3] / 2 > IX1 + 0.01 or a[2] - a[4] / 2 < IY0 - 0.01 or a[2] + a[4] / 2 > IY1 + 0.01):
            bad.append(f"{a[0]} is outside the box")
        if a[6] > BODY_D + 0.01: bad.append(f"{a[0]} is too deep ({a[6]:.1f} > {BODY_D:.1f})")
        for b in B[i + 1:]:
            if (a[0], b[0]) in skip_pairs or (b[0], a[0]) in skip_pairs: continue
            if abs(a[1] - b[1]) < (a[3] + b[3]) / 2 - 0.05 and abs(a[2] - b[2]) < (a[4] + b[4]) / 2 - 0.05 and zo(a[5], a[6], b[5], b[6]): bad.append(f"{a[0]} hits {b[0]}")
        for c in C:
            if c[0] == "box post" or (c[0] in ("brass pillar", "spacer 4.5") and not a[0].startswith("screen")) or (c[0] == "spacer 10" and a[0] != "NA011 button board"):
                dx = max(abs(c[1] - a[1]) - a[3] / 2, 0); dy = max(abs(c[2] - a[2]) - a[4] / 2, 0)
                if dx * dx + dy * dy < c[3] ** 2 - 0.05 and zo(a[5], a[6], c[4], c[5]): bad.append(f"{a[0]} hits the {c[0]}")
    # parts in front of the button board must stay out of the joystick / button columns
    for c in C:
        if c[0].startswith(("joystick", "button", "stem")):
            for a in B:
                if a[0] in ("NA011 button board",): continue
                dx = max(abs(c[1] - a[1]) - a[3] / 2, 0); dy = max(abs(c[2] - a[2]) - a[4] / 2, 0)
                if dx * dx + dy * dy < c[3] ** 2 - 0.05 and zo(a[5], a[6], max(c[4], FACE), c[5]): bad.append(f"{a[0]} hits the {c[0]}")
    for w, n, along, z, a, b in wall_cuts():
        if z + b / 2 > SEAM - 1.0: bad.append(f"wall hole {n} reaches the seam of the halves")
        if w == "L":
            for t0, t1 in TABS_SIDE:
                if along + a / 2 > t0 - 1.0 and along - a / 2 < t1 + 1.0: bad.append(f"wall hole {n} is on a latch tab ({t0:g}-{t1:g})")
        if w == "T" and along + a / 2 > NOTCH_TOP[0] - 1.0 and along - a / 2 < NOTCH_TOP[1] + 1.0: bad.append(f"wall hole {n} is on the opening notch")
        if w in ("T", "B"):     # stay clear of the front posts that stand at that end
            for pn, x, y, h, half in POSTS:
                if half == "F" and abs(y - (IY0 if w == "T" else IY1)) < 12 and abs(x - along) < a / 2 + POST_R + 0.5 and z - b / 2 < FACE + h: bad.append(f"wall hole {n} is behind {pn}")
    for k, n, cx, cy, a, b, a0, b0 in face_cuts():
        for pn, x, y, h, half in POSTS:
            if half != "F": continue
            dx = max(abs(x - cx) - (a / 2 if k == "rect" else 0), 0); dy = max(abs(y - cy) - (b / 2 if k == "rect" else 0), 0)
            r = (a / 2 if k == "circle" else 0)
            if math.hypot(dx, dy) - r < POST_R + 1.0: bad.append(f"face hole {n} cuts into {pn}")
    for k, n, cx, cy, a, b, a0, b0 in lid_cuts():
        if not (RECESS[0] + 1 < cx - a / 2 and cx + a / 2 < RECESS[2] - 1 and RECESS[1] + 1 < cy - b / 2 and cy + b / 2 < RECESS[3] - 1): bad.append(f"lid hole {n} is not inside the label recess")
    knob_out = -(NA_BACK - NA["pcb_t"] - NA["joy_total"])
    print(f"box {BOX_W} x {BOX_H} x {BOX_D}, inside {IX1 - IX0:.1f} x {IY1 - IY0:.1f} x {IN_D:.1f}")
    print(f"  joystick knob stands {knob_out:.1f} mm out of the face, colour caps {NA['btn_top'] - NA_SPACER - FACE:.1f} mm out")
    print(f"  free under the bottom of the speaker: {IY1 - (spk_c[1] + SPK[1] / 2):.1f} mm; USB-C slot y {scr_c[1] - 6.25:.1f}-{scr_c[1] + 6.25:.1f}")
    print("  inside: " + ("everything fits" if not bad else "\n    ".join([""] + sorted(set(bad)))))
    return not bad

# ============================== the paper template ==============================
def template(path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.backends.backend_pdf import PdfPages
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch, Polygon
    def page():
        fig = plt.figure(figsize=(210 / 25.4, 297 / 25.4)); ax = fig.add_axes([0, 0, 1, 1])
        ax.set_xlim(0, 210); ax.set_ylim(0, 297); ax.set_aspect("equal"); ax.axis("off"); return fig, ax
    def cross(ax, X, Y, s=2.0): ax.plot([X - s, X + s], [Y, Y], color="k", lw=0.35); ax.plot([X, X], [Y - s, Y + s], color="k", lw=0.35)
    def scale_bars(ax):
        ax.plot([55, 155], [16, 16], color="k", lw=1); ax.plot([55, 55], [14, 18], color="k", lw=1); ax.plot([155, 155], [14, 18], color="k", lw=1)
        ax.text(105, 10, "100 mm (measure it: if it is not 100, the print is scaled - print again at 100 %)", ha="center", fontsize=6.5)
        ax.plot([8, 8], [40, 140], color="k", lw=1); ax.plot([6, 10], [40, 40], color="k", lw=1); ax.plot([6, 10], [140, 140], color="k", lw=1)
        ax.text(5, 90, "100 mm (check this one too)", rotation=90, ha="center", va="center", fontsize=6)
    def rect2(ax, X, Y, a, b, a0, b0, col="tab:blue"):
        ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color=col))
        if a0: ax.add_patch(Rectangle((X - a0 / 2, Y - b0 / 2), a0, b0, fill=False, lw=0.5, ls=(0, (2, 1.5)), color=col))
    def circ2(ax, X, Y, a, a0, col):
        ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color=col))
        if a0 and a0 < a - 0.2: ax.add_patch(Circle((X, Y), a0 / 2, fill=False, lw=0.5, ls=(0, (2, 1.5)), color=col))
    with PdfPages(path) as pdf:
        # ---- page 1: the face, 1:1 ----
        fig, ax = page(); ox, oy = (210 - BOX_W) / 2 + 4, 297 - 36 - BOX_H
        T = lambda x, y: (ox + x, oy + BOX_H - y)
        ax.add_patch(FancyBboxPatch(T(0, BOX_H), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.0))
        ax.add_patch(Rectangle(T(IX0, IY1), IX1 - IX0, IY1 - IY0, fill=False, lw=0.3, ls=":", color="gray"))
        # Game Boy look (no cutting)
        def rr(x0, y0, x1, y1, radii):   # rounded rectangle, radius per corner: top-left, top-right, bottom-right, bottom-left
            pts = []
            for (cx, cy, r, a0) in ((x0 + radii[0], y0 + radii[0], radii[0], 180), (x1 - radii[1], y0 + radii[1], radii[1], 270),
                                    (x1 - radii[2], y1 - radii[2], radii[2], 0), (x0 + radii[3], y1 - radii[3], radii[3], 90)):
                pts += [T(cx + r * math.cos(math.radians(a0 + 9 * k)), cy + r * math.sin(math.radians(a0 + 9 * k))) for k in range(11)]
            return pts
        ax.add_patch(Polygon(rr(*BEZEL, (2.5, 2.5, 12.0, 2.5)), closed=True, fill=False, lw=0.5, ls=(0, (1, 1.5)), color="#7a5aa0"))
        X, Y = T(BEZEL[0] + 2, BEZEL[1] + 2.6); ax.text(X, Y, "(optional) dark bezel: paint or sticker, no cutting", fontsize=4.2, color="#7a5aa0", va="center")
        X, Y = T(*LOGO); ax.text(X, Y, "SOMUDTICK", fontsize=8, color="#7a5aa0", va="center", weight="bold", style="italic")
        X, Y = T(LOGO[0] + 33, LOGO[1]); ax.text(X, Y, "(optional logo)", fontsize=4.2, color="#7a5aa0", va="center")
        for n, cx, cy, w, h, z0, z1, col in boxes():
            if n in ("screen board", "NA011 button board", "speaker", "18650 cell", "18650 wire end (BMS)"):
                ax.add_patch(Rectangle(T(cx - w / 2, cy + h / 2), w, h, fill=False, lw=0.4, ls="--", color="gray"))
        for pn, x, y, h, half in POSTS:
            if half == "F": ax.add_patch(Circle(T(x, y), POST_R, fill=False, lw=0.6, color="gray")); X, Y = T(x, y); ax.text(X, Y + POST_R + 1.2, "post", ha="center", fontsize=4.5, color="gray")
        for kind, n, cx, cy, a, b, a0, b0 in face_cuts():
            X, Y = T(cx, cy)
            if kind == "rect": rect2(ax, X, Y, a, b, a0, b0)
            else: circ2(ax, X, Y, a, a0, "tab:red" if n.startswith("M3") else ("tab:green" if n == "speaker" else "tab:blue"))
            cross(ax, X, Y, 0.9 if n == "speaker" else (1.5 if a < 5 else 2.5))
        k = [c for c in face_cuts() if c[1] == "big screen window"][0]; X, Y = T(k[2], k[3])
        ax.text(X, Y + 6, "big screen window", ha="center", fontsize=6.5, color="tab:blue")
        ax.text(X, Y - 5, f"cut the DASHED line first ({k[6]:.1f} x {k[7]:.1f})\nfit the screen, then file to the solid line ({k[4]:.2f} x {k[5]:.1f})", ha="center", va="top", fontsize=4.6, color="tab:blue")
        jx, jy = na(NA["joy"]); X, Y = T(jx, jy); ax.text(X, Y - 6, "joystick\nfirst 24, final 28", ha="center", va="top", fontsize=5, color="tab:blue")
        for kk, p in {**NA["btn"], **NA["small"]}.items(): X, Y = T(*na(p)); ax.text(X + (7.2 if kk in NA["btn"] else 2.8), Y + (3.5 if kk in NA["btn"] else 2.5), kk, fontsize=6)
        X, Y = T(spk_c[0] + SPK[0] / 2 + 1.5, spk_c[1] - 9); ax.text(X, Y, "speaker grille:\n30 holes 2.5\n(6 slanted rows)\nfile each row into\na slot if you like", ha="left", va="top", fontsize=4.4, color="tab:green")
        X, Y = T(batt_x0 + BATT_L / 2, BATT_Y); ax.text(X, Y, "18650 lies here inside (no holes)", ha="center", va="center", fontsize=5, color="gray")
        X, Y = T(sw_x, IY1 - 1.0); ax.text(X, Y, "switch in\nbottom wall", ha="center", va="bottom", fontsize=4.2, color="gray")
        X, Y = T(4, 37); ax.text(X - 3.5, Y, "USB-C in left wall", rotation=90, ha="center", va="center", fontsize=4.5, color="gray")
        ax.text(105 + 4, 297 - 10, "SomudTick grey box, GAME BOY layout - FRONT FACE (outside of the half with the 2 short posts)", ha="center", fontsize=8.5, weight="bold")
        ax.text(105 + 4, 297 - 15.5, "the 2 short posts are at the BOTTOM now (turn the box round). Print at 100 % (Actual size). Check both 100 mm lines.", ha="center", fontsize=6.5)
        ax.text(105 + 4, 297 - 20, "solid line = final size, dashed line = cut this first, + = centre (drill a 2.5 pilot there first)", ha="center", fontsize=6.5)
        ax.text(105 + 4, 297 - 24.5, "blue = openings   red = M3 screws   green = speaker   gray = boards behind / posts inside   purple = looks only (no cutting)", ha="center", fontsize=5.5)
        scale_bars(ax)
        fig.savefig(path.replace(".pdf", "_face.png"), dpi=110); pdf.savefig(fig); plt.close(fig)
        # ---- page 2: the walls ----
        fig, ax = page()
        ax.text(105, 297 - 12, "SomudTick grey box, GAME BOY layout - WALLS of the front half (1:1)", ha="center", fontsize=9, weight="bold")
        ax.text(105, 297 - 18, "z = from the FACE side edge. Every wall hole is under 15 mm (the halves meet at 16).", ha="center", fontsize=6.5)
        lx, ly = 22, 297 - 36 - BOX_H
        ax.add_patch(Rectangle((lx, ly), SEAM, BOX_H, fill=False, lw=1.0))
        ax.text(lx + SEAM / 2, ly + BOX_H + 3, "LEFT wall", ha="center", fontsize=8, weight="bold")
        ax.text(lx - 3, ly + BOX_H / 2, "face side", rotation=90, ha="center", va="center", fontsize=6.5)
        ax.text(lx + SEAM / 2, ly - 4, "bottom end (posts)", ha="center", fontsize=5.5)
        for t0, t1 in TABS_SIDE:
            ax.add_patch(Rectangle((lx, ly + BOX_H - t1), SEAM, t1 - t0, fc="#f3d6d6", ec="none"))
            ax.text(lx + SEAM + 1, ly + BOX_H - (t0 + t1) / 2, f"latch tab {t0:g}-{t1:g}\n(do not cut here)", fontsize=5, va="center", color="#b03030")
        for w, n, along, z, a, b in wall_cuts():
            if w != "L": continue
            X, Y = lx + z, ly + BOX_H - along
            ax.add_patch(Rectangle((X - b / 2, Y - a / 2), b, a, fill=False, lw=0.8, color="tab:blue")); cross(ax, X, Y)
            ax.add_patch(Rectangle((X - (b - 1.5) / 2, Y - (a - 1.5) / 2), b - 1.5, a - 1.5, fill=False, lw=0.5, ls=(0, (2, 1.5)), color="tab:blue"))
            ax.text(X + b / 2 + 2, Y, f"{n}: {a:g} x {b:g}\ncentre {along:.1f} from the top\n{z:.1f} from the face\ncut dashed first, try the plug", fontsize=5.5, va="center")
        def end_wall(tx, ty, wall, title):
            ax.add_patch(Rectangle((tx, ty), BOX_W, SEAM, fill=False, lw=1.0))
            ax.text(tx + BOX_W / 2, ty + SEAM + 4, title, ha="center", fontsize=7.5, weight="bold")
            ax.text(tx + BOX_W / 2, ty - 3.5, "face side   (left edge of the drawing = left edge of the face)", ha="center", fontsize=5.5)
            if wall == "T":
                ax.add_patch(Rectangle((tx + NOTCH_TOP[0], ty), NOTCH_TOP[1] - NOTCH_TOP[0], SEAM, fc="#f3d6d6", ec="none"))
                ax.text(tx + (NOTCH_TOP[0] + NOTCH_TOP[1]) / 2, ty + SEAM / 2, "notch\n(no holes)", ha="center", va="center", fontsize=4.5, color="#b03030")
            else:
                for pn, x, y, h, half in POSTS:
                    if half == "F":
                        ax.add_patch(Rectangle((tx + x - POST_R, ty), 2 * POST_R, FACE + h, fc="#e6e6e6", ec="gray", lw=0.4))
                        ax.text(tx + x, ty + FACE + h + 1, "post behind", ha="center", fontsize=4.2, color="gray")
            cuts = [c for c in wall_cuts() if c[0] == wall]
            for i, (w, n, along, z, a, b) in enumerate(cuts):
                X, Y = tx + along, ty + z
                if "round" in n: ax.add_patch(Circle((X, Y), a / 2, fill=False, lw=0.8, color="tab:blue"))
                else:
                    ax.add_patch(Rectangle((X - a / 2, Y - b / 2), a, b, fill=False, lw=0.8, color="tab:blue"))
                    ax.add_patch(Rectangle((X - (a - 1.5) / 2, Y - (b - 1.5) / 2), a - 1.5, b - 1.5, fill=False, lw=0.5, ls=(0, (2, 1.5)), color="tab:blue"))
                ly2 = ty - 9 - i * 5
                cross(ax, X, Y); ax.plot([X, X], [Y - b / 2, ly2 + 1.2], color="gray", lw=0.4); ax.plot([tx, X], [ly2 + 1.2, ly2 + 1.2], color="gray", lw=0.3)
                ax.text(tx, ly2, f"{n}: {a:g} x {b:g}, centre {along:.1f} from left, {z:.1f} from face", fontsize=5.5, va="top")
        end_wall(98, 297 - 60, "T", "TOP wall (screen end), seen from above")
        end_wall(98, 297 - 112, "B", "BOTTOM wall (post end), seen from below")
        ax.text(98, 297 - 140, "Box as measured: 94 x 172, closed 34.5. Front half 16 outside / 14 inside, back half 19 / 16.5.\n"
                "Turned round from layout 4: the 2 short posts are at the BOTTOM now.\n\n"
                "IR: drill these two LAST, after the IR boards are in, so the LED and the\nreceiver sit right in the holes (bend their legs to the marks).\n"
                "Switch: cut the dashed slot, push the switch in, file until it clicks in.\n"
                "Back half: posts 7 and 8 do NOT need cutting in this layout.", fontsize=6.3, va="top")
        scale_bars(ax)
        fig.savefig(path.replace(".pdf", "_walls.png"), dpi=110); pdf.savefig(fig); plt.close(fig)
        # ---- page 3: the back half, seen from the BACK (outside): left and right swapped ----
        fig, ax = page(); ox, oy = (210 - BOX_W) / 2 + 4, 297 - 36 - BOX_H
        L = lambda x, y: (ox + (BOX_W - x), oy + BOX_H - y)
        ax.add_patch(FancyBboxPatch(L(BOX_W, BOX_H), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.0))
        X, Y = L(RECESS[2], RECESS[3]); ax.add_patch(Rectangle((X, Y), RECESS[2] - RECESS[0], RECESS[3] - RECESS[1], fill=False, lw=0.6, ls=(0, (3, 2)), color="#7a5aa0"))
        X, Y = L((RECESS[0] + RECESS[2]) / 2, RECESS[1]); ax.text(X, Y + 1.2, "label recess (from your photo, +-2 mm): lay the paper on it", ha="center", fontsize=4.8, color="#7a5aa0")
        X, Y = L(*oled_c); ax.add_patch(Rectangle((X - OLED["w"] / 2, Y - OLED["h"] / 2), OLED["w"], OLED["h"], fill=False, lw=0.4, ls="--", color="gray"))
        for pn, x, y, h, half in POSTS:
            if half == "B":
                X2, Y2 = L(x, y); ax.add_patch(Circle((X2, Y2), POST_R, fill=False, lw=0.4, ls=":", color="gray"))
                ax.text(X2, Y2 - POST_R - 2.2, pn.replace("back ", ""), ha="center", fontsize=4.2, color="gray")
        for pn, x, y, h, half in POSTS:
            if pn in ("back post 1", "back post 2"):
                X2, Y2 = L(x, y); ax.add_patch(Circle((X2, Y2), 1.7, fc="#bbbbbb", ec="k", lw=0.4))
        X, Y = L(MIDX, POSTS[0][2]); ax.text(X, Y, "the 2 screw holes\n(already there)", ha="center", va="center", fontsize=5, color="gray")
        for kind, n, cx, cy, a, b, a0, b0 in lid_cuts():
            X, Y = L(cx, cy); rect2(ax, X, Y, a, b, a0, b0); cross(ax, X, Y)
            ax.text(X, Y + 30, f"small screen window {a:g} x {b:g} (cut the dashed {a0:g} x {b0:g} first)\n"
                    f"centre {BOX_W - cx:.1f} from the left, {cy:.1f} from the top (as you look at the BACK)\n"
                    "if your recess sits a bit off, put the window in the MIDDLE of the real recess\n(the screen inside has room for +-5 mm)", ha="center", fontsize=5.5, va="bottom")
        X, Y = L(MIDX, 30); ax.text(X, Y, "no holes in the rest of the back half\n(the speaker is at the front now)", ha="center", fontsize=6, color="gray")
        ax.text(105 + 4, 297 - 10, "SomudTick grey box, GAME BOY layout - BACK HALF (seen from the BACK, outside)", ha="center", fontsize=8.5, weight="bold")
        ax.text(105 + 4, 297 - 15.5, "the end with the 2 screw holes is at the BOTTOM of this page. Only one hole: the small screen window.", ha="center", fontsize=6.5)
        scale_bars(ax)
        fig.savefig(path.replace(".pdf", "_lid.png"), dpi=110); pdf.savefig(fig); plt.close(fig)

def drill_list(path):
    L = ["SomudTick grey box, Game Boy layout (layout 5) - every hole in mm.",
         "Face: x from the LEFT edge, y from the TOP edge (looking at the face; the 2 short posts are at the BOTTOM).", ""]
    for kind, n, cx, cy, a, b, a0, b0 in face_cuts():
        if n == "speaker": continue
        L.append(f"  face  {n:20s} centre x {cx:6.1f}  y {cy:6.1f}   " + (f"final {a:.2f} x {b:.1f}, first {a0:.2f} x {b0:.1f}" if kind == "rect" else f"final diameter {a:.1f}, first {a0:.1f}"))
    L.append(f"  face  speaker grille        30 holes 2.5: 6 slanted rows of 5 (see the template), centre x {spk_c[0]:.1f} y {spk_c[1]:.1f}")
    L.append("")
    for w, n, along, z, a, b in wall_cuts():
        L.append(f"  {dict(L='left  ', T='top   ', B='bottom')[w]} wall {n:18s} {'y' if w == 'L' else 'x'} {along:6.1f}  from face {z:5.1f}   {a:.1f} x {b:.1f}")
    L += ["", "  BACK HALF, looking at the BACK: x from the LEFT edge as you see it from behind, y from the top (the screw holes are at the bottom)"]
    for kind, n, cx, cy, a, b, a0, b0 in lid_cuts():
        L.append(f"  back  {n:20s} centre x {BOX_W - cx:6.1f}  y {cy:6.1f}   final {a:.1f} x {b:.1f}, first {a0:.1f} x {b0:.1f}")
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")

def layout(path):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    fig, ax = plt.subplots(figsize=(6.6, 11)); ax.set_aspect("equal"); ax.axis("off")
    M = lambda x: BOX_W - x
    ax.add_patch(FancyBboxPatch((0, 0), BOX_W, BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.2))
    for n, cx, cy, w, h, z0, z1, col in boxes():
        if n in ("screen glass", "screen back parts", "switch tags + wires"): continue
        a = 0.35 if n == "OLED 1.3\" (back)" else (0.8 if z0 > 12 else 0.45)
        ax.add_patch(Rectangle((M(cx) - w / 2, BOX_H - cy - h / 2), w, h, fc=col, ec="k", lw=0.6, alpha=a, ls="--" if "back" in n and "OLED" in n else "-"))
        if n not in ("screen board", "IR LED", "IR receiver"):
            ax.text(M(cx), BOX_H - cy, f"{n}\nz {z0:.0f}-{z1:.0f}", ha="center", va="center", fontsize=5.5, color="k")
    for c in cylinders():
        if c[0] == "box post": ax.add_patch(Circle((M(c[1]), BOX_H - c[2]), c[3], fc=c[6], ec="k", lw=0.5, alpha=0.8))
    ax.text(BOX_W / 2, BOX_H + 8, "inside, seen from the BACK (back half off); screen end at the top", ha="center", fontsize=9)
    ax.text(BOX_W / 2, -8, "z = depth from the outside of the face (mm). Dashed blue = the small screen, glued inside the BACK half", ha="center", fontsize=6.5)
    ax.set_xlim(-8, BOX_W + 8); ax.set_ylim(-14, BOX_H + 14)
    fig.tight_layout(); fig.savefig(path, dpi=130); plt.close(fig)

def asm_json(path):
    """for the 3D view: x right, y up (centre origin), z into the box"""
    cx0, cy0 = BOX_W / 2, BOX_H / 2
    X = lambda x: x - cx0; Y = lambda y: cy0 - y
    items = [dict(k="box", name=n, cx=X(cx), cy=Y(cy), w=w, h=h, z0=z0, z1=z1, c=col) for n, cx, cy, w, h, z0, z1, col in boxes()]
    items += [dict(k="cyl", name=n, cx=X(cx), cy=Y(cy), r=r, z0=z0, z1=z1, c=col) for n, cx, cy, r, z0, z1, col in cylinders() if n != "box post"]
    holes = [dict(kind=k, cx=X(cx), cy=Y(cy), a=a, b=b) for k, n, cx, cy, a, b, a0, b0 in face_cuts()]
    walls = [dict(wall=w, along=(Y(al) if w == "L" else X(al)), z=z, a=a, b=b) for w, n, al, z, a, b in wall_cuts()]
    Bx = {b[0]: b for b in boxes()}
    def at(name): b = Bx[name]; return (X(b[1]), Y(b[2]), (b[5] + b[6]) / 2)
    jx, jy = na(NA["joy"]); knob_z = NA_BACK - NA["pcb_t"] - NA["joy_total"]
    labels = [
        ("จอใหญ่ ES3C28P (ตัวบอร์ดหลัก)", (X(scr_c[0] - 30), Y(scr_c[1] - 17), FACE + SCR["pcb_front"]), "บนสุด แบบจอ Game Boy · น็อต M3 4 ตัวผ่านหน้ากล่อง + เสารอง 4.5 มม. เข้าเสาทองเหลืองเดิมของจอ"),
        ("ช่อง USB-C (ชาร์จ / แฟลช)", at("USB-C"), "ผนังซ้าย ตรงกลางจอใหญ่ (หลบเขี้ยวล็อก 17–29)"),
        ("บอร์ดปุ่ม NA011", at("NA011 button board"), "ใต้จอ · น็อต M3 2 ตัว + เสารอง 10 มม. · ปุ่มสี A B C D ขวา, E F กลาง"),
        ("จอย (โผล่ 18 มม.)", (X(jx), Y(jy), knob_z + 3), "ซ้าย ติดกับบอร์ดปุ่ม · รูหน้ากล่อง 28 มม. ให้โยกได้"),
        ("แบต 18650 (มี BMS)", at("18650 cell"), "ใต้บอร์ดปุ่ม นอนขวาง ปลายสายไฟ (หนา 24) ไว้ซ้าย ดันชิดขอบล่างบอร์ดปุ่ม · ตีนตุ๊กแก"),
        ("ลำโพง 40×28", at("speaker"), "ล่างสุด ระหว่างเสาน็อต 2 ต้น · หน้าลำโพงคว่ำลงบนตะแกรงเฉียงแบบ Game Boy · กาวร้อนรอบขอบ"),
        ("สวิตช์กระดก", at("switch"), "ผนังล่าง มุมซ้าย ข้างปลายสายแบต · ใส่ช่อง 12.5 × 9 กดล็อกเอง"),
        ("จอเล็ก OLED 1.3\" (ด้านหลัง)", at("OLED 1.3\" (back)"), "แปะด้านในซีกหลัง ตรงช่องแปะป้าย · พลิกเครื่องดูเวลา · ขาอยู่ด้านบน"),
        ("แผงรวมสาย (hub)", at("hub board"), "หลังจอใหญ่ ซ้าย · ราง 3V3 / GND / SDA / SCL · เทปโฟม ทับเทปกันไฟฟ้า"),
        ("นาฬิกา DS3231", at("DS3231 clock"), "หลังบอร์ดปุ่ม ซ้าย (มองจากหน้า) · เทปโฟม"),
        ("PCF8574 (ตัวอ่านปุ่ม 7 ปุ่ม)", at("PCF8574"), "หลังบอร์ดปุ่ม ขวา · เทปโฟม"),
        ("หลอดส่ง IR KY-005", at("IR LED board"), "หลังจอใหญ่ ขอบบน วางบนโฟม · ดัดขาหลอดให้หัวหลอดชี้ออกรูผนังบน"),
        ("ตัวรับ IR KY-022", at("IR receiver board"), "หลังจอใหญ่ ขอบบนขวา วางบนโฟม · ดัดขาตัวรับให้มองออกช่องผนังบน"),
    ]
    free = [("ที่ว่างมุมซ้ายล่าง", (IX0 + 1 + 20.5 / 2 + 2.5, (BATT_Y + BATT_END / 2 + IY1 - SW[2] - SW_TAIL) / 2), 20.5, IY1 - SW[2] - SW_TAIL - (BATT_Y + BATT_END / 2) - 1, FACE, BODY_D - 6.0)]
    free_j = [dict(name=n, cx=X(c[0]), cy=Y(c[1]), w=w, h=h, z0=z0, z1=z1, size=f"{w:.0f} × {h:.0f} ลึก {z1 - z0:.0f} มม.") for n, c, w, h, z0, z1 in free]
    json.dump(dict(box=[BOX_W, BOX_H, BOX_D], wall=WALL, face=FACE, lid=LID,
                   labels=[dict(name=n, x=p[0], y=p[1], z=p[2], where=w) for n, p, w in labels], free=free_j,
                   posts=[[X(x), Y(y), *post_z(n, h, half)] for n, x, y, h, half in POSTS], post_r=POST_R, seam=SEAM,
                   body_rim=SEAM, tab="กล่องเทา Game Boy", title="กล่องเทา 172 × 94 × 34.5 แบบ Game Boy (แบบ 5, เจาะเอง)", template="case/out_box5/drill_template.pdf",
                   notes=["กลับหัวกล่องจากแบบ 4: ด้านที่มีเสาน็อตอยู่ล่าง (ช่อง USB-C จะได้ไม่ทับเขี้ยวล็อกที่ผนังข้าง)",
                          "จอเล็กย้ายไปซีกหลัง มองผ่านช่องแปะป้าย · ลำโพงย้ายมาหน้ากล่องด้านล่าง ตะแกรงเฉียงแบบ Game Boy",
                          "ซีกหลังไม่ต้องตัดเสา 7, 8 แล้ว"],
                   items=items, holes=holes, walls=walls,
                   lid_holes=[dict(kind=k, cx=X(cx), cy=Y(cy), a=a, b=b) for k, n, cx, cy, a, b, a0, b0 in lid_cuts()]), open(path, "w"), ensure_ascii=False)
    json.dump(dict(wires=[], joints=[]), open(os.path.join(os.path.dirname(path), "box_wires.json"), "w"))

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box5"); os.makedirs(out, exist_ok=True)
    ok = check()
    template(os.path.join(out, "drill_template.pdf")); drill_list(os.path.join(out, "drill_list.txt"))
    layout(os.path.join(out, "inside_layout.png")); asm_json(os.path.join(out, "box_asm.json"))
    print("written to", out, "" if ok else "  (SOMETHING DOES NOT FIT)")
