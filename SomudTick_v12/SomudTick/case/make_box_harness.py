"""SomudTick in the ready-made box (make_box.py): where every wire runs, where wires are joined, how long to cut them.

    python make_box_harness.py  -> out_box/wiring_box.png, out_box/cut_list.md, out_box/box_wires.json (3D view)

Every path is measured in 3D through lanes that are free inside the box:
  - just behind the big screen (z 16), in the gap between the screen and the button board,
  - over the tops of the battery / clock / PCF8574 (z 28, under the lid).
Cut length = path + 4 mm stripped at each end + 15 mm spare, rounded up to 5 mm.
The ES3C28P plug places on the back of the board are approximate (maker drawing): check the names printed on the board.
"""
import json, math, os
import make_box as M

cx, cy = M.scr_c
T, SB = cy - M.SCR["h"] / 2, cy + M.SCR["h"] / 2           # top and bottom edge of the big screen (y grows downwards)
ZP = M.FACE + M.SCR["pcb_front"] + M.SCR["pcb_t"] + 1.5    # plugs on the back of the screen board
ZS = 16.0                                                  # lane behind the screen
ZT = M.BODY_D - 4.0                                        # lane over the modules, under the lid
ZM = M.Z_BEHIND + 2.0                                      # solder pads on the clock / PCF8574
ZO = M.FACE + M.OLED["t"] + 0.5                            # back of the small screen
GAPY = (SB + M.na_top) / 2                                 # the gap between the screen and the button board
TOPY = M.OLED_TOP + M.OLED["h"] + 3.0                      # the gap between the small screen and the big screen
hx, hy = M.na(M.HDR)

P = {
    "ES_I2C": (cx - 25, T + 3, ZP), "ES_EXP": (cx - 14, T + 3, ZP), "ES_BAT": (cx - 3, T + 3, ZP), "ES_SPK": (cx + 7, T + 3, ZP),
    "PCF_IN": (M.pcf_c[0] + 15, M.pcf_c[1], ZM), "PCF_OUT": (M.pcf_c[0] - 15, M.pcf_c[1], ZM), "PCF_P": (M.pcf_c[0], M.pcf_c[1] - 7, ZM),
    "DS_4": (M.ds_c[0] - 16, M.ds_c[1], ZM), "DS_6": (M.ds_c[0] + 16, M.ds_c[1], ZM),
    "OLED": (M.oled_c[0], M.OLED_TOP + 2.5, ZO),
    "IRRX": (M.irrx_c[0], M.irrx_c[1] + M.IR_RX[1] / 2 - 2.5, M.FACE + 2.5),
    "IRTX": (M.irtx_c[0], M.irtx_c[1] + M.IR_TX[1] / 2 - 2.5, M.FACE + 2.5),
    "SW": (M.IX0 + M.SW[2] + 0.5, M.sw_y, 9.0),
    "HDR": (hx, hy, M.NA_BACK),
    "BAT": (M.batt_c[0] - M.BATT[0] / 2 + 6, M.batt_c[1] - M.BATT[1] / 2, M.Z_BEHIND + 5),
    "SPK": (M.spk_c[0] + 7, M.spk_c[1], M.FACE + 3),
}
def p(name): return P[name]
LANE1 = P["PCF_IN"][0] - 7.0                               # the I2C-in run keeps off the clock > small screen run

