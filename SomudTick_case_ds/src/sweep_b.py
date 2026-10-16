"""DS-B checks: open the lid 0 -> 180 degrees (no hits), and tilt the stick 25 degrees all round (no hits)."""
import io, contextlib, math
with contextlib.redirect_stdout(io.StringIO()):
    import build_b as B
from cad import cyl
base = B.base_shell + B.base_floor
for pn, pm in B.base_parts.items():
    if pn.startswith(("print_", "paint_")) or pn in ("bolt", "stick_body", "stick_cap"): continue
    base = base + pm
lid = B.lid_shell + B.lid_back
for pn, pm in B.lid_parts.items():
    if not pn.startswith("paint_"): lid = lid + pm
lid = lid.translate([0, 0, B.BH + B.RIM_H])
stick = B.base_parts["stick_cap"] + B.base_parts["stick_body"]
worst = 0
for ang in range(0, 181, 5):
    m = lid.translate([0, -B.AX_D, -B.AX_Z]).rotate([ang, 0, 0]).translate([0, B.AX_D, B.AX_Z])
    v = (m ^ (base + stick)).volume(); worst = max(worst, v)
    if v > 0.5: print("lid angle %3d: hit %.1f mm3" % (ang, v))
print("worst hit while opening: %.2f mm3" % worst)
# stick: knob + shaft turn about the ball centre
sx, sd = B.STICK; pz = B.NA_TOP_Z + 12
moving = B.base_parts["stick_cap"] + cyl(sx, sd, pz + 6, B.KNOB_TOP - 5.5, 2.5, 24)
import sys
TILT = float(sys.argv[1]) if len(sys.argv) > 1 else 25.0
tw = 0
for k in range(16):
    a = k * math.pi / 8
    ax, ay = -math.sin(a), math.cos(a)      # tilt axis in the x-d plane
    m = moving.translate([-sx, -sd, -pz])
    # rotate 25 deg about the axis: compose with rotations about x and y (small-angle split is fine for a check)
    m = m.rotate([TILT * ax, TILT * ay, 0]).translate([sx, sd, pz])
    v = (m ^ B.base_shell).volume(); tw = max(tw, v)
    if v > 0.5: print("stick tilt %3d deg dir: hits the face %.1f mm3" % (k * 22.5, v))
print("worst stick hit: %.2f mm3" % tw)
