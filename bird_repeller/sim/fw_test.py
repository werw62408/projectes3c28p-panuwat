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

        # ---------- รอบไล่ ----------
        time.sleep(1.5)
        r = b.cmd("REPEL")
        check("REPEL -> OK REPEL", first(r, "OK REPEL") is not None, r)
        r = b.cmd("REPEL")
        check("REPEL ซ้ำระหว่างไล่ -> ERR BUSY", first(r, "ERR BUSY") is not None, r)
        ln, seen = b.wait_for("EVT REPEL DONE", 10)
        check("รอบไล่จบเอง EVT REPEL DONE", ln is not None, seen)
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

        # ---------- PIR ----------
        b.set_ctl(PIR=1, ECHO_CM=12)
        ln, seen = b.wait_for("EVT PIR", 2)
        check("PIR ยังไม่ทริกช่วงอุ่นเครื่อง 60 วิ", ln is None, seen)

        # ---------- watchdog ----------
        b.cmd("MOT R 100")
        ln, seen = b.wait_for("EVT SAFE TIMEOUT", 18)
        check("Pi เงียบเกิน 15 วิ -> EVT SAFE TIMEOUT", ln is not None, seen)
    finally:
        b.close()

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
