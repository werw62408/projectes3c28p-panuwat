#!/usr/bin/env python3
"""จำลองการใช้หน้าเว็บจากมือถือ (Playwright) กับระบบจำลองทั้งชุด แล้วเก็บภาพหน้าจอ

    ./build.sh
    python3 web_test.py --clip clip.mp4 --model yolov8n.pt [--chromium /path/to/chrome]

ภาพหน้าจออยู่ที่ build/shots/
"""
import argparse
from datetime import datetime
import http.cookiejar
import io
import json
import os
import shutil
import re
import subprocess
import sys
import time
import urllib.error
import urllib.request
import zipfile

from playwright.sync_api import sync_playwright

from run_sim import HERE, SERVER, Sim

SHOTS = os.path.join(HERE, "build", "shots")
FAILS = []


def check(name, ok, detail=""):
    print(("  ✔ " if ok else "  ✘ ") + name + ("" if ok else f"   <-- {detail}"), flush=True)
    if not ok:
        FAILS.append(name)


def opener(sim):
    """urllib ที่ล็อกอินแล้ว (เก็บคุกกี้ไว้) ใช้เรียก API ตรง ๆ จากสคริปต์ทดสอบ"""
    op = getattr(sim, "_op", None)
    if op is None:
        op = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(http.cookiejar.CookieJar()))
        req = urllib.request.Request(sim.url + "/api/login", method="POST",
                                     data=json.dumps({"password": sim.password}).encode(),
                                     headers={"Content-Type": "application/json", "X-Bird-UI": "1"})
        op.open(req, timeout=5).read()
        sim._op = op
    return op


def get(sim, path, op=None):
    """คืน (status, body) ไม่โยน error ตอนได้ 4xx"""
    try:
        with (op or opener(sim)).open(sim.url + path, timeout=30) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def api_post(sim, path, body, op=None):
    req = urllib.request.Request(sim.url + path, method="POST", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json", "X-Bird-UI": "1"})
    try:
        with (op or opener(sim)).open(req, timeout=10) as r:
            return r.status, json.load(r)
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b"{}")


def state(sim):
    with opener(sim).open(sim.url + "/api/state", timeout=5) as r:
        return json.load(r)


def open_ui(page, sim):
    """เปิดหน้าเว็บ ถ้าเจอหน้าล็อกอินก็ใส่รหัสให้"""
    page.goto(sim.url)
    if "/login" in page.url:
        page.fill("#pw", sim.password)
        page.click("#go")
        page.wait_for_url(sim.url + "/", timeout=10000)


def wait_state(sim, pred, timeout):
    end = time.time() + timeout
    s = state(sim)
    while time.time() < end:
        s = state(sim)
        if pred(s):
            return s
        time.sleep(0.3)
    return None


