"""3D page for the GB3 cases.  python3 make_viewer_gb3.py na|diy  -> out_gb3_<v>/somudtick_gb3_<v>_3d.html"""
import base64, os, json, sys
v = sys.argv[1]
d = os.path.dirname(os.path.abspath(__file__)); out = os.path.join(d, "out_gb3_" + v)
meta = json.load(open(os.path.join(out, "meta.json")))
b = base64.b64encode(open(os.path.join(out, "somudtick_ds.glb"), "rb").read()).decode()
t = open(os.path.join(d, "viewer_ds_tpl.html")).read()
def rep(a, c):
    global t
    assert a in t, a[:60]; t = t.replace(a, c, 1)
board = "บอร์ดปุ่ม NA011" if v == "na" else "จอยกับปุ่มบนแผ่นปริ๊นท์ 80×42 ที่ทำเอง"
rep("ES3C28P · Joystick Shield · 2 screens", "ES3C28P · " + ("NA011" if v == "na" else "DIY perfboard") + " · 18650")
rep('$("m-sub").textContent = "กล่องพิมพ์ 3D ทรง Game Boy ชิ้นเดียว ไม่มีบานพับ จอใหญ่หมุนข้างอยู่ด้านบน แผงจอยแดงทั้งแผ่นอยู่ด้านล่าง จอเล็กกับลำโพงอยู่ด้านหลัง ทุกชิ้นวางตามขนาดจริง";',
    '$("m-sub").textContent = "ทรง Game Boy ชิ้นเดียว เล็กที่สุดที่ของชุดนี้ใส่ได้ จอใหญ่บน ' + board + 'ล่าง แบต 18650 นอนขวางท้ายเครื่อง สวิตช์ IR และการ์ด SD อยู่ขอบบน จอเล็กกับลำโพงอยู่ด้านหลัง ข้างในมีแผ่นนูนกั้นล็อกบอร์ดทุกตัว";')
