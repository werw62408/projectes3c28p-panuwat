"""Turn the lid from closed (0) to flat open (180) and look for any hit with the base."""
import builtins, io, contextlib, os
with contextlib.redirect_stdout(io.StringIO()):
    import build_ds as B
from manifold3d import Manifold
base = B.base_shell + B.base_floor
for pn, pm in B.base_parts.items():
    if pn.startswith("print_") or pn in ("bolt",): continue
    base = base + pm
lid = B.lid_shell + B.lid_back
for pn, pm in B.lid_parts.items(): lid = lid + pm
lid = lid.translate([0, 0, B.BH + B.RIM_H])
worst = 0
for ang in range(0, 181, 5):
    m = lid.translate([0, -B.AX_D, -B.AX_Z]).rotate([ang, 0, 0]).translate([0, B.AX_D, B.AX_Z])
    v = (m ^ base).volume()
    worst = max(worst, v)
    if v > 0.5: print("angle %3d: hit %.1f mm3" % (ang, v))
# stick + caps vs closed lid (the real clash that matters)
stick = B.base_parts["stick_cap"] + B.base_parts["stick_body"]
for k in B.base_parts:
    if k.startswith("print_"): stick = stick + B.base_parts[k]
print("stick/caps vs closed lid: %.2f mm3" % (stick ^ (B.lid_shell + B.lid_back).translate([0, 0, B.BH + B.RIM_H])).volume())
print("worst hit while opening: %.2f mm3" % worst)
