"""SomudTick DS-B: clamshell like the photo the owner sent (16 Oct 2026).

  LID  (top half):    the 2.8" ES3C28P screen (landscape, USB-C to the right wall), the DS3231 clock.
  BASE (bottom half): the NA011 button board set DEEP (stick 9 mm out, printed caps over the buttons),
                      a 0.96" OLED between the stick and the buttons, 18650 battery standing on the right,
                      speaker, PCF8574, hub, both IR boards (looking out of the hinge edge), power switch.

Coordinates (both halves, looking at the open case from the player):
  x = 0 at the left outer edge, -> right
  d = distance from the hinge outer edge, -> toward the player
  z = height. BASE: 0 = outer bottom, BH = the face with the buttons.  LID: 0 = the screen face, LH = the back.

Parts measured by the owner (grey box, Oct 2026) unless marked EST.
"""
NAME = "SomudTick DS-B"
OW, OD = 124.0, 87.0          # footprint of each half
R_CORNER = 7.0
CHAMFER = 1.2
WALL = 2.2
PLATE = 2.0

# ---------------- BASE ----------------
BH = 29.0                     # outer height of the base (face top)
RIM_H = 1.0                   # raised edge: the closed lid rests on it
FACE_TOP = BH
FACE_BOT = BH - PLATE         # 27

# NA011 Funduino Joystick Shield V1.A, 87 x 53, its top edge (pads V A C E K X, yellow header) toward the hinge.
# Positions on the board: x from its left edge, y from its top edge (measured on the owner's board photo).
NA_W, NA_H, NA_T = 87.0, 53.0, 1.5
NA_X0, NA_D0 = 2.9, 31.2                  # where the board sits in the base
NA_TOP_Z = 8.0                            # top of the board (component side). Deep: the stick stands 9 mm out
JOY = (18.5, 30.9); KNOB_D, JOY_TOTAL = 20.0, 30.0       # knob top = board top + 30
BTN = {"A": (65.0, 18.4), "B": (77.1, 29.9), "C": (65.0, 42.0), "D": (53.1, 29.9)}
BTN_COL = {"A": "b", "B": "y", "C": "r", "D": "g"}
CAP_D, BTN_TOP = 11.5, 14.0               # coloured caps: diameter, board top -> cap top
SMALL = {"E": (47.5, 41.6), "F": (34.5, 41.6)}; SMALL_TOP = 5.0
NA_HOLES = [(4.2, 2.5), (82.8, 17.5)]     # M3: a screw from under the floor clamps floor + board + face post
JOY_HOLE_D = 31.5                         # the knob tilts into this hole (chamfered)
BTN_HOLE_D = 10.8; EXT_D = 10.0; EXT_FOOT_D = 11.4
SMALL_HOLE_D = 5.6; PLUNGER_D = 4.8; SLEEVE_Z0 = 19.0
CAP_UP = 0.6                              # printed caps stand this much above the face (rim is 1.0)

# 0.96" OLED (SSD1306 128x64, I2C 0x3C), module 27.3 x 27.8 (EST: measure yours), pins edge toward the hinge,
# pin header taken off (wires soldered on). Glued under the face plate.
OLED = (36.4, 15.4, 63.7, 43.2)           # x0, d0, x1, d1 (between the stick hole and the A/D buttons)
OLED_T = 2.7                              # board 1.2 + glass 1.5
OLED_GLASS = (26.7, 19.3, 4.6)            # glass w, h, distance from the pin edge (EST)
OLED_WIN = (23.0, 12.4, 12.1)             # window w, h, centre distance from the pin edge (lit area 21.7 x 10.9, EST)

# the strip between the hinge wall and the board (d 2.2 .. 31.2)
SPK = (9.5, 2.7, 49.5, 30.7, 2.0, 12.0)            # oval speaker 40 x 28 x 10, cone down to the floor grille
PCF = (12.0, 7.0, 47.0, 26.0, 12.5, 17.5)          # PCF8574 35 x 19, pin headers off, on foam over the speaker
KY005 = (52.0, 3.6, 70.0, 18.6, 2.0, 5.5)          # IR LED board 18 x 15 lying flat, LED out of the hinge wall
IR_LED = (61.0, 5.6); IR_LED_D = 5.4               # x, z of its hole
KY022 = (73.0, 2.7, 88.0, 21.2, 2.0, 4.0)          # IR receiver board 15 x 18.5 lying flat, receiver at the wall
IR_RX = (80.5, 6.3); IR_RX_WIN = (7.6, 8.0)        # x, z of its window (w, h)
HUB = (55.0, 10.5, 85.0, 30.5, 9.5, 13.5)          # perfboard hub 30 x 20 (3V3 / GND / SDA / SCL)

