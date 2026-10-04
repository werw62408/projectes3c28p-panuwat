"""SomudTick v12 wire harness: where each wire runs inside the case, where wires are joined, how long to cut them.
    python make_harness.py   -> ../wiring_harness_v12.png  + cut list (markdown, printed)
Positions come from make_case.py (the same numbers as the printed case). Drawn as seen FROM THE BACK with the back
plate off, because that is how you solder it: left and right are swapped compared with the front.
Plug positions on the ES3C28P are approximate (maker drawing): check the names printed on the board."""
import math, os
import make_case as C
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, Polygon

def bx(x): return -x                      # front x -> seen from the back
TOP = C.SCR_CY + C.SCR_H / 2              # top edge of the screen board
S, N = C.SCR_CY, C.NA_TOP
# ---- points, already in BACK VIEW x (y as usual), z = depth ----
P = {
    "ES_I2C": (bx(-24), TOP - 3, 13), "ES_EXP": (bx(-13), TOP - 3, 13), "ES_BAT": (bx(-2), TOP - 3, 13), "ES_SPK": (bx(8), TOP - 3, 13),
    "PCF_IN": (bx(4), S - 4, 16), "PCF_OUT": (bx(36), S - 4, 16), "PCF_P": (bx(20), S - 14, 16),
    "RTC_A": (bx(-5), S - 2, 16), "RTC_B": (bx(-39), S - 2, 16),
    "OLED": (bx(-17.5), C.OLED_CY, 5), "IR": (bx(C.IR_X), C.IN_H / 2 - 9, 10),
    "NA": (bx(C.NA_LEFT + 71), N - 5, 25.5), "SW": (bx(C.IN_W / 2 - 3), C.SW_Y, 9),
    "BAT": (bx(C.SCR_CX - 12), S + 26, 25), "SPK": (bx(C.SCR_CX + 24), S + 12, 26),
}
GAP_X = (bx(4) + bx(-5)) / 2               # the free lane between the PCF8574 and the DS3231
FRONT_SLACK, HINGE_SLACK = 15, 60