def raw_post(url, body, headers):
    req = urllib.request.Request(url, data=body, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status
    except urllib.error.HTTPError as e:
        return e.code


def shot(page, name):
    page.screenshot(path=os.path.join(SHOTS, name), full_page=True)


def msg(page):
    return page.inner_text("#msg")


def wait_msg(page, text, timeout=4.0):
    end = time.time() + timeout
    while time.time() < end:
        m = msg(page)
        if text in m:
            return m
        time.sleep(0.1)
    return msg(page)


def main_flow(page, sim):
    open_ui(page, sim)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.wait_for_timeout(4000)                  # รอ STAT รอบแรก
    warns = page.inner_text("#warns")
    check("หน้าเว็บเตือนว่าอยู่ในโหมดทดสอบ", "โหมดทดสอบ" in warns, warns)
    check("หน้าเว็บเตือนว่ายังไม่ได้ตั้งจุดกลางมอเตอร์", "ตั้งจุดกลาง" in warns, warns)
    s = wait_state(sim, lambda s: "พบนก" in s["status"], 40)
    check("ตรวจเจอนกจากคลิป (ภาพเต็ม ไม่มี ROI)", s is not None, state(sim)["status"])
    page.wait_for_function("document.getElementById('cnt').textContent !== '0'", timeout=8000)
    page.wait_for_timeout(700)
    shot(page, "01_detect_full_frame.png")

    # ---------- มอเตอร์ ----------
    page.select_option("#steps", "120")
    page.click("#mR")
    page.click("#mR")
    m = wait_msg(page, "หมุนอยู่", 2)
    check("กด ▶ ซ้ำตอนมอเตอร์ยังหมุน -> ปฏิเสธพร้อมข้อความไทย", "หมุนอยู่" in m, m)
    page.wait_for_timeout(1500)
    page.once("dialog", lambda d: d.accept())
    page.click("#mZ")
    s = wait_state(sim, lambda s: s["hw"]["pos_ok"] and s["hw"]["pos"] == 0, 5)
    check("ตั้งจุดกลาง -> POS=0 และคำเตือนหายไป", s is not None)
    page.wait_for_timeout(1200)
    check("คำเตือนเรื่องจุดกลางหายไปจากหน้าเว็บ", "ตั้งจุดกลาง" not in page.inner_text("#warns"))
    page.evaluate("run('/api/motor', {action:'right', steps:400})")
    page.wait_for_timeout(60)
    page.click("#mS")
    s = wait_state(sim, lambda s: not s["hw"]["moving"], 5)
    check("กดหยุดหมุนกลางทาง -> ชะลอแล้วหยุดก่อนถึง 400 สเต็ป", s and 0 < s["hw"]["pos"] < 400, s and s["hw"]["pos"])
    page.click("#mH")
    wait_state(sim, lambda s: s["hw"]["pos"] == 0 and not s["hw"]["moving"], 5)

    # ---------- การกวาดตอนไล่ (v7.7) ----------
    page.wait_for_selector("#swBox:not([hidden])", timeout=5000)
    check("ตัวปรับการกวาดแสดงค่าเริ่มต้น ±40° / 30%",
          page.inner_text("#swDegV") == "±40°" and page.inner_text("#swSpdV") == "30%",
          (page.inner_text("#swDegV"), page.inner_text("#swSpdV")))
    page.fill("#swDeg", "25")
    page.dispatch_event("#swDeg", "input")
    page.wait_for_timeout(1500)                         # รอบอัปเดตสถานะต้องไม่ทับค่าที่กำลังแก้
    check("เลื่อนแล้วค่าไม่เด้งกลับระหว่างยังไม่บันทึก", page.input_value("#swDeg") == "25")
    page.click("#swTest")                               # ยังไม่บันทึก = บันทึกให้ก่อนแล้วทดสอบ
    m = wait_msg(page, "ทดสอบกวาด", 4)
    check("ทดสอบกวาด -> บันทึกค่าก่อนแล้วกวาด", "ทดสอบกวาด" in m, m)
    peak, pump, end = 0, False, time.time() + 8
    while time.time() < end:
        hw = state(sim)["hw"]
        peak = max(peak, abs(hw["pos"]))
        pump |= hw["pump"]
        if not hw["repel"] and not hw["moving"] and peak > 0:
            break
        time.sleep(0.1)
    check("ทดสอบกวาดไม่ฉีดน้ำ และกว้างไม่เกิน ±25° (111 สเต็ป)", not pump and 0 < peak <= 111, (pump, peak))
    s = state(sim)["hw"]
    check("บอร์ดจำค่าใหม่ SWP=25", s["sweep_deg"] == 25 and s["sweep_spd"] == 30, s.get("sweep_deg"))
    check("แถบเวลาฉีดน้ำแสดงค่าเริ่มต้น 3 วิ (บอร์ด v7.9)",
          page.is_visible("#swPumpRow") and page.inner_text("#swPumpV") == "3 วิ", page.inner_text("#swPumpV"))
    page.fill("#swPump", "4.5")
    page.dispatch_event("#swPump", "input")
    page.click("#swSave")
    s = wait_state(sim, lambda s: s["hw"]["repel_pump_ms"] == 4500, 5)
    check("ตั้งเวลาฉีดน้ำ 4.5 วิ -> บอร์ดจำ RPM=4500 และปุ่มปั๊มเปลี่ยนตาม",
          s is not None and s["pump_button_ms"] == 4500, s and (s["hw"]["repel_pump_ms"], s["pump_button_ms"]))
    page.wait_for_timeout(1200)
    check("ปุ่มปั๊มบนเว็บบอกเวลาใหม่", page.inner_text("#pumpS") == "4.5", page.inner_text("#pumpS"))
    shot(page, "01b_sweep.png")
    page.evaluate("run('/api/sweep', {deg:40, spd:30, pump_ms:3000})")
    wait_state(sim, lambda s: s["hw"]["sweep_deg"] == 40 and s["hw"]["repel_pump_ms"] == 3000, 5)
    code, _ = api_post(sim, "/api/sweep", {"deg": 40, "spd": 30, "pump_ms": 9000})
    check("เวลาฉีดน้ำเกิน 8 วิ -> ปฏิเสธ", code == 400, code)

    # ---------- ปั๊ม ----------
    page.click("#bPump")
    m = wait_msg(page, "OK PUMP", 3)
    check("กดปั๊ม 3 วินาที (เท่ารอบไล่ของบอร์ด v7.9) -> OK", "OK PUMP 3000" in m, m)
    page.click("#bPump")
    m = wait_msg(page, "ปั๊มเปิดอยู่แล้ว", 3)
    check("กดปั๊มซ้ำตอนปั๊มเปิดอยู่ -> ไม่ยืดเวลา", "ปั๊มเปิดอยู่แล้ว" in m, m)
    shot(page, "02_pump_busy.png")
    wait_state(sim, lambda s: not s["hw"]["pump"], 6)
    page.click("#bRepel")
    m = wait_msg(page, "พัก", 3)
    check("สั่งไล่ทันทีหลังปั๊มดับ -> ต้องพักก่อน (ไม่กวาดแห้ง)", "พัก" in m, m)

    # ---------- ไล่อัตโนมัติ ----------
    page.wait_for_timeout(3200)
    page.check("#autoRep")
    s = wait_state(sim, lambda s: s["hw"]["repel_count"] >= 1, 40)
    check("เปิดไล่อัตโนมัติ -> เจอนกแล้วสั่งไล่เอง", s is not None, state(sim)["status"])
    s = wait_state(sim, lambda s: s["hw"]["repel"] or s["hw"]["pump"], 5)
    page.wait_for_timeout(600)
    shot(page, "03_auto_repel.png")
    wait_state(sim, lambda s: not s["hw"]["repel"] and not s["hw"]["moving"], 15)
    page.uncheck("#autoRep")

    # ---------- ROI ----------
    check("โหมดสาธิตเริ่มแบบปิด ROI", not page.is_checked("#roiOn"))
    check("โหมดสาธิตไม่มีสวิตช์ไล่ทันที (เจอเฟรมเดียวก็นับอยู่แล้ว)", page.is_hidden("#instRow"))
    page.check("#roiOn")
    for k, v in (("cutL", 0.35), ("cutT", 0.05), ("cutR", 0.25), ("cutB", 0.45)):
        page.evaluate(f"setCut('{k}', {v})")
    check("ปรับแถบเลื่อนแล้วขึ้นว่ายังไม่บันทึก", "ยังไม่ได้บันทึก" in page.inner_text("#roiNote"))
    page.click("#roiSave")
    m = wait_msg(page, "บันทึก ROI", 3)
    check("บันทึก ROI จากหน้าเว็บ", "บันทึก ROI แล้ว" in m, m)
    r = state(sim)["roi"]
    check("ค่า ROI ที่บันทึกตรงกับแถบเลื่อน", r["enabled"] and abs(r["x1"] - 0.35) < 1e-6 and abs(r["x2"] - 0.75) < 1e-6
          and abs(r["y2"] - 0.55) < 1e-6, r)
    before = state(sim)["frame_seq"]
    s = wait_state(sim, lambda s: s["frame_seq"] > before + 1 and "พบนก" in s["status"], 40)
    check("เปิด ROI (ตัดภาพก่อนตรวจ) แล้วยังเจอนกในกรอบ", s is not None, state(sim)["status"])
    page.wait_for_function("document.getElementById('cnt').textContent !== '0'", timeout=30000)
    page.wait_for_timeout(700)
    m = msg(page)
    check("ข้อความสถานะตรงกับจำนวนนกที่แสดง", "พบนก" in m, m)
    shot(page, "04_detect_roi.png")
    page.evaluate("document.getElementById('roiCard').scrollIntoView()")
    page.wait_for_timeout(400)
    page.screenshot(path=os.path.join(SHOTS, "04b_roi_editor.png"))

    # ---------- ภาพย้อนหลัง ----------
    page.wait_for_selector("#hist .hi", timeout=5000)
    # นับว่ารายการภาพถูกสร้างใหม่กี่ครั้งใน 4 วิ เทียบกับจำนวนครั้งที่มีภาพใหม่เข้ามาจริง
    page.evaluate("""window._histBuilds = 0;
        new MutationObserver(() => window._histBuilds++).observe(document.getElementById('hist'), {childList: true})""")
    keys = set()
    for _ in range(8):
        keys.add(tuple(h["file"] for h in state(sim)["history"]))
        page.wait_for_timeout(500)
    builds = page.evaluate("window._histBuilds")
    n_before = page.evaluate("document.querySelectorAll('#hist .hi').length")
    check(f"ภาพย้อนหลังสร้างใหม่เฉพาะตอนมีภาพเพิ่ม ({builds} ครั้ง / ภาพใหม่ {len(keys) - 1} ครั้ง)",
          builds <= len(keys), (builds, len(keys)))
    page.click("#hist .hi")
    page.wait_for_function("!document.getElementById('lb').hidden && document.getElementById('lbImg').naturalWidth > 0",
                           timeout=4000)
    check("กดภาพย้อนหลัง -> เปิดภาพใหญ่ได้", "พบนก" in page.inner_text("#lbCap"), page.inner_text("#lbCap"))
    page.wait_for_timeout(300)
    page.screenshot(path=os.path.join(SHOTS, "04c_history_viewer.png"))
    if n_before > 1:
        page.click("#lbNext")
        check("ปุ่มเก่ากว่าในตัวดูภาพใช้ได้", "(2/" in page.inner_text("#lbCap"), page.inner_text("#lbCap"))
    page.click("#lbX")
    check("ปิดตัวดูภาพได้", page.evaluate("document.getElementById('lb').hidden"))

    # ---------- ภาพสด ----------
    page.click("#btnLive")
    page.wait_for_timeout(2500)
    w = page.evaluate("document.getElementById('live').naturalWidth")
    check("เปิดภาพสดได้", w > 0, w)
    page.click("#btnLive")

    # ---------- กันคำสั่งจากที่อื่น ----------
    code = raw_post(sim.url + "/api/pump", b"ms=3000",
                    {"Content-Type": "application/x-www-form-urlencoded"})
    check("ฟอร์มจากเว็บอื่นสั่งปั๊ม -> ถูกปฏิเสธ 403", code == 403, code)
    code = raw_post(sim.url + "/api/repel", b"{}", {"Content-Type": "application/json"})
    check("JSON ที่ไม่มี header ของหน้าเว็บ -> ถูกปฏิเสธ 403", code == 403, code)

    # ---------- บอร์ดรีบูตเพราะไฟตก ----------
    sim.reset_board("BROWNOUT")
    s = wait_state(sim, lambda s: s["hw"]["reset_reason"] == "BROWNOUT", 15)
    check("Pi รู้ว่าบอร์ดรีบูตเพราะไฟตก", s is not None)
    s = wait_state(sim, lambda s: s["hw"]["pos_ok"] is False and s["hw"]["ready"], 15)
    resent = open(sim.server_log_path, encoding="utf-8").read().count("แจ้งบอร์ดว่ากล้องออนไลน์")
    check("หลังบอร์ดรีบูต Pi แจ้งสถานะกล้องใหม่ -> ไฟเขียวกลับมาติด", s is not None and resent >= 2, resent)
    page.wait_for_timeout(2500)
    warns = page.inner_text("#warns")
    check("หน้าเว็บเตือนว่าบอร์ดรีบูตเองและตำแหน่งมอเตอร์อาจเพี้ยน", "BROWNOUT" in warns and "ตั้งจุดกลาง" in warns, warns)
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "05_brownout_warning.png")


