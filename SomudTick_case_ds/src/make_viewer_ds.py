"""Put the GLB into the 3D page: out/somudtick_ds_3d.html (one file, opens in any browser)."""
import base64, os
d = os.path.dirname(os.path.abspath(__file__))
b = base64.b64encode(open(os.path.join(d, "out/somudtick_ds.glb"), "rb").read()).decode()
meta = open(os.path.join(d, "out/meta.json")).read()
t = open(os.path.join(d, "viewer_ds_tpl.html")).read().replace("__GLB__", b).replace("__META__", meta)
open(os.path.join(d, "out/somudtick_ds_3d.html"), "w").write(t)
open(os.path.join(d, "out/_preview.html"), "w").write('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + t + '</body></html>')
print(len(t) // 1024, "KB")
