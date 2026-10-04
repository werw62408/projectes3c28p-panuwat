"""
Fault-injection model of SomudTick's log storage (core_log.h), v11.9 ("old") vs v12 ("new").
The flash is adversarial (worse than real LittleFS):
  - a power cut can hit before ANY file operation, and in the middle of a write (half the bytes land)
  - "w" empties the file at once; writes beyond free space land partly and report a short count
After every cut the board "restarts": repair + load, then invariants are checked.
"""
import json, random, sys, itertools

class PowerCut(Exception): pass

class FS:
    def __init__(s, cap=10**9):
        s.f = {}; s.cap = cap; s.ops = 0; s.cut_at = -1; s.half = False
    def used(s): return sum(len(v) for v in s.f.values())
    def _op(s):
        s.ops += 1
        if s.ops == s.cut_at: raise PowerCut()
    def exists(s, p): return p in s.f
    def open_w(s, p): s._op(); s.f[p] = ""
    def write(s, p, data):   # returns bytes written (short when full / cut in the middle)
        s.ops += 1
        free = max(0, s.cap - s.used())
        n = min(len(data), free)
        if s.ops == s.cut_at:
            s.f[p] = s.f.get(p, "") + data[: n // 2]; raise PowerCut()
        s.f[p] = s.f.get(p, "") + data[:n]
        return n
    def remove(s, p): s._op(); s.f.pop(p, None)
    def rename(s, a, b):
        s._op()
        if a not in s.f or b in s.f: return False
        s.f[b] = s.f.pop(a); return True
    def read(s, p): return s.f.get(p)

def c_toint(s):   # Arduino String::toInt (atol): leading digits, else 0
    n = ""
    for ch in s:
        if ch.isdigit(): n += ch
        else: break
    return int(n) if n else 0
def c_tofloat(s):
    import re
    m = re.match(r"\s*[-+]?\d*\.?\d*", s); x = m.group(0).strip() if m else ""
    try: return float(x)
    except ValueError: return 0.0
def parse_line(l, new=True):   # a copy of parseLine() in core_log.h
    if new and "!" in l: return None
    p1 = l.find(","); p2 = l.find(",", p1 + 1); p3 = l.find(",", p2 + 1)
    if p1 < 0 or p2 < 0 or p3 < 0 or len(l) <= p3 + 1: return None
    t = c_toint(l[:p1]); i = l[p1 + 1:p2]; v = c_tofloat(l[p2 + 1:p3])
    return (t, i, v) if t > 0 and i else None
class Board:
    def __init__(s, fs, new):
        s.fs = fs; s.new = new; s.logFull = False
        s.totals = {}; s.acts = None; s.today = []; s.day = "/log/d1.csv"
        s.totDay = ""; s.totDayN = 0
    # ---- helpers
    def events(s, p):
        d = s.fs.read(p)
        if d is None: return []
        ls = d.split("\n")
        if s.new and not d.endswith("\n") and d: ls = ls[:-1]   # eachEvent(): a cut-off last line is not a log
        return [e for e in (parse_line(l, s.new) for l in ls) if e]
    def write_safe(s, p, data):   # v12: .tmp (unchecked) -> .ok (checked) -> file
        fs = s.fs; tmp = p + ".tmp"; ok = p + ".ok"
        fs.remove(tmp); fs.remove(ok)
        fs.open_w(tmp)
        full = fs.write(tmp, data) == len(data)
        if not full or not fs.rename(tmp, ok): fs.remove(tmp); return False
        fs.remove(p)
        return fs.rename(ok, p)
    def write_old(s, p, data):   # v11.9 writeDayFile: never checks the write
        fs = s.fs; tmp = p + ".new"
        fs.open_w(tmp); fs.write(tmp, data)
        fs.remove(p); fs.rename(tmp, p); return True
    def repair(s, p):
        fs = s.fs
        if not s.new:
            tmp = p + ".new"
            if tmp in fs.f:
                if p in fs.f: fs.remove(tmp)
                else: fs.rename(tmp, p)
            return
        if p + ".tmp" in fs.f: fs.remove(p + ".tmp")
        if p + ".ok" in fs.f: fs.remove(p); fs.rename(p + ".ok", p)
        if p + ".new" in fs.f:
            if p in fs.f: fs.remove(p + ".new")
            else: fs.rename(p + ".new", p)
    def write_day(s, p, lines):
        data = "".join(lines)
        if s.new:
            if not s.write_safe(p, data): s.logFull = True; return False
            if p == s.day: s.logFull = False
            return True
        return s.write_old(p, data)
    # ---- totals
    def totals_save(s):
        body = "".join(f"{k},{c},{v}\n" for k, (c, v) in sorted(s.totals.items()))
        if s.new:
            body += f"#ok,{s.totDay},{s.totDayN}\n"
            if not s.write_safe("/totals.csv", body): s.fs.remove("/totals.csv")
        else:
            s.fs.open_w("/totals.csv"); s.fs.write("/totals.csv", body)
    def totals_rebuild(s):
        s.totals = {}
        for p in sorted(x for x in s.fs.f if x.startswith("/log/") and x.endswith(".csv")):
            for t, i, v in s.events(p):
                c, sm = s.totals.get(i, (0, 0.0)); s.totals[i] = (c + 1, sm + v)
        s.totDay, s.totDayN = "", 0
        s.totals_save()
    def totals_load(s):
        s.totals = {}
        if s.new: s.repair("/totals.csv")
        d = s.fs.read("/totals.csv")
        if d is None: s.totals_rebuild(); return
        complete = not s.new
        for l in d.split("\n"):
            if l.startswith("#ok"):
                complete = True
                p = l.split(",")
                if len(p) >= 3 and p[1]:
                    if len(s.events(p[1])) != (int(p[2]) if p[2].isdigit() else 0): complete = False   # (C++ toInt: "" = 0)
                break
            p = l.split(",")
            if len(p) == 3 and p[0]:
                try: s.totals[p[0]] = (int(p[1]), float(p[2]))
                except ValueError: pass
        if not complete: s.totals_rebuild()
    def totals_add(s, i, dc, dv):
        c, v = s.totals.get(i, (0, 0.0)); s.totals[i] = (max(0, c + dc), v + dv); s.totals_save()
    # ---- activities
    def save_acts(s, lst):
        data = json.dumps(lst)
        if s.new: return s.write_safe("/acts.json", data)
        s.fs.open_w("/acts.json"); s.fs.write("/acts.json", data); return True
    def load_acts(s):
        if s.new: s.repair("/acts.json")
        d = s.fs.read("/acts.json"); had = d is not None
        try: s.acts = json.loads(d) if d else None
        except ValueError: s.acts = None
        if not s.acts:
            if s.new and had: s.fs.remove("/acts.bad.json"); s.fs.rename("/acts.json", "/acts.bad.json")
            s.acts = ["DEFAULT"]; s.save_acts(s.acts)
    # ---- boot / log / undo
    def boot(s):
        days = {x[:x.index(".csv.") + 4] for x in list(s.fs.f) if x.startswith("/log/") and ".csv." in x}
        for q in days: s.repair(q)
        s.totals_load(); s.load_acts()
        s.today = s.events(s.day)
    def log(s, t, i):
        line = f"{t},{i},1,H,1\n"
        if s.new:
            d = s.fs.read(s.day) or ""
            data = ("" if (not d or d.endswith("\n")) else "!\n") + line
            n0 = len(s.today)
            s.totDay, s.totDayN = s.day, n0 + 1
            s.totals_add(i, 1, 1.0)                     # totals first
            ok = s.fs.write(s.day, data) == len(data)
            s.logFull = not ok
            if not ok:
                s.totDayN = n0; s.totals_add(i, -1, -1.0); return False
            s.today.append((t, i, 1.0)); return True
        else:
            ok = s.fs.write(s.day, line) == len(line)
            s.logFull = not ok
            s.today.append((t, i, 1.0))           # v11.9: counted even when not saved
        s.totals_add(i, 1, 1.0); return True
    def undo(s, i):
        for k in range(len(s.today) - 1, -1, -1):
            if s.today[k][1] != i: continue
            lines = [f"{t},{j},1,H,1\n" for n, (t, j, v) in enumerate(s.today) if n != k]
            if s.new:
                s.totDay, s.totDayN = s.day, len(lines); s.totals_add(i, -1, -1.0)
                if not s.write_day(s.day, lines):
                    s.totDayN = len(s.today); s.totals_add(i, 1, 1.0); return False
                s.today.pop(k); return True
            else:
                gone = s.today.pop(k); s.totals_add(i, -1, -1.0); s.write_day(s.day, lines); return True
            s.totals_add(i, -1, -1.0); return True
        return False

IDS = ["water", "smoke", "toilet"]
def script(seed, n):
    r = random.Random(seed); out = []; t = 1000
    for _ in range(n):
        x = r.random()
        if x < 0.65: t += 1; out.append(("log", t, r.choice(IDS)))
        elif x < 0.9: out.append(("undo", r.choice(IDS)))
        else: out.append(("acts", [f"a{r.randint(0,99)}"]))
    return out

def run(new, seed, n, cut_at=-1, cap=10**9):
    fs = FS(); b = Board(fs, new)
    b.boot(); b.save_acts(["mine"]); b.boot()          # a working board first, then the flash gets full / the power goes
    base = fs.used()
    fs.cap = base + cap if cap < 10**9 else cap         # cap = free bytes left
    fs.ops = 0; fs.cut_at = cut_at
    alive = {}      # t -> id: logs that were saved and not undone (the truth the user expects)
    maybe = set()   # t whose op was cut in the middle (either outcome is fine)
    acts_ok = [["mine"]]
    try:
        for op in script(seed, n):
            if op[0] == "log":
                _, t, i = op; maybe.add(t)
                if b.log(t, i): alive[t] = i
                maybe.discard(t)
            elif op[0] == "undo":
                cand = [t for t, (tt, j, v) in zip([e[0] for e in b.today], b.today) if j == op[1]]
                last = cand[-1] if cand else None
                if last is not None: maybe.add(last)
                if b.undo(op[1]) and last is not None: alive.pop(last, None)
                if last is not None: maybe.discard(last)
            else:
                acts_ok.append(op[1])
                if b.save_acts(op[1]): acts_ok = [op[1]]
    except PowerCut:
        pass
    fs.cut_at = -1
    # ---- restart
    b2 = Board(fs, new); b2.boot()
    errs = []
    evs = b2.events(b2.day)
    ts = [e[0] for e in evs]
    real = {t for t in ts}
    for e in evs:
        if e[0] not in alive and e[0] not in maybe and e[0] not in [x for x in alive]:
            if e[0] not in maybe: errs.append(f"ghost log t={e[0]} (undone or never saved, but in the file)")
    for t, i in alive.items():
        if t not in real and t not in maybe: errs.append(f"LOST log t={t} {i} (it was saved)")
    raw = fs.read(b2.day) or ""
    for e in evs:
        if e[1] not in IDS or e[0] > 10**6 or e[0] < 1000: errs.append(f"GARBAGE event {e!r} (a cut line read as a log)")
    if len(ts) != len(set(ts)): errs.append("DOUBLE log")
    want = {}
    for t, i, v in evs:
        c, sm = want.get(i, (0, 0.0)); want[i] = (c + 1, sm + v)
    got = {k: v for k, v in b2.totals.items() if v[0] > 0}
    if got != want: errs.append(f"totals wrong: {got} vs files {want}")
    if b2.acts == ["DEFAULT"]: errs.append("ACTIVITIES RESET to the default list")
    elif b2.acts not in acts_ok: errs.append(f"activities {b2.acts} not one of {acts_ok}")
    return errs, fs.ops

def sweep(new, label):
    seeds, n = range(60), 25
    bad = {}; cases = 0
    for seed in seeds:
        _, total = run(new, seed, n)
        for k in range(1, total + 2):                       # a power cut before every single operation
            cases += 1
            errs, _ = run(new, seed, n, cut_at=k)
            for e in errs: bad.setdefault(e.split(" ")[0] + " " + e.split(" ")[1], []).append((seed, k, e))
        for cap in range(0, 700, 7):                        # the flash full at every size
            cases += 1
            errs, _ = run(new, seed, n, cap=cap)
            for e in errs: bad.setdefault("FULL:" + e.split(" ")[0] + " " + e.split(" ")[1], []).append((seed, cap, e))
        for cap in range(0, 700, 37):                       # full AND a power cut
            for k in range(1, total + 2, 3):
                cases += 1
                fs_errs, _ = run(new, seed, n, cut_at=k, cap=cap)
                for e in fs_errs: bad.setdefault("FULL+CUT:" + e.split(" ")[0] + " " + e.split(" ")[1], []).append((seed, (cap, k), e))
    print(f"\n=== {label}: {cases} runs ===")
    if not bad: print("  ALL INVARIANTS HOLD"); return 0
    for k, v in sorted(bad.items(), key=lambda x: -len(x[1])):
        print(f"  {len(v):6d} x {k:28s} e.g. seed {v[0][0]} at {v[0][1]}: {v[0][2]}")
    return sum(len(v) for v in bad.values())

if __name__ == "__main__":
    old = sweep(False, "v11.9 (old)")
    new = sweep(True, "v12 (new)")
    sys.exit(1 if new else 0)


