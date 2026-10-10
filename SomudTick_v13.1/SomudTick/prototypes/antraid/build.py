#!/usr/bin/env python3
"""Join shell.html + src/*.js into one index.html (one file, works offline and as an artifact)."""
from pathlib import Path

HERE = Path(__file__).parent
ORDER = ["data", "world", "sim", "draw", "ui", "meta", "main"]
js = "\n".join((HERE / "src" / f"{n}.js").read_text() for n in ORDER)
html = (HERE / "shell.html").read_text().replace("/*@SCRIPTS@*/", js)
(HERE / "index.html").write_text(html)
print("index.html", len(html), "bytes")

# artifact copy: the publish step adds its own doctype/head, so leave those lines out
art = "\n".join(l for l in html.split("\n") if not l.startswith(("<!doctype", "<meta charset", "<meta name=\"viewport\"")))
(HERE / "artifact.html").write_text(art)
print("artifact.html", len(art), "bytes")