def pir_flow(page, sim):
    """ปิดตรวจอัตโนมัติ -> PIR ทริก -> กล้องจับนก -> ไฟส้มติด -> ดับเอง"""
    open_ui(page, sim)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.evaluate("document.getElementById('tech').open = true")   # บันทึก PIR อยู่ในส่วนพับเก็บ
    page.uncheck("#autoDet")
    page.wait_for_timeout(4000)
    check("ไฟเขียวติดเมื่อ Pi + กล้องพร้อม", "ติด" in page.inner_text("#rdy"), page.inner_text("#rdy"))
    check("ไฟส้มดับตอนยังไม่เจอนก", page.inner_text("#blamp") == "ดับ", page.inner_text("#blamp"))
    s = wait_state(sim, lambda s: s["hw"]["pir_warm"] == 0, 30)
    check("PIR อุ่นเครื่องเสร็จเร็ว (ไม่ต้องรอ 60 วิ)", s is not None)
    frames = state(sim)["total_frames"]
    page.wait_for_timeout(5000)
    check("ปิดตรวจอัตโนมัติแล้ว ไม่มีการถ่ายภาพเอง", state(sim)["total_frames"] == frames)

    t0 = time.time()
    sim.set_sensors(PIR=1, ECHO_CM=15)
    s = wait_state(sim, lambda s: s["hw"]["bird_lamp"], 15)
    lag = time.time() - t0
    check(f"ขยับหน้า PIR -> ไฟส้มติด (ใช้เวลา {lag:.1f} วิ)", s is not None, state(sim)["status"])
    st = state(sim)
    check("รอบนั้นมาจาก PIR และเจอนก", st["last_source"] == "pir" and st["bird_count"] > 0, st["status"])
    page.wait_for_timeout(1200)
    check("หน้าเว็บแสดงไฟส้มติด", page.inner_text("#blamp") == "ติด", page.inner_text("#blamp"))
    check("หน้าเว็บเห็น PIR กำลัง HIGH ทันที (ไม่ต้องรอ STAT)", "เคลื่อนไหว" in page.inner_text("#pir"), page.inner_text("#pir"))
    check("บันทึก PIR มีรายการที่ทริก", "ทริก ✓" in page.inner_text("#pirLog"), page.inner_text("#pirLog"))
    check("ข้อความบอกว่ามาจาก PIR", "PIR ทริก" in msg(page), msg(page))
    page.evaluate("window.scrollTo(0, document.getElementById('cnt').getBoundingClientRect().top - 300)")
    shot(page, "07_pir_bird_lamp.png")
    with open(os.path.join(HERE, "build", "fw_pins.log"), encoding="utf-8") as f:
        check("ขารีเลย์ไฟส้ม (GPIO21) เป็น HIGH จริง", "LAMP_ORANGE HIGH" in f.read())

    sim.set_sensors(PIR=0, ECHO_CM=15)
    page.wait_for_timeout(1500)
    check("บันทึก PIR บอกว่า HIGH นานกี่วินาที", "HIGH " in page.inner_text("#pirLog") and "วิ" in page.inner_text("#pirLog"),
          page.inner_text("#pirLog"))
    page.evaluate("document.getElementById('tech').open = true")
    page.evaluate("document.getElementById('pirLog').scrollIntoView({block:'center'})")
    shot(page, "08_pir_log.png")
    s = wait_state(sim, lambda s: not s["hw"]["bird_lamp"], 9)
    check("ไม่เจอนกต่อ -> ไฟส้มดับเองใน 5 วิ", s is not None)

    page.click("#bLamp")
    m = wait_msg(page, "OK BIRD", 3)
    check("ปุ่มทดสอบไฟส้มใช้ได้", "OK BIRD 1" in m, m)


