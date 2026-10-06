"""SomudTick in the ready-made box, third layout (make_box4.py, the real grey box): where every wire runs, how long to cut them.

    python make_box_harness2.py  -> out_box4/wiring_box.png, out_box4/cut_list.md, out_box4/box_wires.json

Power and I2C go through a small perfboard HUB behind the big screen (rails 3V3 / GND / SDA / SCL):
every module gets its own wires from the hub, so no pin ever has two wires soldered into it.

Every path is measured in 3D through lanes that are free inside the box:
  - just behind the big screen (z 16), in the gap between the screen and the button board,
  - over the tops of the battery / clock / PCF8574 (z 28, under the lid).
Cut length = path + 4 mm stripped at each end + 15 mm spare, rounded up to 5 mm.
The ES3C28P plug places on the back of the board are approximate (maker drawing): check the names printed on the board.
"""
import json, math, os
import make_box4 as M

cx, cy = M.scr_c
T, SB = cy - M.SCR["h"] / 2, cy + M.SCR["h"] / 2           # top and bottom edge of the big screen (y grows downwards)
ZP = M.FACE + M.SCR["pcb_front"] + M.SCR["pcb_t"] + 1.5    # plugs on the back of the screen board
ZS = 16.0                                                  # lane behind the screen
ZT = M.BODY_D - 4.0                                        # lane over the modules, under the lid
ZH = M.Z_SCR_BACK + 0.5 + M.HUB[2]                         # pads on top of the hub
ZM = M.Z_BEHIND + 2.0                                      # solder pads on the clock / PCF8574
ZO = M.FACE + M.OLED["t"] + 0.5                            # back of the small screen
GAPY = (SB + M.na_top) / 2                                 # the gap between the screen and the button board
TOPY = M.OLED_TOP + M.OLED["h"] + 3.0                      # the gap between the small screen and the big screen
hx, hy = M.na(M.HDR)

hubx, huby = M.hub_c
P = {
    "ES_I2C": (cx - 25, T + 3, ZP), "ES_EXP": (cx - 14, T + 3, ZP), "ES_BAT": (cx - 3, T + 3, ZP), "ES_SPK": (cx + 7, T + 3, ZP),
    "HUB": (hubx, huby, ZH), "HUB_T": (hubx, huby - M.HUB[1] / 2 + 2, ZH), "HUB_B": (hubx, huby + M.HUB[1] / 2 - 2, ZH),
    "PCF_IN": (M.pcf_c[0] + 15, M.pcf_c[1], ZM), "PCF_P": (M.pcf_c[0], M.pcf_c[1] + 7, ZM),
    "DS_4": (M.ds_c[0] + 16, M.ds_c[1], ZM),
    "OLED": (M.oled_c[0], M.OLED_TOP + 2.5, ZO),
    "IRRX": (M.irrx_c[0], M.irrx_c[1] + M.IR_RX[1] / 2 - 2.5, M.FACE + 2.5),
    "IRTX": (M.irtx_c[0], M.irtx_c[1] + M.IR_TX[1] / 2 - 2.5, M.FACE + 2.5),
    "SW": (M.IX0 + M.SW[2] + 0.5, M.sw_y, 9.0),
    "HDR": (hx, hy, M.NA_BACK),
    "BAT": (M.batt_c[0] + M.BATT[0] / 2 - 3, M.batt_c[1] - 4, M.FACE + 0.5 + M.BATT[2] - 3),   # BMS end, on the right seen from the front
    "SPK": (M.spk_c[0] - 6, M.spk_c[1], M.SPK_Z0 + 2),
}
def p(name): return P[name]
LX = M.pcf_c[0] + 15                                       # the run down to the PCF8574 IN pins
RX = M.IX1 - 4                                             # a lane along the right inside wall (seen from the front)

def bus(ids, a, b, rails=("SDA", "SCL", "GND", "3V3")):
    col = {"SDA": "#1e88e5", "SCL": "#f9a825", "GND": "#212121", "3V3": "#e53935"}
    return [(w, r, col[r], 28, a, b) for w, r in zip(ids, rails)]
