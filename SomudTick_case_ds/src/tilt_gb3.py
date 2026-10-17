"""GB3 check: tilt the stick knob + shaft all round and look for hits with the front shell."""
import io, contextlib, math, sys
VAR, TILT = sys.argv[1], float(sys.argv[2])
sys.argv = ["x", VAR]
with contextlib.redirect_stdout(io.StringIO()):
    import build_gb3 as B
from cad import cyl
from manifold3d import Manifold
sx, sy = B.STICK; pz = B.CB_FRONT - 12          # ball centre
moving = B.parts["stick_cap"] + cyl(sx, sy, B.CB_FRONT - B.JOY_TOTAL + 5.5, pz, 2.5, 24) + Manifold.sphere(7.0, 32).translate([sx, sy, pz])
worst = 0
for k in range(16):
    a = k * math.pi / 8
    m = moving.translate([-sx, -sy, -pz]).rotate([TILT * -math.sin(a), TILT * math.cos(a), 0]).translate([sx, sy, pz])
    v = (m ^ B.front_shell).volume(); worst = max(worst, v)
print("%s tilt %g: worst hit %.2f mm3" % (VAR, TILT, worst))