BATT = (95.6, 2.7, 119.6, 74.7, 2.0, 26.0)         # 18650 + BMS 72 x 24 x 24 standing along d, wires to the front
SW_C = (107.0, 18.0)                                # x, z of the rocker switch hole in the FRONT wall
SW_HOLE = (12.5, 9.0)
SW_BODY = (101.0, 75.8, 113.0, 84.8, 13.75, 22.25)  # 12 x 9 into the case x 8.5

BOSSES = [(5.5, 5.5), (92.75, 6.0), (92.75, 81.5), (117.8, 81.0)]   # floor screws (M2.5 self-tapping)
BOSS_R, PILOT_D, SCREW_D, CSK_R = 2.5, 2.0, 2.8, 2.9
WIRE_X = (38.0, 50.0)                     # wires between the halves pass the hinge wall here
WIRE_Z = (23.5, 9.0)                      # slot centre height: base, lid

# ---------------- LID ----------------
LID_IN = 12.0
LH = PLATE + LID_IN + PLATE               # 16
LB = LH - PLATE                           # underside of the back plate
# ES3C28P turned so USB-C is at the right wall (the firmware turns the picture)
PCB = (34.8, 6.0, 120.8, 56.0)            # board 86 x 50
GLASS = (43.2, 6.0, 112.4, 56.0)          # glass 69.2 x 50, z PLATE .. PLATE+2.3
VIS = (45.0, 8.4, 104.4, 53.6)            # lit area 59.45 x 45.2 (3.07 away from the USB side)
PCB_Z = (PLATE + 4.5, PLATE + 6.0)        # board front / back
BACK_Z = PLATE + 10.0                     # tallest parts behind the board
SCREWS = [(38.8, 10.0), (116.8, 10.0), (38.8, 52.0), (116.8, 52.0)]   # M3 through the face into the brass pillars
SPACER_R = 3.0
USB = (31.0, PLATE + 7.4); USB_HOLE = (12.5, 7.5)       # d, z in the right wall
SD = (70.8, PLATE + 7.5); SD_HOLE = (14.0, 4.0)         # x, z in the hinge wall (SD card toward the hinge)
MIC = (39.3, 45.5)
BOOT, RESET = (116.3, 20.3), (116.3, 43.1)              # holes in the back plate
PLUGS = [(83.3, 56.0, 62.0), (70.3, 56.0, 62.0), (52.8, 56.0, 62.0)]   # I2C, IO, speaker plugs + wires (x, d0, d1)
BAT_PLUG = (97.3, 4.0, 7.0)
DS3231 = (47.0, 62.0, 85.0, 83.5, PLATE, PLATE + 9.0)  # clock board 38 x 21.5 x 9 (coin cell toward the back)
LID_BOSSES = [(5.5, 5.5), (28.0, 5.0), (5.5, 81.5), (90.0, 81.5), (118.5, 81.5)]
POCKET_R, POCKET_WALL = 11.0, 1.2         # the stick knob goes in here when the lid is closed

# ---------------- both ----------------
HINGE_R, HINGE_OFF = 4.0, 4.0
HINGE_SETS = [(10.0, 32.0), (92.0, 114.0)]
HINGE_GAP = 0.4
PIN_D = 3.3
MAGNETS = [(40.0, 82.0), (110.0, 82.0)]   # 6 x 3 mm
MAG_D, MAG_H = 6.2, 3.2
NOTCH_X = 62.0
TEXT_POS = (34.0, 64.0, 1.4)              # SOMUDTICK on the lid back
BUY = ["จอ OLED 0.96\" I2C 4 ขา (SSD1306 128×64) 1 จอ ~50 บาท",
       "น็อต M3×25 + หัวน็อต 2 ชุด (แกนบานพับ)",
       "น็อต M3×10 หัวจม ×4 (ยึดจอผ่านหน้าฝา)",
       "น็อต M3×20 ×2 (ใต้เครื่อง ผ่านบอร์ดปุ่มเข้าเสา)",
       "น็อตเกลียวปล่อย M2.5×8 ×9 (ฝาใต้ 4 + ฝาหลัง 5)",
       "แม่เหล็กกลม 6×3 มม. ×4",
       "สายซิลิโคนเส้นเล็ก ~12 เส้น ข้ามบานพับ (เผื่อหย่อน 3 ซม.)"]