up = lambda x: [(hubx, P["HUB_T"][1], ZS), (hubx, TOPY, ZS), (x, TOPY, ZS)]
# bundles: (name, label colour, [(wire, signal, colour, AWG, from, to)], path)
B = [
    ("I2C in: screen > hub", "#1565c0", bus(["W1", "W2", "W3", "W4"], "ES_I2C", "HUB_T"),
     [p("ES_I2C"), (P["ES_I2C"][0], T + 3, ZS), (P["ES_I2C"][0], P["HUB_T"][1], ZS), (hubx, P["HUB_T"][1], ZS), p("HUB_T")]),
    ("hub > PCF8574", "#1565c0", bus(["W5", "W6", "W7", "W8"], "HUB_B", "PCF_IN"),
     [p("HUB_B"), (hubx, P["HUB_B"][1], ZS), (hubx, GAPY, ZS), (LX, GAPY, ZS), (LX, GAPY, ZT), (LX, P["PCF_IN"][1], ZT), p("PCF_IN")]),
    ("hub > clock", "#1565c0", bus(["W9", "W10", "W11", "W12"], "HUB_B", "DS_4"),
     [p("HUB_B"), (hubx, P["HUB_B"][1], ZS), (hubx, GAPY, ZS), (P["DS_4"][0], GAPY, ZS), (P["DS_4"][0], GAPY, ZT), (P["DS_4"][0], P["DS_4"][1], ZT), p("DS_4")]),
    ("hub > small screen", "#1565c0", bus(["W13", "W14", "W15", "W16"], "HUB_T", "OLED"),
     [p("HUB_T")] + up(P["OLED"][0]) + [(P["OLED"][0], TOPY, ZO + 1), (P["OLED"][0], P["OLED"][1], ZO + 1), p("OLED")]),
    ("hub > IR receiver (power)", "#e65100", bus(["W17", "W18"], "HUB_T", "IRRX", ("GND", "3V3")),
     [p("HUB_T")] + up(P["IRRX"][0] + 2) + [(P["IRRX"][0] + 2, TOPY, ZO + 2), (P["IRRX"][0] + 2, P["IRRX"][1], ZO + 2), p("IRRX")]),
    ("hub > IR LED (power)", "#e65100", bus(["W19", "W20"], "HUB_T", "IRTX", ("GND", "3V3")),
     [p("HUB_T")] + up(P["IRTX"][0] + 2) + [(P["IRTX"][0] + 2, TOPY, ZO + 2), (P["IRTX"][0] + 2, P["IRTX"][1], ZO + 2), p("IRTX")]),
    ("hub > button board power", "#b71c1c", bus(["W21", "W22"], "HUB_B", "HDR", ("GND", "3V3")),
     [p("HUB_B"), (hubx, P["HUB_B"][1], ZS), (hubx, GAPY, ZS), (hx - 4, GAPY, ZS), (hx - 4, hy, ZS), (hx - 4, hy, M.NA_BACK)]),
    ("IR signals", "#e65100",
     [("W23", "IO21 > IR LED S", "#fb8c00", 28, "ES_EXP", "IRTX"), ("W24", "IO14 > IR receiver S", "#ffb74d", 28, "ES_EXP", "IRRX")],
     [p("ES_EXP"), (P["ES_EXP"][0], TOPY, ZP), (P["IRTX"][0] - 2, TOPY, ZP), (P["IRTX"][0] - 2, P["IRTX"][1], ZO + 2), p("IRTX")]),
    ("stick X / Y", "#6a1b9a",
     [("W25", "IO2 > X", "#8e24aa", 28, "ES_EXP", "HDR"), ("W26", "IO3 > Y", "#6a1b9a", 28, "ES_EXP", "HDR")],
     [p("ES_EXP"), (P["ES_EXP"][0], T + 3, ZS), (P["ES_EXP"][0] + 4, GAPY - 1, ZS), (hx, GAPY - 1, ZS), (hx, hy, ZS), p("HDR")]),
    ("buttons (7-wire ribbon)", "#2e7d32",
     [(f"W{27 + k}", f"P{k} > {n}", "#43a047", 28, "PCF_P", "HDR") for k, n in enumerate("ABCDEFK")],
     [p("PCF_P"), (P["PCF_P"][0], P["PCF_P"][1], ZT), (P["PCF_P"][0] + 6, hy + 4, ZT), (hx, hy + 4, ZT), (hx, hy + 4, M.NA_BACK + 1), p("HDR")]),
    ("battery > switch > screen", "#c62828",
     [("W34", "BAT + > switch", "#c62828", 24, "BAT", "SW"), ("W36", "BAT -", "#424242", 24, "BAT", "ES_BAT")],
     [p("BAT"), (RX, P["BAT"][1], P["BAT"][2]), (RX, P["BAT"][1], ZT), (RX, GAPY, ZT), (RX, GAPY, ZS), (P["ES_BAT"][0] + 2, GAPY, ZS),
      (P["ES_BAT"][0] + 2, T - 1, ZS), (P["ES_BAT"][0], T - 1, ZS), (P["ES_BAT"][0], T + 3, ZS), p("ES_BAT")]),
    ("switch > screen", "#c62828",
     [("W35", "switch > BAT + (plug)", "#c62828", 24, "SW", "ES_BAT")],
     [p("SW"), (P["SW"][0], P["SW"][1], 12), (P["SW"][0], TOPY, 12), (P["ES_BAT"][0], TOPY, 12), (P["ES_BAT"][0], T + 3, ZP), p("ES_BAT")]),
    ("speaker", "#616161",
     [("W37", "SPK +", "#9e9e9e", 26, "ES_SPK", "SPK"), ("W38", "SPK -", "#616161", 26, "ES_SPK", "SPK")],
     [p("ES_SPK"), (P["ES_SPK"][0], T + 3, ZS), (P["ES_SPK"][0], P["SPK"][1], ZS), (P["SPK"][0], P["SPK"][1], ZS), p("SPK")]),
]
# W34 does not end at the plug: from the lane behind the screen it turns off to the switch
W30_TAIL = [(P["ES_BAT"][0] + 2, T - 1, ZS), (P["SW"][0] + 1, T - 1, ZS), (P["SW"][0] + 1, P["SW"][1], 12), p("SW")]
J = []    # no two-wire joints any more: the hub takes them
S = [("S1", "ES_I2C", "I2C plug lead x4 <-> W1-W4"), ("S2", "ES_EXP", "expansion plug lead: IO21 W23, IO14 W24, IO2 W25, IO3 W26"),
     ("S3", "ES_BAT", "battery plug lead: + W35, - W36"), ("S4", "ES_SPK", "speaker plug lead: W37 / W38")]