# ---- bundles: one path, several wires (drawn side by side) ----
# (name, colour of the label, [(wire id, from, to, signal, colour, AWG)], path through points, back plate?)
def pt(name): return P[name][:2]
B = [
    ("I2C in", "#1565c0", [("W1", "ES_I2C", "PCF_IN", "SDA", "#1e88e5", 28), ("W2", "ES_I2C", "PCF_IN", "SCL", "#f9a825", 28),
                           ("W3", "ES_I2C", "PCF_IN", "GND", "#212121", 26), ("W4", "ES_I2C", "PCF_IN", "3V3", "#e53935", 26)],
     [pt("ES_I2C"), (P["ES_I2C"][0], TOP - 8), (P["PCF_IN"][0], TOP - 8), pt("PCF_IN")], False),
    ("I2C to DS3231", "#1565c0", [("W5", "PCF_OUT", "RTC_A", "SDA", "#1e88e5", 28), ("W6", "PCF_OUT", "RTC_A", "SCL", "#f9a825", 28),
                                  ("W7", "PCF_OUT", "RTC_A", "GND", "#212121", 28), ("W8", "PCF_OUT", "RTC_A", "3V3", "#e53935", 28)],
     [pt("PCF_OUT"), (P["PCF_OUT"][0], S - 19), (P["RTC_A"][0], S - 19), pt("RTC_A")], False),
    ("I2C to OLED", "#1565c0", [("W9", "RTC_B", "OLED", "SDA", "#1e88e5", 28), ("W10", "RTC_B", "OLED", "SCL", "#f9a825", 28),
                                ("W11", "RTC_B", "OLED", "GND", "#212121", 28), ("W12", "RTC_B", "OLED", "3V3", "#e53935", 28)],
     [pt("RTC_B"), (P["RTC_B"][0], TOP + 3), (P["OLED"][0], TOP + 3), pt("OLED")], False),
    ("IR power", "#e65100", [("W13", "OLED", "IR", "3V3", "#e53935", 28), ("W14", "OLED", "IR", "GND", "#212121", 28)],
     [pt("OLED"), (P["OLED"][0] - 3, C.OLED_CY), (P["OLED"][0] - 3, TOP + 7), (P["IR"][0], TOP + 7), pt("IR")], False),
    ("IR signal", "#e65100", [("W15", "ES_EXP", "IR", "IO21", "#fb8c00", 28)],
     [pt("ES_EXP"), (P["ES_EXP"][0], TOP + 9), (P["IR"][0] + 2, TOP + 9), (P["IR"][0] + 2, P["IR"][1])], False),
    ("stick X / Y", "#6a1b9a", [("W16", "ES_EXP", "NA", "IO2 > X", "#8e24aa", 28), ("W17", "ES_EXP", "NA", "IO3 > Y", "#6a1b9a", 28)],
     [pt("ES_EXP"), (P["ES_EXP"][0], TOP - 6), (GAP_X, TOP - 6), (GAP_X, N + 1.5), (P["NA"][0] + 6, N + 1.5), (P["NA"][0] + 6, P["NA"][1])], False),
    ("buttons (7-wire ribbon)", "#2e7d32", [(f"W{18+k}", "PCF_P", "NA", f"P{k} > {n}", "#43a047", 28) for k, n in enumerate("ABCDEFK")],
     [pt("PCF_P"), (P["PCF_P"][0], N + 4), (P["NA"][0], N + 4), pt("NA")], False),
    ("NA011 power", "#b71c1c", [("W25", "PCF_OUT", "NA", "3V3 > 3V", "#e53935", 26), ("W26", "PCF_OUT", "NA", "GND > G", "#212121", 26)],
     [pt("PCF_OUT"), (P["PCF_OUT"][0] + 3, P["PCF_OUT"][1]), (P["PCF_OUT"][0] + 3, N + 1.5), (P["NA"][0] - 6, N + 1.5), (P["NA"][0] - 6, P["NA"][1])], False),
    ("switch > board", "#c62828", [("W28", "SW", "ES_BAT", "BAT + (switched)", "#c62828", 24)],
     [pt("SW"), (P["SW"][0], TOP + 12), (P["ES_BAT"][0], TOP + 12), pt("ES_BAT")], False),
    # from the back plate: over the top edge (lay the plate open upwards)
    ("battery", "#c62828", [("W27", "BAT", "SW", "BAT + > switch", "#c62828", 24), ("W29", "BAT", "ES_BAT", "BAT -", "#424242", 24)],
     [pt("BAT"), (P["BAT"][0], C.IN_H / 2 - 2)], True),
    ("speaker", "#616161", [("W30", "SPK", "ES_SPK", "SPK +", "#9e9e9e", 26), ("W31", "SPK", "ES_SPK", "SPK -", "#616161", 26)],
     [pt("SPK"), (P["SPK"][0], C.IN_H / 2 - 2)], True),
]
J = [("J1", "PCF_OUT", 2.5, 0, "PCF8574 OUT pin VCC: W8 (to DS3231) + W25 (to NA011 3V)"),
     ("J2", "PCF_OUT", 2.5, -2.5, "PCF8574 OUT pin GND: W7 (to DS3231) + W26 (to NA011 G)"),
     ("J3", "OLED", -2.5, 1.2, "OLED VCC pad: W12 (from DS3231) + W13 (to IR middle)"),
     ("J4", "OLED", -2.5, -1.2, "OLED GND pad: W11 (from DS3231) + W14 (to IR -)"),
     ("S1", "ES_I2C", 0, 3, "I2C plug lead x4 <-> W1-W4 (only if the lead is short); stagger the joints 8 mm"),
     ("S2", "ES_EXP", 0, 3, "expansion plug lead: IO21 W15, IO2 W16, IO3 W17; IO14: cut and heat-shrink"),
     ("S3", "ES_BAT", 0, 3, "battery plug lead: + W28, - W29"),
     ("S4", "ES_SPK", 0, 3, "speaker plug lead <-> W30 / W31")]

def plen(path): return sum(math.dist(path[i], path[i + 1]) for i in range(len(path) - 1))
def wire_len(bundle, w):
    name, _, wires, path, back = bundle
    a, b = P[w[1]], P[w[2]]
    d = plen(path) + abs(a[2] - b[2]) + 6              # path + up/down + 3 mm at each solder end
    if back:                                            # from the top edge of the plate on to its plug / the switch
        d += abs(P[w[2]][0] - path[-1][0]) + abs(P[w[2]][1] - path[-1][1])
    d += HINGE_SLACK if back else FRONT_SLACK
    return int(math.ceil(d / 5.0) * 5)