rep('<tr><td>ความสูงจอย: ใต้แผงถึงยอดหัวจอย</td><td class="v">33 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>ความสูงจอย: บนแผงถึงยอดหัวจอย</td><td class="v">30 <span class="est">ประมาณ</span></td></tr>')
rep('<tr><td>ความสูงปุ่มใหญ่: บนแผงถึงยอดหมวกสี</td><td class="v">15 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>ความสูงปุ่มใหญ่: บนแผงถึงยอดหมวกสี</td><td class="v">14 <span class="est">ประมาณ</span></td></tr>')
rep('<tr><td>จอ OLED 0.91" (ทั้งแผ่น)</td><td class="v">38×12 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>จอเล็ก 1.3" (ทั้งแผ่น)</td><td class="v">35.5×33.5×3.7 ✓</td></tr><tr><td>ขาใต้บอร์ดปุ่มต้องตัดให้เหลือ</td><td class="v">≤ 2 มม.</td></tr>')
extra = """
Object.assign(INFO, {
  front_shell:["ตัวหน้า","พิมพ์ · ช่องจอ · รูจอย Ø26 · แผ่นนูนล็อกมุมบอร์ดจอและบอร์ดปุ่ม · เสายึดบอร์ดปุ่ม"],
  back_plate:["ฝาหลัง","พิมพ์ · ช่องจอเล็ก · ตะแกรงลำโพง · ร่องรับแบต · ขอบกั้นลำโพง DS3231 PCF8574 IR"],
  shield:["บอร์ดปุ่ม NA011 87×53","ล็อกด้วยแผ่นนูน 4 มุม + น็อต 2 ตัวเข้าเสาหน้า · ไฟ 3V3 ที่แผ่น 3 กับ G (ห้าม V)"],
  board:["แผ่นปริ๊นท์ 80×42 (ทำเอง)","จอย PS2 + ปุ่ม 12×12 ×4 + ปุ่ม 6×6 ×2 · เป็นแผงรวมสายในตัว · เจาะรู 2.5 มม. 2 มุม"],
  stick_cap:["หัวจอย","โผล่ 13 มม. · โยกได้ 30° ไม่ชนขอบ"], header:["ขาเหลืองบน NA011",""], joints:["จุดบัดกรีใต้บอร์ด","ตัดขาให้เหลือ ≤ 2 มม."],
  cap_b:["ปุ่มสีเดิม",""], cap_y:["ปุ่มสีเดิม",""], cap_r:["ปุ่มสีเดิม",""], cap_g:["ปุ่มสีเดิม",""],
  print_cap_b:["หมวกปุ่ม A (พิมพ์)","สูง %(cap)g วางบนปุ่มสีเดิม"], print_cap_y:["หมวกปุ่ม B (พิมพ์)",""], print_cap_r:["หมวกปุ่ม C (พิมพ์)",""], print_cap_g:["หมวกปุ่ม D (พิมพ์)",""],
  print_small:["แกนปุ่ม E / F (พิมพ์)","ยาว %(pl)g มีปลอกนำ"],
  battery:["แบต 18650 + BMS","นอนในร่องโค้งฝาหลัง · ปลายสายไปทางขวา · บัดกรี"],
  speaker:["ลำโพง 40×28×10","หันออกตะแกรงหลัง · เสียบช่อง SPEAKER"], ds3231:["นาฬิกา DS3231","หลังจอ · ถอด R4 หรือใช้ LIR2032"], coin:["ถ่านกระดุม",""],
  oled_pcb:["จอเล็ก 1.3\\" SH1106","ฝาหลัง (มองเห็นตอนคว่ำเครื่อง)"], oled_glass:["จอเล็ก 1.3\\"",""],
  pcf8574:["PCF8574","0x20 · จัมเปอร์ฝั่ง −"], hub:["แผงรวมสาย 30×20",""],
  ky005:["ส่ง IR KY-005","IO21 · หลอดออกขอบบน (ชี้แอร์)"], ky022:["รับ IR KY-022","IO14 · ช่องขอบบน"], ir_rx:["ตัวรับ IR",""], led:["หลอด IR",""],
  switch:["สวิตช์เปิดปิด","ขอบบนแบบ Game Boy"], switch_rocker:["สวิตช์เปิดปิด",""],
  pcb:["บอร์ด ES3C28P","USB-C ผนังซ้าย · การ์ด SD เสียบจากขอบบน"], glass:["จอ 2.8\\"",""]
});
Object.assign(COL, { cap_r:[0xd93b3b,.5,0], cap_g:[0x3fae4f,.5,0], print_cap_r:[0xd93b3b,.4,0], print_cap_g:[0x3fae4f,.4,0],
  shield:[0xb3262c,.7,0], board:[0x2f8a3a,.7,0], header:[0xf2c230,.6,0], joints:[0x9aa0a8,.4,.6], ky022:[0x111111,.7,0], ir_rx:[0x222222,.4,0], hub:[0x2a6b3a,.6,0] });
""" % {"cap": meta["CAP_H"], "pl": meta["PL_H"]}
rep("const $ = id => document.getElementById(id);", extra + "const $ = id => document.getElementById(id);")
rep("const oledTex = screenTex(512, Math.round(512 * OH_ / OW_), (g, w, h) => {   // 0.91\" 128x32 look: time + today on one line",
    """const oledTex = screenTex(512, Math.round(512 * OH_ / OW_), (g, w, h) => {
  { g.fillStyle = "#000"; g.fillRect(0, 0, w, h); g.fillStyle = "#e8f4ff"; g.textBaseline = "middle";
    g.font = "700 " + Math.round(h * 0.42) + "px IBM Plex Mono, monospace"; g.fillText("20:15", w * 0.04, h * 0.32);
    g.font = "500 " + Math.round(h * 0.2) + "px IBM Plex Mono, monospace"; g.textAlign = "right"; g.fillText("65%", w * 0.96, h * 0.2); g.fillText("Sat 17", w * 0.96, h * 0.44);
    g.textAlign = "left"; g.fillText("Water 5/8   Log", w * 0.04, h * 0.78); g.fillRect(0, h * 0.6, w, 2); return; }""")
t = t.replace("__GLB__", b).replace("__META__", json.dumps(meta)).replace("__NAME__", meta["NAME"]).replace("__H1__", "SomudTick <b>GB3</b> " + v.upper())
fn = os.path.join(out, "somudtick_gb3_%s_3d.html" % v)
open(fn, "w").write(t)
open(os.path.join(out, "_preview.html"), "w").write('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + t + '</body></html>')
print(os.path.basename(fn), len(t) // 1024, "KB")