def normal_flow(page, sim):
    """เวอร์ชันใช้งานปกติ: ไม่ต้องกดอะไร เจอนกแล้วไล่จริง เก็บ log/ภาพย้อนหลัง แต่ไม่เก็บภาพเทรน"""
    log_path = os.path.join(HERE, "build", "fw_pins.log")
    pins_before = open(log_path, encoding="utf-8").read()
    open_ui(page, sim)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.wait_for_timeout(1500)
    warns = page.inner_text("#warns")
    check("หน้าเว็บบอกว่าเป็นเวอร์ชันใช้งานปกติ", page.inner_text("#modeBadge") == "ใช้งานปกติ"
          and "โหมดทดสอบเปิดอยู่" not in warns and "เก็บภาพเทรน" not in warns, (page.inner_text("#modeBadge"), warns))
    check("ไล่อัตโนมัติเปิดอยู่ตั้งแต่เริ่ม", page.is_checked("#autoRep"))
    check("เวอร์ชันปกติไม่มีการ์ดข้อมูลเทรน", page.is_hidden("#dsCard"))
    page.once("dialog", lambda d: d.accept())
    page.click("#mZ")                                   # ตั้งจุดกลางเหมือนตอนติดตั้งจริง
    s = wait_state(sim, lambda s: s["hw"]["repel_count"] >= 1, 60)
    check("เจอนกแล้วสั่งไล่เองโดยไม่ต้องกดอะไร", s is not None, state(sim)["status"])
    s = wait_state(sim, lambda s: not s["hw"]["repel"] and not s["hw"]["busy"], 20)
    pins = open(log_path, encoding="utf-8").read()[len(pins_before):]
    check("รีเลย์ปั๊ม (GPIO18) เปิดแล้วปิดเอง", "PUMP HIGH" in pins and "PUMP LOW" in pins, pins[-400:])
    check("มอเตอร์กวาดหัวฉีด", "STEPS" in pins, pins[-400:])
    check("ไฟส้มติดตอนเจอนก", "LAMP_ORANGE HIGH" in pins)
    page.wait_for_timeout(6000)
    with open(os.path.join(sim.data_dir, "detection_log.csv"), encoding="utf-8-sig") as f:
        rows = f.read()
    check("detection_log.csv บันทึกรอบที่สั่งไล่", "สั่งไล่แล้ว" in rows, rows[-300:])
    check("detection_log.csv บันทึกรอบรอยืนยันก่อนไล่ (เฟรมแรก)", "รอยืนยัน 1/2" in rows, rows[-300:])
    check("เฟรมยืนยันถูกถ่ายตามมาเอง (source=confirm)", ",confirm," in rows, rows[-300:])
    lines = [ln.split(",") for ln in rows.splitlines()[1:]]
    first = next((i for i, ln in enumerate(lines) if "รอยืนยัน" in ln[-3]), None)
    gap = None
    if first is not None and first + 1 < len(lines):
        t = [datetime.strptime(lines[k][0], "%Y-%m-%d %H:%M:%S") for k in (first, first + 1)]
        gap = (t[1] - t[0]).total_seconds()
    check(f"เฟรมยืนยันห่างจากเฟรมแรก {gap} วิ (ไม่ต้องรอรอบ 5 วิ)", gap is not None and gap <= 4.5, gap)
    st = state(sim)
    check("ROI เริ่มต้นเปิด ตัดขอบซ้าย-ขวา", st["roi"]["enabled"] and st["roi"]["x1"] > 0 and st["roi"]["x2"] < 1,
          st["roi"])
    ds = os.path.join(sim.data_dir, "dataset")
    n = len([f for f in os.listdir(ds) if f.endswith(".jpg")]) if os.path.isdir(ds) else 0
    check("เวอร์ชันปกติไม่เก็บภาพเทรน (ประหยัด SD การ์ด)", n == 0, n)
    hist = os.listdir(os.path.join(sim.data_dir, "history"))
    check(f"เวอร์ชันปกติยังเก็บภาพย้อนหลังที่พบนก ({len(hist)} ภาพ)", len(hist) >= 1)
    code, _ = get(sim, "/api/dataset")
    check("เวอร์ชันปกติเรียก API ข้อมูลเทรนไม่ได้ (403)", code == 403, code)
    st = state(sim)
    check(f"ภายใน 1 นาทีไล่ไม่เกินที่ช่วงพักกำหนด ({st['hw']['repel_count']} ครั้ง)",
          st["hw"]["repel_count"] <= 4, st["hw"]["repel_count"])
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "11_normal_mode.png")

    # ---------- v7.3: ไล่ทันทีเมื่อเจอภาพเดียว ----------
    check("มีสวิตช์ไล่ทันที ปิดอยู่เป็นค่าเริ่มต้น", page.is_visible("#instRep") and not page.is_checked("#instRep"))
    page.click("#instRow")
    s = wait_state(sim, lambda s: s["instant_repel"] and s["confirm_frames"] == 1, 5)
    check("เปิดสวิตช์แล้วต้องเจอแค่ 1 เฟรม", s is not None)
    saved = json.load(open(os.path.join(sim.data_dir, "settings.json")))
    check("จำค่าไว้ใน settings.json", saved.get("instant_repel") is True, saved)
    page.wait_for_timeout(1200)
    check("หน้าเว็บบอกว่าไม่ต้องยืนยัน", page.inner_text("#cfm") == "ไม่ต้องยืนยัน", page.inner_text("#cfm"))
    n_rows = len(open(os.path.join(sim.data_dir, "detection_log.csv"), encoding="utf-8-sig").read().splitlines())
    n0 = state(sim)["hw"]["repel_count"]
    s = wait_state(sim, lambda s: s["hw"]["repel_count"] > n0, 40)
    rows = [ln.split(",") for ln in
            open(os.path.join(sim.data_dir, "detection_log.csv"), encoding="utf-8-sig").read().splitlines()[n_rows:]]
    hit = [r for r in rows if r[-3] == "สั่งไล่แล้ว"]
    check("เจอนกภาพเดียวก็สั่งไล่ (รอบปกติ ไม่ใช่เฟรมยืนยัน)",
          s is not None and hit and hit[0][1] != "confirm" and not any("รอยืนยัน" in r[-3] for r in rows),
          [(r[1], r[-3]) for r in rows][:6])
    shot(page, "11c_instant_repel.png")
    page.click("#instRow")
    s = wait_state(sim, lambda s: not s["instant_repel"] and s["confirm_frames"] == 2, 5)
    check("ปิดสวิตช์แล้วกลับไปยืนยัน 2 เฟรม", s is not None)


