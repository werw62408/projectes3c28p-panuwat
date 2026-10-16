"""3D page for the DS-B case: the DS template with the small screen moved to the base face.
   python3 build_b.py && python3 make_viewer_b.py   -> out_b/somudtick_dsb_3d.html (+ artifact copy)"""
import base64, os, json
d = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(d, "out_b")
meta = json.load(open(os.path.join(out, "meta.json")))
b = base64.b64encode(open(os.path.join(out, "somudtick_ds.glb"), "rb").read()).decode()
t = open(os.path.join(d, "viewer_ds_tpl.html")).read()

def rep(a, c, count=1):
    global t
    assert a in t, a[:60]
    t = t.replace(a, c, count)

rep("ES3C28P · Joystick Shield · 2 screens", "ES3C28P · NA011 · 18650 · 2 screens")
rep('กล่องพิมพ์ 3D แบบฝาพับเหมือน Nintendo DS จอใหญ่อยู่ฝาบน ปุ่มกับจอยอยู่ตัวล่าง ปิดฝาแล้วเห็นเวลาที่จอเล็กด้านหลัง ทุกชิ้นวางตามขนาดจริง',
    'กล่องพิมพ์ 3D แบบฝาพับ จอใหญ่อยู่ฝาบน จอเล็ก 0.96" อยู่ตัวล่างระหว่างจอยกับปุ่ม บอร์ดปุ่มวางลึก หัวจอยโผล่ %g มม. ปุ่มใช้หมวกพิมพ์ ทุกชิ้นวางตามขนาดจริง' % meta["KNOB_OUT"])
# printed parts table
rep('base_shell ตัวล่าง (หน้าปุ่ม)<br>', 'base_shell ตัวล่าง (หน้าปุ่ม + ช่องจอเล็ก)<br>')
rep('lid_shell ตัวบน (หน้าจอ)<br>', 'lid_shell ตัวบน (หน้าจอ + หลุมรับหัวจอย)<br>')
rep('lid_back ฝาหลัง + ช่อง OLED + โลโก้', 'lid_back ฝาหลัง + โลโก้')
rep('btn_cap ×4 หมวกปุ่ม A B C D (พิมพ์สี)', 'btn_cap ×4 หมวกปุ่ม A B C D (สูง %g วางบนปุ่มสีเดิม)' % meta["CAP_H"])
rep('small_cap ×2 แกนปุ่ม E F', 'small_cap ×2 แกนปุ่ม E F (ยาว %g)' % meta["PL_H"])
# measure table
rep('<tr><td>ความสูงจอย: ใต้แผงถึงยอดหัวจอย</td><td class="v">33 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>ความสูงจอย: บนแผงถึงยอดหัวจอย</td><td class="v">30 <span class="est">ประมาณ</span></td></tr>')
rep('<tr><td>ความสูงปุ่มใหญ่: บนแผงถึงยอดหมวกสี</td><td class="v">15 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>ความสูงปุ่มใหญ่: บนแผงถึงยอดหมวกสี</td><td class="v">14 <span class="est">ประมาณ</span></td></tr>')
rep('<tr><td>ตำแหน่งจอยและปุ่มบนแผง</td><td class="v">จากรูปร้าน <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>ตำแหน่งจอยและปุ่มบนแผง NA011</td><td class="v">จากรูปที่ส่งมา <span class="est">ประมาณ</span></td></tr>')
rep('<tr><td>จอ OLED 0.91" (ทั้งแผ่น)</td><td class="v">38×12 <span class="est">ประมาณ</span></td></tr>',
    '<tr><td>จอ OLED 0.96" (ทั้งแผ่น) ยังไม่ได้ซื้อ</td><td class="v">27.3×27.8 <span class="est">ประมาณ</span></td></tr>'
    '<tr><td>ขาใต้บอร์ด NA011 ต้องตัดให้เหลือ</td><td class="v">≤ 3 มม.</td></tr>')
