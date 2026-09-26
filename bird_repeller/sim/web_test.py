#!/usr/bin/env python3
"""จำลองการใช้หน้าเว็บจากมือถือ (Playwright) กับระบบจำลองทั้งชุด แล้วเก็บภาพหน้าจอ

    ./build.sh
    python3 web_test.py --clip clip.mp4 --model yolov8n.pt [--chromium /path/to/chrome]

ภาพหน้าจออยู่ที่ build/shots/
"""
import argparse
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
    check("กดปั๊ม 3 วินาที -> OK", "OK PUMP 3000" in m, m)
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
    page.check("#roiOn")
    for k, v in (("rx1", "0.35"), ("ry1", "0.05"), ("rx2", "0.75"), ("ry2", "0.55")):
        page.fill("#" + k, v)
    page.click("#roiSave")
    m = wait_msg(page, "ROI", 3)
    check("บันทึก ROI จากหน้าเว็บ", "บันทึก ROI แล้ว" in m, m)
    before = state(sim)["frame_seq"]
    s = wait_state(sim, lambda s: s["frame_seq"] > before + 1 and "พบนก" in s["status"], 40)
    check("เปิด ROI (ตัดภาพก่อนตรวจ) แล้วยังเจอนกในกรอบ", s is not None, state(sim)["status"])
    page.wait_for_function("document.getElementById('cnt').textContent !== '0'", timeout=8000)
    page.wait_for_timeout(700)
    m = msg(page)
    check("ข้อความสถานะตรงกับจำนวนนกที่แสดง", "พบนก" in m, m)
    shot(page, "04_detect_roi.png")

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
    check("หลังบอร์ดรีบูต Pi แจ้งสถานะกล้องใหม่ -> ไฟส้มกลับมาติด", s is not None and resent >= 2, resent)
    page.wait_for_timeout(2500)
    warns = page.inner_text("#warns")
    check("หน้าเว็บเตือนว่าบอร์ดรีบูตเองและตำแหน่งมอเตอร์อาจเพี้ยน", "BROWNOUT" in warns and "ตั้งจุดกลาง" in warns, warns)
    page.evaluate("window.scrollTo(0,0)")
    shot(page, "05_brownout_warning.png")


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
