"""SomudTick DS case: every size and position in one place (mm).

The case opens like a Nintendo DS. Two halves joined by a hinge on the long back edge:
  LID  (top half):    the 2.8" screen on the inside, a small OLED on the outside, USB-C and SD card.
  BASE (bottom half): Joystick Shield (stick + buttons), battery, speaker, DS3231, PCF8574, IR, switch.

Coordinates, both halves (looking at the inside face, case open flat):
  x = 0 at the left outer edge, -> right
  d = distance from the hinge edge (0 = hinge side outer wall), -> away from the hinge
  z = height. BASE: 0 = outer bottom, BH = the face with the buttons.
             LID:  0 = the face with the screen, LH = the outer back (OLED side).

Sizes marked (EST) are estimates from shop photos: measure the real part and change them.
"""

# ---------------- outer shape ----------------
OW, OD = 152.0, 90.0          # footprint of each half
R_CORNER = 7.0                # corner radius
CHAMFER = 1.2                 # 45 degree edge on the outer faces
WALL = 2.2                    # side walls
PLATE = 2.0                   # face plates, floor, back

# ---------------- BASE (bottom half) ----------------
BASE_IN = 20.0                # inside height between floor and face plate
BH = PLATE + BASE_IN + PLATE  # 24

# Joystick Shield (Funduino V1.A): Arduino Uno size. The side with the Nokia 5110 socket faces the hinge.
SHIELD = (9.5, 33.3, 69.0, 53.3)      # x0, d0, w, h
SHIELD_Z = PLATE + 2.0                # underside of its board (sits on 2 mm posts; bottom pins removed)
SHIELD_T = 1.6
# on the shield, measured from its left edge and its hinge-side edge (EST, from the shop photo)
STICK_ON = (13.2, 32.0)
STICK_H = 33.0                # underside of the board -> top of the stick cap (EST: measure!)
STICK_CAP_D = 25.0
BTN_ON = {"A": (51.0, 14.8), "D": (41.0, 29.0), "B": (60.5, 29.0), "C": (51.0, 43.0)}   # big buttons
SMALL_ON = {"E": (37.8, 41.3), "F": (27.9, 41.3)}                                     # small 6x6 buttons
BTN_H = 15.0                  # board top -> top of the big coloured caps (EST)
SMALL_H = 5.0                 # board top -> top of the small buttons (EST)
STICK_HOLE_D = 27.0
BTN_HOLE_D, BTN_CAP_D = 10.8, 10.0
SMALL_HOLE_D, SMALL_CAP_D = 5.8, 4.8

# other parts in the base: x0, d0, x1, d1, z0, z1
BATT = (81.0, 4.0, 129.0, 34.0, 2.0, 12.0)       # 48 x 30 x 10
SPK = (102.0, 57.0, 142.0, 85.0, 2.0, 12.0)      # 40 x 28 x 10, faces the floor (grille below)
DS3231 = (78.6, 46.0, 100.6, 84.0, 2.0, 11.0)    # 22 x 38
PCF = (104.0, 35.5, 140.0, 55.5, 2.0, 8.0)       # PCF8574 board 36 x 20 (EST)
KY = (131.0, 10.0, 146.0, 28.5, 2.0, 5.0)        # KY-005 lying flat, LED bent to the hinge wall
IR_LED = (138.5, 9.0)                            # x, z of the hole in the hinge wall
IR_D = 5.3
SW = (18.0, 12.0)                                # d, z of the switch hole in the left wall (KCD11)
SW_HOLE = (13.0, 8.5)
SW_BODY = (2.2, 11.5, 18.0, 24.5, 7.5, 16.5)
WIRE_X = (44.0, 60.0)                            # wires pass between the halves here (hinge wall)

# ---------------- LID (top half) ----------------
LID_IN = 14.0                 # face plate -> back plate (= ES3C28P front of acrylic -> back of the brass standoffs)
LH = PLATE + LID_IN + PLATE   # 18

