"""SomudTick wiring for box layout 2 -> ../wiring_box2.png   (python make_wiring2.py)
Schematic style: pins with the same coloured name tag are wired together.
Changes from wiring_v12.png: 1.3" OLED (SH1106), KY-022 IR receiver on IO14, 18650 cell with BMS,
and a perfboard HUB that carries 3V3 / GND / SDA / SCL to every module."""
import os
import matplotlib; matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

NET = {"SDA": "#1e88e5", "SCL": "#f9a825", "3V3": "#e53935", "GND": "#212121", "IO2": "#8e24aa", "IO3": "#6a1b9a",
       "IO21": "#fb8c00", "IO14": "#ffb300", "BAT+": "#c62828", "BAT-": "#424242", "SPK": "#757575"}
fig, ax = plt.subplots(figsize=(17.8, 13.0), dpi=110)
ax.set_xlim(0, 178); ax.set_ylim(-26, 105); ax.axis("off")

def tag(x, y, net, left):
    w = 2.2 + 1.25 * len(net); x0 = x - w - 2.5 if left else x + 2.5
    ax.plot([x, x0 + (w if left else 0)], [y, y], color=NET[net], lw=2)
    ax.add_patch(FancyBboxPatch((x0, y - 1.3), w, 2.6, boxstyle="round,pad=0.1,rounding_size=0.8", fc=NET[net], ec="none"))
    ax.text(x0 + w / 2, y, net, color="white", fontsize=8, weight="bold", ha="center", va="center")

def board(x, y, w, title, pins, side, note="", h=None):
    """pins: list of (label, net or None); side 'L' or 'R' = where the pins are"""
    h = h or 9 + 3.4 * len(pins)
    ax.add_patch(FancyBboxPatch((x, y - h), w, h, boxstyle="round,pad=0.3,rounding_size=1.4", fc="#1f2126", ec="#555"))
    ax.text(x + 1.5, y - 1.8, title, color="white", fontsize=11, weight="bold", va="top")
    if note: ax.text(x + 1.5, y - 5.2, note, color="#bdbdbd", fontsize=7.5, va="top")
    pos = {}
    for i, (lab, net) in enumerate(pins):
        py = y - 9.5 - i * 3.4; px = x if side == "L" else x + w
        if lab:
            ax.plot(px, py, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5)
            ax.text(px + (1.6 if side == "L" else -1.6), py, lab, color="white", fontsize=8.5, va="center", ha="left" if side == "L" else "right")
            if net: tag(px, py, net, left=(side == "L"))
        pos[lab] = (px, py)
    return pos

ax.text(2, 102, "SomudTick wiring  (box layout 2: 1.3\" OLED, 18650, wiring hub)", fontsize=15, weight="bold")
ax.text(2, 98.8, "Pins with the same coloured tag are joined.  Everything runs on 3.3 V.  I2C wires short (< 15 cm).  "
        "I2C: touch 0x38, DS3231 0x68, OLED 0x3C, PCF8574 0x20.  Every SDA / SCL / GND / 3V3 tag = one wire to the HUB", fontsize=9, color="#444")

board(26, 94, 32, "ES3C28P  (back)", [("I2C  SDA / IO16", "SDA"), ("I2C  SCL / IO15", "SCL"), ("I2C  GND", "GND"), ("I2C  3.3V", "3V3"), ("", None),
      ("EXP  IO21", "IO21"), ("EXP  IO14", "IO14"), ("EXP  IO3", "IO3"), ("EXP  IO2", "IO2"), ("", None),
      ("BAT  +", "BAT+"), ("BAT  -", "BAT-"), ("SPK", "SPK")], "R", "1.25 mm plugs. I2C plug -> HUB (all 4 wires)")

pcf = board(78, 94, 30, "PCF8574 module", [("SDA", "SDA"), ("SCL", "SCL"), ("GND", "GND"), ("VCC", "3V3")], "L", "A0 A1 A2 jumpers -> GND = 0x20", h=52)
# PCF right side: P0..P6 wired straight across to the NA011
labels = ["P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7  (free)"]
na_pins = ["A", "B", "C", "D", "E", "F", "K"]
for i, lab in enumerate(labels):
    py = 94 - 9.5 - i * 3.4 - 13.6
    ax.plot(108, py, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5); ax.text(106.4, py, lab, color="white", fontsize=8.5, va="center", ha="right")
    if i < 7:
        ax.plot([108, 128], [py, py], color="#43a047", lw=2.2)
        ax.plot(128, py, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5)