# bundles: (name, label colour, [(wire, signal, colour, AWG, from, to)], path)
B = [
    ("I2C: screen > PCF8574", "#1565c0",
     [("W1", "SDA", "#1e88e5", 28, "ES_I2C", "PCF_IN"), ("W2", "SCL", "#f9a825", 28, "ES_I2C", "PCF_IN"),
      ("W3", "GND", "#212121", 26, "ES_I2C", "PCF_IN"), ("W4", "3V3", "#e53935", 26, "ES_I2C", "PCF_IN")],
     [p("ES_I2C"), (cx - 25, T + 3, ZS), (LANE1, T + 3, ZS), (LANE1, GAPY, ZS), (LANE1, GAPY, ZT),
      (LANE1, P["PCF_IN"][1], ZT), (P["PCF_IN"][0], P["PCF_IN"][1], ZT), p("PCF_IN")]),
    ("I2C: PCF8574 > clock", "#1565c0",
     [("W5", "SDA", "#1e88e5", 28, "PCF_OUT", "DS_4"), ("W6", "SCL", "#f9a825", 28, "PCF_OUT", "DS_4"),
      ("W7", "GND", "#212121", 28, "PCF_OUT", "DS_4"), ("W8", "3V3", "#e53935", 28, "PCF_OUT", "DS_4")],
     [p("PCF_OUT"), (P["PCF_OUT"][0], P["PCF_OUT"][1], ZT), (P["DS_4"][0], P["DS_4"][1], ZT), p("DS_4")]),
    ("I2C: clock > small screen", "#1565c0",
     [("W9", "SDA", "#1e88e5", 28, "DS_6", "OLED"), ("W10", "SCL", "#f9a825", 28, "DS_6", "OLED"),
      ("W11", "GND", "#212121", 28, "DS_6", "OLED"), ("W12", "3V3", "#e53935", 28, "DS_6", "OLED")],
     [p("DS_6"), (P["DS_6"][0], P["DS_6"][1], ZT), (P["DS_6"][0], GAPY, ZT), (P["DS_6"][0], GAPY, ZS), (P["DS_6"][0], TOPY, ZS),
      (P["OLED"][0], TOPY, ZS), (P["OLED"][0], TOPY, ZO + 1), (P["OLED"][0], P["OLED"][1], ZO + 1), p("OLED")]),
    ("IR power: small screen > IR receiver", "#e65100",
     [("W13", "3V3", "#e53935", 28, "OLED", "IRRX"), ("W14", "GND", "#212121", 28, "OLED", "IRRX")],
     [p("OLED"), (P["OLED"][0], P["OLED"][1], ZO + 2), (P["OLED"][0], TOPY, ZO + 2), (P["IRRX"][0], TOPY, ZO + 2), (P["IRRX"][0], P["IRRX"][1], ZO + 2), p("IRRX")]),
    ("IR power: receiver > IR LED", "#e65100",
     [("W15", "3V3", "#e53935", 28, "IRRX", "IRTX"), ("W16", "GND", "#212121", 28, "IRRX", "IRTX")],
     [p("IRRX"), (P["IRRX"][0], P["IRRX"][1], ZO + 2), (P["IRTX"][0], P["IRRX"][1], ZO + 2), p("IRTX")]),
    ("IR signals", "#e65100",
     [("W17", "IO21 > IR LED S", "#fb8c00", 28, "ES_EXP", "IRTX"), ("W18", "IO14 > IR receiver S", "#ffb74d", 28, "ES_EXP", "IRRX")],
     [p("ES_EXP"), (P["ES_EXP"][0], TOPY, ZP), (P["IRTX"][0] + 3, TOPY, ZP), (P["IRTX"][0] + 3, P["IRTX"][1], ZO + 2), p("IRTX")]),
    ("stick X / Y", "#6a1b9a",
     [("W19", "IO2 > X", "#8e24aa", 28, "ES_EXP", "HDR"), ("W20", "IO3 > Y", "#6a1b9a", 28, "ES_EXP", "HDR")],
     [p("ES_EXP"), (P["ES_EXP"][0], T + 3, ZS), (P["ES_EXP"][0], GAPY, ZS), (hx, GAPY, ZS), (hx, hy, ZS), p("HDR")]),
    ("buttons (7-wire ribbon)", "#2e7d32",
     [(f"W{21 + k}", f"P{k} > {n}", "#43a047", 28, "PCF_P", "HDR") for k, n in enumerate("ABCDEFK")],
     [p("PCF_P"), (P["PCF_P"][0], P["PCF_P"][1], ZT), (P["PCF_P"][0], hy + 4, ZT), (hx, hy + 4, ZT), (hx, hy + 4, M.NA_BACK + 1), p("HDR")]),
    ("button board power", "#b71c1c",
     [("W28", "3V3 > 3V", "#e53935", 26, "PCF_OUT", "HDR"), ("W29", "GND > G", "#212121", 26, "PCF_OUT", "HDR")],
     [p("PCF_OUT"), (P["PCF_OUT"][0], P["PCF_OUT"][1], ZT), (P["PCF_OUT"][0], hy + 4, ZT), (hx - 4, hy + 4, ZT), (hx - 4, hy + 4, M.NA_BACK + 1), (hx - 4, hy, M.NA_BACK)]),
    ("battery > switch > screen", "#c62828",
     [("W30", "BAT + > switch", "#c62828", 24, "BAT", "SW"), ("W32", "BAT -", "#424242", 24, "BAT", "ES_BAT")],
     [p("BAT"), (P["BAT"][0], P["BAT"][1], ZT), (P["BAT"][0], GAPY, ZT), (P["BAT"][0], GAPY, ZS), (P["BAT"][0], T - 1, ZS),
      (P["ES_BAT"][0], T - 1, ZS), (P["ES_BAT"][0], T + 3, ZS), p("ES_BAT")]),
    ("switch > screen", "#c62828",
     [("W31", "switch > BAT + (plug)", "#c62828", 24, "SW", "ES_BAT")],
     [p("SW"), (P["SW"][0], P["SW"][1], 12), (P["SW"][0], TOPY, 12), (P["ES_BAT"][0], TOPY, 12), (P["ES_BAT"][0], T + 3, ZP), p("ES_BAT")]),
    ("speaker", "#616161",
     [("W33", "SPK +", "#9e9e9e", 26, "ES_SPK", "SPK"), ("W34", "SPK -", "#616161", 26, "ES_SPK", "SPK")],
     [p("ES_SPK"), (P["ES_SPK"][0], T + 3, ZS), (P["ES_SPK"][0], GAPY, ZS), (P["ES_SPK"][0], GAPY, ZT), (P["SPK"][0], GAPY, ZT),
      (P["SPK"][0], M.na_top + M.NA["h"] + 1.5, ZT), (P["SPK"][0], M.na_top + M.NA["h"] + 1.5, M.FACE + 4), p("SPK")]),
]
# W30 does not end at the plug: from the top lane it turns off to the switch
W30_TAIL = [(P["BAT"][0], T - 1, ZS), (P["SW"][0] + 1, T - 1, ZS), (P["SW"][0] + 1, P["SW"][1], 12), p("SW")]
J = [("J1", "PCF_OUT", "PCF8574 OUT VCC: W8 (to the clock) + W28 (to NA011 3V)"),
     ("J2", "PCF_OUT", "PCF8574 OUT GND: W7 (to the clock) + W29 (to NA011 G)"),
     ("J3", "OLED", "small screen VCC: W12 (from the clock) + W13 (to the IR receiver)"),
     ("J4", "OLED", "small screen GND: W11 (from the clock) + W14 (to the IR receiver)"),
     ("J5", "IRRX", "IR receiver +: W13 (in) + W15 (on to the IR LED)"),
     ("J6", "IRRX", "IR receiver -: W14 (in) + W16 (on to the IR LED)")]
