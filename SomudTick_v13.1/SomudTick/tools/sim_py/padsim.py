"""Model of padPoll() + padTask() + padChatter() (joystick.h / ext_io.h), stepped 1 ms at a time like loop().
Checks what a hand, a bouncing contact, a chattering (loose) contact, a stuck button and a pocket produce.
v13.1: page = "fast" (Remotes / Deck / Sudoku: A pressed fast on purpose) or "typing" (keyboard / number pad):
A and K there take 25 in 5 s or 60 in a minute; E and the others keep the normal rule (40 in a minute, off when typing)."""
import random, sys

N_CHAT, MUTE_MS = 60, 30000   # (v13.1: 60 kept, was 40)
class Pad:
    def __init__(s, page="normal"):
        s.page = page
        s.bits = 0; s.last = 0; s.readT = -10**9
        s.was = s.held = s.ign = 0; s.downT = [0] * 7
        s.pT = [[0] * N_CHAT for _ in range(7)]; s.pI = [0] * 7; s.mute = [0] * 7; s.lastT = [0] * 7; s.mutes = [0] * 7
        s.press = []; s.hold = []; s.muted = 0; s.wakes = 0
        s.off = False
    def chatter(s, b, ms):
        if s.lastT[b] and ms - s.lastT[b] > 120000: s.mutes[b] = 0
        s.lastT[b] = ms or 1
        if s.mute[b] and ms - s.mute[b] < 0: return True
        s.mute[b] = 0
        fast = b in (0, 6) and s.page in ("fast", "typing"); typing = s.page == "typing"
        i = s.pI[b]; backS = s.pT[b][(i + N_CHAT - (24 if fast else 9)) % N_CHAT]; backL = s.pT[b][(i + N_CHAT - (59 if fast else 39)) % N_CHAT]
        s.pT[b][i] = ms; s.pI[b] = (i + 1) % N_CHAT
        if (backS and ms - backS < (30000 if s.mutes[b] else 5000)) or ((fast or not typing) and backL and ms - backL < 60000):
            s.pT[b] = [0] * N_CHAT
            s.mute[b] = ms + (MUTE_MS << min(s.mutes[b], 6)); s.mutes[b] = min(6, s.mutes[b] + 1); s.muted += 1
            return True
        return False
    def step(s, ms, raw_down):   # raw_down: bitmask of contacts closed right now
        if ms - s.readT >= 8:
            s.readT = ms
            if raw_down == s.last: s.bits = raw_down
            s.last = raw_down
        now = s.bits
        for b in range(7):
            m = 1 << b; d = now & m; was = s.was & m
            if d and not was:
                s.downT[b] = ms; s.held &= ~m; s.ign &= ~m
                if s.off: s.ign |= m; s.off = False; s.wakes += 1
            elif d and was:
                lim = 1000 if b == 5 else 650
                if not (s.ign & m) and not (s.held & m) and ms - s.downT[b] >= lim: s.held |= m; s.hold.append((ms, b))
            elif not d and was:
                if not (s.ign & m) and not (s.held & m) and ms - s.downT[b] >= 30 and not s.chatter(b, ms): s.press.append((ms, b))
                s.held &= ~m; s.ign &= ~m
        s.was = now

def run(signal, ms_total, off=False, page="normal"):
    p = Pad(page); p.off = off
    for ms in range(1, ms_total):
        p.step(ms, signal(ms))
    return p

def presses(times, dur, bounce=0, b=0):   # a hand: presses at given times, dur ms each, contacts bounce for `bounce` ms
    rnd = random.Random(1)
    def sig(ms):
        for t in times:
            if t <= ms < t + dur:
                if bounce and (ms - t < bounce or t + dur - ms < bounce): return (1 << b) if rnd.random() < 0.5 else 0
                return 1 << b
        return 0
    return sig

fails = []
def check(name, cond, info):
    print(("  ok   " if cond else "  FAIL ") + f"{name}: {info}")
    if not cond: fails.append(name)

