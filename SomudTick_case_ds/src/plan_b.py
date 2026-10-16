"""Top view of the DS-B halves with every part labelled (images/dsb/plan.png), Thai labels."""
import os, matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle, Circle, FancyBboxPatch
from matplotlib import font_manager
import io, contextlib
with contextlib.redirect_stdout(io.StringIO()):
    import build_b as B
from layout_b import *
for f in ("/usr/share/fonts/opentype/tlwg/Loma.otf",):
    if os.path.exists(f): font_manager.fontManager.addfont(f); plt.rcParams["font.family"] = font_manager.FontProperties(fname=f).get_name()
fig, axs = plt.subplots(1, 2, figsize=(14, 6.2))
def half(ax, title, lid=False):
    ax.add_patch(FancyBboxPatch((0, 0), OW, OD, boxstyle="round,pad=0,rounding_size=7", fc="#e4e0d8", ec="#333", lw=1.2))
    ax.plot([0, OW], [-1.5, -1.5], color="k", lw=4); ax.text(OW + 2, -1.5, "บานพับ", ha="left", va="center", fontsize=9)
    ax.set_title(title, fontsize=11); ax.set_xlim(-4, OW + 18)
    ax.set_ylim((-8, OD + 4) if lid else (OD + 4, -8)); ax.set_aspect("equal"); ax.axis("off")
def r(ax, t, lab, c, ls="-", fs=7.5, a=0.85):
    ax.add_patch(Rectangle((t[0], t[1]), t[2] - t[0], t[3] - t[1], fc=c, ec="#222", lw=0.8, ls=ls, alpha=a))
    ax.text((t[0] + t[2]) / 2, (t[1] + t[3]) / 2, lab, ha="center", va="center", fontsize=fs)
ax = axs[0]; half(ax, "ตัวล่าง (มองจากด้านบน ตอนกางฝา) · สูง %g มม." % (BH + RIM_H))
r(ax, (NA_X0, NA_D0, NA_X0 + NA_W, NA_D0 + NA_H), "", "#e9a3a3", a=0.55)
ax.text(NA_X0 + 2, NA_D0 + NA_H - 2, "บอร์ดปุ่ม NA011 (วางลึก)", fontsize=7.5)
ax.add_patch(Circle(B.STICK, JOY_HOLE_D / 2, fc="none", ec="k", ls="--")); ax.add_patch(Circle(B.STICK, KNOB_D / 2, fc="#333")); ax.text(*B.STICK, "จอย", color="w", ha="center", va="center", fontsize=8)
for k, p in B.BTNS.items(): ax.add_patch(Circle(p, CAP_D / 2, fc={"A": "#2d7fd6", "B": "#e8c21e", "C": "#d93b3b", "D": "#3fae4f"}[k], ec="k")); ax.text(*p, k, ha="center", va="center", fontsize=8)
for k, p in B.SMALLS.items(): ax.add_patch(Circle(p, 2.4, fc="#111")); ax.text(p[0], p[1] + 5, k, ha="center", fontsize=8)
r(ax, OLED, 'จอเล็ก 0.96"', "#3355aa", fs=7.5)
r(ax, SPK, "ลำโพง (ล่างสุด)\nPCF8574 ทับบน", "#9ad0f5", ls="--", a=0.5)
r(ax, KY005, "ส่ง IR", "#f5d39a", fs=6.5); r(ax, KY022, "รับ\nIR", "#f5d39a", fs=6.5)
r(ax, HUB, "แผงรวมสาย (ชั้นบน)", "#9be3b5", ls=":", a=0.35, fs=6.5)
r(ax, BATT, "แบต 18650\n(ปลายสาย\nด้านหน้า)", "#f2a65a", fs=8)
r(ax, (SW_BODY[0], SW_BODY[1], SW_BODY[2], SW_BODY[3]), "สวิตช์", "#e0c040", fs=6.5)
ax.annotate("หลอด IR / ตัวรับ มองออกขอบบานพับ", xy=(IR_LED[0], 0), xytext=(70, -6.5), fontsize=7, arrowprops=dict(arrowstyle="->"))
for x, d in B.HOLES: ax.add_patch(Circle((x, d), 1.6, fc="w", ec="k")); 
ax = axs[1]; half(ax, "ฝาบน (มองหน้าจอ ตอนกางฝา: บานพับอยู่ล่าง) · หนา %g มม." % LH, True)
r(ax, PCB, "", "#9ec5e8", a=0.5); r(ax, VIS, 'จอใหญ่ 2.8"\nUSB-C ที่ผนังขวา', "#1d3557", fs=8)
ax.texts[-1].set_color("w")
ax.add_patch(Circle(B.STICK, POCKET_R + POCKET_WALL, fc="#ccc", ec="k")); ax.text(*B.STICK, "หลุมรับ\nหัวจอย", ha="center", va="center", fontsize=7)
r(ax, (DS3231[0], DS3231[1], DS3231[2], DS3231[3]), "DS3231 (ใต้หน้าฝา)", "#9be3b5", fs=7.5)
ax.text(SD[0], 3.0, "การ์ด SD", ha="center", va="center", fontsize=7)
for x, d in SCREWS: ax.add_patch(Circle((x, d), 1.7, fc="w", ec="k"))
plt.tight_layout()
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "images", "dsb", "plan.png")
plt.savefig(out, dpi=120); print(out)
