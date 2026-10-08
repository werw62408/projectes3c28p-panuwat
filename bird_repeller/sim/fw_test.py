#!/usr/bin/env python3
"""ทดสอบเฟิร์มแวร์ (รันบนคอมผ่าน build/fw_host_test) ด้วยการคุยทาง Serial จริง

    ./build.sh && python3 fw_test.py
"""
import os
import pty
import re
import subprocess
import sys
import tempfile
import time
import tty

HERE = os.path.dirname(os.path.abspath(__file__))
FAILS = []


class Board:
    def __init__(self, exe="build/fw_host_test", reset="POWERON"):
        self.ctl = tempfile.NamedTemporaryFile("w", delete=False, suffix=".ctl")
        self.set_ctl(PIR=0, ECHO_CM=15)
        self.master, slave = pty.openpty()
        tty.setraw(slave)
        self.slave = slave
        self.log = open(os.path.join(HERE, "build", "fw_pins.log"), "w")
        env = dict(os.environ, SIM_CTL=self.ctl.name, SIM_RESET=reset)
        self.p = subprocess.Popen([os.path.join(HERE, exe)], stdin=self.master,
                                  stdout=self.master, stderr=self.log, env=env)
        self.buf = b""
        os.set_blocking(slave, False)

    def set_ctl(self, **kv):
        with open(self.ctl.name, "w") as f:
            for k, v in kv.items():
                f.write(f"{k}={v}\n")

    def lines(self, wait=0.2):
        end = time.time() + wait
        out = []
        while time.time() < end:
            try:
                self.buf += os.read(self.slave, 4096)
            except BlockingIOError:
                time.sleep(0.02)
            while b"\n" in self.buf:
                ln, self.buf = self.buf.split(b"\n", 1)
                ln = ln.decode().strip()
                if ln:
                    out.append(ln)
        return out

    def cmd(self, c, wait=0.25):
        os.write(self.slave, (c + "\n").encode())
        return self.lines(wait)

    def wait_for(self, pat, timeout):
        end = time.time() + timeout
        seen = []
        while time.time() < end:
            for ln in self.lines(0.2):
                seen.append(ln)
                if re.search(pat, ln):
                    return ln, seen
        return None, seen

    def stat(self):
        for ln in self.cmd("STAT"):
            if ln.startswith("OK POS="):
                return dict(t.split("=", 1) for t in ln.split()[1:])
        return {}

    def close(self):
        self.p.kill()
        self.log.close()


def check(name, ok, detail=""):
    print(("  ✔ " if ok else "  ✘ ") + name + ("" if ok else f"   <-- {detail}"))
    if not ok:
        FAILS.append(name)


def first(lines, prefix):
    return next((l for l in lines if l.startswith(prefix)), None)


def has(lines, exact):
    return any(l == exact for l in lines)