# ---------------- drawing ----------------
fig, (A1, A2) = plt.subplots(1, 2, figsize=(19, 14), dpi=105, gridspec_kw=dict(width_ratios=[1.25, 1]))
for ax in (A1, A2):
    ax.set_aspect("equal"); ax.axis("off")
    ax.set_xlim(-C.OUT_W / 2 - 4, C.OUT_W / 2 + 4); ax.set_ylim(-C.OUT_H / 2 - 4, C.OUT_H / 2 + 24)
    ax.add_patch(Polygon([(bx(x), y) for x, y in C.rrect(0, 0, C.OUT_W, C.OUT_H, C.R_CORNER)], fill=False, lw=1.5))
def box(ax, x0, x1, y0, y1, label, fc, a=0.35, ls="-", fs=8, va="top"):
    ax.add_patch(Rectangle((min(x0, x1), y0), abs(x1 - x0), y1 - y0, fc=fc, ec="#555", alpha=a, ls=ls, lw=1))
    ax.text((x0 + x1) / 2, y1 - 1.5 if va == "top" else (y0 + y1) / 2, label, ha="center", va=va, fontsize=fs, color="#222")
# layer 1: the front shell with its boards
box(A1, bx(C.SCR_CX - 43), bx(C.SCR_CX + 43), S - 25, S + 25, "", "#90caf9", 0.15)
A1.text(bx(C.SCR_CX), S + 20.5, "ES3C28P (back side)", ha="center", fontsize=8, color="#1565c0")
box(A1, bx(C.NA_LEFT), bx(C.NA_LEFT + C.NA_L), N - C.NA_H, N, "NA011 (back side)", "#ef9a9a", 0.15)
box(A1, bx(-19), bx(19), C.OLED_CY - 6, C.OLED_CY + 6, "OLED (from behind)", "#b0bec5", 0.4, fs=7)
box(A1, bx(2), bx(38), S - 14, S + 6, "PCF8574\n(foam tape on the\nscreen board)", "#80cbc4", 0.55, fs=7, va="center")
box(A1, bx(-3), bx(-41), S - 13, S + 9, "DS3231\n(foam tape on the\nscreen board)", "#a5d6a7", 0.55, fs=7, va="center")
box(A1, bx(C.IR_X - 8), bx(C.IR_X + 8), C.IN_H / 2 - 14, C.IN_H / 2 - 3, "IR LED", "#ffcc80", 0.6, fs=7)
box(A1, P["SW"][0] - 2.5, P["SW"][0] + 2.5, C.SW_Y - 5, C.SW_Y + 5, "", "#9e9e9e", 0.8)
A1.text(P["SW"][0] + 4, C.SW_Y, "switch", va="center", fontsize=7)
A1.text(bx(-C.IN_W / 2) - 1, S, "USB-C", ha="right", va="center", fontsize=7, color="#555", rotation=90)
for k in ("ES_I2C", "ES_EXP", "ES_BAT", "ES_SPK"):
    x, y, _ = P[k]; A1.add_patch(Rectangle((x - 3.5, y - 1.5), 7, 3, fc="white", ec="k", lw=0.8, zorder=6))
    A1.text(x, y - 2.3, k[3:], ha="center", va="top", fontsize=6.5, zorder=6)
x, y, _ = P["NA"]; A1.add_patch(Rectangle((x - 8, y - 3), 16, 6, fc="#fdd835", ec="k", lw=0.8, zorder=6))
A1.text(x, y - 4, "yellow 2x6 block\n(solder on the underside)", ha="center", va="top", fontsize=6.5)
# layer 2: the back plate (inside up, same left/right as layer 1)
box(A2, P["BAT"][0] - 17, P["BAT"][0] + 17, S - 25, S + 25, "battery 503450\n(foam tape in its bay)", "#fff59d", 0.6, fs=8, va="center")
A2.add_patch(Circle((P["SPK"][0], S), 12, fc="#e0e0e0", ec="#555", alpha=0.7)); A2.text(P["SPK"][0], S, "speaker\n(on the grille)", ha="center", va="center", fontsize=7)
A2.annotate("", xy=(0, C.OUT_H / 2 + 9), xytext=(0, C.OUT_H / 2 + 1), arrowprops=dict(arrowstyle="->", lw=1.2))
A2.text(0, C.OUT_H / 2 + 10, "hinge: lay the back plate open UPWARDS; its wires go over the top edge", ha="center", fontsize=8)