# ES3C28P turned sideways: USB-C to the right wall, SD card toward the hinge, plugs (I2C, IO, speaker) away from it
ACR = (60.0, 10.0, 145.0, 70.0)   # acrylic plate 85 x 60
ACR_T, PCB_BACK = 3.0, 9.0
GLASS = (67.0, 15.5, 138.0, 64.5) # visible screen 71 x 49
SCREWS = [(63.5, 19.0), (141.5, 19.0), (63.5, 61.0), (141.5, 61.0)]
MIC = (64.0, 54.5)
USB_C = (40.1, 10.5)              # d, depth from the acrylic front
USB_HOLE = (12.0, 7.0)
SD = (95.5, 10.5)                 # x, depth ; slot in the hinge wall
SD_HOLE = (14.0, 4.0)
BOOT, RESET = (141.0, 29.3), (141.0, 52.1)
PLUGS = [(108.0, 65.0), (95.0, 65.0), (77.5, 65.0)]   # I2C, IO, speaker plugs (on the far PCB edge)
BAT_PLUG = (122.0, 15.0)

# small OLED (SSD1306 0.96", 4 pins) on the outer back of the lid
OLED = (11.0, 9.0, 38.3, 36.8)    # module 27.3 x 27.8 (x0, d0, x1, d1)
OLED_WIN = (12.0, 7.0)            # window w (along x), h (along d)
OLED_T = 4.0

# stick pocket: when closed, the stick top goes into the lid here (same x, same distance from the hinge)
POCKET_R = 15.0
POCKET_WALL = 1.4

# ---------------- hinge, screws, magnets ----------------
HINGE_R = 4.0                     # knuckle radius
HINGE_OFF = 4.0                   # axis this far outside the hinge wall
HINGE_SETS = [(10.0, 34.0), (118.0, 142.0)]    # x ranges; each: base | lid | base knuckle
HINGE_GAP = 0.4
PIN_D = 3.3                       # M3 bolt
BOSSES = [(6.0, 6.0), (146.0, 6.0), (6.0, 84.0), (146.0, 84.0)]
BOSS_R, PILOT_D, SCREW_D = 3.0, 2.5, 3.2
MAGNETS = [(90.0, 85.0), (126.0, 85.0)]          # 6 x 3 mm round magnets
MAG_D, MAG_H = 6.2, 3.2
RIM_H = 1.0                       # raised edge on the base face: a closed lid rests on it, never on the buttons
NOTCH_X = 76.0                    # finger notch on the lid's front edge

# ---------------- settings that differ between the case variants ----------------
NAME = "SomudTick DS Case"
SW_WALL = "left"                  # which wall the power switch is in
LID_BOSSES = BOSSES               # screw bosses in the lid (the lid may need other spots than the base)
CSK_R = 3.3                       # countersink radius for the cover screws
PCF_IN = "base"                   # PCF8574 lies in the base (x0, d0, x1, d1, z0, z1 as PCF above)
PCB = (59.5, 15.0, 145.5, 65.0)   # ES3C28P board (x0, d0, x1, d1)
GLASS_Z = (0.3, 5.5)              # glass front / back, measured from the inside of the face plate
STANDOFF_BACK = LID_IN            # back of the brass standoffs
SD_PORT_D = (14.0, 28.0)
PLUG_D = (65.0, 71.0)
BAT_PLUG_D = (10.0, 15.0)
FRAME_OFF = 3.2                   # engraved frame this far outside the screen window
WIRE_Z = (16.5, 7.5)              # height of the wire slot: base, lid
TEXT_POS = (52.0, 70.0, 1.45)     # SOMUDTICK logo on the lid back: x0, d0, pixel size
TECH_LINES = [(40.65, 140.0, 19.9), (40.65, 118.0, 25.9)]   # engraved lines from the OLED: x from, x to, d
DS3231_ON, IR_ON = True, True
BUY = ["น็อต M3×25 + หัวน็อต 2 ชุด (แกนบานพับ)",
       "น็อตเกลียวปล่อย M3×8 ×8 (ฝาใต้ + ฝาหลัง) และ M3×10 ×4 (ยึดจอ)",
       "แม่เหล็กกลม 6×3 มม. ×4 (ปิดฝาแล้วติดแน่น)",
       "สายไฟอ่อน (ซิลิโคน) ข้ามบานพับ ~12 เส้น เผื่อหย่อน 3 ซม."]
SCREEN_NOTE = "จอ ES3C28P + อะคริลิก 85×60×14 ✓ วัดแล้ว"
