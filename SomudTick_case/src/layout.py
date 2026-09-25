"""SomudTick case: one place for every size and position (mm).

Coordinates (front view, holding it upright, USB-C at the top, joystick at the bottom):
  x = 0 at the left outer edge,  -> right
  y = 0 at the top outer edge,   -> down
  zi = depth from the INSIDE of the front wall, -> toward the back
The clear box is 80 x 135 x 30 with ~1.5 mm walls; the printed case keeps the same
inside space (77 x 132 x 27) with 2 mm walls.
"""

INNER_W, INNER_H, INNER_D = 77.0, 132.0, 27.0
IX0, IY0 = 1.5, 1.5                      # inside corner (same for both cases)
IX1, IY1 = IX0 + INNER_W, IY0 + INNER_H  # 78.5, 133.5

# ---- display (ES3C28P + front acrylic plate) ----
ACR = (12.5, 2.0, 72.5, 87.0)            # acrylic plate 60 x 85
PCB = (17.5, 1.5, 67.5, 87.5)            # board 50 x 86 (USB-C edge touches the top wall)
DISP_D = 14.0                            # front of acrylic -> back of the brass standoffs
ACR_T, PCB_BACK = 3.0, 9.0               # acrylic thickness, depth of the board's back face
GLASS = (18.0, 9.0, 67.0, 80.0)          # screen glass 49 x 71 (trace the real one before cutting)
SCREWS = [(21.5, 5.5), (63.5, 5.5), (21.5, 83.5), (63.5, 83.5)]   # M3 into the brass standoffs
MIC = (28.0, 83.0)

# ports on the board edges
USB_C = (42.4, 10.5)                     # x, zi (centre)
USB_HOLE = (12.0, 7.0)                   # w, h
BOOT, RESET = (53.2, 6.0), (30.4, 6.0)   # buttons face the BACK
SD = (51.5, 10.5)                        # y, zi on the right wall
SD_HOLE = (14.0, 4.0)
BAT_PLUG = (67.5, 25.0)                  # right edge (edge A)
SPK_PLUG, I2C_PLUG, IO_PLUG = (17.5, 39.0), (17.5, 52.0), (17.5, 69.5)   # left edge (edge B)

# ---- parts ----
BATT = (18.0, 12.0, 66.0, 42.0, 16.0, 26.0)     # x0 y0 x1 y1 zi0 zi1   (48 x 30 x 10)
SPK = (22.0, 47.0, 62.0, 75.0, 17.0, 27.0)      # 40 x 28 x 10, faces the back
SPK_C = (42.0, 61.0)
JOY = (27.0, 89.0, 53.0, 123.0)                 # board 26 x 34, pins toward the screen
JOY_STICK = (40.0, 110.0)
JOY_H = 30.0                                    # board bottom -> top of the cap
JOY_LIFT = 6.5                                  # board sits this high off the back wall
JOY_HOLE_D = 26.0
DS = (55.5, 88.0, 77.5, 126.0, 19.0, 27.0)      # DS3231 38 x 22, lies on the back wall
KY = (70.0, 3.5, 72.0, 18.5, 4.0, 19.0)         # KY-005 board standing by the right wall
LED = (10.5, 9.0)                                # y, zi ; LED points out of the right wall
LED_D = 5.3
SW = (110.0, 10.0)                              # y, zi on the left wall (mini rocker KCD11)
SW_HOLE = (13.0, 8.5)                           # along y, along z
SW_BODY = (1.5, 103.5, 17.5, 116.5, 5.5, 14.5)  # body + solder tabs inside
SPLICE = (7.0, 86.0)

# printed case only
WALL = 2.0
SPLIT_ZI = 15.0                                 # front shell = inside depth 0..15
PILLARS = [(5.5, 5.5), (75.5, 40.0), (5.5, 129.5), (75.5, 129.5)]
PILLAR_R = 3.0

# wires (x, y, zi) polylines, coloured as in the wiring plan
WIRES = {
    "i2c":   [(17.5, 52.0, 12), (10, 52, 12), (10, 86, 22), (55, 87, 22), (62, 88, 22)],
    "joy":   [(17.5, 69.5, 12), (12, 69.5, 12), (12, 86, 20), (33, 88, 20)],
    "ky":    [(17.5, 69.5, 13), (9, 72, 20), (9, 11, 20), (70, 11, 20)],
    "power": [(17.5, 52.0, 13), (SPLICE[0], SPLICE[1] - 2, 18), (SPLICE[0], SPLICE[1], 18)],
    "sw":    [(66, 30, 20), (69, 30, 20), (69, 84, 24), (20, 86, 25), (10, 100, 24), (10, 104, 12)],
}
