#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
==========================================================================
 Bird Detection Server  (Raspberry Pi 4 Model B)  —  v5-usb
--------------------------------------------------------------------------
 เปลี่ยนจาก v5 (รุ่นนี้):
   - เปลี่ยนกล้องจาก ESP32-S3-CAM (Wi-Fi) เป็นเว็บแคม USB เสียบ Pi ตรง
     ไม่ต้องรอ heartbeat / IP ของกล้องอีกต่อไป
   - มีเธรดอ่านเว็บแคมตลอดเวลา ภาพที่ใช้ตรวจจับจึงเป็นเฟรมล่าสุดเสมอ
     (ถ่ายหลังได้รับคำสั่ง และรอแกนหมุนหยุดก่อน ไม่ใช่เฟรมค้างในบัฟเฟอร์)
   - ภาพสดเสิร์ฟจาก Pi เองที่ /stream (เดิมดึงจากกล้องพอร์ต 81)

 เปลี่ยนจาก v2:
   - Pi เป็นฝ่ายดึงภาพจากกล้องเอง (GET /capture) แทนที่จะรอกล้อง poll
     ทำให้เวลาจากสั่งถึงได้ภาพลดจากหลายวินาทีเหลือระดับ 200-400 ms
   - ลูปตรวจจับย้ายจากจาวาสคริปต์บนเบราว์เซอร์ มาเป็นเธรดบน Pi
     ปิดหน้าเว็บแล้วเครื่องยังทำงานต่อ (เดิมปิดแท็บ = เครื่องหยุดตรวจจับ)
   - เพิ่ม ROI กำหนดพื้นที่สนใจ ตัดการตรวจจับนอกกรอบทิ้ง
   - เพิ่มการยืนยัน 2 เฟรมติดก่อนสั่งไล่ ลดการไล่เพราะโมเดลเห็นผิด
   - อ่านระดับน้ำและแรงดันแบตจากบอร์ดควบคุมมาแสดง
   - ฝังภาพสตรีมสดจากกล้องในหน้าเว็บ
   - ถอดเลเซอร์ออกทั้งหมด

 ติดตั้งและตั้งค่าให้รันเองตอนบูต: ดูท้ายไฟล์