def pir_tests():
    print("  -- PIR --")
    b = Board()
    try:
        t0 = time.time()
        b.lines(0.5)
        st = b.stat()
        check("หลังบูต PIR ยังอุ่นเครื่อง (PIRW > 0, PIR=0)", int(st.get("PIRW", 0)) > 0 and st.get("PIR") == "0", st)
        b.set_ctl(PIR=1, ECHO_CM=15)                    # ช่วงอุ่นเครื่อง OUT กระตุกมั่ว
        time.sleep(1.0)
        b.set_ctl(PIR=0, ECHO_CM=15)
        seen = []
        ln = None
        while time.time() - t0 < 25 and ln is None:
            ln, more = b.wait_for("EVT PREADY", 1.0)
            seen += more
        dt = time.time() - t0
        check(f"อุ่นเครื่องแบบปรับตัว: พร้อมใน {dt:.0f} วิ (ไม่ต้องรอ 60)", ln is not None and 10 <= dt <= 17, (dt, seen))
        check("ช่วงอุ่นเครื่องไม่มีการทริก", not has(seen, "EVT PIR"), seen)

        # สัญญาณกระตุกสั้น ๆ (เช่นจากรีเลย์/มอเตอร์) ต้องถูกกรองทิ้ง
        out = []
        for _ in range(6):
            b.set_ctl(PIR=1, ECHO_CM=15)
            time.sleep(0.12)
            b.set_ctl(PIR=0, ECHO_CM=15)
            out += b.lines(0.4)
        st = b.stat()
        check("สัญญาณกระตุกสั้น ๆ ไม่ทริก", not has(out, "EVT PIR") and st.get("PIRN") == "0", out)
        check("นับสัญญาณกระตุกไว้ (PIRG > 0)", int(st.get("PIRG", 0)) > 0, st)

        # การเคลื่อนไหวจริง
        b.set_ctl(PIR=1, ECHO_CM=15)
        r = b.lines(0.8)
        check("เคลื่อนไหวจริง -> EVT PLVL 1 + EVT PIR ภายใน 0.8 วิ", has(r, "EVT PLVL 1") and has(r, "EVT PIR"), r)
        ln, seen = b.wait_for("^EVT PIR REP$", 6)
        check("OUT ยัง HIGH ค้าง -> ทริกซ้ำทุก 5 วิ (EVT PIR REP)", ln is not None, seen)
        st = b.stat()
        check("ตัวนับแยก: ทริกใหม่ PIRN=1 / ทริกซ้ำ PIRR=1", st.get("PIRN") == "1" and st.get("PIRR") == "1", st)
        b.set_ctl(PIR=0, ECHO_CM=15)
        ln, seen = b.wait_for("EVT PLVL 0", 1.5)
        held = int(ln.split()[3]) if ln else 0
        check(f"OUT กลับเป็น LOW -> EVT PLVL 0 บอกเวลาที่ HIGH ({held} ms)", ln is not None and held >= 5000, seen)

        # ตอนมอเตอร์หมุน ต้องไม่ทริก
        time.sleep(5.2)
        b.cmd("MOT R 300", wait=0.02)
        b.set_ctl(PIR=1, ECHO_CM=15)
        r = b.lines(1.2)
        st = b.stat()
        check("มอเตอร์หมุนอยู่ -> ไม่ทริก (นับเป็น PIRI)", not has(r, "EVT PIR") and int(st.get("PIRI", 0)) >= 1, (r, st))
        b.set_ctl(PIR=0, ECHO_CM=15)
        b.lines(0.5)
    finally:
        b.close()

    # OUT ค้าง HIGH นานโดยไม่เจอนก -> เลิกทริกซ้ำ / เจอนกระหว่างค้าง -> ทริกซ้ำต่อ
    b = Board()
    try:
        b.wait_for("EVT PREADY", 25)
        b.lines(0.3)
        b.set_ctl(PIR=1, ECHO_CM=15)
        t0 = time.time()
        ln, seen = b.wait_for("EVT PSTUCK 1", 33)
        dt = time.time() - t0
        reps = sum(1 for l in seen if l == "EVT PIR REP")
        check(f"ค้าง HIGH ไม่เจอนก -> EVT PSTUCK 1 หลัง {dt:.0f} วิ (ทริกซ้ำไป {reps} รอบ)",
              ln is not None and 29 <= dt <= 32 and reps == 5, seen)
        r = b.lines(11)
        st = b.stat()
        check("ค้างแล้วเลิกทริกซ้ำ (11 วิไม่มี EVT PIR REP) / STAT PSTK=1",
              not has(r, "EVT PIR REP") and st.get("PSTK") == "1", (r, st))
        b.cmd("BIRD 1", wait=0)                         # กล้องกลับมาเจอนก
        seen = b.lines(1.0)
        ln, ln2 = has(seen, "EVT PSTUCK 0") or None, has(seen, "EVT PIR REP") or None
        check("กล้องเจอนกระหว่างค้าง -> PSTUCK 0 และทริกซ้ำต่อทันที", ln is not None and ln2 is not None, seen)
        r = b.lines(31)
        check("30 วิหลังเจอนกครั้งสุดท้าย ถ้ายังค้าง -> PSTUCK 1 อีก", has(r, "EVT PSTUCK 1") or
              any(l.startswith("EVT PSTUCK 1") for l in r), r)
        b.set_ctl(PIR=0, ECHO_CM=15)
        r = b.lines(1.0)
        st = b.stat()
        check("OUT กลับเป็น LOW -> PSTUCK 0", any(l == "EVT PSTUCK 0" for l in r) and st.get("PSTK") == "0", (r, st))
    finally:
        b.close()

    # OUT ค้าง HIGH ตั้งแต่เปิดเครื่อง -> รอถึงเพดาน 60 วิ และไม่ทริกหลอกตอนอุ่นเสร็จ
    b = Board()
    try:
        b.set_ctl(PIR=1, ECHO_CM=15)
        ln, seen = b.wait_for("EVT PREADY", 63)
        check("OUT ค้าง HIGH ตลอด -> อุ่นเครื่องจนถึงเพดาน 60 วิ", ln is not None and int(ln.split()[2]) >= 59, (ln, seen[-3:]))
        r = b.lines(2.0)
        check("อุ่นเสร็จตอน OUT เป็น HIGH -> ไม่ทริกหลอกทันที", not has(r, "EVT PIR"), r)
    finally:
        b.close()