def plen(path): return sum(math.dist(path[i], path[i + 1]) for i in range(len(path) - 1))
def cut(mm): return int(math.ceil((mm + 8 + 15) / 5.0) * 5)
def length(bundle, w):
    path = bundle[3]
    if w[0] == "W34": i = path.index(W30_TAIL[0]); path = path[:i + 1] + W30_TAIL[1:]
    if w[0] == "W24": path = bundle[3][:-3] + [(P["IRRX"][0] - 2, TOPY, ZP), (P["IRRX"][0] - 2, P["IRRX"][1], ZO + 2), p("IRRX")]
    return plen(path), path

rows, wires3d = [], []
for b in B:
    for w in b[2]:
        L, path = length(b, w)
        rows.append((w[0], w[4], w[5], w[1], w[3], cut(L), round(L)))
        wires3d.append(dict(id=w[0], c=w[2], awg=w[3], sig=w[1], a=w[4], b=w[5], cut=cut(L), pts=[[x - M.BOX_W / 2, M.BOX_H / 2 - y, z] for x, y, z in path]))
rows.sort(key=lambda r: int(r[0][1:]))
NAMES = {"ES_I2C": "จอใหญ่ ปลั๊ก I2C", "ES_EXP": "จอใหญ่ ปลั๊กขยาย", "ES_BAT": "จอใหญ่ ปลั๊กแบต", "ES_SPK": "จอใหญ่ ปลั๊กลำโพง",
         "HUB_T": "แผงรวมสาย (ขอบบน)", "HUB_B": "แผงรวมสาย (ขอบล่าง)",
         "PCF_IN": "PCF8574 ขา IN", "PCF_P": "PCF8574 ขา P0-P6", "DS_4": "DS3231 หัว 4 ขา",
         "OLED": "จอเล็ก", "IRRX": "ตัวรับ IR (KY-022)", "IRTX": "หลอดส่ง IR (KY-005)", "SW": "สวิตช์", "HDR": "NA011 ขาเหลือง",
         "BAT": "แบต 18650 (ปลาย BMS)", "SPK": "ลำโพง"}