S = [("S1", "ES_I2C", "I2C plug lead x4 <-> W1-W4"), ("S2", "ES_EXP", "expansion plug lead: IO21 W17, IO14 W18, IO2 W19, IO3 W20"),
     ("S3", "ES_BAT", "battery plug lead: + W31, - W32"), ("S4", "ES_SPK", "speaker plug lead: W33 / W34")]

def plen(path): return sum(math.dist(path[i], path[i + 1]) for i in range(len(path) - 1))
def cut(mm): return int(math.ceil((mm + 8 + 15) / 5.0) * 5)
def length(bundle, w):
    path = bundle[3]
    if w[0] == "W30": i = path.index(W30_TAIL[0]); path = path[:i + 1] + W30_TAIL[1:]
    if w[0] == "W18": path = bundle[3][:-2] + [(P["IRRX"][0] - 3, TOPY, ZP), (P["IRRX"][0] - 3, P["IRRX"][1], ZO + 2), p("IRRX")]
    return plen(path), path

rows, wires3d = [], []
for b in B:
    for w in b[2]:
        L, path = length(b, w)
        rows.append((w[0], w[4], w[5], w[1], w[3], cut(L), round(L)))
        wires3d.append(dict(id=w[0], c=w[2], awg=w[3], sig=w[1], a=w[4], b=w[5], cut=cut(L), pts=[[x - M.BOX_W / 2, M.BOX_H / 2 - y, z] for x, y, z in path]))