def train_flow(page, sim):
    """เวอร์ชันเก็บภาพเทรน: ไม่ไล่เองเป็นค่าเริ่มต้น เก็บภาพถี่ มีการ์ดติดป้าย"""
    open_ui(page, sim)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.wait_for_timeout(1500)
    st = state(sim)
    check("ชื่อโหมดเก่า field ใช้ได้ = เวอร์ชันเก็บภาพเทรน", st["mode"] == "train" and st["train_mode"], st["mode"])
    warns = page.inner_text("#warns")
    check("หน้าเว็บบอกว่าเป็นเวอร์ชันเก็บภาพเทรน", page.inner_text("#modeBadge") == "เวอร์ชันเก็บภาพเทรน"
          and "เก็บภาพเทรน" in warns, (page.inner_text("#modeBadge"), warns))
    check("เวอร์ชันเทรนไม่ไล่อัตโนมัติเป็นค่าเริ่มต้น", not page.is_checked("#autoRep"))
    check("เวอร์ชันเทรนมีการ์ดข้อมูลเทรน", page.is_visible("#dsCard"))
    page.once("dialog", lambda d: d.accept())
    page.click("#mZ")
    s = wait_state(sim, lambda s: s["total_detections"] >= 3, 60)
    check("เจอนกหลายรอบแต่ไม่สั่งไล่ (ปล่อยนกอยู่ให้ถ่าย)", s is not None and s["hw"]["repel_count"] == 0,
          s and s["hw"]["repel_count"])
    page.check("#autoRep")
    s = wait_state(sim, lambda s: s["hw"]["repel_count"] >= 1, 60)
    check("เปิดสวิตช์ไล่อัตโนมัติแล้วไล่ได้ตามปกติ", s is not None)
    wait_state(sim, lambda s: not s["hw"]["repel"] and not s["hw"]["busy"], 20)
    page.wait_for_timeout(3000)
    ds_dir = os.path.join(sim.data_dir, "dataset")
    repel_imgs = [f for f in os.listdir(ds_dir) if "_repel_" in f] if os.path.isdir(ds_dir) else []
    check(f"สั่งไล่แล้วเก็บภาพเข้าชุดข้อมูลเสมอ ({len(repel_imgs)} ภาพ)", len(repel_imgs) >= 1)
    page.uncheck("#autoRep")                      # ปิดไล่ กลับไปเก็บภาพอย่างเดียว
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "11b_train_mode.png")


def empty_clip():
    """คลิปภาพว่าง ๆ ไม่มีนก ไว้ทดสอบ PIR ค้าง"""
    import cv2
    import numpy as np
    path = os.path.join(HERE, "build", "empty.mp4")
    if not os.path.exists(path):
        vw = cv2.VideoWriter(path, cv2.VideoWriter_fourcc(*"mp4v"), 10, (640, 480))
        rng = np.random.default_rng(1)
        for _ in range(30):
            vw.write((np.full((480, 640, 3), 120) + rng.integers(0, 6, (480, 640, 3))).astype("uint8"))
        vw.release()
    return path


def pir_stuck_flow(page, sim):
    """PIR ค้าง HIGH และกล้องไม่เจอนก -> บอร์ดเลิกทริกซ้ำ หน้าเว็บเตือน"""
    open_ui(page, sim)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.evaluate("document.getElementById('tech').open = true")   # บันทึก PIR อยู่ในส่วนพับเก็บ
    page.uncheck("#autoDet")
    wait_state(sim, lambda s: s["hw"]["pir_warm"] > 0, 8)      # รอ STAT แรกจากบอร์ด (ค่าเริ่มต้นเป็น 0)
    s = wait_state(sim, lambda s: s["hw"]["pir_warm"] == 0, 30)
    check("PIR พร้อม", s is not None)
    sim.set_sensors(PIR=1, ECHO_CM=15)
    page.wait_for_timeout(12000)
    st = state(sim)["hw"]
    check(f"ช่วงค้าง: ทริกใหม่ {st['pir_trig']} / ทริกซ้ำ {st['pir_rep']} นับแยกกัน",
          st["pir_trig"] == 1 and st["pir_rep"] >= 1, st)
    check("หน้าเว็บแสดงช่องทริกซ้ำ", page.inner_text("#pirR") == str(st["pir_rep"]) or
          int(page.inner_text("#pirR")) >= 1, page.inner_text("#pirR"))
    s = wait_state(sim, lambda s: s["hw"]["pir_stuck"], 25)
    check("ค้างเกิน 30 วิไม่เจอนก -> บอร์ดแจ้งค้าง", s is not None)
    frames = state(sim)["total_frames"]
    page.wait_for_timeout(11000)
    check("ค้างแล้วไม่ถ่ายซ้ำอีก", state(sim)["total_frames"] == frames, (frames, state(sim)["total_frames"]))
    warns = page.inner_text("#warns")
    check("หน้าเว็บเตือน PIR ค้าง ให้เช็ก Sx/Tx", "PIR ค้าง" in warns and "Sx" in warns, warns)
    check("ช่อง PIR บอกว่าค้าง", "ค้าง" in page.inner_text("#pir"), page.inner_text("#pir"))
    check("บันทึก PIR บอกจำนวนรอบซ้ำและว่าค้าง", "ซ้ำ" in page.inner_text("#pirLog") and "ค้าง!" in page.inner_text("#pirLog"),
          page.inner_text("#pirLog"))
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "09_pir_stuck_warning.png")
    page.evaluate("document.getElementById('tech').open = true")
    page.evaluate("document.getElementById('pirLog').scrollIntoView({block:'center'})")
    shot(page, "10_pir_stuck_log.png")
    sim.set_sensors(PIR=0, ECHO_CM=15)
    s = wait_state(sim, lambda s: not s["hw"]["pir_stuck"], 6)
    check("OUT กลับเป็น LOW -> หายค้าง", s is not None)
    page.wait_for_timeout(1500)
    check("คำเตือนหายไป เหลือคำแนะนำในบันทึก", "PIR ค้าง" not in page.inner_text("#warns") and
          "Sx" in page.inner_text("#pirHint"), page.inner_text("#pirHint"))


