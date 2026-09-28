"""Put the GLB into the 3D page (one file, opens in any browser).
   python3 make_viewer_ds.py          -> out/somudtick_ds_3d.html
   python3 make_viewer_ds.py mini     -> out_mini/somudtick_mini_3d.html"""
import base64, os, sys, json
d = os.path.dirname(os.path.abspath(__file__))
v = sys.argv[1] if len(sys.argv) > 1 else ""
out = os.path.join(d, "out_" + v if v else "out")
b = base64.b64encode(open(os.path.join(out, "somudtick_ds.glb"), "rb").read()).decode()
meta = open(os.path.join(out, "meta.json")).read()
name = json.loads(meta)["NAME"]
h1 = name.replace("DS", "<b>DS</b>", 1)
t = open(os.path.join(d, "viewer_ds_tpl.html")).read().replace("__GLB__", b).replace("__META__", meta).replace("__NAME__", name).replace("__H1__", h1)
fn = "somudtick_%s_3d.html" % (v or "ds")
open(os.path.join(out, fn), "w").write(t)
open(os.path.join(out, "_preview.html"), "w").write('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + t + '</body></html>')
print(fn, len(t) // 1024, "KB")