rows.sort(key=lambda r: int(r[0][1:]))
NAMES = {"ES_I2C": "จอใหญ่ ปลั๊ก I2C", "ES_EXP": "จอใหญ่ ปลั๊กขยาย", "ES_BAT": "จอใหญ่ ปลั๊กแบต", "ES_SPK": "จอใหญ่ ปลั๊กลำโพง",
         "PCF_IN": "PCF8574 ขา IN", "PCF_OUT": "PCF8574 ขา OUT", "PCF_P": "PCF8574 ขา P0-P6", "DS_4": "DS3231 หัว 4 ขา", "DS_6": "DS3231 หัว 6 ขา",
         "OLED": "จอเล็ก", "IRRX": "ตัวรับ IR (KY-022)", "IRTX": "หลอดส่ง IR (KY-005)", "SW": "สวิตช์", "HDR": "NA011 ขาเหลือง",
         "BAT": "แบต", "SPK": "ลำโพง"}

def cut_list(path):
    L = ["# SomudTick กล่องสำเร็จรูป: ตัดสาย", "",
         "ความยาว = ทางเดินสายจริงในกล่อง (วัดแบบ 3 มิติ) + ปลอกปลายละ 4 มม. + เผื่อ 15 มม. ปัดขึ้นทีละ 5 มม.", "",
         "| สาย | จาก → ถึง | สัญญาณ | ขนาด | **ตัดยาว (มม.)** | ทางจริง |", "|---|---|---|---|---|---|"]
    tot = {}
    for wid, a, b, sig, awg, c, raw in rows:
        L.append(f"| {wid} | {NAMES[a]} → {NAMES[b]} | {sig} | {awg} AWG | **{c}** | {raw} |"); tot[awg] = tot.get(awg, 0) + c
    L += ["", "**รวม:** " + " · ".join(f"{awg} AWG {v / 1000:.2f} ม." for awg, v in sorted(tot.items())) + f" · ทั้งหมด {sum(tot.values()) / 1000:.2f} ม.", "",
          "**จุดร่วม (บัดกรีสาย 2 เส้นลงขาเดียวกัน):**"] + [f"- **{j}** {t}" for j, _, t in J] + \
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
    ax.add_patch(Circle((X(M.spk_c[0]), Y(M.spk_c[1])), M.SPK_D / 2, fc="#90caf9", ec="#444", lw=0.6, alpha=0.4)); ax.text(X(M.spk_c[0]), Y(M.spk_c[1]), "speaker", ha="center", fontsize=6)
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
    ax.text(0, -6, "red J = 2 wires soldered into one pin (see cut_list.md)   pale = near the face, stronger = behind (z > 15)\n"
            "lanes: behind the big screen (z 16), across the gap under it, then over the battery / clock / PCF8574 (z 28, under the lid)\n"
            "wire colours: red 3V3 / BAT+, black GND, blue SDA, yellow SCL, green buttons, purple stick, orange IR, grey speaker", fontsize=6.5, va="top")
    fig.savefig(outpath, bbox_inches="tight"); plt.close(fig)

if __name__ == "__main__":
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "out_box")
    tot = cut_list(os.path.join(out, "cut_list.md")); drawing(os.path.join(out, "wiring_box.png"))
    for w in wires3d: w["a"], w["b"] = NAMES[w["a"]], NAMES[w["b"]]
    json.dump(dict(wires=sorted(wires3d, key=lambda w: int(w["id"][1:])), joints=[dict(j=j, t=tx) for j, _, tx in J]), open(os.path.join(out, "box_wires.json"), "w", encoding="utf-8"), ensure_ascii=False)
    for r in rows: print(f"  {r[0]:4s} {NAMES[r[1]]:>18s} -> {NAMES[r[2]]:18s} {r[3]:22s} {r[4]} AWG  cut {r[5]:4d} mm (path {r[6]})")
    print("  total", {k: f"{v / 1000:.2f} m" for k, v in tot.items()})