def cut_list(path):
    L = ["# SomudTick กล่องจริงสีเทา (แบบ 4: 172 × 94 × 34.5 สองซีก): ตัดสาย", "",
         "ความยาว = ทางเดินสายจริงในกล่อง (วัดแบบ 4 มิติ) + ปลอกปลายละ 4 มม. + เผื่อ 15 มม. ปัดขึ้นทีละ 5 มม.", "",
         "| สาย | จาก → ถึง | สัญญาณ | ขนาด | **ตัดยาว (มม.)** | ทางจริง |", "|---|---|---|---|---|---|"]
    tot = {}
    for wid, a, b, sig, awg, c, raw in rows:
        L.append(f"| {wid} | {NAMES[a]} → {NAMES[b]} | {sig} | {awg} AWG | **{c}** | {raw} |"); tot[awg] = tot.get(awg, 0) + c
    L += ["", "**รวม:** " + " · ".join(f"{awg} AWG {v / 1000:.2f} ม." for awg, v in sorted(tot.items())) + f" · ทั้งหมด {sum(tot.values()) / 1000:.2f} ม.", "",
          "**แผงรวมสาย (hub):** บอร์ดไข่ปลาตัดเหลือ 30×20 มม. ทำ 4 ราง (บัดกรีจุดต่อเป็นแถวยาว): **3V3 · GND · SDA · SCL**",
          "- ขอบบนของแผง: สายเข้าจากปลั๊ก I2C ของจอใหญ่ (W1-W4), และสายออกไปจอเล็ก (W13-W16), ตัวรับ IR (W17-W18), หลอดส่ง IR (W19-W20)",
          "- ขอบล่างของแผง: สายออกไป PCF8574 (W5-W8), DS3231 (W9-W12), ไฟบอร์ดปุ่ม (W21-W22)",
          "- ไม่มีขาไหนต้องบัดกรีสาย 2 เส้นแล้ว · ติดแผงด้วยเทปโฟม ทับเทปกันไฟฟ้าบนหลังจอใหญ่",
          "- ⚠️ จอ 1.3\" บางรุ่นเรียงขา VCC-GND สลับกัน: ดูตัวหนังสือบนบอร์ดจอก่อนต่อ W15/W16", ""] + \
         ["", "**จุดต่อกับสายปลั๊ก 1.25 มม. ของบอร์ดจอ** (ถ้าสายปลั๊กยาวพอ ต่อตรงเข้าโมดูลได้เลย แล้วไม่ต้องตัดสายเส้นนั้น):"] + [f"- **{s}** {t}" for s, _, t in S] + \
         ["", "ตำแหน่งปลั๊กหลังจอใหญ่เป็นค่าประมาณจากแบบผู้ผลิต ดูชื่อบนบอร์ดจริงก่อน ความยาวอาจคลาด ±2 ซม."]
    open(path, "w", encoding="utf-8").write("\n".join(L) + "\n")
    return tot