def main():
    b = Board()
    try:
        boot = b.lines(0.8)
        check("บูตแล้วส่ง EVT BOOT (ไม่ใช่ OK READY)",
              any(l.startswith("EVT BOOT") and "RST=POWERON" in l for l in boot), boot)

        r = b.cmd("PING")
        check("PING -> OK PONG", first(r, "OK PONG") is not None, r)
        st = b.stat()
        check("STAT มี POSOK/PDUTY/WFAULT/RST", all(k in st for k in ("POSOK", "PDUTY", "WFAULT", "RST")), st)
        check("หลังบูต POSOK=0 (ยังไม่ได้ตั้งจุดกลาง)", st.get("POSOK") == "0", st)

        r = b.cmd("CAM 1")
        check("CAM 1 -> OK CAM 1", first(r, "OK CAM 1") is not None, r)
        r = b.cmd("CAM x1")
        check("CAM ค่าแปลก -> ERR ARG (เดิมเจอเลข 1 ตรงไหนก็เปิด)", first(r, "ERR ARG") is not None, r)

        # ---------- ไฟส้ม (เจอนก) ----------
        r = b.cmd("BIRD 1")
        check("BIRD 1 -> ไฟส้มติด", first(r, "OK BIRD 1") is not None and "EVT BIRDLAMP 1" in r, r)
        check("STAT รายงาน BIRD=1", b.stat().get("BIRD") == "1")
        r = b.cmd("BIRD 2")
        check("BIRD ค่าแปลก -> ERR ARG", first(r, "ERR ARG") is not None, r)
        t0 = time.time()
        ln, seen = b.wait_for("EVT BIRDLAMP 0", 8)
        held = time.time() - t0
        check(f"ไฟส้มดับเองหลัง 5 วินาที ({held:.1f} วิ)", ln is not None and 4.0 <= held <= 6.0, seen)

        r = b.cmd("X" * 80)
        check("บรรทัดยาวเกิน -> ERR TOOLONG ทั้งบรรทัด", r == ["ERR TOOLONG"], r)

        # ---------- ปั๊ม ----------
        time.sleep(3.0)                                  # พ้นช่วงพักปั๊มหลังบูต
        r = b.cmd("PUMP 2000")
        check("PUMP 2000 -> OK", first(r, "OK PUMP 2000") is not None, r)
        r = b.cmd("PUMP 2000")
        check("สั่ง PUMP ซ้ำตอนปั๊มเปิด -> ERR PUMP BUSY (เดิมยืดเวลาได้)", first(r, "ERR PUMP BUSY") is not None, r)
        time.sleep(2.0)
        check("ปั๊มดับเองเมื่อครบเวลา", b.stat().get("PUMP") == "0")
        r = b.cmd("PUMP 1000")
        check("เปิดซ้ำทันทีหลังดับ -> ERR PUMP GAP", first(r, "ERR PUMP GAP") is not None, r)

        # ---------- มอเตอร์ ----------
        r = b.cmd("MOT R 300", wait=0.04)
        check("MOT R 300 -> OK MOT 300", first(r, "OK MOT 300") is not None, r)
        r = b.cmd("MOT L 10", wait=0.04)
        check("สั่งหมุนใหม่ระหว่างหมุน -> ERR MOVING", first(r, "ERR MOVING") is not None, r)
        r = b.cmd("MOT STOP", wait=0.0)
        pos_at_stop = int(b.stat().get("POS", 0))
        time.sleep(0.5)
        st = b.stat()
        check("MOT STOP ชะลอแล้วหยุด (ไม่ถึงปลายทาง 300)",
              st.get("MOVING") == "0" and 0 < int(st["POS"]) < 300, st)
        check("MOT STOP ไม่หยุดกึก (เดินต่ออีกนิดเพื่อชะลอ)", int(st["POS"]) >= pos_at_stop, (pos_at_stop, st))
        r = b.cmd("MOT ZERO")
        st = b.stat()
        check("MOT ZERO -> POS=0 POSOK=1", first(r, "OK ZERO") and st.get("POS") == "0" and st.get("POSOK") == "1", (r, st))
        r = b.cmd("MOT R 5000")
        b.wait_for("$^", 2.0)
        check("หมุนเกินกำแพง ±400 ถูกตัดที่ 400", b.stat().get("POS") == "400")
        b.cmd("MOT HOME")
        b.wait_for("$^", 2.0)

        # ---------- ตั้งค่าการกวาด (v7.7) ----------
        st = b.stat()
        check("ค่าเริ่มต้นกวาด ±40° ความเร็ว 30%", st.get("SWP") == "40" and st.get("RSP") == "30", st)
        for bad in ("RCFG 5 30", "RCFG 90 30", "RCFG 40 5", "RCFG 40 101", "RCFG 40"):
            r = b.cmd(bad)
            check(f"{bad} -> ERR", first(r, "ERR") is not None, r)
        r = b.cmd("RCFG 30 50")
        check("RCFG 30 50 -> OK", first(r, "OK RCFG 30 50") is not None, r)
        st = b.stat()
        check("STAT เห็นค่าใหม่", st.get("SWP") == "30" and st.get("RSP") == "50", st)

        # REPEL TEST: กวาดอย่างเดียว ไม่เปิดปั๊ม และไม่เกิน ±30° (133 สเต็ป)
        r = b.cmd("REPEL TEST")
        check("REPEL TEST -> OK REPEL TEST", first(r, "OK REPEL TEST") is not None, r)
        peak, pump_seen, t0 = 0, False, time.time()
        while time.time() - t0 < 8:
            st = b.stat()
            peak = max(peak, abs(int(st.get("POS", 0))))
            pump_seen |= st.get("PUMP") == "1"
            if st.get("REPEL") == "0" and st.get("MOVING") == "0":
                break
        check("REPEL TEST ไม่เปิดปั๊ม", not pump_seen)
        check("REPEL TEST กวาดถึงแต่ไม่เกิน 133 สเต็ป", 110 <= peak <= 133, peak)
        check("REPEL TEST จบที่จุดกลาง", b.stat().get("POS") == "0")
        b.cmd("RCFG 40 30")

        # ---------- รอบไล่ ----------
        time.sleep(1.5)
        r = b.cmd("REPEL")
        check("REPEL -> OK REPEL", first(r, "OK REPEL") is not None, r)
        r = b.cmd("REPEL")
        check("REPEL ซ้ำระหว่างไล่ -> ERR BUSY", first(r, "ERR BUSY") is not None, r)
        peak, done, t0 = 0, False, time.time()
        while time.time() - t0 < 10 and not done:
            for ln in b.cmd("STAT"):
                done |= ln.startswith("EVT REPEL DONE")
                m = re.search(r"\bPOS=(-?\d+)", ln)
                if m:
                    peak = max(peak, abs(int(m.group(1))))
        check("รอบไล่กวาดไม่เกิน ±40° (177 สเต็ป) ไม่ชนขอบ", 150 <= peak <= 177, peak)
        check("รอบไล่จบเอง EVT REPEL DONE", done)
        b.wait_for("$^", 1.5)
        st = b.stat()
        check("จบรอบไล่แล้วกลับจุดกลาง ปั๊มดับ", st.get("POS") == "0" and st.get("PUMP") == "0", st)
        r = b.cmd("REPEL")
        check("ไล่ซ้ำทันที (ปั๊มยังไม่พัก) -> ERR PUMP GAP ไม่กวาดแห้ง", first(r, "ERR PUMP GAP") is not None, r)

        # ---------- โควตาปั๊ม (บิลด์ทดสอบตั้งไว้ 10 วินาที) ----------
        time.sleep(3.2)
        b.cmd("PUMP 8000")                              # รวมกับรอบไล่ 4 วิ = เกินโควตา 10 วิ
        time.sleep(6.5)
        st = b.stat()
        check("โควตาตัดปั๊มก่อนครบ 8 วิ", st.get("PUMP") == "0" and st.get("PDUTY") == "0", st)
        time.sleep(3.2)
        r = b.cmd("PUMP 1000")
        check("โควตาหมด -> ERR PUMP DUTY", first(r, "ERR PUMP DUTY") is not None, r)

        # ---------- เซนเซอร์น้ำเสีย ----------
        b.set_ctl(PIR=0, ECHO_CM=-1)
        ln, seen = b.wait_for("EVT WATER FAULT", 10)
        check("เซนเซอร์น้ำอ่านไม่ได้ติดกัน -> EVT WATER FAULT", ln is not None, seen)
        st = b.stat()
        check("เซนเซอร์เสีย -> ล็อกปั๊ม WEMPTY=1", st.get("WEMPTY") == "1" and st.get("WFAULT") == "1", st)
        r = b.cmd("REPEL")
        check("เซนเซอร์เสียแล้วสั่งไล่ -> ERR WATER EMPTY", first(r, "ERR WATER EMPTY") is not None, r)
        b.set_ctl(PIR=0, ECHO_CM=12)
        ln, seen = b.wait_for("EVT WATER SENSOR OK", 6)
        check("เซนเซอร์กลับมา -> ปลดล็อก", ln is not None and b.stat().get("WEMPTY") == "0", seen)

        # ---------- watchdog ----------
        b.cmd("MOT R 100")
        ln, seen = b.wait_for("EVT SAFE TIMEOUT", 18)
        check("Pi เงียบเกิน 15 วิ -> EVT SAFE TIMEOUT", ln is not None, seen)
    finally:
        b.close()

    pir_tests()

    # ---------- รีบูตเพราะไฟตก ----------
    b = Board(reset="BROWNOUT")
    try:
        boot = b.lines(0.8)
        check("รีบูตเพราะไฟตก -> EVT BOOT ... RST=BROWNOUT",
              any("RST=BROWNOUT" in l for l in boot), boot)
    finally:
        b.close()

    print()
    print("ผ่านทั้งหมด" if not FAILS else f"ไม่ผ่าน {len(FAILS)} ข้อ")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    os.chdir(HERE)
    main()
