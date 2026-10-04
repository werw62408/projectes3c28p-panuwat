# builds index.html: the case parts (STL, embedded) + an assembled view (parts + boxes for the boards and modules)
import base64, json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
CASE = os.path.join(HERE, "..", "..", "case")   # (v13: next to this folder, not a fixed Windows path)
BOX = "out_box3"   # the box layout shown: out_box3 = layout 3, the clear box D384 180 x 100 x 25 (v13.1); out_box2 = layout 2
sys.path.insert(0, CASE); os.chdir(CASE)
import make_case as mc

items = []
def box(name, cx, cy, w, h, z0, z1, col): items.append(dict(k="box", name=name, cx=cx, cy=cy, w=w, h=h, z0=z0, z1=z1, c=col))
def cyl(name, cx, cy, r, z0, z1, col): items.append(dict(k="cyl", name=name, cx=cx, cy=cy, r=r, z0=z0, z1=z1, c=col))
T = mc.T_FACE
# big screen: glass, image, PCB, parts on its back, brass pillars, USB-C
box("glass", mc.SCR_CX, mc.SCR_CY, mc.GLASS_L, mc.GLASS_H, T, T + mc.GLASS_T, "#10161c")
box("screen image", mc.VIS_CX, mc.SCR_CY, mc.VIS_L, mc.VIS_H, T - 0.05, T + 0.2, "#2a6f9e")
box("screen PCB", mc.SCR_CX, mc.SCR_CY, mc.SCR_L, mc.SCR_H, mc.SCR_PCB_FRONT, mc.SCR_PCB_FRONT + mc.SCR_PCB_T, "#1f5f3a")
box("screen back parts", mc.SCR_CX, mc.SCR_CY, mc.SCR_L - 16, mc.SCR_H - 14, mc.SCR_PCB_FRONT + mc.SCR_PCB_T, T + mc.SCR_BACK_PARTS, "#2c3a33")
for x, y in mc.scr_holes(): cyl("brass pillar", x, y, 2.5, mc.SCR_PCB_FRONT + mc.SCR_PCB_T, mc.SCR_PCB_FRONT + mc.SCR_PCB_T + mc.STANDOFF, "#c9a44a")
ux = mc.SCR_CX + mc.USB_SIDE * (mc.SCR_L / 2 + mc.USB_OUT / 2 - 3.5)
box("USB-C", ux, mc.SCR_CY, 7.5 + mc.USB_OUT, 9.0, mc.USB_Z - 1.6, mc.USB_Z + 1.6, "#b8bec4")
# small screen
box("OLED", mc.OLED_CX, mc.OLED_CY, mc.OLED_L, mc.OLED_H, T, T + mc.OLED_T, "#1a3a7a")
box("OLED glass", mc.OLED_CX + mc.OLED_WIN_DX, mc.OLED_CY, mc.OLED_WIN_L + 3, mc.OLED_WIN_H + 2, T - 0.05, T + 1.2, "#0b0f14")
# button board NA011 (joystick, colour buttons, E/F, the yellow pin block)
nx, ny = mc.NA_LEFT + mc.NA_L / 2, mc.NA_TOP - mc.NA_H / 2
box("NA011 board", nx, ny, mc.NA_L, mc.NA_H, mc.NA_PCB_FRONT, mc.NA_PCB_BACK, "#b3262c")
jx, jy = mc.na(mc.NA_JOY); pf = mc.NA_PCB_FRONT; knob_top = mc.NA_PCB_BACK - mc.JOY_TOTAL
box("joystick base", jx, jy, 16, 16, pf - 11, pf, "#2b2b2b")
cyl("joystick stick", jx, jy, 2.5, knob_top + 5, pf - 11, "#555555")
cyl("joystick knob", jx, jy, mc.JOY_KNOB_D / 2, knob_top, knob_top + 5, "#222222")
for (k, p), col in zip(mc.NA_BTN.items(), ["#2d7fd6", "#e8c21e", "#d93b3b", "#3fae4f"]):
    x, y = mc.na(p); box("button base", x, y, 12, 12, pf - 3.5, pf, "#333333"); cyl("button " + k, x, y, mc.BTN_CAP_D / 2, pf - mc.BTN_TOP, pf - 3.5, col)
for k, p in mc.NA_SMALL.items():
    x, y = mc.na(p); box("tact " + k, x, y, 6, 6, pf - 3.5, pf, "#333333"); cyl("stem " + k, x, y, 1.75, pf - mc.SMALL_TOP, pf - 3.5, "#111111")