ax.add_patch(FancyBboxPatch((128, 94 - 62), 36, 62, boxstyle="round,pad=0.3,rounding_size=1.4", fc="#1f2126", ec="#555"))
ax.text(129.5, 92.2, "NA011  (2x6 pin block)", color="white", fontsize=11, weight="bold", va="top")
ax.text(129.5, 88.8, "slide switch -> 3V3 !   pin V: no wire\n(V gets 3V through the switch)\ncut the long Arduino pins underneath", color="#ffcc80", fontsize=7.5, va="top")
for i, n in enumerate(na_pins):
    py = 94 - 9.5 - i * 3.4 - 13.6; ax.text(129.8, py, f"{n}", color="white", fontsize=9, va="center", weight="bold")
    ax.text(134, py, {"A": "up", "B": "right", "C": "down", "D": "left", "E": "small E", "F": "small F", "K": "stick press"}[n], color="#bdbdbd", fontsize=7.5, va="center")
for i, (n, net) in enumerate((("3V", "3V3"), ("G", "GND"), ("X", "IO2"), ("Y", "IO3"))):
    py = 94 - 9.5 - (7 + i) * 3.4 - 13.6 - 1.5
    ax.plot(164, py, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5); ax.text(162.4, py, n, color="white", fontsize=9, va="center", ha="right", weight="bold")
    tag(164, py, net, left=False)

board(78, 38, 30, "OLED 1.3\"  SH1106", [("GND", "GND"), ("VCC", "3V3"), ("SCL", "SCL"), ("SDA", "SDA")], "L", "128x64, 0x3C. Check GND / VCC order!", h=24)
board(66, 11.5, 30, "DS3231 clock", [("SCL", "SCL"), ("SDA", "SDA"), ("VCC", "3V3"), ("GND", "GND")], "L", "(already fitted)", h=24) if False else None
board(120, 30, 24, "DS3231 clock", [("SCL", "SCL"), ("SDA", "SDA"), ("VCC", "3V3"), ("GND", "GND")], "L", "already fitted", h=24)
board(152, 30, 22, "KY-005 IR LED", [("S", "IO21"), ("middle", "3V3"), ("-", "GND")], "L", "sends (air con)", h=20)
board(152, 6, 22, "KY-022 IR rx", [("S", "IO14"), ("middle", "3V3"), ("-", "GND")], "L", "learns remotes", h=20)
hub = board(78, 9, 30, "HUB  (perfboard 30 x 20)", [("3V3 rail", "3V3"), ("GND rail", "GND"), ("SDA rail", "SDA"), ("SCL rail", "SCL")], "L",
            "behind the big screen, one rail per net", h=24)

# power and speaker, drawn as real wires
ax.add_patch(FancyBboxPatch((2, 12), 20, 13, boxstyle="round,pad=0.3,rounding_size=1.4", fc="#1f2126", ec="#555"))
ax.text(3.5, 23.3, "18650  3.7 V", color="white", fontsize=11, weight="bold", va="top"); ax.text(3.5, 19.9, "Li-ion + BMS, 3000 mAh\n(not LiFePO4)", color="#bdbdbd", fontsize=7.5, va="top")
ax.add_patch(FancyBboxPatch((28, 12), 16, 13, boxstyle="round,pad=0.3,rounding_size=1.4", fc="#1f2126", ec="#555"))
ax.text(29.5, 23.3, "switch", color="white", fontsize=11, weight="bold", va="top"); ax.text(29.5, 19.9, "SS12D00 (side wall)", color="#bdbdbd", fontsize=7.5, va="top")
for (x, y) in ((22, 16.5), (22, 13.8), (28, 16.5), (44, 16.5)): ax.plot(x, y, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5)
ax.plot([22, 28], [16.5, 16.5], color=NET["BAT+"], lw=2.2); ax.text(18.2, 16.5, "+", color="white", va="center"); ax.text(18.2, 13.8, "-", color="white", va="center")
tag(44, 16.5, "BAT+", left=False); tag(22, 13.8, "BAT-", left=False)
ax.add_patch(FancyBboxPatch((2, 36), 20, 11, boxstyle="round,pad=0.3,rounding_size=1.4", fc="#1f2126", ec="#555"))
ax.text(3.5, 45.3, "speaker", color="white", fontsize=11, weight="bold", va="top"); ax.text(3.5, 41.9, "8 ohm 1 W, 20-28 mm", color="#bdbdbd", fontsize=7.5, va="top")
ax.plot(22, 38.5, "o", ms=5.5, mfc="#e0e0e0", mec="#777", zorder=5); tag(22, 38.5, "SPK", left=False)

ax.text(2, -24, "Check before power: battery + / - with a meter (the 1.25 mm plug has no standard colours)   |   OLED pin order   |   a PCF8574A chip sits at 0x38-0x3F: set its jumpers off 0x38 and 0x3C   |   solder, then heat-shrink every joint", fontsize=8.5, color="#b71c1c")
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "wiring_box2.png")
fig.savefig(out, bbox_inches="tight"); print("written", os.path.abspath(out))


