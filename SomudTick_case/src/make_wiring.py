from PIL import Image, ImageDraw, ImageFont
FT = "/usr/share/fonts/opentype/tlwg/Loma.otf"; FB = "/usr/share/fonts/opentype/tlwg/Loma-Bold.otf"
f = lambda n, b=False: ImageFont.truetype(FB if b else FT, n)
W, H = 1440, 1250
im = Image.new("RGB", (W, H), "white"); d = ImageDraw.Draw(im)
INK, SOFT = (28, 36, 32), (100, 112, 106)
C = dict(v33=(214, 40, 40), gnd=(30, 30, 30), sda=(40, 110, 220), scl=(225, 180, 0), vrx=(20, 160, 80), vry=(150, 150, 150),
         sw=(140, 70, 190), ir=(235, 130, 10), bat=(214, 40, 40))
d.text((40, 30), "SomudTick · แผนเดินสาย (บัดกรีทั้งหมด ไม่ใช้บอร์ดทดลอง)", font=f(34, True), fill=INK)
d.text((40, 78), "สีสายเป็นคำแนะนำ ใช้สีอื่นได้ แต่ให้จดไว้ · ทุกอุปกรณ์ใช้ 3.3V ห้ามต่อ 5V", font=f(22), fill=SOFT)

def box(x, y, w, h, title, sub, fill):
    d.rounded_rectangle([x, y, x + w, y + h], 14, fill=fill, outline=INK, width=2)
    d.text((x + 16, y + 12), title, font=f(24, True), fill=INK); d.text((x + 16, y + 44), sub, font=f(18), fill=SOFT)

def pin(x, y, name, side="r"):
    d.ellipse([x - 7, y - 7, x + 7, y + 7], fill="white", outline=INK, width=2)
    if side == "r": d.text((x - 14, y), name, font=f(20, True), fill=INK, anchor="rm")
    else: d.text((x + 14, y), name, font=f(20, True), fill=INK, anchor="lm")
    return (x, y)

# board
box(40, 140, 420, 700, "บอร์ดจอ ES3C28P", "ขั้วต่อด้านซ้ายของบอร์ด (ถือแนวตั้ง)", (236, 242, 238))
d.text((60, 215), "ช่อง I2C", font=f(22, True), fill=INK)
P = {}
for i, (k, n) in enumerate([("v33", "3.3V"), ("gnd", "GND"), ("scl", "IO15 (SCL)"), ("sda", "IO16 (SDA)")]):
    P["b_" + k] = pin(460, 270 + i * 55, n)
d.text((60, 520), "ช่อง Expansion", font=f(22, True), fill=INK)
for i, (k, n) in enumerate([("vrx", "IO2"), ("vry", "IO3"), ("sw", "IO14"), ("ir", "IO21")]):
    P["b_" + k] = pin(460, 575 + i * 55, n)
d.text((60, 910), "แบต + สวิตช์ (ช่อง BAT อยู่ขอบขวาของบอร์ด)", font=f(22, True), fill=INK)

# splice
sx, sy = 700, 300
d.rounded_rectangle([sx - 70, sy - 95, sx + 70, sy + 95], 12, fill=(255, 236, 230), outline=C["v33"], width=3)
d.text((sx, sy - 70), "จุดรวม", font=f(22, True), fill=C["v33"], anchor="mm")
d.text((sx, sy + 78), "หุ้มท่อหด", font=f(17), fill=SOFT, anchor="mm")
P["s_v33"] = (sx, sy - 25); P["s_gnd"] = (sx, sy + 25)
for k, t in (("s_v33", "3.3V"), ("s_gnd", "GND")):
    x, y = P[k]; d.ellipse([x - 12, y - 12, x + 12, y + 12], fill=C["v33" if "v33" in k else "gnd"]); d.text((x - 20, y - 16), t, font=f(16, True), fill=INK, anchor="rm")

# devices
def dev(x, y, h, title, sub, pins, fill):
    box(x, y, 380, h, title, sub, fill)
    out = {}
    for i, (k, n) in enumerate(pins): out[k] = pin(x, y + 100 + i * 50, n, "l")
    return out