def drawing(outpath):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    fig, ax = plt.subplots(figsize=(10.5, 16), dpi=110); ax.set_aspect("equal"); ax.axis("off")
    X = lambda x: M.BOX_W - x; Y = lambda y: M.BOX_H - y        # seen from the BACK (lid off): left and right swap
    ax.add_patch(FancyBboxPatch((0, 0), M.BOX_W, M.BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.5))
    for n, bx_, by_, w, h, z0, z1, col in M.boxes():
        if n in ("screen glass", "speaker ears", "screen back parts"): continue
        ax.add_patch(Rectangle((X(bx_) - w / 2, Y(by_) - h / 2), w, h, fc=col, ec="#444", lw=0.6, alpha=0.18 if z1 < 15 else 0.3))
        ax.text(X(bx_), Y(by_ - h / 2) - 1.5, n, ha="center", va="top", fontsize=6, color="#333")
    ax.add_patch(Rectangle((X(M.spk_c[0]) - M.SPK[0] / 2, Y(M.spk_c[1]) - M.SPK[1] / 2), M.SPK[0], M.SPK[1], fc="#90caf9", ec="#444", lw=0.6, alpha=0.4)); ax.text(X(M.spk_c[0]), Y(M.spk_c[1]), "speaker", ha="center", fontsize=6)
    ax.text(X(hubx), Y(huby), "HUB\n3V3 GND SDA SCL", ha="center", va="center", fontsize=6, weight="bold", zorder=8, bbox=dict(fc="#fff8e1", ec="#9e8a5a", pad=0.6))
    ax.add_patch(Rectangle((X(hx) - 8, Y(hy) - 2.6), 16, 5.2, fc="#fdd835", ec="k", lw=0.8, zorder=6))
    ax.text(X(hx) - 9, Y(hy), "NA011 yellow 2x6\n(solder on the back)", ha="right", va="center", fontsize=5.5, zorder=6, bbox=dict(fc="white", ec="none", pad=0.3, alpha=0.85))
    for k in ("ES_I2C", "ES_EXP", "ES_BAT", "ES_SPK"):
        x, y, _ = P[k]; ax.add_patch(Rectangle((X(x) - 3.5, Y(y) - 1.5), 7, 3, fc="white", ec="k", lw=0.8, zorder=7)); ax.text(X(x), Y(y) - 2.3, k[3:], ha="center", va="top", fontsize=5.5, zorder=7)
    for b in B:
        n = len(b[2])
        for k, w in enumerate(b[2]):
            _, path = length(b, w); o = (k - (n - 1) / 2) * 0.7
            ax.plot([X(q[0]) + o for q in path], [Y(q[1]) + o for q in path], color=w[2], lw=1.3 if w[3] >= 28 else 2.1, zorder=4, solid_capstyle="round")
        path = b[3]; i = max(range(len(path) - 1), key=lambda i: math.dist(path[i][:2], path[i + 1][:2]))
        mx, my = (X(path[i][0]) + X(path[i + 1][0])) / 2, (Y(path[i][1]) + Y(path[i + 1][1])) / 2
        ids = b[2][0][0] + ("-" + b[2][-1][0] if n > 1 else "")
        ax.text(mx, my, ids, fontsize=6, color=b[1], weight="bold", ha="center", va="center", zorder=9, bbox=dict(fc="white", ec="none", pad=0.3, alpha=0.85))
    for j, where, _ in J:
        x, y, _ = P[where]; o = 2.5 if j in ("J1", "J3", "J5") else -2.5
        ax.add_patch(Circle((X(x) + o, Y(y) + 2), 1.2, fc="#d50000", ec="white", lw=0.6, zorder=10)); ax.text(X(x) + o, Y(y) + 4.2, j, color="#d50000", fontsize=6, weight="bold", ha="center", zorder=10)
    ax.set_xlim(-6, M.BOX_W + 6); ax.set_ylim(-26, M.BOX_H + 12)
    ax.text(M.BOX_W / 2, M.BOX_H + 6, "SomudTick box: wires, seen from the BACK (lid off; left and right swapped vs. the front)", ha="center", fontsize=9, weight="bold")
    ax.text(0, -6, "HUB = perfboard with 4 rails; every module has its own wires to it (no two-wire joints)   pale = near the face, stronger = behind (z > 15)\n"
            "lanes: behind the big screen (z 16), across the gap under it, then over the clock / PCF8574 (z 28, under the lid); battery wires along the right wall\n"
            "wire colours: red 3V3 / BAT+, black GND, blue SDA, yellow SCL, green buttons, purple stick, orange IR, grey speaker", fontsize=6.5, va="top")
    fig.savefig(outpath, bbox_inches="tight"); plt.close(fig)


