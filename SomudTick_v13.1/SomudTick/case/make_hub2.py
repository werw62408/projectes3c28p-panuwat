"""SomudTick box layout 2: the wiring HUB perfboard -> out_box2/hub_board.png   (python make_hub2.py)

A piece of perfboard 12 x 8 holes (about 30 x 20 mm, 2.54 mm pitch).
Four rails, one per net, on rows 0 / 2 / 4 / 6 (the odd rows stay empty, so the rails cannot touch):
    row 0 = 3V3 (red)   row 2 = GND (black)   row 4 = SDA (blue)   row 6 = SCL (yellow)
Each cable gets its own COLUMN: its wires go into that column, one wire per rail.
Seen from the WIRE side (the side that faces the lid), top edge = towards the small screen.
"""
import os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyBboxPatch

COLS, ROWS, P = 12, 8, 2.54
RAIL = {0: ("3V3", "#e53935"), 2: ("GND", "#212121"), 4: ("SDA", "#1e88e5"), 6: ("SCL", "#f9a825")}
# (column, edge the cable leaves from, wire numbers, where it goes, nets used)
CABLES = [
    (1, "top",    "W13-W16", "small screen (OLED)",   ("3V3", "GND", "SDA", "SCL")),
    (2, "bottom", "W21-W22", "button board 3V / G",   ("3V3", "GND")),
    (3, "top",    "W17-W18", "IR receiver KY-022",    ("3V3", "GND")),
    (4, "bottom", "W5-W8",   "PCF8574 (IN side)",     ("3V3", "GND", "SDA", "SCL")),
    (5, "top",    "W19-W20", "IR LED KY-005",         ("3V3", "GND")),
    (6, "bottom", "W9-W12",  "DS3231 clock",          ("3V3", "GND", "SDA", "SCL")),
    (7, "top",    "W1-W4",   "IN: big screen I2C plug", ("3V3", "GND", "SDA", "SCL")),
]
SPARE = (8, 9, 10)
NETROW = {v[0]: r for r, v in RAIL.items()}

def board(ax, x0, y0, mirror, title):
    W, H = COLS * P, ROWS * P
    ax.add_patch(FancyBboxPatch((x0, y0), W, H, boxstyle="round,pad=0.4,rounding_size=1", fc="#c8a96a" if not mirror else "#b89a5c", ec="#7a6230", lw=1.2))
    X = lambda c: x0 + P / 2 + ((COLS - 1 - c) if mirror else c) * P
    Y = lambda r: y0 + H - P / 2 - r * P
    for c in range(COLS):
        for r in range(ROWS):
            ax.add_patch(Circle((X(c), Y(r)), 0.5, fc="#f3e3b8" if not mirror else "#d9d9d9", ec="#7a6230", lw=0.5, zorder=2))
    for c in range(COLS): ax.text(X(c), y0 + H + 1.6, str(c), ha="center", va="center", fontsize=7, color="#7a6230")
    for r, (net, col) in RAIL.items():                 # rail names outside the board, on the left of A and the right of B
        ax.text(x0 - 1.4 if not mirror else x0 + W + 1.4, Y(r), net, ha="right" if not mirror else "left", va="center", fontsize=9, weight="bold",
                color=col if net != "GND" else "#212121")
    if mirror:   # solder side: a bare wire along each rail, soldered at every hole of columns 1-9
        for r, (net, col) in RAIL.items():
            ax.plot([X(1), X(9)], [Y(r), Y(r)], color=col if net != "GND" else "#555", lw=5, solid_capstyle="round", zorder=3)
            for c in range(1, 10): ax.add_patch(Circle((X(c), Y(r)), 0.7, fc="#c0c0c0", ec="#666", lw=0.5, zorder=4))
    else:        # wire side: a coloured dot = one wire goes into that hole; the cable number sits on the edge it leaves from
        for r, (net, col) in RAIL.items():
            ax.plot([X(1), X(9)], [Y(r), Y(r)], color=col, lw=0.8, ls=(0, (2, 2)), alpha=0.6, zorder=1)
        for i, (c, edge, wires, to, nets) in enumerate(CABLES):
            for net in nets:
                r = NETROW[net]
                ax.add_patch(Circle((X(c), Y(r)), 0.85, fc=RAIL[r][1], ec="white", lw=0.9, zorder=5))
            yy = y0 + H + 4.6 if edge == "top" else y0 - 2.6
            ax.text(X(c), yy, f"cable\n{c}" if edge == "top" else f"{c}\ncable", ha="center", va="center", fontsize=8, weight="bold", color="#0d47a1")
        for c in SPARE: ax.text(X(c), Y(7), "-", ha="center", va="center", fontsize=6, color="#7a6230")
    ax.text(x0 + W / 2, y0 + H + 9.5, title, ha="center", fontsize=10.5, weight="bold")

fig, ax = plt.subplots(figsize=(15, 11), dpi=110); ax.set_aspect("equal"); ax.axis("off")
board(ax, 0, 0, False, "A  WIRE side (faces the lid)")
board(ax, 50, 0, True, "B  SOLDER side (faces the screen)")
ax.text(50 + 15.2, -2.5, "B = A turned over left to right: column 1 is now on the RIGHT.\nBridge columns 1-9 of each rail with a bare wire.\nTape this side before sticking the hub on.",
        ha="center", va="top", fontsize=8)
# the column table
rows = [("col", "leaves from", "wires", "goes to", "holes used")] + [(str(c), edge + " edge", w, to, " ".join(nets)) for c, edge, w, to, nets in CABLES] + \
       [("8 9 10", "-", "-", "spare (later: a sensor on the UART port, etc.)", "-")]
xs = [-8, -2, 10, 22, 52]
for j, row in enumerate(rows):
    y = -14 - j * 3.3
    for x, t in zip(xs, row): ax.text(x, y, t, fontsize=8.5, weight="bold" if j == 0 else "normal", va="center", family="monospace" if j else None)
ax.plot([-8, 80], [-15.8, -15.8], color="#999", lw=0.6)
steps = ["ORDER",
         "1  Cut perfboard 12 x 8 holes (about 30 x 20 mm).",
         "2  Side B: solder one bare wire along rows 0, 2, 4, 6 (columns 1-9). Odd rows stay empty.",
         "3  Meter on beep: touch every pair of rails. No beep at all = OK.",
         "4  Col 7 first (cable W1-W4 from the big screen's I2C plug). Switch on: 3.3 V between the 3V3 and GND rails. Switch off.",
         "5  Then one cable at a time: col 6 clock, col 4 PCF8574, col 1 small screen, col 3 and 5 IR, col 2 button board.",
         "    After each one: beep test 3V3 - GND (must be silent), switch on, check that part works, switch off.",
         "6  Tape side B, stick the hub behind the big screen with foam tape (wire side towards the lid)."]
for j, t in enumerate(steps): ax.text(-8, -48 - j * 3.2, t, fontsize=8.8, weight="bold" if j == 0 else "normal", color="#b71c1c" if j == 0 else "#222", va="center")
ax.text(-8, -76, "SomudTick wiring HUB (box layout 2): 4 rails, every cable in its own column.  Wire colours: red 3V3, black GND, blue SDA, yellow SCL", fontsize=10, color="#333")
ax.set_xlim(-10, 92); ax.set_ylim(-79, 32)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box2", "hub_board.png")
fig.savefig(out, bbox_inches="tight"); print("written", out)
