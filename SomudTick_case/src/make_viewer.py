import base64, os
d = os.path.dirname(os.path.abspath(__file__))
b = base64.b64encode(open(os.path.join(d, "out/somudtick_case.glb"), "rb").read()).decode()
t = open(os.path.join(d, "viewer_tpl.html")).read().replace("__GLB__", b)
open(os.path.join(d, "out/somudtick_case_3d.html"), "w").write(t)
# local preview wrapper with a skeleton like the artifact host
open(os.path.join(d, "out/_preview.html"), "w").write('<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"></head><body>' + t + '</body></html>')
print(len(t) // 1024, "KB")