# ---- the easy-to-read drawing: 3 panels, one thick line per cable, nothing drawn on top of anything else ----
PANELS = [
    ("1  POWER + I2C (all through the HUB)", CABLE_COL := ["#1565c0", "#00897b", "#6a1b9a", "#ef6c00", "#2e7d32", "#ad1457", "#5d4037"], ["hub > small screen", "hub > IR receiver (power)", "hub > IR LED (power)", "I2C in: screen > hub",
                                             "hub > clock", "hub > button board power", "hub > PCF8574"]),
    ("2  SIGNALS (stick, buttons, IR)", ["#6a1b9a", "#2e7d32", "#ef6c00"], ["stick X / Y", "buttons (7-wire ribbon)", "IR signals"]),
    ("3  BATTERY + SPEAKER", ["#c62828", "#ef6c00", "#616161"], ["battery > switch > screen", "switch > screen", "speaker"]),
]
SHORT = {"ES_I2C": "screen I2C plug", "ES_EXP": "screen EXP plug", "ES_BAT": "screen BAT plug", "ES_SPK": "screen SPK plug",
         "HUB_T": "hub (top edge)", "HUB_B": "hub (bottom edge)", "PCF_IN": "PCF8574 IN", "PCF_P": "PCF8574 P0-P6", "DS_4": "DS3231 4-pin",
         "OLED": "small screen", "IRRX": "IR receiver", "IRTX": "IR LED", "SW": "switch", "HDR": "NA011 yellow pins", "BAT": "18650 (BMS end)", "SPK": "speaker"}