# part names and colours for this case
extra = """
Object.assign(INFO, {
  base_shell:["ตัวล่าง (หน้าปุ่ม)","พิมพ์ · รูจอย Ø31.5 (จอยโยกได้ 30°) · ช่องจอเล็ก · ขอบยก 1 มม."],
  base_floor:["ฝาใต้","พิมพ์ · ตะแกรงลำโพง · น็อต M2.5 4 ตัว + M3 2 ตัวผ่านบอร์ดปุ่ม"],
  lid_shell:["ตัวบน (หน้าจอ)","พิมพ์ · ช่องจอ 60×45 · หลุมรับหัวจอยตอนปิดฝา"],
  lid_back:["ฝาหลัง","พิมพ์ · โลโก้ SOMUDTICK · รู BOOT / RESET"],
  shield:["บอร์ดปุ่ม NA011 87×53","วางลึก: บนแผงอยู่ใต้หน้ากล่อง 21 มม. · ไฟ 3V3 ที่แผ่น 3 กับ G (ห้าม V)"],
  stick_cap:["หัวจอย","โผล่จากหน้ากล่อง %g มม. · ปิดฝาแล้วเข้าหลุมในฝาบน"],
  cap_b:["ปุ่มสีเดิม",""], cap_y:["ปุ่มสีเดิม",""], cap_r:["ปุ่มสีเดิม",""], cap_g:["ปุ่มสีเดิม",""],
  print_cap_b:["หมวกปุ่ม A (พิมพ์)","วางบนปุ่มสีเดิม · โผล่ 0.6 มม."], print_cap_y:["หมวกปุ่ม B (พิมพ์)",""], print_cap_r:["หมวกปุ่ม C (พิมพ์)",""], print_cap_g:["หมวกปุ่ม D (พิมพ์)",""],
  print_small:["แกนปุ่ม E / F (พิมพ์)","Ø4.8 · มีปลอกนำในตัวกล่อง"],
  header:["ขาเหลืองบน NA011",""],
  oled_pcb:["จอเล็ก 0.96\\" SSD1306","I2C 0x3C · ถอดขาออก บัดกรีสายตรง · แปะใต้หน้ากล่อง"], oled_glass:["จอเล็ก 0.96\\"","128×64"],
  battery:["แบต 18650 + BMS","72×24×24 · ปลายสายไปทางสวิตช์ (ด้านหน้า) · บัดกรี"],
  speaker:["ลำโพง 40×28×10","หันลงตะแกรงฝาใต้ · เสียบช่อง SPEAKER ของจอ"],
  pcf8574:["PCF8574","0x20 · จัมเปอร์ฝั่ง − · ถอดขา · เทปโฟมบนลำโพง"],
  ky005:["ส่ง IR KY-005","IO21 · หลอดชี้ออกขอบบานพับ"], led:["หลอด IR","ยิงออกด้านบานพับ (ชี้ไปที่แอร์)"],
  ky022:["รับ IR KY-022","IO14 · ตัวรับมองออกช่องขอบบานพับ"], ir_rx:["ตัวรับ IR",""],
  hub:["แผงรวมสาย 30×20","3V3 · GND · SDA · SCL"],
  switch:["สวิตช์เปิดปิด","ผนังหน้า ตัวล่าง ข้างปลายสายแบต"], switch_rocker:["สวิตช์เปิดปิด",""],
  ds3231:["นาฬิกา DS3231","ฝาบนใต้จอ · ถอด R4 หรือใช้ LIR2032"], coin:["ถ่านกระดุม",""],
  pcb:["บอร์ด ES3C28P","USB-C ผนังขวาฝาบน · การ์ด SD ขอบบานพับ"], glass:["จอ 2.8\\" 320×240 (แนวนอน)",""],
  bolt:["น็อต M3","บานพับ / ยึดบอร์ดปุ่ม"]
});
Object.assign(COL, { cap_r:[0xd93b3b,.5,0], cap_g:[0x3fae4f,.5,0], print_cap_r:[0xd93b3b,.4,0], print_cap_g:[0x3fae4f,.4,0],
  shield:[0xb3262c,.7,0], header:[0xf2c230,.6,0], ky022:[0x111111,.7,0], ir_rx:[0x222222,.4,0], hub:[0x2a6b3a,.6,0] });
""" % meta["KNOB_OUT"]
rep("const $ = id => document.getElementById(id);", extra + "const $ = id => document.getElementById(id);")
# the small screen picture: 128x64 look, on the base face
rep("const oledTex = screenTex(512, Math.round(512 * OH_ / OW_), (g, w, h) => {   // 0.91\" 128x32 look: time + today on one line",
    """const oledTex = screenTex(512, Math.round(512 * OH_ / OW_), (g, w, h) => {
  if (META.KIND === "b") { g.fillStyle = "#000"; g.fillRect(0, 0, w, h); g.fillStyle = "#e8f4ff"; g.textBaseline = "middle";
    g.font = "700 " + Math.round(h * 0.42) + "px IBM Plex Mono, monospace"; g.fillText("20:15", w * 0.04, h * 0.32);
    g.font = "500 " + Math.round(h * 0.2) + "px IBM Plex Mono, monospace"; g.textAlign = "right"; g.fillText("65%", w * 0.96, h * 0.2); g.fillText("Fri 16", w * 0.96, h * 0.44);
    g.textAlign = "left"; g.fillText("Water 5/8   Log", w * 0.04, h * 0.78); g.fillRect(0, h * 0.6, w, 2); return; }""")
rep("  addScreenPlane(oledTex, META.OLED_WIN[0], META.OLED_WIN[1], META.OLED_WIN[2], META.OLED_WIN[3], META.BH + META.OLED_Y, true);",
    """  if (META.KIND === "b") { const m = new THREE.Mesh(new THREE.PlaneGeometry(META.OLED_WIN[1] - META.OLED_WIN[0], META.OLED_WIN[3] - META.OLED_WIN[2]), new THREE.MeshBasicMaterial({map: oledTex, toneMapped:false}));
    m.rotation.x = -Math.PI / 2; m.position.set((META.OLED_WIN[0] + META.OLED_WIN[1]) / 2 - META.OW / 2, META.OLED_BASE_Y, (META.OLED_WIN[2] + META.OLED_WIN[3]) / 2 - META.OD / 2); baseG.add(m); }
  else addScreenPlane(oledTex, META.OLED_WIN[0], META.OLED_WIN[1], META.OLED_WIN[2], META.OLED_WIN[3], META.BH + META.OLED_Y, true);""")
t = t.replace("__GLB__", b).replace("__META__", json.dumps(meta)).replace("__NAME__", meta["NAME"]).replace("__H1__", "SomudTick <b>DS-B</b>")
fn = os.path.join(out, "somudtick_dsb_3d.html")
open(fn, "w").write(t)
open(os.path.join(out, "_preview.html"), "w").write('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + t + '</body></html>')
print(os.path.basename(fn), len(t) // 1024, "KB")