hx, hy = mc.na((74.0, 6.0)); box("yellow pin block", hx, hy, 15.5, 5.2, pf - mc.NA_HDR_TOP, pf, "#e8b820")
# things inside (the same boxes the fit check uses)
cols = {"battery": "#e07b39", "speaker": "#4a90d9", "DS3231 clock": "#3b8f5e", "PCF8574": "#7a4fb0", "IR board": "#c94c4c", "switch": "#e0c040"}
parts, fixed, posts = mc.inside_parts()
for n, x, y, w, h, z0, z1 in parts:
    if n == "speaker": cyl("speaker", x, y, mc.SPK_D / 2, z0, z1, cols[n]); box("speaker ears", x, y, 5, mc.SPK_EARS, z1 - 1.2, z1, "#2f5f94")
    else: box(n, x, y, w, h, z0, z1, cols[n])
cyl("IR LED", mc.IR_X, mc.IN_H / 2 - 2.5, 2.5, 8.0 - 2.5, 8.0 + 2.5, "#dddddd")
stls = [dict(key="front_shell", dx=0, dy=0, c="#e0662a", shell=True), dict(key="back_plate", dx=0, dy=0, c="#c95a24", back=True)]
for p in mc.NA_BTN.values(): x, y = mc.na(p); stls.append(dict(key="btn_cap", dx=x, dy=y, c="#f2f2f2"))
for p in mc.NA_SMALL.values(): x, y = mc.na(p); stls.append(dict(key="small_cap", dx=x, dy=y, c="#f2f2f2"))
asm = dict(items=items, stls=stls, size=[mc.OUT_W, mc.OUT_H, mc.DEPTH])

out = os.path.join(CASE, "out")
defs = [
  ("na_test", "แผ่นลองปุ่ม", "แผ่นลองปุ่ม (2 แผ่น)", "หน้าเคสเฉพาะส่วนปุ่ม 2 แผ่น รูจอย 26 มม. (บน) กับ 32 มม. (ล่าง) ครอบลงบนบอร์ด NA011 เพื่อลองโยกจอยและดูว่าปุ่มอยู่กลางรูไหม", "หน้าแผ่นคว่ำลงถาด ไม่ต้องซัพพอร์ต ประมาณ 20 นาที"),
  ("front_shell", "หน้าเคส", "หน้าเคส (front shell)", "ตัวเคสด้านหน้า: จอเล็กมุมขวาบน จอใหญ่ รูจอยกับปุ่ม และผนังรอบ", "หน้าเคสคว่ำลงถาด ไม่ต้องซัพพอร์ต"),
  ("back_plate", "ฝาหลัง", "ฝาหลัง (back plate)", "ฝาปิดด้านหลัง: รั้วใส่แบต 48×30 ตะแกรงลำโพง และเสาน็อตยึดจอ", "ด้านนอกลงถาด ไม่ต้องซัพพอร์ต"),
  ("btn_cap", "หมวกปุ่ม", "หมวกปุ่มใหญ่ (×4)", "ครอบปุ่มสี A B C D ใส่จากด้านในเคส", "ด้านกดลงถาด พิมพ์ 4 ชิ้น"),
  ("small_cap", "ปุ่มเล็ก", "ปุ่มเล็ก E/F (×2)", "ก้านกดปุ่มเล็ก E กับ F", "ด้านกดลงถาด พิมพ์ 2 ชิ้น"),
]
data = [{"key": f, "tab": tab, "name": name, "what": what, "print": pr, "file": f + ".stl",
         "stl": base64.b64encode(open(os.path.join(out, f + ".stl"), "rb").read()).decode()} for f, tab, name, what, pr in defs]
t = open(os.path.join(HERE, "template.html"), encoding="utf-8").read()
boxd = json.load(open(os.path.join(CASE, BOX, "box_asm.json"), encoding="utf-8"))
boxd.update(json.load(open(os.path.join(CASE, BOX, "box_wires.json"), encoding="utf-8")))
boxj = json.dumps(boxd, ensure_ascii=False)
t = t.replace("/*PARTS*/[]", json.dumps(data, ensure_ascii=False)).replace("/*ASM*/null", json.dumps(asm)).replace("/*BOX*/null", boxj)
open(os.path.join(HERE, "index.html"), "w", encoding="utf-8").write(t)
print("index.html", len(t), "items", len(items))