==========================================================================
"""

import csv
import json
import logging
import os
import queue
import shutil
import threading
import time
import urllib.error
import urllib.request
from collections import deque
from datetime import datetime

import cv2
import numpy as np
from flask import Flask, Response, jsonify, request, send_file

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    serial = None
    list_ports = None

# ============================ CONFIG ============================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.environ.get("BIRD_DATA_DIR",
                          os.path.join(os.path.expanduser("~"), "bird_images"))
HIST_DIR = os.path.join(DATA_DIR, "history")
LOG_CSV = os.path.join(DATA_DIR, "detection_log.csv")
ROI_FILE = os.path.join(DATA_DIR, "roi.json")

MODEL_PT = os.environ.get("BIRD_MODEL", os.path.join(BASE_DIR, "yolov8n.pt"))
MODEL_NCNN = os.path.join(BASE_DIR, "yolov8n_ncnn_model")

BIRD_CLASS_ID = 14          # COCO: 14 = bird
CONF_THRESHOLD = 0.35
IMG_SIZE = 640              # 320 เร็วสุด / 480 สมดุล / 640 แม่นสุดแต่ช้า
HISTORY_KEEP = 60

# ---------- เว็บแคม USB ----------
# ใช้ชื่อจาก /dev/v4l/by-id เพราะเลข /dev/videoX สลับได้ตอนรีบูต
CAM_DEVICE = os.environ.get(
    "BIRD_CAM",
    "/dev/v4l/by-id/usb-Jieli_Technology_USB_Composite_Device-video-index0")
CAM_DEVICE_FALLBACK = 0     # ถ้าหา by-id ไม่เจอ ลอง /dev/video0
CAM_WIDTH = 640             # กล้อง DV20 ตัวนี้เคยหลุดตอนใช้ 1280x720 ต่อเนื่อง
CAM_HEIGHT = 480
CAM_FPS = 15
CAM_FETCH_TIMEOUT = 4.0     # รอเฟรมใหม่นานสุดกี่วินาที
CAM_OFFLINE_AFTER = 3.0     # ไม่มีเฟรมใหม่เกินกี่วินาที ถือว่ากล้องหลุด
CAM_WAIT_STILL_S = 5.0      # ก่อนถ่าย รอแกนหมุนหยุดนานสุดกี่วินาที
STREAM_WIDTH = 960          # ย่อภาพสดก่อนส่งให้มือถือ ลดภาระฮอตสปอต
STREAM_MAX_FPS = 10

# ---------- ลูปตรวจจับอัตโนมัติ (อยู่บน Pi ไม่ใช่บนเบราว์เซอร์แล้ว) ----------
AUTO_DETECT_DEFAULT = True  # เปิดลูปตรวจจับตั้งแต่สตาร์ทไหม
AUTO_INTERVAL_S = 10        # ตรวจทุกกี่วินาที

# ---------- PIR ----------
# PIR ใช้งานได้แล้ว: เจอความเคลื่อนไหวเมื่อไหร่ สั่งตรวจจับทันที ไม่ต้องรอรอบ 10 วิ
PIR_TRIGGER_ENABLED = True

# ---------- เก็บชุดข้อมูลสำหรับเทรนโมเดลเอง ----------
# บันทึก "ภาพดิบ" (ไม่มีกรอบวาดทับ) ลง DATASET_DIR ไม่ลบทิ้งเอง
# ชื่อไฟล์บอกเหตุผลที่ถูกเก็บ: เวลา_เหตุผล_จำนวนนก.jpg
#   manual  = กดปุ่มถ่ายภาพบนหน้าเว็บ
#   bird    = YOLO เจอนก (เว้นระยะกันภาพซ้ำ)
#   lowconf = YOLO เห็นแต่ความมั่นใจต่ำกว่าเกณฑ์  <- มักเป็นนกที่โมเดลพลาด
#   pir     = PIR ทริกแต่ YOLO ไม่เจอ            <- มักเป็นนกที่โมเดลพลาด
#   motion  = ภาพเปลี่ยนเยอะแต่ YOLO ไม่เจอ
#   random  = สุ่มเก็บทุก ๆ N รอบ ไว้เป็นภาพพื้นหลังไม่มีนก
DATASET_ENABLED = True
DATASET_DIR = os.path.join(DATA_DIR, "dataset")
DATASET_LOG = os.path.join(DATASET_DIR, "dataset_log.csv")
DATASET_LOWCONF = 0.10          # เก็บกรอบที่ความมั่นใจตั้งแต่เท่านี้ แม้จะต่ำกว่าเกณฑ์นับ
DATASET_MOTION_FRAC = 0.02      # สัดส่วนพิกเซลที่เปลี่ยน (2%) ถึงจะถือว่ามีอะไรขยับ
DATASET_RANDOM_EVERY = 5        # สุ่มเก็บทุก ๆ กี่รอบ
DATASET_BIRD_MIN_GAP_S = 30     # ภาพที่เจอนก เก็บห่างกันอย่างน้อยกี่วินาที (นกเกาะนิ่งจะได้ไม่ซ้ำเป็นร้อยใบ)
DATASET_MIN_FREE_MB = 1024      # พื้นที่ว่างเหลือน้อยกว่านี้ หยุดเก็บ

# ---------- การยืนยันก่อนสั่งไล่ ----------
# โมเดล COCO ที่ยังไม่ได้เทรนเองมีโอกาสเห็นผิด การบังคับให้เจอติดกันหลายเฟรม
# ช่วยตัดพวกที่โผล่มาเฟรมเดียวแล้วหายไปได้มาก
CONFIRM_FRAMES = 2          # ต้องเจอนกติดกันกี่ครั้งถึงจะสั่งไล่ (1 = ไม่ต้องยืนยัน)
CONFIRM_WINDOW_S = 30       # ถ้าห่างกันเกินนี้ ให้เริ่มนับใหม่

# ---------- ชุดขับไล่ ----------
SERIAL_PORT = os.environ.get("BIRD_SERIAL", "auto")
SERIAL_BAUD = 115200
HW_POLL_SEC = 3.0
HW_POLL_BUSY_SEC = 0.35
HW_LINK_LOST = 10.0

AUTO_REPEL_DEFAULT = True
REPEL_COOLDOWN_S = 10
MOTOR_STEP_MAX = 400
PUMP_MS_MAX = 8000
STEPS_PER_REV = 1600

# ============================ โหมดทดสอบ ============================
# True  = โชว์กรอบอย่างเดียว: ROI เริ่มแบบปิด, ไม่ต้องยืนยันหลายเฟรม,
#         ไม่สั่งไล่เอง และลดเกณฑ์คะแนนให้เห็นกรอบง่ายขึ้น
#         (ROI กับการไล่เอง เปิด-ปิดจากหน้าเว็บได้ตามปกติ)
# False = กลับไปใช้ค่าจริงข้างบนทั้งหมด (ใช้ตอนวันสาธิต)
TEST_MODE = True

if TEST_MODE:
    CONF_THRESHOLD = 0.25       # ต่ำลงนิดนึง จะได้เห็นว่าโมเดลเห็นอะไรบ้าง
    CONFIRM_FRAMES = 1          # เจอครั้งเดียวก็นับ ไม่ต้องรอยืนยัน
    AUTO_REPEL_DEFAULT = False  # ไม่สั่งปั๊ม/มอเตอร์เอง แค่ตีกรอบโชว์
                                # (อยากลองไล่จริงก็เปิดได้จากหน้าเว็บ)

_KNOWN_USB = {
    (0x10C4, 0xEA60),   # CP2102
    (0x1A86, 0x7523),   # CH340
    (0x1A86, 0x55D4),   # CH9102
    (0x303A, 0x1001),   # ESP32-S3 USB ในตัว
    (0x0403, 0x6001),   # FT232
}

os.makedirs(DATA_DIR, exist_ok=True)
os.makedirs(HIST_DIR, exist_ok=True)

app = Flask(__name__)


class _QuietPoll(logging.Filter):
    def filter(self, record):
        msg = record.getMessage()
        return "/register" not in msg and "/api/state" not in msg


logging.getLogger("werkzeug").addFilter(_QuietPoll())

# ============================ MODEL ============================
_model = None
_model_lock = threading.Lock()
_model_name = "ยังไม่ได้โหลด"


def load_model():
    global _model, _model_name
    from ultralytics import YOLO

    if os.path.isdir(MODEL_NCNN):
        _model = YOLO(MODEL_NCNN, task="detect")
        _model_name = "yolov8n (NCNN)"
    else:
        _model = YOLO(MODEL_PT)
        _model_name = "yolov8n (PyTorch)"

    dummy = np.zeros((IMG_SIZE, IMG_SIZE, 3), dtype=np.uint8)
    _model.predict(dummy, imgsz=IMG_SIZE, verbose=False)
    print(f"[Model] โหลดสำเร็จ: {_model_name}  imgsz={IMG_SIZE}  conf={CONF_THRESHOLD}",
          flush=True)


# ============================ STATE ============================
_state_lock = threading.Lock()
STATE = {
    "frame_seq": 0,
    "preview_seq": 0,
    "status": "ยังไม่มีข้อมูล",
    "bird_count": 0,
    "max_conf": 0.0,
    "infer_ms": 0,
    "shutter_ms": 0,
    "detect_ms": 0,
    "image_w": 0,
    "image_h": 0,
    "image_kb": 0,
    "last_update": None,
    "total_frames": 0,
    "total_detections": 0,
    "processing": False,
    "last_source": "-",
    # ---- กล้อง ----
    "cam_id": None,
    "cam_ip": "เว็บแคม USB",           # ใช้เป็นข้อความแสดงบนหน้าเว็บ
    "cam_last_seen": 0.0,
    "cam_rssi": None,
    # ---- การยืนยัน ----
    "confirm_streak": 0,
    "auto_detect": AUTO_DETECT_DEFAULT,
}
HISTORY = deque(maxlen=HISTORY_KEEP)

LATEST_PATH = os.path.join(DATA_DIR, "latest_bird.jpg")
PREVIEW_PATH = os.path.join(DATA_DIR, "latest_raw.jpg")

# ROI เก็บเป็นสัดส่วน 0-1 จะได้ไม่ผูกกับความละเอียดภาพ
ROI = {"enabled": False, "x1": 0.0, "y1": 0.0, "x2": 1.0, "y2": 1.0}


def load_roi():
    if TEST_MODE:
        # โหมดทดสอบ: ไม่โหลดค่าเก่าจาก roi.json เลย เริ่มแบบปิด ROI เสมอ
        # (ไฟล์เดิมไม่ถูกลบ ปิด TEST_MODE แล้วค่าเก่ากลับมาเอง)
        ROI["enabled"] = False
        print("[ROI] โหมดทดสอบ -> ปิด ROI", flush=True)
        return
    try:
        with open(ROI_FILE, encoding="utf-8") as f:
            data = json.load(f)
        for k in ROI:
            if k in data:
                ROI[k] = data[k]
        print(f"[ROI] โหลดค่าเดิม: {ROI}", flush=True)
    except (OSError, ValueError):
        pass


def save_roi():
    try:
        with open(ROI_FILE, "w", encoding="utf-8") as f:
            json.dump(ROI, f)
    except OSError:
        pass


# ============================ HELPERS ============================
def atomic_write_bytes(path, data):
    tmp = f"{path}.{os.getpid()}.part"
    with open(tmp, "wb") as f:
        f.write(data)
    os.replace(tmp, path)


def atomic_write_jpeg(path, image_bgr, quality=85):
    tmp = path + ".part.jpg"
    if not cv2.imwrite(tmp, image_bgr, [int(cv2.IMWRITE_JPEG_QUALITY), quality]):
        raise IOError(f"เขียนไฟล์ไม่สำเร็จ: {tmp}")
    os.replace(tmp, path)


_log_lock = threading.Lock()


def append_log(row):
    with _log_lock:
        new_file = not os.path.exists(LOG_CSV)
        with open(LOG_CSV, "a", newline="", encoding="utf-8-sig") as f:
            w = csv.writer(f)
            if new_file:
                w.writerow(["timestamp", "source", "bird_count", "max_conf",
                            "infer_ms", "shutter_ms", "width", "height",
                            "image_kb", "image_file", "repel", "water_pct", "vbat"])
            w.writerow(row)


def prune_history_files():
    try:
        files = sorted(
            (os.path.join(HIST_DIR, f) for f in os.listdir(HIST_DIR) if f.endswith(".jpg")),
            key=os.path.getmtime,
        )
        for old in files[:-HISTORY_KEEP]:
            os.remove(old)
    except OSError:
        pass


# ============================ DETECTION ============================
def detect_and_draw(image_bgr):
    """รัน YOLO เฉพาะคลาสนก กรองด้วย ROI แล้ววาดกรอบลงภาพ

    คืน (จำนวนนกที่นับ, ความมั่นใจสูงสุด, เวลาที่ใช้เป็น ms)
    """
    if _model is None:
        return 0, 0.0, 0, 0.0

    h, w = image_bgr.shape[:2]

    # กรอบ ROI เป็นพิกเซล
    rx1 = int(ROI["x1"] * w)
    ry1 = int(ROI["y1"] * h)
    rx2 = int(ROI["x2"] * w)
    ry2 = int(ROI["y2"] * h)

    t0 = time.perf_counter()
    with _model_lock:
        results = _model.predict(
            image_bgr,
            imgsz=IMG_SIZE,
            conf=min(CONF_THRESHOLD, DATASET_LOWCONF) if DATASET_ENABLED else CONF_THRESHOLD,
            classes=[BIRD_CLASS_ID],
            verbose=False,
        )
    infer_ms = int((time.perf_counter() - t0) * 1000)

    count = 0
    max_conf = 0.0
    weak_max = 0.0

    for r in results:
        boxes = getattr(r, "boxes", None)
        if boxes is None:
            continue
        for b in boxes:
            x1, y1, x2, y2 = (int(v) for v in b.xyxy[0].tolist())
            conf = float(b.conf[0])

            # กรอบที่ต่ำกว่าเกณฑ์: ไม่นับ ไม่วาด แต่จำไว้ให้ระบบเก็บชุดข้อมูล
            if conf < CONF_THRESHOLD:
                weak_max = max(weak_max, conf)
                continue

            # กรอง ROI โดยดูจุดกึ่งกลางกล่อง ไม่ใช่ทั้งกล่อง
            # เพราะนกที่เกาะคร่อมขอบกรอบก็ยังควรนับ
            cx = (x1 + x2) // 2
            cy = (y1 + y2) // 2
            inside = (not ROI["enabled"]) or (rx1 <= cx <= rx2 and ry1 <= cy <= ry2)

            if inside:
                count += 1
                max_conf = max(max_conf, conf)
                color = (0, 220, 0)
                label = f"bird {conf:.2f}"
            else:
                color = (120, 120, 120)          # นอกกรอบ วาดสีจาง ๆ ให้เห็นว่าเห็นแต่ไม่นับ
                label = f"({conf:.2f})"

            cv2.rectangle(image_bgr, (x1, y1), (x2, y2), color, 2)
            cv2.putText(image_bgr, label, (x1, max(18, y1 - 6)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, color, 2)

    if ROI["enabled"]:
        cv2.rectangle(image_bgr, (rx1, ry1), (rx2, ry2), (0, 160, 255), 2)
        cv2.putText(image_bgr, "ROI", (rx1 + 4, ry1 + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 160, 255), 2)

    return count, max_conf, infer_ms, weak_max


# ============================ CONTROLLER ============================
class Controller:
    """คุยกับบอร์ด ESP32-CTRL ผ่าน USB Serial"""

    def __init__(self):
        self._ser = None
        self._port = None
        self._skip_ports = set()
        self._skip_at = 0.0
        self._tx = threading.Lock()
        self._resp = queue.Queue(maxsize=8)
        self._hw_lock = threading.Lock()
        self._wake = threading.Event()
        self.last_rx = 0.0
        self.hw = {
            "pos": 0, "deg": 0.0, "pump": False,
            "moving": False, "repel": False, "pir": False,
            "water_pct": -1, "water_cm": -1.0,
            "water_low": False, "water_empty": False,
            "vbat": 0.0,
        }
        self.last_error = ""
        self.auto_repel = AUTO_REPEL_DEFAULT
        self.last_repel_at = 0.0
        self.repel_count = 0

    # ---------- การเชื่อมต่อ ----------
    def _find_port(self):
        if SERIAL_PORT != "auto":
            return SERIAL_PORT
        if list_ports is None:
            return None

        now = time.time()
        if now - self._skip_at > 60:
            self._skip_ports.clear()
            self._skip_at = now

        ports = [p for p in list_ports.comports() if p.device not in self._skip_ports]
        for p in ports:
            if (p.vid, p.pid) in _KNOWN_USB:
                return p.device
        for p in ports:
            if "ttyUSB" in p.device or "ttyACM" in p.device:
                return p.device
        return None

    def _open(self):
        port = self._find_port()
        if not port:
            self.last_error = "ไม่พบพอร์ต USB ของบอร์ดควบคุม"
            return False
        try:
            # ไม่ยืนยัน DTR/RTS ตอนเปิดพอร์ต ไม่งั้นวงจร auto-reset จะรีเซ็ตบอร์ด
            ser = serial.Serial()
            ser.port = port
            ser.baudrate = SERIAL_BAUD
            ser.timeout = 1.0
            ser.write_timeout = 2.0
            ser.dtr = False
            ser.rts = False
            ser.open()
            time.sleep(2.0)
            ser.reset_input_buffer()
            self._ser = ser
            self._port = port
            self.last_error = ""

            # ยืนยันว่าเป็นบอร์ดควบคุมจริง ไม่ใช่บอร์ดกล้องที่เสียบ USB อยู่ด้วย
            ok, line = self.send("PING", timeout=3.0)
            if not (ok and "PONG" in str(line).upper()):
                self._drop(f"{port} ไม่ตอบ PING")
                self._skip_ports.add(port)
                return False

            print(f"[CTRL] เชื่อมต่อบอร์ดควบคุมที่ {port}  ({line})", flush=True)
            return True
        except Exception as e:                                  # noqa: BLE001
            self.last_error = f"เปิดพอร์ต {port} ไม่ได้: {e}"
            return False

    def _drop(self, why):
        if self._ser is not None:
            try:
                self._ser.close()
            except Exception:                                   # noqa: BLE001
                pass
        self._ser = None
        self.last_error = why
        print(f"[CTRL] หลุดการเชื่อมต่อ: {why}", flush=True)

    @property
    def linked(self):
        return self._ser is not None and (time.time() - self.last_rx) < HW_LINK_LOST

    # ---------- เธรดเบื้องหลัง ----------
    def _reader(self):
        while True:
            ser = self._ser
            if ser is None:
                time.sleep(1.0)
                continue
            try:
                raw = ser.readline()
            except Exception as e:                              # noqa: BLE001
                self._drop(f"อ่านพอร์ตไม่ได้: {e}")
                continue
            if not raw:
                continue

            line = raw.decode("utf-8", "replace").strip()
            if not line:
                continue
            self.last_rx = time.time()

            if line.startswith("EVT"):
                self._on_event(line)
            elif line.startswith("OK") or line.startswith("ERR"):
                if line.startswith("OK POS="):
                    self._parse_stat(line)
                try:
                    self._resp.put_nowait(line)
                except queue.Full:
                    pass
            else:
                print(f"[CTRL] {line}", flush=True)

    def _connector(self):
        while True:
            if self._ser is None:
                self._open()
                time.sleep(3.0 if self._ser is None else 0.5)
                continue

            ok, line = self.send("STAT", timeout=4.0)
            if not ok and "ไม่ตอบ" in str(line):
                self._drop("บอร์ดไม่ตอบ STAT")
                continue

            with self._hw_lock:
                busy = (self.hw["moving"] or self.hw["pump"] or self.hw["repel"])

            self._wake.wait(timeout=HW_POLL_BUSY_SEC if busy else HW_POLL_SEC)
            self._wake.clear()

    def start(self):
        if serial is None:
            self.last_error = "ยังไม่ได้ติดตั้ง pyserial (pip install pyserial)"
            print(f"[CTRL] {self.last_error}", flush=True)
            return
        threading.Thread(target=self._reader, daemon=True).start()
        threading.Thread(target=self._connector, daemon=True).start()

    # ---------- รับ-ส่ง ----------
    def send(self, cmd, timeout=6.0):
        if serial is None:
            return False, "ยังไม่ได้ติดตั้ง pyserial"
        with self._tx:
            ser = self._ser
            if ser is None:
                return False, "ยังไม่ได้เชื่อมต่อบอร์ดควบคุม"
            while True:
                try:
                    self._resp.get_nowait()
                except queue.Empty:
                    break
            try:
                ser.write((cmd.strip() + "\n").encode())
                ser.flush()
            except Exception as e:                              # noqa: BLE001
                self._drop(f"เขียนพอร์ตไม่ได้: {e}")
                return False, "เขียนพอร์ตไม่ได้"
            try:
                line = self._resp.get(timeout=timeout)
            except queue.Empty:
                return False, "บอร์ดไม่ตอบ"

        if not cmd.strip().upper().startswith("STAT"):
            self._wake.set()
        return (not line.startswith("ERR")), line

    def _parse_stat(self, line):
        vals = {}
        for tok in line.split():
            if "=" in tok:
                k, v = tok.split("=", 1)
                vals[k] = v
        with self._hw_lock:
            try:
                self.hw["pos"] = int(vals.get("POS", 0))
                self.hw["deg"] = round(float(vals.get("DEG", 0.0)), 1)
                self.hw["pump"] = vals.get("PUMP") == "1"
                self.hw["moving"] = vals.get("MOVING") == "1"
                self.hw["repel"] = vals.get("REPEL") == "1"
                self.hw["pir"] = vals.get("PIR") == "1"
                self.hw["water_pct"] = int(vals.get("WPCT", -1))
                self.hw["water_cm"] = round(float(vals.get("WCM", -1.0)), 1)
                self.hw["water_low"] = vals.get("WLOW") == "1"
                self.hw["water_empty"] = vals.get("WEMPTY") == "1"
                self.hw["vbat"] = round(float(vals.get("VBAT", 0.0)), 2)
            except ValueError:
                pass

    def _on_event(self, line):
        print(f"[CTRL] {line}", flush=True)
        if line.startswith("EVT SAFE"):
            self.last_error = "บอร์ดตัดโหลดเองเพราะขาดการติดต่อกับ Pi"
        elif line.startswith("EVT LINK OK"):
            self.last_error = ""
        elif line.startswith("EVT WATER EMPTY"):
            self.last_error = "น้ำหมด ปั๊มถูกล็อกไว้"
        elif line.startswith("EVT WATER OK"):
            self.last_error = ""
        elif line.startswith("EVT PIR"):
            if PIR_TRIGGER_ENABLED:
                request_detection("pir")

    def snapshot(self):
        with self._hw_lock:
            hw = dict(self.hw)
        now = time.time()
        left = REPEL_COOLDOWN_S - (now - self.last_repel_at)
        hw.update({
            "link": self.linked,
            "port": self._port,
            "error": self.last_error,
            "auto_repel": self.auto_repel,
            "cooldown_left": round(left, 1) if left > 0 else 0,
            "repel_count": self.repel_count,
            "steps_per_rev": STEPS_PER_REV,
        })
        return hw

    # ---------- ตรรกะไล่อัตโนมัติ ----------
    def maybe_auto_repel(self, bird_count):
        """เรียกหลังตรวจจับเสร็จ คืนข้อความเหตุผลเพื่อบันทึกลง log"""
        if bird_count <= 0:
            with _state_lock:
                STATE["confirm_streak"] = 0
            return "ไม่เจอนก"

        # นับเฟรมที่เจอติดกัน
        now = time.time()
        with _state_lock:
            last = STATE.get("_last_hit_at", 0.0)
            if now - last > CONFIRM_WINDOW_S:
                STATE["confirm_streak"] = 0
            STATE["confirm_streak"] += 1
            STATE["_last_hit_at"] = now
            streak = STATE["confirm_streak"]

        if streak < CONFIRM_FRAMES:
            return f"รอยืนยัน {streak}/{CONFIRM_FRAMES}"
        if not self.auto_repel:
            return "ปิดโหมดอัตโนมัติอยู่"
        if not self.linked:
            return "ไม่ได้เชื่อมต่อบอร์ด"
        with self._hw_lock:
            if self.hw["water_empty"]:
                return "น้ำหมด"
            if self.hw["repel"] or self.hw["moving"] or self.hw["pump"]:
                return "กำลังทำงานอยู่"
        if now - self.last_repel_at < REPEL_COOLDOWN_S:
            return "อยู่ในช่วงพัก"

        ok, line = self.send("REPEL", timeout=5.0)
        if ok:
            self.last_repel_at = now
            self.repel_count += 1
            with _state_lock:
                STATE["confirm_streak"] = 0      # ไล่แล้วเริ่มนับใหม่
            return "สั่งไล่แล้ว"
        return f"สั่งไล่ไม่สำเร็จ: {line}"


CTRL = Controller()


# ============================ WEBCAM ============================
class Webcam:
    """อ่านเว็บแคม USB ในเธรดของตัวเองตลอดเวลา เก็บไว้แค่เฟรมล่าสุด

    ข้อดีคือบัฟเฟอร์ของกล้องไม่มีทางค้าง ใครขอภาพก็ได้เฟรมสดเสมอ
    """

    def __init__(self):
        self._cond = threading.Condition()
        self._cap = None
        self._frame = None
        self._t = 0.0
        self._seq = 0
        self.fps = 0.0
        self.size = (0, 0)
        self.device = None
        self.last_error = "ยังไม่ได้เปิดกล้อง"

    def _open(self):
        dev = CAM_DEVICE if os.path.exists(CAM_DEVICE) else CAM_DEVICE_FALLBACK
        cap = cv2.VideoCapture(dev, cv2.CAP_V4L2)
        if not cap.isOpened():
            cap.release()
            self.last_error = f"เปิดเว็บแคม {dev} ไม่ได้ (เสียบสาย USB อยู่ไหม)"
            return None
        # MJPG ต้องตั้งก่อนขนาดภาพ ไม่งั้นหลายรุ่นจะส่งภาพดิบแล้ว fps ตก
        cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*"MJPG"))
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAM_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAM_HEIGHT)
        cap.set(cv2.CAP_PROP_FPS, CAM_FPS)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.device = dev
        self.size = (w, h)
        self.last_error = ""
        with _state_lock:
            STATE["cam_ip"] = f"เว็บแคม USB {w}x{h}"
        print(f"[Cam] เปิดเว็บแคม {dev} ได้ขนาด {w}x{h}", flush=True)
        return cap

    def _loop(self):
        fails = 0
        last = time.monotonic()
        while True:
            if self._cap is None:
                self._cap = self._open()
                if self._cap is None:
                    time.sleep(2.0)
                    continue
                fails = 0
            ok, frame = self._cap.read()
            if not ok or frame is None:
                fails += 1
                if fails >= 20:
                    print("[Cam] อ่านภาพไม่ได้ติดกันหลายครั้ง เปิดกล้องใหม่", flush=True)
                    self.last_error = "เว็บแคมหลุด กำลังเชื่อมต่อใหม่"
                    self._cap.release()
                    self._cap = None
                    time.sleep(1.0)
                else:
                    time.sleep(0.05)
                continue
            fails = 0
            now = time.monotonic()
            dt = max(now - last, 1e-3)
            last = now
            with self._cond:
                self._frame = frame
                self._t = time.time()
                self._seq += 1
                self.fps = (1.0 / dt) if self.fps == 0 else self.fps * 0.9 + (1.0 / dt) * 0.1
                self._cond.notify_all()

    def start(self):
        threading.Thread(target=self._loop, daemon=True).start()

    def online(self):
        with self._cond:
            return self._frame is not None and (time.time() - self._t) < CAM_OFFLINE_AFTER

    def frame_after(self, t0, timeout):
        """รอจนได้เฟรมที่ถ่ายหลังเวลา t0 คืนสำเนาภาพ หรือ None ถ้าหมดเวลา"""
        deadline = time.time() + timeout
        with self._cond:
            while self._frame is None or self._t <= t0:
                left = deadline - time.time()
                if left <= 0:
                    return None
                self._cond.wait(left)
            return self._frame.copy()

    def next_frame(self, last_seq, timeout=2.0):
        """ใช้กับสตรีม รอเฟรมถัดไป คืน (ภาพ, seq)"""
        with self._cond:
            if self._seq == last_seq:
                self._cond.wait(timeout)
            if self._frame is None or self._seq == last_seq:
                return None, last_seq
            return self._frame, self._seq


CAM = Webcam()


def camera_online():
    return CAM.online()


def fetch_capture():
    """ถ่ายภาพนิ่งหนึ่งใบจากเว็บแคม คืน (bytes JPEG, ข้อความผิดพลาด)

    รอให้แกนหมุนหยุดก่อน แล้วเอาเฟรมที่ถ่ายหลังจากนั้นเท่านั้น
    ภาพจะได้ไม่เบลอและไม่ใช่มุมเก่าก่อนหมุน
    """
    t_wait = time.time()
    while CTRL.snapshot().get("moving") and time.time() - t_wait < CAM_WAIT_STILL_S:
        time.sleep(0.1)

    frame = CAM.frame_after(time.time(), CAM_FETCH_TIMEOUT)
    if frame is None:
        return None, CAM.last_error or "ไม่ได้รับภาพจากเว็บแคม"
    ok, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 90])
    if not ok:
        return None, "เข้ารหัสภาพไม่สำเร็จ"
    return buf.tobytes(), ""


# ============================ DATASET ============================
_ds = {"prev_gray": None, "prev_pos": None, "rounds": 0,
       "last_bird_save": 0.0, "full_warned": False}
_ds_lock = threading.Lock()


def _motion_frac(raw, hw):
    """สัดส่วนพิกเซลที่เปลี่ยนจากรอบก่อน คืน None ถ้าเทียบไม่ได้ (รอบแรก / มอเตอร์เพิ่งหมุน)"""
    gray = cv2.cvtColor(cv2.resize(raw, (160, 120)), cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (5, 5), 0)
    pos = hw.get("pos")
    prev, prev_pos = _ds["prev_gray"], _ds["prev_pos"]
    _ds["prev_gray"], _ds["prev_pos"] = gray, pos
    if prev is None or pos != prev_pos or hw.get("moving"):
        return None           # มุมกล้องเปลี่ยน ภาพต่างทั้งเฟรมอยู่แล้ว ไม่นับ
    diff = cv2.absdiff(gray, prev)
    return float((diff > 25).mean())


def dataset_maybe_save(raw, source, count, max_conf, weak_max, hw):
    with _ds_lock:
        _ds["rounds"] += 1
        motion = _motion_frac(raw, hw)
        now = time.time()

        reason = None
        if source == "manual":
            reason = "manual"
        elif count > 0:
            if now - _ds["last_bird_save"] >= DATASET_BIRD_MIN_GAP_S:
                reason = "bird"
                _ds["last_bird_save"] = now
        elif weak_max >= DATASET_LOWCONF:
            reason = "lowconf"
        elif source == "pir":
            reason = "pir"
        elif motion is not None and motion >= DATASET_MOTION_FRAC:
            reason = "motion"
        elif _ds["rounds"] % DATASET_RANDOM_EVERY == 0:
            reason = "random"
        if reason is None:
            return

        os.makedirs(DATASET_DIR, exist_ok=True)
        free_mb = shutil.disk_usage(DATASET_DIR).free / 1e6
        if free_mb < DATASET_MIN_FREE_MB:
            if not _ds["full_warned"]:
                print(f"[Dataset] พื้นที่เหลือ {free_mb:.0f} MB หยุดเก็บชุดข้อมูล", flush=True)
                _ds["full_warned"] = True
            return

        ts = datetime.now()
        name = f"{ts.strftime('%Y%m%d_%H%M%S_%f')[:-3]}_{reason}_{count}.jpg"
        atomic_write_jpeg(os.path.join(DATASET_DIR, name), raw, quality=92)

        new_file = not os.path.exists(DATASET_LOG)
        with open(DATASET_LOG, "a", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            if new_file:
                w.writerow(["file", "time", "reason", "source", "bird_count",
                            "max_conf", "weak_conf", "motion_frac", "motor_deg"])
            w.writerow([name, ts.strftime("%Y-%m-%d %H:%M:%S"), reason, source, count,
                        round(max_conf, 3), round(weak_max, 3),
                        "" if motion is None else round(motion, 4), hw.get("deg", "")])


def dataset_stats():
    try:
        files = [f for f in os.listdir(DATASET_DIR) if f.endswith(".jpg")]
    except FileNotFoundError:
        return {"total": 0}
    out = {"total": len(files)}
    for f in files:
        parts = f.rsplit("_", 2)
        if len(parts) == 3:
            out[parts[1]] = out.get(parts[1], 0) + 1
    return out


# ============================ WORKER ============================
_job_q = queue.Queue(maxsize=2)


def request_detection(source="manual"):
    """ขอให้ทำงานตรวจจับหนึ่งรอบ คืน True ถ้ารับคำขอไว้แล้ว

    ถ้าคิวเต็มแปลว่ามีงานค้างอยู่แล้ว ไม่ต้องต่อคิวซ้ำ
    """
    try:
        _job_q.put_nowait((source, time.time()))
        return True
    except queue.Full:
        return False


def _process(source, requested_at):
    t_fetch = time.perf_counter()
    data, err = fetch_capture()
    shutter_ms = int((time.perf_counter() - t_fetch) * 1000)

    if data is None:
        with _state_lock:
            STATE["status"] = err
        print(f"[Cam] {err}", flush=True)
        return

    # เก็บภาพดิบไว้ก่อน หน้าเว็บจะได้เห็นทันทีไม่ต้องรอ YOLO
    try:
        atomic_write_bytes(PREVIEW_PATH, data)
        with _state_lock:
            STATE["preview_seq"] += 1
    except OSError:
        pass

    img = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
    if img is None:
        with _state_lock:
            STATE["status"] = "ถอดรหัสภาพไม่สำเร็จ"
        return

    h, w = img.shape[:2]

    with _state_lock:
        STATE["processing"] = True

    t_det = time.perf_counter()
    raw = img.copy()                       # เก็บภาพดิบไว้ก่อนโดนวาดกรอบทับ
    count, max_conf, infer_ms, weak_max = detect_and_draw(img)
    detect_ms = int((time.perf_counter() - t_det) * 1000)

    repel_note = CTRL.maybe_auto_repel(count)
    hw = CTRL.snapshot()

    ts = datetime.now()
    try:
        atomic_write_jpeg(LATEST_PATH, img)
    except OSError as e:
        print(f"[IO] {e}", flush=True)

    hist_file = ""
    if count > 0:
        hist_file = f"{ts.strftime('%Y%m%d_%H%M%S')}_{count}.jpg"
        try:
            atomic_write_jpeg(os.path.join(HIST_DIR, hist_file), img)
            HISTORY.appendleft({"file": hist_file, "ts": ts.strftime("%H:%M:%S"),
                                "count": count, "conf": round(max_conf, 2)})
            prune_history_files()
        except OSError:
            hist_file = ""

    with _state_lock:
        STATE["frame_seq"] += 1
        STATE["processing"] = False
        STATE["bird_count"] = count
        STATE["max_conf"] = round(max_conf, 2)
        STATE["infer_ms"] = infer_ms
        STATE["shutter_ms"] = shutter_ms
        STATE["detect_ms"] = detect_ms
        STATE["image_w"] = w
        STATE["image_h"] = h
        STATE["image_kb"] = round(len(data) / 1024, 1)
        STATE["last_update"] = time.time()
        STATE["last_source"] = source
        STATE["total_frames"] += 1
        if count > 0:
            STATE["total_detections"] += 1
        STATE["status"] = (f"พบนก {count} ตัว — {repel_note}" if count
                           else "ไม่พบนก")

    append_log([ts.strftime("%Y-%m-%d %H:%M:%S"), source, count,
                round(max_conf, 3), infer_ms, shutter_ms, w, h,
                round(len(data) / 1024, 1), hist_file, repel_note,
                hw.get("water_pct", -1), hw.get("vbat", 0.0)])

    if DATASET_ENABLED:
        try:
            dataset_maybe_save(raw, source, count, max_conf, weak_max, hw)
        except Exception as e:                                  # noqa: BLE001
            print(f"[Dataset] บันทึกไม่สำเร็จ: {e}", flush=True)


def _worker():
    while True:
        source, requested_at = _job_q.get()
        try:
            _process(source, requested_at)
        except Exception as e:                                  # noqa: BLE001
            print(f"[Worker] ผิดพลาด: {e}", flush=True)
            with _state_lock:
                STATE["processing"] = False
        finally:
            _job_q.task_done()


def _ready_loop():
    """คอยแจ้ง ESP32 ว่ากล้องออนไลน์ไหม เพื่อให้มันตัดสินใจจุดไฟส้ม"""
    last_sent = None
    while True:
        try:
            on = camera_online()
            if on != last_sent and CTRL.linked:
                ok, _ = CTRL.send(f"CAM {1 if on else 0}", timeout=3.0)
                if ok:
                    last_sent = on
                    print(f"[Ready] แจ้งบอร์ดว่ากล้อง{'ออนไลน์' if on else 'หลุด'}",
                          flush=True)
            elif not CTRL.linked:
                last_sent = None          # ต่อใหม่เมื่อไหร่ให้ส่งซ้ำ
        except Exception as e:                                  # noqa: BLE001
            print(f"[Ready] {e}", flush=True)
        time.sleep(3.0)


def _auto_loop():
    """ลูปตรวจจับอัตโนมัติ — อยู่บน Pi ปิดหน้าเว็บแล้วยังทำงานต่อ"""
    time.sleep(5.0)          # รอให้โมเดลกับบอร์ดพร้อมก่อน
    while True:
        with _state_lock:
            on = STATE["auto_detect"]
        if on:
            request_detection("auto")
        time.sleep(AUTO_INTERVAL_S)


# ============================ API ============================
@app.route("/register", methods=["POST"])
def api_register():
    """(เหลือไว้เผื่อกล้อง ESP ตัวเก่ายังส่ง heartbeat มา ตอนนี้ไม่ได้ใช้แล้ว)"""
    data = request.get_json(silent=True) or {}
    ip = data.get("ip") or request.remote_addr
    return jsonify(ok=True, ignored=True, ip=ip)


@app.route("/api/capture", methods=["POST"])
def api_capture():
    """ปุ่มถ่ายภาพเดี๋ยวนี้บนหน้าเว็บ"""
    ok = request_detection("manual")
    return jsonify(ok=ok, msg="รับคำสั่งแล้ว" if ok else "มีงานค้างอยู่ รอสักครู่")


@app.route("/api/state")
def api_state():
    with _state_lock:
        s = dict(STATE)
    s.pop("_last_hit_at", None)
    s.pop("_last_probe", None)
    s.pop("_probe_result", None)
    s["cam_online"] = camera_online()
    s["cam_fps"] = round(CAM.fps, 1)
    s["dataset"] = dataset_stats() if DATASET_ENABLED else None
    s["model"] = _model_name
    s["auto_interval"] = AUTO_INTERVAL_S
    s["confirm_frames"] = CONFIRM_FRAMES
    s["roi"] = dict(ROI)
    s["hw"] = CTRL.snapshot()
    s["history"] = list(HISTORY)[:12]
    return jsonify(s)


@app.route("/api/auto_detect", methods=["POST"])
def api_auto_detect():
    data = request.get_json(silent=True) or {}
    with _state_lock:
        STATE["auto_detect"] = bool(data.get("on", True))
        on = STATE["auto_detect"]
    return jsonify(ok=True, on=on)


@app.route("/api/roi", methods=["POST"])
def api_roi():
    data = request.get_json(silent=True) or {}
    try:
        if "enabled" in data:
            ROI["enabled"] = bool(data["enabled"])
        for k in ("x1", "y1", "x2", "y2"):
            if k in data:
                ROI[k] = max(0.0, min(1.0, float(data[k])))
        # กันกรอบกลับด้าน
        if ROI["x2"] <= ROI["x1"] or ROI["y2"] <= ROI["y1"]:
            ROI.update({"x1": 0.0, "y1": 0.0, "x2": 1.0, "y2": 1.0})
    except (TypeError, ValueError):
        return jsonify(ok=False, msg="ค่าไม่ถูกต้อง"), 400
    save_roi()
    return jsonify(ok=True, roi=dict(ROI))


@app.route("/api/auto_repel", methods=["POST"])
def api_auto_repel():
    data = request.get_json(silent=True) or {}
    CTRL.auto_repel = bool(data.get("on", True))
    return jsonify(ok=True, on=CTRL.auto_repel)


@app.route("/api/motor", methods=["POST"])
def api_motor():
    data = request.get_json(silent=True) or {}
    action = str(data.get("action", "")).lower()
    if action == "home":
        ok, line = CTRL.send("MOT HOME")
    elif action == "stop":
        ok, line = CTRL.send("MOT STOP")
    elif action in ("left", "right"):
        try:
            steps = int(data.get("steps", 10))
        except (TypeError, ValueError):
            return jsonify(ok=False, msg="ค่าก้าวไม่ถูกต้อง"), 400
        steps = max(1, min(MOTOR_STEP_MAX, steps))
        ok, line = CTRL.send(f"MOT {'L' if action == 'left' else 'R'} {steps}")
    else:
        return jsonify(ok=False, msg="คำสั่งไม่รู้จัก"), 400
    return jsonify(ok=ok, msg=line)


@app.route("/api/pump", methods=["POST"])
def api_pump():
    data = request.get_json(silent=True) or {}
    try:
        ms = int(data.get("ms", 3000))
    except (TypeError, ValueError):
        return jsonify(ok=False, msg="ค่าเวลาไม่ถูกต้อง"), 400
    ms = max(200, min(PUMP_MS_MAX, ms))
    ok, line = CTRL.send(f"PUMP {ms}")
    return jsonify(ok=ok, msg=line)


@app.route("/api/repel", methods=["POST"])
def api_repel():
    ok, line = CTRL.send("REPEL", timeout=5.0)
    if ok:
        CTRL.last_repel_at = time.time()
        CTRL.repel_count += 1
    return jsonify(ok=ok, msg=line)


@app.route("/api/abort", methods=["POST"])
def api_abort():
    ok, line = CTRL.send("ABORT", timeout=4.0)
    return jsonify(ok=ok, msg=line)


@app.route("/api/water", methods=["POST"])
def api_water():
    ok, line = CTRL.send("WATER", timeout=4.0)
    return jsonify(ok=ok, msg=line)


@app.route("/stream")
def api_stream():
    """ภาพสดจากเว็บแคม (MJPEG) เปิดดูได้ทั้งในหน้าเว็บและเบราว์เซอร์ตรง ๆ"""
    def gen():
        seq = -1
        min_gap = 1.0 / STREAM_MAX_FPS
        last_sent = 0.0
        while True:
            frame, seq_new = CAM.next_frame(seq)
            if frame is None:
                continue
            seq = seq_new
            now = time.time()
            if now - last_sent < min_gap:
                continue
            last_sent = now
            h, w = frame.shape[:2]
            if w > STREAM_WIDTH:
                frame = cv2.resize(frame, (STREAM_WIDTH, int(h * STREAM_WIDTH / w)))
            ok, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 70])
            if ok:
                yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n"
                       + buf.tobytes() + b"\r\n")

    return Response(gen(), mimetype="multipart/x-mixed-replace; boundary=frame")


@app.route("/image")
def api_image():
    path = LATEST_PATH if os.path.exists(LATEST_PATH) else PREVIEW_PATH
    if not os.path.exists(path):
        return "ยังไม่มีภาพ", 404
    return send_file(path, mimetype="image/jpeg")


@app.route("/preview")
def api_preview():
    if not os.path.exists(PREVIEW_PATH):
        return "ยังไม่มีภาพ", 404
    return send_file(PREVIEW_PATH, mimetype="image/jpeg")


@app.route("/history/<name>")
def api_history(name):
    if "/" in name or ".." in name:
        return "ไม่อนุญาต", 400
    path = os.path.join(HIST_DIR, name)
    if not os.path.exists(path):
        return "ไม่พบไฟล์", 404
    return send_file(path, mimetype="image/jpeg")


@app.route("/log")
def api_log():
    if not os.path.exists(LOG_CSV):
        return "ยังไม่มี log", 404
    return send_file(LOG_CSV, mimetype="text/csv", as_attachment=True,
                     download_name="detection_log.csv")


@app.route("/health")
def api_health():
    cam_ok = camera_online()
    return jsonify(ok=True, model=_model_name, camera=cam_ok,
                   board=CTRL.linked, ready=(cam_ok and CTRL.linked))


@app.route("/")
def index():
    resp = Response(HTML_PAGE, mimetype="text/html; charset=utf-8")
    resp.headers["Cache-Control"] = "no-store"
    return resp


# ============================ WEB PAGE ============================
HTML_PAGE = r"""<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Bird Detection Live Monitor</title>
<style>
  :root{
    --bg:#11151c; --card:#1a1f29; --line:#2a3140;
    --txt:#e7ecf3; --muted:#8b97a8; --ok:#3ddc84; --warn:#ffb020; --bad:#ff5d5d;
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--txt);
       font-family:-apple-system,"Segoe UI",Roboto,"Noto Sans Thai",sans-serif}
  .wrap{max-width:1100px;margin:0 auto;padding:16px}
  h1{font-size:20px;margin:0 0 14px}
  .card{background:var(--card);border:1px solid var(--line);
        border-radius:12px;padding:14px;margin-bottom:14px}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:10px}
  .stat{background:#151a23;border:1px solid var(--line);border-radius:10px;padding:10px}
  .stat .k{font-size:11px;color:var(--muted);margin-bottom:4px}
  .stat .v{font-size:19px;font-weight:700}
  img.shot{width:100%;border-radius:10px;display:block;background:#000}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:10px}
  button{background:#233044;color:var(--txt);border:1px solid var(--line);
         border-radius:9px;padding:9px 14px;font-size:14px;cursor:pointer}
  button:hover{background:#2c3a52}
  button.pri{background:#1f6f43;border-color:#2b8a55}
  button.dan{background:#6f2626;border-color:#8a3030}
  select,input[type=number]{background:#151a23;color:var(--txt);
         border:1px solid var(--line);border-radius:8px;padding:8px}
  label{font-size:14px}
  .pill{display:inline-block;padding:3px 9px;border-radius:99px;font-size:12px}
  .on{background:rgba(61,220,132,.15);color:var(--ok)}
  .off{background:rgba(255,93,93,.15);color:var(--bad)}
  .wn{background:rgba(255,176,32,.15);color:var(--warn)}
  .msg{font-size:13px;color:var(--muted);margin-top:8px;min-height:18px}
  .hist{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:8px}
  .hist img{width:100%;border-radius:8px;display:block}
  .hist div{font-size:11px;color:var(--muted);text-align:center;margin-top:3px}
  .two{display:grid;grid-template-columns:1fr 1fr;gap:14px}
  @media(max-width:760px){.two{grid-template-columns:1fr}}
  .bar{height:8px;background:#151a23;border-radius:99px;overflow:hidden;margin-top:6px}
  .bar i{display:block;height:100%;background:var(--ok)}
</style>
</head>
<body>
<div class="wrap">
  <h1>🦅 ระบบเฝ้าระวังและไล่นกอัตโนมัติ</h1>

  <div class="two">
    <!-- ---------- ภาพผลตรวจจับ ---------- -->
    <div class="card">
      <div style="font-size:13px;color:var(--muted);margin-bottom:8px">ภาพผลตรวจจับล่าสุด</div>
      <img class="shot" id="shot" alt="ผลตรวจจับ">
      <div class="row">
        <button class="pri" id="btnShot">📷 ถ่ายภาพเดี๋ยวนี้</button>
        <label><input type="checkbox" id="autoDet"> ตรวจอัตโนมัติ</label>
        <span class="pill" id="pillDet">-</span>
        <button id="btnLog">⬇ ดาวน์โหลด log</button>
      </div>
      <div class="msg" id="msg"></div>
    </div>

    <!-- ---------- ภาพสด ---------- -->
    <div class="card">
      <div style="font-size:13px;color:var(--muted);margin-bottom:8px">
        ภาพสดจากกล้อง <span class="pill" id="pillCam">-</span>
      </div>
      <img class="shot" id="live" alt="ภาพสด">
      <div class="row">
        <button id="btnLive">▶ เปิดภาพสด</button>
        <span style="font-size:12px;color:var(--muted)" id="camip"></span>
      </div>
    </div>
  </div>

  <!-- ---------- ตัวเลข ---------- -->
  <div class="card">
    <div class="grid">
      <div class="stat"><div class="k">จำนวนนก</div><div class="v" id="cnt">0</div></div>
      <div class="stat"><div class="k">ความมั่นใจสูงสุด</div><div class="v" id="conf">-</div></div>
      <div class="stat"><div class="k">เวลาประมวลผล</div><div class="v" id="inf">-</div></div>
      <div class="stat"><div class="k">ดึงภาพจากกล้อง</div><div class="v" id="sh">-</div></div>
      <div class="stat"><div class="k">ขนาดภาพ</div><div class="v" id="sz">-</div></div>
      <div class="stat"><div class="k">เฟรมทั้งหมด</div><div class="v" id="tot">0</div></div>
      <div class="stat"><div class="k">ครั้งที่เจอนก</div><div class="v" id="det">0</div></div>
      <div class="stat"><div class="k">ครั้งที่ไล่</div><div class="v" id="rep">0</div></div>
      <div class="stat"><div class="k">ภาพชุดข้อมูล</div><div class="v" id="ds">-</div></div>
    </div>
  </div>

  <!-- ---------- สถานะเครื่อง ---------- -->
  <div class="card">
    <div style="font-size:13px;color:var(--muted);margin-bottom:8px">สถานะเครื่อง</div>
    <div class="grid">
      <div class="stat">
        <div class="k">ระดับน้ำ</div><div class="v" id="wat">-</div>
        <div class="bar"><i id="watBar" style="width:0%"></i></div>
      </div>
      <div class="stat"><div class="k">แรงดันแบต</div><div class="v" id="vb">-</div></div>
      <div class="stat"><div class="k">มุมมอเตอร์</div><div class="v" id="deg">-</div></div>
      <div class="stat"><div class="k">บอร์ดควบคุม</div><div class="v" id="brd">-</div></div>
      <div class="stat"><div class="k">ความพร้อม</div><div class="v" id="rdy">-</div></div>
    </div>
  </div>

  <!-- ---------- ควบคุม ---------- -->
  <div class="card">
    <div style="font-size:13px;color:var(--muted);margin-bottom:8px">ชุดขับไล่</div>
    <div class="row">
      <button id="mL">◀</button>
      <select id="steps">
        <option value="5">5 สเต็ป</option>
        <option value="10" selected>10 สเต็ป</option>
        <option value="40">40 สเต็ป</option>
        <option value="120">120 สเต็ป</option>
      </select>
      <button id="mR">▶</button>
      <button id="mH">◆ กลับจุดกลาง</button>
    </div>
    <div class="row">
      <button id="bPump">💧 ปั๊มน้ำ 3 วินาที</button>
      <button id="bRepel" class="pri">🚿 สั่งไล่เดี๋ยวนี้</button>
      <button id="bWater">📏 วัดระดับน้ำใหม่</button>
      <button id="bAbort" class="dan">■ หยุดทุกอย่าง</button>
    </div>
    <div class="row">
      <label><input type="checkbox" id="autoRep"> ไล่อัตโนมัติเมื่อเจอนก</label>
      <span style="font-size:12px;color:var(--muted)" id="cdTxt"></span>
    </div>
  </div>

  <!-- ---------- ROI ---------- -->
  <div class="card">
    <div style="font-size:13px;color:var(--muted);margin-bottom:8px">
      พื้นที่สนใจ (ROI) — นกที่อยู่นอกกรอบจะไม่นับและไม่สั่งไล่
    </div>
    <div class="row">
      <label><input type="checkbox" id="roiOn"> เปิดใช้ ROI</label>
    </div>
    <div class="row">
      <label>ซ้าย <input type="number" id="rx1" step="0.05" min="0" max="1" value="0"></label>
      <label>บน <input type="number" id="ry1" step="0.05" min="0" max="1" value="0"></label>
      <label>ขวา <input type="number" id="rx2" step="0.05" min="0" max="1" value="1"></label>
      <label>ล่าง <input type="number" id="ry2" step="0.05" min="0" max="1" value="1"></label>
      <button id="roiSave">บันทึก ROI</button>
    </div>
    <div class="msg">ค่าเป็นสัดส่วน 0 ถึง 1 เทียบกับขนาดภาพ เช่น 0.25 = หนึ่งในสี่จากขอบ</div>
  </div>

  <!-- ---------- ประวัติ ---------- -->
  <div class="card">
    <div style="font-size:13px;color:var(--muted);margin-bottom:8px">ภาพย้อนหลังที่พบนก</div>
    <div class="hist" id="hist"></div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let lastFrame = -1, lastPrev = -1, liveOn = false, camIP = null;

function setMsg(t){ $('msg').textContent = t; }

async function post(url, body){
  try{
    const r = await fetch(url, {method:'POST', headers:{'Content-Type':'application/json'},
                               body: JSON.stringify(body||{})});
    return await r.json();
  }catch(e){ setMsg('ติดต่อเซิร์ฟเวอร์ไม่ได้'); return {ok:false}; }
}

function pill(el, ok, textOk, textNo, warn){
  el.textContent = ok ? textOk : textNo;
  el.className = 'pill ' + (warn ? 'wn' : (ok ? 'on' : 'off'));
}

async function tick(){
  let s;
  try{ s = await (await fetch('/api/state')).json(); }
  catch(e){ return; }

  $('cnt').textContent = s.bird_count;
  if(s.dataset){
    const d = s.dataset;
    $('ds').textContent = d.total;
    $('ds').title = ['bird','lowconf','pir','motion','manual','random']
        .map(k => k + ' ' + (d[k]||0)).join(' / ');
  }
  $('conf').textContent = s.max_conf ? s.max_conf.toFixed(2) : '-';
  $('inf').textContent = s.infer_ms ? s.infer_ms + ' ms' : '-';
  $('sh').textContent  = s.shutter_ms ? s.shutter_ms + ' ms' : '-';
  $('sz').textContent  = s.image_w ? s.image_w + '×' + s.image_h : '-';
  $('tot').textContent = s.total_frames;
  $('det').textContent = s.total_detections;
  $('rep').textContent = s.hw.repel_count;

  // ---- สถานะเครื่อง ----
  const w = s.hw.water_pct;
  if(w < 0){ $('wat').textContent = 'ไม่มีเซนเซอร์'; $('watBar').style.width = '0%'; }
  else{
    $('wat').textContent = w + '%';
    $('watBar').style.width = w + '%';
    $('watBar').style.background = s.hw.water_empty ? 'var(--bad)'
                                : (s.hw.water_low ? 'var(--warn)' : 'var(--ok)');
  }
  $('vb').textContent  = s.hw.vbat > 2 ? s.hw.vbat.toFixed(2) + ' V' : '-';
  $('deg').textContent = s.hw.deg + '°';
  $('brd').textContent = s.hw.link ? 'เชื่อมต่อแล้ว' : 'ไม่ได้เชื่อมต่อ';
  $('brd').style.color = s.hw.link ? 'var(--ok)' : 'var(--bad)';
  const ready = s.hw.link && s.cam_online;
  $('rdy').textContent = ready ? 'พร้อม (ไฟส้มติด)' : 'กำลังเตรียม';
  $('rdy').style.color = ready ? 'var(--warn)' : 'var(--muted)';

  pill($('pillDet'), s.auto_detect, 'ทุก ' + s.auto_interval + ' วิ', 'ปิดอยู่');
  pill($('pillCam'), s.cam_online, 'ออนไลน์', 'ออฟไลน์');
  $('cdTxt').textContent = s.hw.cooldown_left > 0
      ? 'พักอีก ' + s.hw.cooldown_left + ' วิ' : '';

  if(document.activeElement !== $('autoDet')) $('autoDet').checked = s.auto_detect;
  if(document.activeElement !== $('autoRep')) $('autoRep').checked = s.hw.auto_repel;

  if(s.cam_ip && s.cam_ip !== camIP){
    camIP = s.cam_ip;
    $('camip').textContent = camIP;
    if(liveOn) $('live').src = '/stream?t=' + Date.now();
  }

  // ---- ภาพ ----
  if(s.preview_seq !== lastPrev){ lastPrev = s.preview_seq;
    $('shot').src = '/preview?t=' + Date.now(); }
  if(s.frame_seq !== lastFrame){ lastFrame = s.frame_seq;
    $('shot').src = '/image?t=' + Date.now(); }

  if(s.status) setMsg(s.status + (s.processing ? ' (กำลังประมวลผล...)' : ''));

  // ---- ROI ----
  if(document.activeElement.tagName !== 'INPUT'){
    $('roiOn').checked = s.roi.enabled;
    $('rx1').value = s.roi.x1; $('ry1').value = s.roi.y1;
    $('rx2').value = s.roi.x2; $('ry2').value = s.roi.y2;
  }

  // ---- ประวัติ ----
  $('hist').innerHTML = (s.history||[]).map(h =>
    `<div><img src="/history/${h.file}" loading="lazy">
     <div>${h.ts} · ${h.count} ตัว · ${h.conf}</div></div>`).join('');
}

// ---------- ปุ่ม ----------
$('btnShot').onclick = async () => {
  setMsg('กำลังดึงภาพจากกล้อง...');
  const r = await post('/api/capture');
  if(!r.ok) setMsg(r.msg || 'สั่งไม่สำเร็จ');
};
$('autoDet').onchange = e => post('/api/auto_detect', {on: e.target.checked});
$('autoRep').onchange = e => post('/api/auto_repel', {on: e.target.checked});
$('btnLog').onclick = () => location.href = '/log';

$('btnLive').onclick = () => {
  liveOn = !liveOn;
  if(liveOn){
    $('live').src = '/stream?t=' + Date.now();
    $('btnLive').textContent = '⏸ ปิดภาพสด';
  }else{
    // removeAttribute อย่างเดียว เบราว์เซอร์บางตัวไม่ยอมตัดการเชื่อมต่อ
    // ต้องใส่ภาพเปล่าแทน ถึงจะบังคับให้ปิดสตรีมจริง
    $('live').src = 'data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7';
    liveOn = false;
    $('btnLive').textContent = '▶ เปิดภาพสด';
  }
};

const steps = () => parseInt($('steps').value, 10);
$('mL').onclick = () => post('/api/motor', {action:'left',  steps: steps()});
$('mR').onclick = () => post('/api/motor', {action:'right', steps: steps()});
$('mH').onclick = () => post('/api/motor', {action:'home'});
$('bPump').onclick  = async()=>{ const r=await post('/api/pump',{ms:3000});  setMsg(r.msg||''); };
$('bRepel').onclick = async()=>{ const r=await post('/api/repel');           setMsg(r.msg||''); };
$('bWater').onclick = async()=>{ const r=await post('/api/water');           setMsg(r.msg||''); };
$('bAbort').onclick = async()=>{ const r=await post('/api/abort');           setMsg(r.msg||''); };

$('roiOn').onchange = e => post('/api/roi', {enabled: e.target.checked});
$('roiSave').onclick = async () => {
  const r = await post('/api/roi', {
    enabled: $('roiOn').checked,
    x1: parseFloat($('rx1').value), y1: parseFloat($('ry1').value),
    x2: parseFloat($('rx2').value), y2: parseFloat($('ry2').value)});
  setMsg(r.ok ? 'บันทึก ROI แล้ว' : 'ค่าไม่ถูกต้อง');
};

tick();
setInterval(tick, 1000);
</script>
</body>
</html>"""


# ============================ MAIN ============================
def main():
    load_roi()
    print("[Boot] กำลังโหลดโมเดล...", flush=True)
    load_model()

    CAM.start()
    CTRL.start()
    threading.Thread(target=_worker, daemon=True).start()
    threading.Thread(target=_auto_loop, daemon=True).start()
    threading.Thread(target=_ready_loop, daemon=True).start()

    print(f"[Web] เปิดที่ http://0.0.0.0:5000", flush=True)
    if TEST_MODE:
        print("[Mode] *** โหมดทดสอบ: ปิด ROI / ไม่ยืนยันเฟรม / ไม่ไล่เอง / "
              f"conf={CONF_THRESHOLD} ***", flush=True)
    print(f"[Auto] ตรวจจับทุก {AUTO_INTERVAL_S} วินาที "
          f"(ยืนยัน {CONFIRM_FRAMES} เฟรมก่อนสั่งไล่)", flush=True)
    if DATASET_ENABLED:
        print(f"[Dataset] เก็บภาพดิบสำหรับเทรนที่ {DATASET_DIR}", flush=True)
    if not PIR_TRIGGER_ENABLED:
        print("[PIR] ปิดการทริกด้วย PIR อยู่ "
              "(แก้ PIR_TRIGGER_ENABLED = True เมื่อเซนเซอร์พร้อม)", flush=True)

    app.run(host="0.0.0.0", port=5000, threaded=True, debug=False)


if __name__ == "__main__":
    main()


# ==========================================================================
# วิธีติดตั้งและตั้งค่าให้รันเองตอนเปิดเครื่อง
# --------------------------------------------------------------------------
# 1) ติดตั้งไลบรารี (ทำครั้งเดียว)
#
#      mkdir -p ~/pip_tmp
#      export TMPDIR=~/pip_tmp
#      pip install ultralytics pyserial flask opencv-python-headless \
#                  --break-system-packages --timeout 120 --retries 5
#
#    ตั้ง TMPDIR ก่อนเสมอ เพราะ /tmp บน Pi เป็น RAM ขนาดเล็ก
#    ถ้าไม่ตั้งจะเจอ OSError: [Errno 28] No space left on device ตอนโหลด torch
#
# 2) แปลงโมเดลเป็น NCNN ให้เร็วขึ้น 2-3 เท่า (ทำครั้งเดียว ใช้เวลาสักพัก)
#
#      cd ~/bird
#      python3 -c "from ultralytics import YOLO; YOLO('yolov8n.pt').export(format='ncnn')"
#
#    เสร็จแล้วจะได้โฟลเดอร์ yolov8n_ncnn_model/ โปรแกรมจะเลือกใช้เองอัตโนมัติ
#
# 3) ทดสอบรันด้วยมือก่อน
#
#      cd ~/bird
#      python3 raspberry_pi_server.py
#
#    ต้องขึ้น [Model] โหลดสำเร็จ / [CTRL] เชื่อมต่อบอร์ดควบคุมที่ /dev/ttyACM0
#    แล้วเปิดเบราว์เซอร์ไปที่  http://<ip ของ Pi>:5000
#
# 4) ตั้งให้รันเองตอนบูต — ขาดข้อนี้คือต้องต่อคอม ssh เข้าไปสั่งรันทุกครั้ง
#
#      sudo nano /etc/systemd/system/bird.service
#
#    วางข้อความนี้ (แก้ชื่อผู้ใช้กับ path ให้ตรงของจริง)
#
#      [Unit]
#      Description=Bird Detection Server
#      After=network-online.target
#      Wants=network-online.target
#
#      [Service]
#      Type=simple
#      User=panuwat
#      WorkingDirectory=/home/panuwat/bird
#      ExecStart=/usr/bin/python3 /home/panuwat/bird/raspberry_pi_server.py
#      Restart=always
#      RestartSec=5
#
#      [Install]
#      WantedBy=multi-user.target
#
#    แล้วสั่ง
#
#      sudo systemctl daemon-reload
#      sudo systemctl enable bird.service
#      sudo systemctl start bird.service
#
#    ดูสถานะ / ดู log สด
#
#      systemctl status bird.service
#      journalctl -u bird.service -f
#
#    Restart=always ทำให้ถ้าโปรแกรมพังจะสตาร์ทใหม่เองใน 5 วินาที
# ==========================================================================