def drawing_easy(outpath):
    import matplotlib; matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
    byname = {b[0]: b for b in B}
    fig, axs = plt.subplots(1, 3, figsize=(19, 13.5), dpi=100)
    X = lambda x: M.BOX_W - x; Y = lambda y: M.BOX_H - y          # seen from the BACK (lid off)
    for ax, (title, cols, names) in zip(axs, PANELS):
        ax.set_aspect("equal"); ax.axis("off")
        ax.add_patch(FancyBboxPatch((0, 0), M.BOX_W, M.BOX_H, boxstyle="round,pad=0,rounding_size=4", fill=False, lw=1.5))
        for n, bx_, by_, w, h, z0, z1, col in M.boxes():
            if n in ("screen glass", "speaker ears", "screen back parts", "speaker foam pad", "USB-C"): continue
            ax.add_patch(Rectangle((X(bx_) - w / 2, Y(by_) - h / 2), w, h, fc="#eceff1", ec="#90a4ae", lw=0.7, zorder=1))
            ax.text(X(bx_), Y(by_ - h / 2) - 1.2, n.replace(" button board", "").replace(" board", ""), ha="center", va="top", fontsize=6.5, color="#546e7a", zorder=2)
        ax.add_patch(Rectangle((X(M.spk_c[0]) - M.SPK[0] / 2, Y(M.spk_c[1]) - M.SPK[1] / 2), M.SPK[0], M.SPK[1], fc="#eceff1", ec="#90a4ae", lw=0.7, zorder=1))
        ax.text(X(M.spk_c[0]), Y(M.spk_c[1]), "speaker", ha="center", va="center", fontsize=6.5, color="#546e7a", zorder=2)
        ax.add_patch(Rectangle((X(hubx) - M.HUB[0] / 2, Y(huby) - M.HUB[1] / 2), *M.HUB[:2], fc="#fff8e1", ec="#9e8a5a", lw=1.2, zorder=3))
        ax.text(X(hubx), Y(huby), "HUB\n3V3 GND\nSDA SCL", ha="center", va="center", fontsize=6.5, weight="bold", zorder=3)
        ax.add_patch(Rectangle((X(hx) - 8, Y(hy) - 2.6), 16, 5.2, fc="#fdd835", ec="k", lw=0.8, zorder=3))
        n_b = len(names); notes = []
        for i, name in enumerate(names):
            b = byname[name]; col = cols[i]
            o = (i - (n_b - 1) / 2) * 3.0                              # every cable gets its own lane, 3 mm apart
            ox = -o if b[2][0][4] == "HUB_B" else o                    # under the hub the lanes fan the other way, so nothing crosses
            paths = []
            for w in b[2]:
                pth = length(b, w)[1]
                if pth not in paths: paths.append(pth)
            for pth in paths:
                ax.plot([X(q[0]) + ox for q in pth], [Y(q[1]) + o for q in pth], color=col, lw=3.0, zorder=5, solid_capstyle="round", solid_joinstyle="round")
                for q in (pth[0], pth[-1]): ax.add_patch(Circle((X(q[0]) + ox, Y(q[1]) + o), 1.3, fc="white", ec=col, lw=1.6, zorder=6))
            pth = paths[0]; k = max(range(len(pth) - 1), key=lambda j: math.dist(pth[j][:2], pth[j + 1][:2]))
            mx, my = (X(pth[k][0]) + X(pth[k + 1][0])) / 2 + ox, (Y(pth[k][1]) + Y(pth[k + 1][1])) / 2 + o
            ids = b[2][0][0] + ("-" + b[2][-1][0] if len(b[2]) > 1 else "")
            ax.text(mx, my, f"{i + 1}", fontsize=7.5, color="white", weight="bold", ha="center", va="center", zorder=8,
                    bbox=dict(boxstyle="circle,pad=0.25", fc=col, ec="white", lw=0.8))
            ends = sorted({(w[4], w[5]) for w in b[2]})
            sig = ", ".join(w[1].split(" >")[0] if ">" in w[1] else w[1] for w in b[2])
            notes.append((col, f"{i + 1}  {ids}  ({len(b[2])} wire{'s' if len(b[2]) > 1 else ''})  " + "; ".join(f"{SHORT[a]} > {SHORT[c]}" for a, c in ends) + f"\n     {sig}"))
        ax.set_xlim(-4, M.BOX_W + 4); ax.set_ylim(-62, M.BOX_H + 10)
        ax.text(M.BOX_W / 2, M.BOX_H + 5, title, ha="center", fontsize=11, weight="bold")
        for j, (col, t) in enumerate(notes):
            ax.text(-2, -6 - j * 7.8, t, fontsize=7, color="#222", va="top", family="monospace")
            ax.add_patch(Rectangle((-4, -6 - j * 7.8 - 2.6), 1.4, 2.6, fc=col, ec="none"))
    fig.suptitle("SomudTick box (layout 2): wires seen from the BACK, lid off (left and right are swapped compared with the front)\n"
                 "one thick line = one cable (2-7 wires together); the circled number matches the list under each panel; cut lengths are in cut_list.md",
                 fontsize=10)
    fig.tight_layout(rect=(0, 0, 1, 0.95)); fig.savefig(outpath, dpi=100); plt.close(fig)

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box4")
    tot = cut_list(os.path.join(out, "cut_list.md")); drawing(os.path.join(out, "wiring_detail.png")); drawing_easy(os.path.join(out, "wiring_box.png"))
    for w in wires3d: w["a"], w["b"] = NAMES[w["a"]], NAMES[w["b"]]
    json.dump(dict(wires=sorted(wires3d, key=lambda w: int(w["id"][1:])), joints=[dict(j=j, t=tx) for j, _, tx in J]), open(os.path.join(out, "box_wires.json"), "w", encoding="utf-8"), ensure_ascii=False)
    for r in rows: print(f"  {r[0]:4s} {NAMES[r[1]]:>18s} -> {NAMES[r[2]]:18s} {r[3]:22s} {r[4]} AWG  cut {r[5]:4d} mm (path {r[6]})")
    print("  total", {k: f"{v / 1000:.2f} m" for k, v in tot.items()})