def dataset_flow(page, sim):
    """การ์ดข้อมูลสำหรับเทรน: ติดป้าย ลบ และดาวน์โหลด ZIP แบบ YOLO"""
    ds_dir = os.path.join(sim.data_dir, "dataset")
    page.evaluate("window.scrollTo(0,0)")
    end = time.time() + 60                       # โหลดซ้ำจนมีภาพพอ (การ์ดอาจกำลังโหลดเองอยู่)
    while time.time() < end and not page.evaluate("ds.counts.all >= 4 && ds.items.length >= 4"):
        page.evaluate("dsLoad(true)")
        page.wait_for_timeout(2000)
    c0 = page.evaluate("ds.counts")
    check(f"การ์ดข้อมูลเทรนแสดงภาพ ({c0['all']} ภาพ ยังไม่ติดป้าย {c0['todo']})",
          c0["all"] == c0["todo"] and c0["all"] >= 4, c0)
    jpgs = sorted(f for f in os.listdir(ds_dir) if f.endswith(".jpg"))
    metas = [json.load(open(os.path.join(ds_dir, f[:-4] + ".json"))) for f in jpgs
             if os.path.exists(os.path.join(ds_dir, f[:-4] + ".json"))]
    check("ทุกภาพในชุดข้อมูลมีไฟล์กรอบ .json คู่กัน", len(metas) == len(jpgs), (len(jpgs), len(metas)))
    check("ไฟล์กรอบมีกรอบนกจากโมเดลอย่างน้อยหนึ่งภาพ",
          any(any(b[4] >= 0.25 for b in m["boxes"]) for m in metas))
    page.locator("#dsCard").scroll_into_view_if_needed()
    page.wait_for_timeout(800)
    page.locator("#dsCard").screenshot(path=os.path.join(SHOTS, "12_dataset_card.png"))

    names = page.evaluate("ds.items.slice(0, 4).map(i => i.n)")
    page.click("#dsGrid .dsi >> nth=0")
    page.wait_for_selector("#dsv:not([hidden])")
    page.wait_for_timeout(600)
    shot(page, "12b_dataset_viewer.png")
    for lab in ("bird", "none", "fix"):
        cur = page.evaluate("ds.cur")
        page.click(f".dsvBtns .{lab}")
        page.wait_for_function(f"ds.cur === {cur + 1}", timeout=5000)
    c1 = page.evaluate("ds.counts")
    check("ติดป้าย ✓ ✗ ✎ แล้วเลื่อนไปภาพถัดไปเอง และตัวนับตรง",
          (c1["bird"], c1["none"], c1["fix"], c1["todo"]) == (1, 1, 1, c0["all"] - 3), c1)
    page.once("dialog", lambda d: d.accept())
    page.click("#dsvDel")
    page.wait_for_function(f"ds.counts.all === {c0['all'] - 1}", timeout=5000)
    check("ลบภาพจากตัวดูได้ (ไฟล์ภาพและไฟล์กรอบหายจริง)",
          not os.path.exists(os.path.join(ds_dir, names[3]))
          and not os.path.exists(os.path.join(ds_dir, names[3][:-4] + ".json")))
    page.click("#dsvX")

    _, j = get(sim, "/api/dataset?f=bird")
    j = json.loads(j)
    check("API แยกหมวดป้ายถูกต้อง", [i["n"] for i in j["items"]] == [names[0]], j["items"][:1])
    page.evaluate("ds.f = 'all'; dsLoad(true)")
    page.wait_for_timeout(800)
    page.evaluate("dsLoad(true)")
    labels = json.load(open(os.path.join(ds_dir, "labels.json")))
    check("ป้ายจำไว้ในไฟล์ labels.json", labels == {names[0]: "bird", names[1]: "none", names[2]: "fix"}, labels)

    # ---------- ZIP ----------
    code, data = get(sim, "/dataset/export.zip?scope=labeled")
    z = zipfile.ZipFile(io.BytesIO(data))
    imgs = sorted(n for n in z.namelist() if n.startswith("images/"))
    lbls = sorted(n for n in z.namelist() if n.startswith("labels/"))
    check("ZIP เฉพาะที่ติดป้ายมี 3 ภาพพร้อมไฟล์กรอบครบ",
          code == 200 and len(imgs) == 3 and [n[7:-4] for n in imgs] == [n[7:-4] for n in lbls], (code, imgs))
    check("ZIP มี data.yaml (คลาส 0 = bird) และ README", "0: bird" in z.read("data.yaml").decode()
          and "README.txt" in z.namelist())
    line_re = re.compile(r"^0( (0|1)\.\d{6}){4}$")
    bad = [ln for n in lbls for ln in z.read(n).decode().splitlines() if not line_re.match(ln)]
    check("ทุกบรรทัดในไฟล์กรอบเป็นรูปแบบ YOLO ที่ถูกต้อง", not bad, bad[:3])
    check("ภาพที่ติดป้าย ✗ ไม่มีนก ได้ไฟล์กรอบว่าง", z.read(f"labels/{names[1][:-4]}.txt") == b"")
    review = z.read("review_needed.txt").decode()
    check("ภาพ ✎ ต้องแก้กรอบ อยู่ในรายการ review_needed.txt", names[2] in review, review)
    check("ZIP ตรวจไฟล์ภาพครบไม่เสีย", z.testzip() is None)
    code, data = get(sim, "/dataset/export.zip?scope=all")
    n_all = len([n for n in zipfile.ZipFile(io.BytesIO(data)).namelist() if n.startswith("images/")])
    check(f"ZIP ทั้งหมดมีครบทุกภาพ ({n_all})", n_all >= c0["all"] - 1, n_all)

    code, _ = api_post(sim, "/api/dataset/label", {"name": "../auth.json", "label": "bird"})
    check("ชื่อไฟล์แปลก ๆ (../) ถูกปฏิเสธ", code == 400, code)
    code, _ = get(sim, "/dataset/img/" + "x" * 5 + ".jpg")
    check("ขอไฟล์ภาพที่ชื่อไม่ตรงรูปแบบ -> ปฏิเสธ", code == 400, code)

    # ---------- v7.2: ถ่ายภาพทันที / ลบภาพซ้ำ / ลบทั้งแท็บ / ไฟล์หายจากเทอร์มินัล ----------
    before = set(os.listdir(ds_dir))
    t0 = time.time()
    code, j = api_post(sim, "/api/capture", {})
    dt = time.time() - t0
    check(f"ปุ่มถ่ายภาพตอบภายในไม่ถึง 1.5 วิ ({dt:.2f} วิ) และได้ภาพทันที", code == 200 and j["ok"] and dt < 1.5, (code, j))
    end = time.time() + 15
    while time.time() < end and not [f for f in set(os.listdir(ds_dir)) - before if "_manual_" in f]:
        time.sleep(0.3)
    check("ภาพที่กดถ่ายเข้าชุดข้อมูล", any("_manual_" in f for f in set(os.listdir(ds_dir)) - before))
    # v7.5: เดิมกดถ่ายแล้วเธรดตรวจจับตาย กดครั้งต่อไปได้แค่ภาพดิบ ไม่มีผลตรวจ/กรอบ
    for k in range(3):
        seq = state(sim)["frame_seq"]
        code, j = api_post(sim, "/api/capture", {})
        s = wait_state(sim, lambda s: s["frame_seq"] > seq, 15)
        check(f"กดถ่ายติดกันครั้งที่ {k + 2} ยังได้ผลตรวจใหม่ (เธรดตรวจจับไม่ตาย)",
              j.get("ok") and s and s["frame_seq"] > seq and "กดถ่ายภาพ" in s["status"], (j, s and s["status"]))

    page.evaluate("ds.f = 'todo'; dsLoad(true)")
    page.wait_for_timeout(800)
    todo = page.evaluate("ds.items.map(i => i.n)")
    src = names[0]                                                  # ภาพที่ติดป้ายนกไว้ (ไม่โดนลบ ใช้เป็นต้นฉบับ)
    dup = src[:15] + "_999" + src[19:]                              # ชื่อเวลาถัดไปนิดเดียว ภาพเดียวกัน
    shutil.copy(os.path.join(ds_dir, src), os.path.join(ds_dir, dup))
    shutil.copy(os.path.join(ds_dir, src[:-4] + ".json"), os.path.join(ds_dir, dup[:-4] + ".json"))
    os.remove(os.path.join(ds_dir, todo[0]))                        # ลบจากเทอร์มินัลขณะเซิร์ฟเวอร์รัน
    page.evaluate("dsLoad(true)")
    page.wait_for_timeout(800)
    check("ไฟล์ที่ถูกลบจากเทอร์มินัล หายจากการ์ดเอง ไม่ขึ้น ?",
          todo[0] not in page.evaluate("ds.items.map(i => i.n)"))
    code, j = api_post(sim, "/api/dataset/bulk", {"action": "dedupe"})
    check(f"ลบภาพซ้ำ: {j.get('msg')}", code == 200 and j["ok"] and j["n"] >= 1
          and not os.path.exists(os.path.join(ds_dir, dup)), j)
    check("ลบภาพซ้ำไม่แตะภาพที่ติดป้ายแล้ว", all(os.path.exists(os.path.join(ds_dir, n)) for n in names[:3]))

    page.evaluate("ds.f = 'none'; dsLoad(true)")
    page.wait_for_timeout(800)
    check("แท็บไม่มีนกมีปุ่มลบทั้งแท็บ", page.is_visible("#dsDelTab"), page.inner_text("#dsDelTab"))
    page.once("dialog", lambda d: d.accept())
    page.click("#dsDelTab")
    page.wait_for_function("ds.counts.none === 0", timeout=5000)
    check("ลบทั้งแท็บไม่มีนกแล้ว ไฟล์หายจริง ภาพหมวดอื่นยังอยู่",
          not os.path.exists(os.path.join(ds_dir, names[1])) and os.path.exists(os.path.join(ds_dir, names[0])))
    check("แท็บนกไม่มีปุ่มลบทั้งแท็บ", page.evaluate("ds.f = 'bird'; dsRenderCounts(); $('dsDelTab').hidden"))
    code, _ = api_post(sim, "/api/dataset/bulk", {"action": "delete", "f": "bird", "upto": names[0]})
    check("API ไม่ยอมลบทั้งแท็บนก", code == 400, code)
    check("บอกพื้นที่ว่างบนการ์ด", "GB" in page.inner_text("#dsFree"), page.inner_text("#dsFree"))