DS = dev(1000, 130, 330, "DS3231 (นาฬิกา)", "ใช้แถว 6 ขา · 32K, SQW ไม่ต่อ", [("vcc", "VCC"), ("gnd", "GND"), ("sda", "SDA"), ("scl", "SCL")], (228, 238, 252))
JY = dev(1000, 490, 380, "จอยสติ๊ก KY-023", "ขา +5V ต่อ 3.3V เท่านั้น", [("vcc", "+5V"), ("gnd", "GND"), ("vrx", "VRX"), ("vry", "VRY"), ("sw", "SW")], (232, 244, 236))
KY = dev(1000, 900, 220, "KY-005 (ยิง IR)", "ขากลางไม่ต้องต่อก็ได้", [("s", "S"), ("gnd", "-  (GND)")], (252, 240, 226))

def wire(a, b, col, mid=None, w=6):
    pts = [a] + (mid or []) + [b]
    d.line(pts, fill=col, width=w, joint="curve")
# power from I2C to splice
wire(P["b_v33"], P["s_v33"], C["v33"], [(560, 270), (560, P["s_v33"][1])])
wire(P["b_gnd"], P["s_gnd"], C["gnd"], [(540, 325), (540, P["s_gnd"][1])])
# splice to devices
for dv, x in ((DS, 860), (JY, 880)):
    wire(P["s_v33"], dv["vcc"], C["v33"], [(x, P["s_v33"][1]), (x, dv["vcc"][1])])
for dv, x in ((DS, 900), (JY, 920), (KY, 940)):
    wire(P["s_gnd"], dv["gnd"], C["gnd"], [(x, P["s_gnd"][1]), (x, dv["gnd"][1])])
wire(P["b_sda"], DS["sda"], C["sda"], [(600, 435), (600, 470), (960, 470), (960, DS["sda"][1])])
wire(P["b_scl"], DS["scl"], C["scl"], [(620, 380), (620, 455), (975, 455), (975, DS["scl"][1])])
for k, x in (("vrx", 640), ("vry", 660), ("sw", 680)):
    wire(P["b_" + k], JY[k], C[k], [(x, P["b_" + k][1]), (x, JY[k][1] if False else P["b_" + k][1]), (980 - (680 - x), P["b_" + k][1]), (980 - (680 - x), JY[k][1])])
wire(P["b_ir"], KY["s"], C["ir"], [(700, P["b_ir"][1]), (700, KY["s"][1])])
# battery + switch (own little diagram)
by = 1010
d.rounded_rectangle([60, by - 40, 290, by + 40], 12, fill=(250, 238, 190), outline=INK, width=2)
d.text((175, by), "แบต 3.7V 1500mAh", font=f(20, True), fill=INK, anchor="mm")
d.text((300, by - 28), "+", font=f(24, True), fill=C["bat"]); d.text((300, by + 6), "-", font=f(24, True), fill=INK)
d.rounded_rectangle([380, by - 58, 480, by - 18], 8, fill=(60, 60, 60)); d.text((430, by - 38), "สวิตช์", font=f(19, True), fill="white", anchor="mm")
d.rounded_rectangle([560, by - 45, 680, by + 45], 10, fill=(236, 242, 238), outline=INK, width=2)
d.text((620, by - 62), "ช่อง BAT", font=f(19, True), fill=INK, anchor="mm")
pb = pin(560, by - 20, "+", "l"); pn = pin(560, by + 20, "-", "l")
wire((290, by - 20), (380, by - 38), C["bat"], [(335, by - 20), (335, by - 38)])
wire((480, by - 38), pb, C["bat"], [(520, by - 38), (520, by - 20)])
wire((290, by + 20), pn, C["gnd"])
d.text((60, by + 60), "ตัดสายแดงของแบตแล้วต่อผ่านสวิตช์ 2 ขา · สายดำต่อตรง", font=f(18), fill=SOFT)
# legend
lx, ly = 40, 1160
for i, (k, t) in enumerate([("v33", "3.3V แดง"), ("gnd", "GND ดำ"), ("sda", "SDA ฟ้า"), ("scl", "SCL เหลือง"), ("vrx", "VRX เขียว"), ("vry", "VRY เทา"), ("sw", "SW ม่วง"), ("ir", "IR ส้ม")]):
    x = lx + i * 172
    d.rectangle([x, ly, x + 34, ly + 12], fill=C[k]); d.text((x + 44, ly + 6), t, font=f(19), fill=INK, anchor="lm")
d.text((40, 1205), "เปิดสวิตช์ค้างไว้ตอนชาร์จ ถ้าปิดสวิตช์ บอร์ดจะใช้ไฟ USB ได้แต่แบตจะไม่ถูกชาร์จ", font=f(20), fill=C["v33"])
im.save("out/wiring.png"); print("ok")
