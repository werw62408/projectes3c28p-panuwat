#!/usr/bin/env python3
"""จำลองทั้งระบบบนคอม Linux: เฟิร์มแวร์ ESP32 (คอมไพล์ด้วย build.sh) + เซิร์ฟเวอร์ Pi ตัวจริง

    ./build.sh
    python3 run_sim.py --clip ~/clip.mp4 --model ~/yolov8n.pt
    แล้วเปิด http://localhost:5055

- เฟิร์มแวร์กับเซิร์ฟเวอร์คุยกันผ่าน pseudo-terminal เหมือนสาย USB จริง
- ใช้คลิปวิดีโอแทนเว็บแคม (เล่นวน)
- log ของเฟิร์มแวร์ (ปั๊ม/ไฟ/มอเตอร์ เปิดปิดตอนไหน) อยู่ที่ build/fw_pins.log
"""
import argparse
import os
import pty
import subprocess
import sys
import tempfile
import time
import tty
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = os.path.join(HERE, "..", "raspberry_pi_server.py")


class Sim:
    def __init__(self, clip, model, port=5055, pin="", fw="build/fw_host"):
        self.clip, self.model, self.port, self.pin = clip, model, port, pin
        self.fw_exe = os.path.join(HERE, fw)
        self.data_dir = tempfile.mkdtemp(prefix="bird_sim_")
        self.master, self.slave = pty.openpty()
        tty.setraw(self.slave)
        self.slave_path = os.ttyname(self.slave)
        self.fw = None
        self.server = None
        self.fw_log = open(os.path.join(HERE, "build", "fw_pins.log"), "a")
        self.server_log_path = os.path.join(HERE, "build", "server.log")

    @property
    def url(self):
        return f"http://127.0.0.1:{self.port}"

    def start_board(self, reset="POWERON"):
        env = dict(os.environ, SIM_RESET=reset)
        self.fw_log.write(f"----- board start RST={reset} -----\n")
        self.fw_log.flush()
        self.fw = subprocess.Popen([self.fw_exe], stdin=self.master, stdout=self.master,
                                   stderr=self.fw_log, env=env)

    def reset_board(self, reason="BROWNOUT"):
        """จำลองบอร์ดรีเซ็ต (ไฟตก) — สายยังเสียบอยู่ Pi ไม่เห็นพอร์ตหาย"""
        self.fw.kill()
        self.fw.wait()
        time.sleep(0.3)
        self.start_board(reason)

    def start(self):
        self.start_board()
        env = dict(os.environ, BIRD_SERIAL=self.slave_path, BIRD_CAM=self.clip,
                   BIRD_MODEL=self.model, BIRD_DATA_DIR=self.data_dir,
                   BIRD_PORT=str(self.port), BIRD_PIN=self.pin, PYTHONUNBUFFERED="1")
        self.server = subprocess.Popen([sys.executable, SERVER], env=env,
                                       stdout=open(self.server_log_path, "w"),
                                       stderr=subprocess.STDOUT)
        for _ in range(120):
            try:
                urllib.request.urlopen(self.url + "/health", timeout=1)
                return self
            except OSError:
                time.sleep(0.5)
        raise RuntimeError("เซิร์ฟเวอร์ไม่ขึ้น ดู build/server.log")

    def stop(self):
        for p in (self.server, self.fw):
            if p and p.poll() is None:
                p.kill()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clip", required=True, help="คลิปวิดีโอที่ใช้แทนเว็บแคม")
    ap.add_argument("--model", required=True, help="ไฟล์ yolov8n.pt")
    ap.add_argument("--port", type=int, default=5055)
    ap.add_argument("--pin", default="")
    a = ap.parse_args()
    sim = Sim(a.clip, a.model, a.port, a.pin).start()
    print(f"เปิด {sim.url}  (Ctrl+C เพื่อหยุด)")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        sim.stop()


if __name__ == "__main__":
    main()