def draw_bundle(ax, bundle):
    name, lc, wires, path, back = bundle
    n = len(wires)
    for k, w in enumerate(wires):
        o = (k - (n - 1) / 2) * 0.9
        xs = [p[0] + o for p in path]; ys = [p[1] + o for p in path]
        ax.plot(xs, ys, color=w[4], lw=1.5 if w[5] >= 28 else 2.3, zorder=4, solid_capstyle="round")
    # label on the longest segment
    i = max(range(len(path) - 1), key=lambda i: math.dist(path[i], path[i + 1]))
    mx, my = (path[i][0] + path[i + 1][0]) / 2, (path[i][1] + path[i + 1][1]) / 2
    ids = wires[0][0] + ("-" + wires[-1][0] if n > 1 else "")
    ax.text(mx, my + (1.8 if path[i][1] == path[i + 1][1] else 0), f"{ids}  {name}", fontsize=6.8, color=lc, weight="bold", ha="center",
            va="bottom", zorder=9, rotation=0 if path[i][1] == path[i + 1][1] else 90, bbox=dict(fc="white", ec="none", pad=0.4, alpha=0.85))
for b in B: draw_bundle(A2 if b[4] else A1, b)
for b in B:   # back plate leads continue in layer 1 from the top edge to their plug / the switch (dashed)
    if not b[4]: continue
    for k, w in enumerate(b[2]):
        end = P[w[2]]; o = k * 1.2 + (4 if b[0] == "speaker" else 0)
        A1.plot([end[0] + o, end[0] + o, end[0] + o], [C.IN_H / 2 - 2, end[1] + 2, end[1]], color=w[4], lw=2, ls="--", zorder=4)
    A1.text(P[b[2][0][2]][0] + 6, C.IN_H / 2 - 2.5, f"{b[2][0][0]}-{b[2][-1][0]} from back plate", fontsize=6, color=b[1], va="top")
for j, where, dx, dy, _ in J:
    x, y, _ = P[where]; col = "#d50000" if j[0] == "J" else "#6200ea"
    A1.add_patch(Circle((x + dx, y + dy), 1.4, fc=col, ec="white", lw=0.8, zorder=10))
    A1.text(x + dx + (2 if dx >= 0 else -2), y + dy, j, color=col, fontsize=7, weight="bold", ha="left" if dx >= 0 else "right", va="center", zorder=10)

A1.set_title("LAYER 1: inside the front shell, seen from the back", fontsize=12, weight="bold")
A2.set_title("LAYER 2: back plate, inside up", fontsize=12, weight="bold")
fig.suptitle("SomudTick v12 wire routing   (left/right swapped vs. the front)", fontsize=15, weight="bold", y=0.985)
jl = "   ".join(f"{j}: {t}" for j, _, _, _, t in J[:4])
fig.text(0.02, 0.035, "red J = two wires soldered into one point:  " + jl, fontsize=7.5)
fig.text(0.02, 0.018, "purple S = splice onto the board's 1.25 mm plug lead (or plug straight in if the lead is long enough).  "
         "Lengths: see the cut list (README); cut 20 mm longer, trim when fitting.", fontsize=7.5)
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "wiring_harness_v12.png")
fig.savefig(out, bbox_inches="tight"); print("written", os.path.abspath(out))

print("\n| สาย | จาก → ถึง | สัญญาณ | ขนาด | ตัดยาว (มม.) |\n|---|---|---|---|---|")
tot = 0
for b in B:
    for w in b[2]:
        L = wire_len(b, w); tot += L
        print(f"| {w[0]} | {w[1]} → {w[2]} | {w[3]} | {w[5]} AWG | {L} |")
print("total", tot, "mm")
