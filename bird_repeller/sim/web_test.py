#!/usr/bin/env python3
"""จำลองการใช้หน้าเว็บจากมือถือ (Playwright) กับระบบจำลองทั้งชุด แล้วเก็บภาพหน้าจอ

    ./build.sh
    python3 web_test.py --clip clip.mp4 --model yolov8n.pt [--chromium /path/to/chrome]

ภาพหน้าจออยู่ที่ build/shots/
"""
import argparse
from datetime import datetime
import json
import os
import sys
import time
import urllib.error
import urllib.request

from playwright.sync_api import sync_playwright

from run_sim import HERE, Sim

SHOTS = os.path.join(HERE, "build", "shots")
FAILS = []


def check(name, ok, detail=""):
    print(("  ✔ " if ok else "  ✘ ") + name + ("" if ok else f"   <-- {detail}"), flush=True)
    if not ok:
        FAILS.append(name)


def state(sim):
    with urllib.request.urlopen(sim.url + "/api/state", timeout=5) as r:
        return json.load(r)


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
    page.goto(sim.url)
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

    # ---------- ปั๊ม ----------
    page.click("#bPump")
    m = wait_msg(page, "OK PUMP", 3)
    check("กดปั๊ม 5 วินาที -> OK", "OK PUMP 5000" in m, m)
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
    src0 = page.evaluate("document.querySelector('#hist .hi img').src")
    page.wait_for_timeout(2500)
    same = page.evaluate("document.querySelector('#hist .hi img').src") == src0
    n_before = page.evaluate("document.querySelectorAll('#hist .hi').length")
    check("ภาพย้อนหลังไม่ถูกสร้างใหม่ทุกวินาที", same)
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
    page.goto(sim.url)
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
        check("ขารีเลย์ไฟส้ม (GPIO26) เป็น HIGH จริง", "LAMP_ORANGE HIGH" in f.read())

    sim.set_sensors(PIR=0, ECHO_CM=15)
    page.wait_for_timeout(1500)
    check("บันทึก PIR บอกว่า HIGH นานกี่วินาที", "HIGH " in page.inner_text("#pirLog") and "วิ" in page.inner_text("#pirLog"),
          page.inner_text("#pirLog"))
    page.evaluate("document.getElementById('tech').open = true")
    page.evaluate("document.getElementById('pirLog').scrollIntoView({block:'center'})")
    shot(page, "08_pir_log.png")
    s = wait_state(sim, lambda s: not s["hw"]["bird_lamp"], 14)
    check("ไม่เจอนกต่อ -> ไฟส้มดับเองใน 10 วิ", s is not None)

    page.click("#bLamp")
    m = wait_msg(page, "OK BIRD", 3)
    check("ปุ่มทดสอบไฟส้มใช้ได้", "OK BIRD 1" in m, m)


def field_flow(page, sim):
    """โหมดทดสอบภาคสนาม: ไม่ต้องกดอะไร เจอนกแล้วไล่จริง + เก็บ log/ภาพ"""
    log_path = os.path.join(HERE, "build", "fw_pins.log")
    pins_before = open(log_path, encoding="utf-8").read()
    page.goto(sim.url)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.wait_for_timeout(1500)
    warns = page.inner_text("#warns")
    check("หน้าเว็บบอกว่าอยู่โหมดทดสอบภาคสนาม (ไล่จริง)", "ภาคสนาม" in warns and "โหมดทดสอบเปิดอยู่" not in warns, warns)
    check("ไล่อัตโนมัติเปิดอยู่ตั้งแต่เริ่ม", page.is_checked("#autoRep"))
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
    check(f"เก็บภาพดิบสำหรับเทรนแล้ว {n} ภาพ", n >= 1)
    st = state(sim)
    check(f"ภายใน 1 นาทีไล่ไม่เกินที่ช่วงพักกำหนด ({st['hw']['repel_count']} ครั้ง)",
          st["hw"]["repel_count"] <= 4, st["hw"]["repel_count"])
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "11_field_mode.png")


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
    page.goto(sim.url)
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


def pin_flow(page, sim):
    code = raw_post(sim.url + "/api/pump", b"{}", {"Content-Type": "application/json", "X-Bird-UI": "1"})
    check("ตั้ง PIN แล้ว สั่งโดยไม่มี PIN -> 401", code == 401, code)
    page.goto(sim.url)
    page.wait_for_function("document.getElementById('brd').textContent === 'เชื่อมต่อแล้ว'", timeout=20000)
    page.once("dialog", lambda d: d.accept("0000"))
    page.click("#mH")
    m = wait_msg(page, "PIN", 3)
    check("ใส่ PIN ผิด -> ไม่ยอมสั่ง", "PIN ไม่ถูกต้อง" in m, m)
    page.evaluate("localStorage.removeItem('birdPin')")
    page.once("dialog", lambda d: d.accept("4821"))
    page.click("#mH")
    m = wait_msg(page, "OK HOME", 3)
    check("ใส่ PIN ถูก -> สั่งงานได้ และจำ PIN ไว้", "OK HOME" in m, m)
    page.click("#mH")
    m = wait_msg(page, "OK HOME", 3)
    check("กดครั้งถัดไปไม่ต้องใส่ PIN ซ้ำ", "OK HOME" in m, m)


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

        print("== ใช้งานปกติ (ไม่มี PIN) ==")
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

        print("== ทดสอบภาคสนาม: เจอแล้วไล่จริง ==")
        sim = Sim(a.bird_clip or a.clip, a.model, port=5059, mode="field").start()
        try:
            field_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== PIR ค้าง (ไม่มีนก) ==")
        sim = Sim(empty_clip(), a.model, port=5058).start()
        try:
            pir_stuck_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()

        print("== ตั้ง PIN ==")
        sim = Sim(a.clip, a.model, port=5056, pin="4821").start()
        try:
            pin_flow(browser.new_page(**phone), sim)
        finally:
            sim.stop()
        browser.close()

    print()
    print("ผ่านทั้งหมด" if not FAILS else f"ไม่ผ่าน {len(FAILS)} ข้อ: {FAILS}")
    sys.exit(1 if FAILS else 0)


if __name__ == "__main__":
    main()