print("hand")
p = run(presses([500 + 400 * k for k in range(8)], 120), 5000); check("8 normal presses = 8 actions", len(p.press) == 8 and not p.muted, f"{len(p.press)} presses, muted {p.muted}")
p = run(presses([500 + 170 * k for k in range(9)], 90), 3000); check("9 fast taps (6 a second) all count", len(p.press) == 9 and not p.muted, f"{len(p.press)}")
p = run(presses([500 + 400 * k for k in range(8)], 120, bounce=6), 5000); check("bouncy contacts: still one action per press", len(p.press) == 8, f"{len(p.press)}")
p = run(presses([500], 20), 2000); check("a 20 ms brush does nothing", len(p.press) == 0, f"{len(p.press)}")
p = run(presses([500], 800), 2000); check("hold A 0.8 s = one long press, no tap", len(p.hold) == 1 and len(p.press) == 0, f"hold {len(p.hold)} press {len(p.press)}")
p = run(presses([500], 800, b=5), 2000); check("F held 0.8 s = a short press (page), not Home/Uni", len(p.hold) == 0 and len(p.press) == 1, f"hold {len(p.hold)} press {len(p.press)}")
p = run(presses([500], 1200, b=5), 2500); check("F held 1.2 s = Home/Uni once", len(p.hold) == 1, f"hold {len(p.hold)}")

p = run(presses([500 + 1600 * k for k in range(38)], 150), 62000); check("a hand pressing the same button every 1.6 s for a minute is never stopped", len(p.press) == 38 and not p.muted, f"{len(p.press)} muted {p.muted}")
print("fast pages (v13.1)")
p = run(presses([500 + 300 * k for k in range(12)], 100), 5000); check("Log page: A 12 times at 3 a second is still caught (as before)", p.muted > 0, f"{len(p.press)} presses, muted {p.muted}")
p = run(presses([500 + 300 * k for k in range(15)], 100), 6000, page="fast"); check("Remotes / Deck: Vol+ with A 15 times at 3 a second all count", len(p.press) == 15 and not p.muted, f"{len(p.press)} presses, muted {p.muted}")
p = run(presses([500 + 250 * k for k in range(30)], 90), 9000, page="typing"); check("keyboard: Del with A 30 times at 4 a second all count", len(p.press) == 30 and not p.muted, f"{len(p.press)} presses, muted {p.muted}")
p = run(presses([500 + 1150 * k for k in range(52)], 100), 62000, page="fast"); check("a quick Sudoku player: 52 presses of A in a minute all count", len(p.press) == 52 and not p.muted, f"{len(p.press)} presses, muted {p.muted}")
p = run(presses([500 + 300 * k for k in range(12)], 100, b=4), 5000, page="fast"); check("E (= +1) keeps the normal rule on those pages", p.muted > 0, f"{len(p.press)} presses, muted {p.muted}")
print("pocket / stuck")
p = run(presses([500], 120), 2000, off=True); check("screen off: a press only wakes", p.wakes == 1 and len(p.press) == 0, f"wakes {p.wakes} press {len(p.press)}")
p = run(lambda ms: 1 if ms > 100 else 0, 3600_000); check("stuck for 1 hour: 1 hold, 0 presses", len(p.hold) == 1 and len(p.press) == 0, f"hold {len(p.hold)} press {len(p.press)}")

print("loose contact (chatter), 1 hour")
worst = 0
for seed in range(20):
    rnd = random.Random(seed); state = 0; nxt = 0
    on_ms, off_ms = rnd.choice([(40, 60), (35, 200), (60, 300), (100, 400), (45, 90)])
    def sig(ms):
        global state, nxt
        if ms >= nxt:
            state ^= 1; nxt = ms + rnd.randint(on_ms // 2, on_ms * 2) if state else ms + rnd.randint(off_ms // 2, off_ms * 2)
        return state
    state, nxt = 0, 0
    p = run(sig, 3600_000)
    worst = max(worst, len(p.press))
    print(f"    seed {seed:2d} on~{on_ms}ms off~{off_ms}ms -> {len(p.press):3d} actions in 1 h, muted {p.muted}x")
check("a loose contact makes at most 100 actions an hour, most in the first minutes (v11.9: 720 logs)", worst <= 100, f"worst {worst}")
print("loose A contact on a fast page (Remotes / Deck / Sudoku), 1 hour")
worstF = 0
for seed in range(20):
    rnd = random.Random(seed); state = 0; nxt = 0
    on_ms, off_ms = rnd.choice([(40, 60), (35, 200), (60, 300), (100, 400), (45, 90)])
    state, nxt = 0, 0
    p = run(sig, 3600_000, page="fast")
    worstF = max(worstF, len(p.press))
    print(f"    seed {seed:2d} on~{on_ms}ms off~{off_ms}ms -> {len(p.press):4d} actions in 1 h, muted {p.muted}x")
check("there it is still stopped: at most 300 actions an hour (a hand: never stopped, see above)", worstF <= 300, f"worst {worstF}")
sys.exit(1 if fails else 0)



