"""SomudTick DS Mini: the smallest DS case for the parts on hand now.

Inside: the red Joystick Shield (whole, with its tall stick), the ES3C28P WITHOUT its acrylic case,
the small OLED, the battery, the speaker, plus the PCF8574 (the shield buttons need it) and the power switch.
Left out for now: DS3231 and the IR LED (the clock falls back to Wi-Fi / phone time).

Same coordinates as layout_ds.py (x right, d from the hinge edge, z up). Only what differs is set here.
"""
from layout_ds import *

NAME = "SomudTick DS Mini"
OW, OD = 136.0, 70.0
R_CORNER = 6.0

# ---------------- BASE ----------------
BASE_IN = 20.0
BH = PLATE + BASE_IN + PLATE              # 24
SHIELD = (8.0, 3.5, 69.0, 53.3)           # stick ends up at x 21.2, d 35.5
BATT = (79.0, 4.2, 127.0, 34.2, 2.0, 12.0)
SPK = (78.5, 36.0, 118.5, 64.0, 2.0, 12.0)
SW_WALL = "right"
SW = (46.0, 12.0)                         # d, z of the switch hole in the right wall
SW_BODY = (119.0, 39.5, 133.8, 52.5, 7.5, 16.5)
DS3231_ON, IR_ON = False, False
WIRE_X = (36.0, 46.0)                     # wires between the halves (left of the screen board)
BOSSES = [(5.0, 5.0), (131.0, 5.0), (5.0, 65.0), (131.0, 65.0)]
LID_BOSSES = [(5.0, 5.0), (40.0, 5.0), (5.0, 65.0), (131.0, 65.0)]   # the lid's right-hinge corner is under the board
BOSS_R, PILOT_D, SCREW_D, CSK_R = 2.5, 2.0, 2.8, 2.9                 # M2.5 screws
MAGNETS = [(30.0, 65.0), (122.0, 64.0)]
HINGE_SETS = [(10.0, 34.0), (102.0, 126.0)]
NOTCH_X = 68.0

# ---------------- LID: ES3C28P without the acrylic, glass right behind the face plate ----------------
LID_IN = 14.0                             # glass front -> back plate (room behind the board for the OLED + PCF8574)
LH = PLATE + LID_IN + PLATE               # 18
ACR = None
PCB = (47.0, 8.0, 133.0, 58.0)            # board 86 x 50, USB-C at the right wall, SD card toward the hinge
PCB_BACK = 6.0                            # glass front -> back of the board
STANDOFF_BACK = 11.0
GLASS_Z = (0.0, 4.4)
GLASS = (54.5, 9.0, 125.5, 57.0)          # visible screen
SCREWS = [(51.0, 12.0), (129.0, 12.0), (51.0, 54.0), (129.0, 54.0)]  # M3 through the face into the brass standoffs
MIC = (51.5, 47.5)
USB_C = (33.1, 7.5)                       # d, depth from the glass front
SD = (83.0, 7.5)                          # x, depth ; slot in the hinge wall
SD_PORT_D = (8.0, 20.0)
PLUGS = [(95.5, 58.0), (82.5, 58.0), (65.0, 58.0)]
PLUG_D = (58.0, 64.0)
BAT_PLUG = (109.5, 8.0)
BAT_PLUG_D = (3.0, 8.0)
BOOT, RESET = (128.5, 22.3), (128.5, 45.1)
FRAME_OFF = 1.2
WIRE_Z = (16.5, 9.0)

# OLED and PCF8574 sit behind the board, against the back plate
OLED = (86.0, 30.0, 124.0, 42.0)      # 0.91" module 38 x 12, pins toward the left
PCF_IN = "lid"
PCF = (47.5, 15.0, 83.5, 35.0, LH - PLATE - 5.0, LH - PLATE)   # 36 x 20, 5 mm thick (no pin headers)

TEXT_POS = (12.0, 53.0, 1.3)
TECH_LINES = [(82.0, 20.0, 33.0), (82.0, 45.0, 39.0)]
BUY = ["น็อต M3×25 + หัวน็อต 2 ชุด (แกนบานพับ)",
       "น็อตเกลียวปล่อย M2.5×8 ×8 (ฝาใต้ + ฝาหลัง)",
       "น็อต M3×8 หัวจม (หัวเตเปอร์) ×4 ยึดจอเข้าเสาทองเหลืองเดิม",
       "แม่เหล็กกลม 6×3 มม. ×4",
       "สายไฟอ่อน (ซิลิโคน) ข้ามบานพับ ~15 เส้น เผื่อหย่อน 3 ซม."]
SCREEN_NOTE = "จอ ES3C28P ไม่ใส่อะคริลิก: กระจกถึงหลังบอร์ด 6"