def auth_flow(browser, phone, sim):
    """ล็อกอิน: ตั้งรหัสครั้งแรก / หน้าที่ป้องกันไว้ / เปลี่ยนรหัส / ลืมรหัส / ล็อกเมื่อเดาผิด"""
    anon = urllib.request.build_opener()          # ไม่มีคุกกี้ = ไม่ได้ล็อกอิน
    blocked = []
    for path in ["/api/state", "/stream", "/image", "/preview", "/history/x.jpg", "/log", "/api/dataset",
                 "/dataset/img/20260101_000000_000_bird_1.jpg", "/dataset/export.zip?scope=all"]:
        code, _ = get(sim, path, anon)
        if code != 401:
            blocked.append((path, code))
    check("ไม่ล็อกอิน -> ทุกหน้าข้อมูล/ภาพ/ไฟล์ตอบ 401", not blocked, blocked)
    with anon.open(sim.url + "/", timeout=5) as r:
        check("ไม่ล็อกอินเปิดหน้าแรก -> เด้งไปหน้าล็อกอิน", r.url.endswith("/login"), r.url)
    code, _ = api_post(sim, "/api/pump", {}, anon)
    check("ไม่ล็อกอินสั่งปั๊ม -> 401", code == 401, code)
    code, _ = get(sim, "/health", anon)
    check("/health ยังเปิดได้ (ไว้เช็กว่าเครื่องทำงาน)", code == 200, code)

    page = browser.new_page(**phone)
    page.goto(sim.url)
    page.wait_for_url("**/login")
    check("ยังไม่มีรหัส -> หน้าตั้งรหัสครั้งแรก", page.is_visible("#setupNote") and page.is_visible("#pw2"))
    shot(page, "13_first_setup.png")
    page.fill("#pw", "abc"); page.fill("#pw2", "abc"); page.click("#go")
    check("รหัสสั้นเกิน -> ไม่ยอม", "6" in page.inner_text("#msg"), page.inner_text("#msg"))
    page.fill("#pw", "secret123"); page.fill("#pw2", "secret124"); page.click("#go")
    check("พิมพ์ซ้ำไม่ตรง -> ไม่ยอม", "ไม่ตรง" in page.inner_text("#msg"), page.inner_text("#msg"))
    page.fill("#pw2", "secret123"); page.click("#go")
    page.wait_for_url(sim.url + "/", timeout=10000)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    check("ตั้งรหัสแล้วเข้าหน้าหลักได้ทันที", True)
    sim.password, sim._op = "secret123", None
    code, _ = api_post(sim, "/api/setup", {"password": "hacker999"})
    check("ตั้งรหัสแล้ว เรียกหน้าตั้งรหัสซ้ำไม่ได้ (409)", code == 409, code)
    perm = oct(os.stat(os.path.join(sim.data_dir, "auth.json")).st_mode & 0o777)
    raw = open(os.path.join(sim.data_dir, "auth.json")).read()
    check("auth.json เก็บแค่ hash อ่านได้เฉพาะเจ้าของ", "secret123" not in raw and perm == "0o600", perm)

    # เครื่องที่สองล็อกอินไว้ แล้วเครื่องแรกเปลี่ยนรหัส -> เครื่องที่สองต้องหลุด
    ctx2 = browser.new_context(**phone)
    p2 = ctx2.new_page()
    open_ui(p2, sim)
    page.evaluate("document.getElementById('tech').open = true")
    page.fill("#pwCur", "wrongpass"); page.fill("#pwNew", "newpass456"); page.fill("#pwNew2", "newpass456")
    page.click("#pwSave")
    m = wait_msg(page, "ไม่ถูกต้อง", 3)
    check("เปลี่ยนรหัสโดยใส่รหัสเดิมผิด -> ไม่ยอม", "ไม่ถูกต้อง" in m, m)
    page.fill("#pwCur", "secret123"); page.fill("#pwNew", "newpass456"); page.fill("#pwNew2", "newpass456")
    page.click("#pwSave")
    m = wait_msg(page, "เปลี่ยนรหัสแล้ว", 3)
    check("เปลี่ยนรหัสได้", "เปลี่ยนรหัสแล้ว" in m, m)
    sim.password, sim._op = "newpass456", None
    try:
        p2.wait_for_url("**/login", timeout=6000)
        kicked = True
    except Exception:                                  # noqa: BLE001
        kicked = False
    check("เปลี่ยนรหัสแล้ว เครื่องอื่นที่ล็อกอินไว้ถูกให้ล็อกอินใหม่", kicked, p2.url)
    page.wait_for_timeout(2500)
    check("เครื่องที่เปลี่ยนรหัสยังใช้งานต่อได้", page.url == sim.url + "/", page.url)
    ctx2.close()

    page.click("#btnLogout")
    page.wait_for_url("**/login", timeout=5000)
    check("ปุ่มออกจากระบบใช้ได้", True)

    # ลืมรหัส: ตั้งใหม่ด้วยคำสั่งบน Pi ขณะเซิร์ฟเวอร์รันอยู่
    env = dict(os.environ, BIRD_DATA_DIR=sim.data_dir)
    r = subprocess.run([sys.executable, SERVER, "--set-password"], input="clipass789\nclipass789\n",
                       env=env, text=True, capture_output=True, timeout=60, start_new_session=True)
    check("คำสั่ง --set-password ตั้งรหัสใหม่ได้", r.returncode == 0 and "บันทึกแล้ว" in r.stdout,
          (r.returncode, r.stdout[-200:], r.stderr[-300:]))
    code, _ = api_post(sim, "/api/login", {"password": "newpass456"}, urllib.request.build_opener())
    check("หลังตั้งด้วยคำสั่ง รหัสเดิมใช้ไม่ได้ทันที (ไม่ต้องรีสตาร์ท)", code == 401, code)
    sim.password, sim._op = "clipass789", None
    open_ui(page, sim)
    check("รหัสที่ตั้งด้วยคำสั่งใช้ล็อกอินได้", page.url == sim.url + "/", page.url)
    page.click("#btnLogout")
    page.wait_for_url("**/login", timeout=5000)
    shot(page, "13b_login.png")

    # เดารหัสผิด 5 ครั้ง -> ล็อก
    for i in range(5):
        page.fill("#pw", f"guess{i}xx")
        page.click("#go")
        page.wait_for_function("document.getElementById('msg').textContent !== ''", timeout=5000)
        page.wait_for_timeout(500)
    m = page.inner_text("#msg")
    check("ใส่ผิด 5 ครั้ง -> ล็อกและบอกเวลารอ", "ลองใหม่ในอีก" in m or "ล็อก" in m, m)
    check("ระหว่างล็อก ปุ่มเข้าสู่ระบบกดไม่ได้", page.is_disabled("#go"))
    code, j = api_post(sim, "/api/login", {"password": "clipass789"}, urllib.request.build_opener())
    check("ระหว่างล็อก ใส่รหัสถูกก็ยังเข้าไม่ได้ (429)", code == 429, (code, j))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--clip", required=True)
    ap.add_argument("--model", required=True)
    ap.add_argument("--chromium", default=None)
    ap.add_argument("--bird-clip", default=None, help="คลิปที่มีนกตลอด ใช้กับการทดสอบ PIR")
    a = ap.parse_args()
    os.makedirs(SHOTS, exist_ok=True)

    with sync_playwright() as p:
        browser = p.chromium.launch(executable_path=a.chromium)
        phone = dict(viewport={"width": 390, "height": 844}, device_scale_factor=2,
                     is_mobile=True, has_touch=True)

        print("== สาธิต (demo) ==")
        sim = Sim(a.clip, a.model, port=5055).start()
        try:
            main_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== PIR -> ตรวจจับ -> ไฟส้ม ==")
        sim = Sim(a.bird_clip or a.clip, a.model, port=5057).start()
        try:
            pir_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== เวอร์ชันใช้งานปกติ: เจอแล้วไล่จริง ==")
        sim = Sim(a.bird_clip or a.clip, a.model, port=5059, mode="normal").start()
        try:
            normal_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== เวอร์ชันเก็บภาพเทรน + ติดป้าย ==")
        sim = Sim(a.bird_clip or a.clip, a.model, port=5064, mode="field").start()   # ชื่อเก่า = train
        try:
            page = browser.new_page(**phone)
            train_flow(page, sim)
            dataset_flow(page, sim)
        finally:
            sim.stop()

        print("== PIR ค้าง (ไม่มีนก) ==")
        sim = Sim(empty_clip(), a.model, port=5058).start()
        try:
            pir_stuck_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== ล็อกอิน ==")
        sim = Sim(a.clip, a.model, port=5056, password="").start()
        try:
            auth_flow(browser, phone, sim)
        finally:
            sim.stop()
        browser.close()

    print()
    print("ผ่านทั้งหมด" if not FAILS else f"ไม่ผ่าน {len(FAILS)} ข้อ: {FAILS}")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()
