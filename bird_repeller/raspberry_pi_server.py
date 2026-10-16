#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
==========================================================================
 Bird Detection Server  (Raspberry Pi 4 Model B)  —  v7.3
 ใช้คู่กับเฟิร์มแวร์บอร์ดควบคุม BIRDCTRL v7.7 ขึ้นไป (v7.9 = ตั้งเวลาปั๊มรอบไล่จากเว็บได้)
--------------------------------------------------------------------------
 เปลี่ยนจาก v7.2 (รุ่นนี้):
   - สวิตช์ "ไล่ทันทีเมื่อเจอภาพเดียว" บนหน้าเว็บ: เจอนกเฟรมเดียวก็ฉีดน้ำเลย ไม่รอเฟรมยืนยัน
     (เร็วขึ้นราว 3 วิ แต่ไล่ผิดบ่อยขึ้น) จำค่าไว้ใน settings.json ปิดเครื่องแล้วไม่หาย

 เปลี่ยนจาก v7.1:
   - ปุ่ม "ถ่ายภาพเดี๋ยวนี้" ถ่ายทันทีตอนกด (เดิมต่อคิวหลังรอบตรวจอัตโนมัติ + รอแกนหยุด)
     และดูหลายเฟรมแล้วเลือกเฟรมที่คมสุด ภาพตรวจอัตโนมัติก็เลือกเฟรมคมสุดจาก 2 เฟรม
   - เก็บภาพเทรนซ้ำน้อยลงมาก: ภาพอัตโนมัติที่แทบเหมือนภาพที่เก็บล่าสุด (กรอบตรงกัน) ไม่เก็บ
     ก้ำกึ่ง/PIR/มีการเคลื่อนไหว มีระยะห่างขั้นต่ำ ภาพสุ่มนับเป็นเวลา (เดิมนับรอบ ได้ทุก 15 วิ)
   - การ์ดเทรน: ปุ่ม "ลบภาพซ้ำ" และ "ลบทั้งหมดในแท็บนี้" (ยังไม่ติดป้าย / ไม่มีนก) บอกพื้นที่ว่าง
   - ภาพเทรนเขียนลงการ์ดจริงก่อนนับว่าเสร็จ (fsync) ไฟดับ/ถอดปลั๊กแล้วไม่ได้ไฟล์ว่าง
     ภาพเสียที่ค้างจากก่อนหน้า (แสดงเป็น ? บนเว็บ) ถูกลบเองตอนเปิดเซิร์ฟเวอร์
   - เวลาฉีดน้ำรอบไล่ตั้งได้จากหน้าเว็บ ค่าเริ่มต้น 3 วิ (ต้องใช้บอร์ด v7.9)
     ปุ่มปั๊มบนเว็บเปิดนานเท่ากับรอบไล่

 เปลี่ยนจาก v7.0:
   - แบ่งเป็น 2 เวอร์ชันด้วยบรรทัด MODE_DEFAULT ต้นไฟล์ (หรือ env BIRD_MODE)
       "normal" = ใช้งานปกติ: ไล่นกจริง ไม่เก็บภาพเทรน ไม่มีการ์ดติดป้าย (ประหยัด SD การ์ด)
       "train"  = เก็บภาพไปเทรน: เก็บภาพถี่ขึ้น + การ์ดติดป้าย/ดาวน์โหลด ZIP
                  ไม่ไล่อัตโนมัติเป็นค่าเริ่มต้น นกจะได้อยู่ให้ถ่ายนาน ๆ (เปิดไล่ได้จากหน้าเว็บ)
     ชื่อโหมดเดิมยังใช้ได้: real = normal, field = train
   - หน้าเว็บบอกชัดว่าอยู่เวอร์ชันไหน

 เปลี่ยนจาก v6.8:
   - ล็อกอินด้วยรหัสผ่าน (แทน PIN เดิม) ทุกหน้าต้องล็อกอินก่อน รวมภาพสด ภาพย้อนหลัง
     log และไฟล์เทรน / ใส่ผิด 5 ครั้งล็อก 5 นาที / จำการล็อกอิน 30 วัน
     ครั้งแรกที่เปิดเว็บจะให้ตั้งรหัส / ลืมรหัส: python3 raspberry_pi_server.py --set-password
   - การ์ด "ข้อมูลสำหรับเทรน": ดูภาพชุดข้อมูลพร้อมกรอบที่โมเดลตีไว้ ติดป้าย
     ✓ นก / ✗ ไม่มีนก / ✎ ต้องแก้กรอบ / ลบภาพ แล้วดาวน์โหลด ZIP รูปแบบ YOLO
     (มีกรอบจากโมเดลให้ก่อน แก้เฉพาะที่ผิด) ไปเทรนต่อบน Colab/คอม
   - เก็บกรอบที่โมเดลเห็นคู่กับภาพชุดข้อมูล (ไฟล์ .json ข้างภาพ) และเก็บภาพทุกครั้งที่สั่งไล่

 เปลี่ยนจาก v6.7:
   - ตั้งความกว้างและความเร็วการกวาดตอนไล่นกได้จากหน้าเว็บ (การ์ด "ควบคุมด้วยมือ")
     พร้อมปุ่มทดสอบกวาดโดยไม่ฉีดน้ำ ไว้หาค่าที่หัวฉีดไม่ชนขอบ (ต้องใช้บอร์ด v7.7)

 เปลี่ยนจาก v6.6:
   - เฟรมยืนยันใช้เกณฑ์ 0.20 (เฟรมแรกยังต้อง 0.30) ซ้อมระบบแล้วพบว่านกที่คะแนน
     ก้ำกึ่ง 0.30-0.35 เฟรมยืนยันได้ 0.297 พลาดซ้ำ ๆ จนไม่ไล่

 เปลี่ยนจาก v6.5:
   - ปั๊มน้ำ 2 วิต่อครั้ง ทั้งปุ่มบนเว็บและรอบไล่ (บอร์ด v7.5) ประหยัดน้ำ
     (ทดสอบจริง: ปั๊มดูดน้ำ 1 ลิตรหมดใน 30-40 วิ)

 เปลี่ยนจาก v6.4:
   - ยืนยันแบบใหม่: ตรวจทุก 5 วิตามปกติ เจอนกเฟรมแรกแล้วถ่ายเฟรมยืนยันใน 3 วิ
     เจอ 2 เฟรมติดกันถึงไล่ (เดิม 2 ใน 3 รอบ ต้องรอ ~10 วิ)
   - ปุ่มปั๊มบนเว็บ 5 วิ (รอบไล่ของบอร์ด v7.4 ก็ 5 วิ)
   - ROI เริ่มต้นแบบเปิด ตัดขอบซ้าย-ขวาออก เหลือตรงกลาง ปรับด้วยแถบเลื่อนบนภาพจริง
     และกดกลับค่าเริ่มต้นได้
   - ภาพย้อนหลังกดดูภาพใหญ่ได้ ไม่กระพริบโหลดซ้ำทุกวินาทีแล้ว
   - จัดหน้าเว็บใหม่ให้ใช้บนมือถือง่ายขึ้น (ธีมเดิม)

 เปลี่ยนจาก v6.3 (รุ่นนี้ — ทดสอบภาคสนาม):
   - TEST_MODE เปลี่ยนเป็น MODE 3 แบบ: field (ค่าเริ่มต้น) / demo / real
     field = เจอนก 2 ใน 3 รอบแล้วไล่จริง + เก็บภาพและ log ทุกรอบ
   - หน้าเว็บบอกว่าอยู่โหมดไหน

 เปลี่ยนจาก v6.2 (รุ่นนี้ — PIR ค้าง):
   - แยกตัวนับ "ทริกใหม่" (EVT PIR) กับ "ทริกซ้ำระหว่าง OUT ค้าง" (EVT PIR REP)
     บันทึก PIR บอกว่าแต่ละครั้งทริกซ้ำไปกี่รอบ
   - บอร์ดเลิกทริกซ้ำเองเมื่อ OUT ค้าง HIGH เกิน 30 วิ และกล้องไม่เจอนก (EVT PSTUCK)
     หน้าเว็บขึ้นเตือนให้เช็กปุ่ม Sx/Tx

 เปลี่ยนจาก v6.1 (PIR):
   - สถานะ PIR บนเว็บมาจาก EVT PLVL ทันทีที่เปลี่ยน ไม่ใช่สุ่มดูจาก STAT ทุก 3 วิ
   - เพิ่มบันทึก PIR: แต่ละครั้ง HIGH นานเท่าไหร่ ทริกจริงไหม พร้อมตัวนับ
     ทริก / กระตุก (สัญญาณรบกวน) / ไม่สนใจเพราะมอเตอร์ ไว้หาสาเหตุ PIR หลอน
   - รับเฉพาะบรรทัด "EVT PIR" ตรงตัวเป็นการทริก

 เปลี่ยนจาก v6 (รุ่นนี้ — ไฟสถานะแบบใหม่):
   - ไฟเขียว = พร้อม (บอร์ดจัดการเอง) / ไฟส้ม = เจอนก: ตรวจเจอนกเมื่อไหร่ สั่ง BIRD 1 ทันที
     ไม่ขึ้นกับโหมดไล่อัตโนมัติ ทดสอบได้แม้อยู่ในโหมดทดสอบ
   - หน้าเว็บแสดงสถานะ PIR (อุ่นเครื่อง/ทริกล่าสุด) และไฟส้ม มีปุ่มทดสอบไฟส้ม
   - ข้อความสถานะบอกว่ารอบนั้นมาจาก PIR หรือจากรอบอัตโนมัติ

 เปลี่ยนจาก v5-usb (รุ่นนี้):
   การตรวจจับ (แก้อาการ "ติดบ้างไม่ติดบ้าง")
   - เปิด ROI แล้วจะตัดภาพเฉพาะกรอบก่อนส่งเข้า YOLO นกจึงตัวใหญ่ขึ้นในสายตาโมเดล
     (ทดสอบกับภาพจากกล้องจริง: ภาพเต็มไม่เจอนก -> ตัดกรอบแล้วเจอ 0.69)
   - เพิ่มคอนทราสต์ด้วย CLAHE ก่อนตรวจ ช่วยตอนภาพสว่างจ้าจนนกเป็นเงาดำ
     (ภาพเดียวกัน 0.42 -> 0.64)
   - ถ่ายรัวหลายเฟรมต่อรอบ เจอนกเฟรมไหนก็นับ คะแนนที่แกว่งตามท่านกจึงพลาดน้อยลง
   - ทิ้งเฟรมที่ภาพเสีย (ลายเส้นจาก USB) แล้วใช้เฟรมถัดไปแทน
   - ยืนยันแบบ "เจอ 2 ใน 3 รอบล่าสุด" แทน "ต้องเจอติดกัน"
   ความปลอดภัย
   - ปุ่มสั่งงานบนหน้าเว็บกันการยิงคำสั่งข้ามเว็บ (CSRF) และตั้ง PIN ได้ (env BIRD_PIN)
   - จำกัดจำนวนครั้งที่ไล่อัตโนมัติต่อชั่วโมง กันน้ำหมดเพราะโมเดลเห็นผิดซ้ำ ๆ
   - รู้ตัวเมื่อบอร์ด ESP32 รีบูต (เช่นไฟตก) แล้วแจ้งสถานะกล้องให้ใหม่ ไฟพร้อมไม่ดับค้าง
     และเตือนบนหน้าเว็บว่าตำแหน่งมอเตอร์อาจเพี้ยน
   - ข้อความตอนบอร์ดบูตไม่ถูกนับเป็นคำตอบของคำสั่งอีกต่อไป
   - ถ่ายภาพรอจนมอเตอร์หยุดจริง (เดิมอาจใช้สถานะเก่าได้ถึง 3 วินาที)
   ความเสถียร
   - ภาพสดไม่ทิ้งเธรดค้างตอนกล้องหลุด และจำกัดจำนวนผู้ชมพร้อมกัน
   - นับจำนวนภาพชุดข้อมูลในหน่วยความจำ ไม่ไล่อ่านโฟลเดอร์ทุกวินาที
   - โหมดทดสอบไม่เขียนทับ roi.json ของจริงอีกต่อไป
   - ใช้ไฟล์วิดีโอแทนเว็บแคมได้ (BIRD_CAM=/path/clip.mp4) ไว้ทดสอบกับคลิปที่อัดไว้

 เปลี่ยนจาก v5:
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
import re
import shutil
import sys
import threading
import time
import zipfile
from collections import deque
from datetime import datetime, timedelta

import cv2
import numpy as np
from flask import Flask, Response, jsonify, redirect, request, send_file, session
from werkzeug.security import check_password_hash, generate_password_hash

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
SETTINGS_FILE = os.path.join(DATA_DIR, "settings.json")   # ค่าที่เปลี่ยนจากหน้าเว็บแล้วต้องจำไว้

MODEL_PT = os.environ.get("BIRD_MODEL", os.path.join(BASE_DIR, "yolov8n.pt"))
MODEL_NCNN = os.path.join(BASE_DIR, "yolov8n_ncnn_model")

BIRD_CLASS_ID = 14          # COCO: 14 = bird
CONF_THRESHOLD = 0.35
IMG_SIZE = 640              # 320 เร็วสุด / 480 สมดุล / 640 แม่นสุดแต่ช้า
HISTORY_KEEP = 60

# ---------- เว็บแคม USB ----------
# ใช้ชื่อจาก /dev/v4l/by-id เพราะเลข /dev/videoX สลับได้ตอนรีบูต
# ใส่เป็นไฟล์วิดีโอก็ได้ (เช่น BIRD_CAM=~/clip.mp4) จะเล่นวนแทนเว็บแคม ไว้ทดสอบกับคลิปที่อัดไว้
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
CAM_SHARP_FRAMES = 2        # ถ่ายแต่ละครั้งดูกี่เฟรมติดกัน แล้วเลือกเฟรมที่คมสุด (กันภาพเบลอจากการขยับ)
CAM_SHARP_FRAMES_MANUAL = 5 # ปุ่มถ่ายภาพเดี๋ยวนี้ ดู 5 เฟรม (~0.3 วิ ที่ 15 fps)
STREAM_WIDTH = 960          # ย่อภาพสดก่อนส่งให้มือถือ ลดภาระฮอตสปอต
STREAM_MAX_FPS = 10
STREAM_MAX_CLIENTS = 3      # เปิดภาพสดพร้อมกันได้กี่เครื่อง แต่ละเครื่องกิน CPU แย่งกับ YOLO

# ---------- ปรับภาพก่อนตรวจจับ ----------
# ภาพจากหน้างานจริงสว่างจ้า นกเลยเป็นเงาดำ CLAHE ดึงรายละเอียดส่วนมืดกลับมา
DETECT_CLAHE = True
# เปิด ROI แล้วตัดภาพเฉพาะกรอบ (+ขอบเผื่อ) ก่อนส่งเข้าโมเดล นกจะตัวใหญ่ขึ้น
ROI_CROP = True
ROI_CROP_MARGIN = 0.10      # เผื่อขอบกี่ส่วนของขนาดกรอบ นกเกาะคร่อมขอบจะได้ไม่โดนตัดครึ่ง
# ถ่ายรัวหลายเฟรมต่อรอบ เจอนกเฟรมไหนก็นับ (เจอแล้วหยุดทันที ไม่เปลือง CPU)
BURST_FRAMES = 3
BURST_GAP_S = 0.25
# เฟรมเสีย: ต่างจากเฟรมก่อนหน้าและเฟรมถัดไปมาก แต่สองเฟรมนั้นเหมือนกัน
CORRUPT_DIFF_FRAC = 0.35    # สัดส่วนพิกเซลที่เปลี่ยนแรง ถึงจะถือว่า "ต่างมาก"

# ---------- ลูปตรวจจับอัตโนมัติ (อยู่บน Pi ไม่ใช่บนเบราว์เซอร์แล้ว) ----------
AUTO_DETECT_DEFAULT = True  # เปิดลูปตรวจจับตั้งแต่สตาร์ทไหม
AUTO_INTERVAL_S = 5         # ตรวจทุกกี่วินาที (เดิม 10 นกที่แวะแป๊บเดียวหลุดบ่อย)

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
DATASET_ENABLED = True          # ถูกตั้งใหม่ตามเวอร์ชันในหัวข้อ "เวอร์ชัน / โหมดการทำงาน" ด้านล่าง
DATASET_DIR = os.path.join(DATA_DIR, "dataset")
DATASET_LOG = os.path.join(DATASET_DIR, "dataset_log.csv")
DATASET_LOWCONF = 0.10          # เก็บกรอบที่ความมั่นใจตั้งแต่เท่านี้ แม้จะต่ำกว่าเกณฑ์นับ
DATASET_MOTION_FRAC = 0.02      # สัดส่วนพิกเซลที่เปลี่ยน (2%) ถึงจะถือว่ามีอะไรขยับ
DATASET_BIRD_MIN_GAP_S = 30     # ภาพที่เจอนก เก็บห่างกันอย่างน้อยกี่วินาที (นกเกาะนิ่งจะได้ไม่ซ้ำเป็นร้อยใบ)
DATASET_GAP_S = {"lowconf": 60, "pir": 20, "motion": 30}   # เหตุผลอื่น เก็บห่างกันอย่างน้อยกี่วินาที
DATASET_RANDOM_GAP_S = 1800     # ภาพพื้นหลังสุ่มเก็บทุกกี่วินาที (v7.2: เดิมนับรอบ ตรวจทุก 5 วิเลยได้ภาพซ้ำเยอะ)
# ภาพอัตโนมัติ (ก้ำกึ่ง/PIR/เคลื่อนไหว/สุ่ม) ที่ต่างจากภาพที่เก็บล่าสุดไม่ถึงเท่านี้ของพิกเซล
# และกรอบที่โมเดลเห็นอยู่ที่เดิม = ภาพซ้ำ ไม่เก็บ (เช่นถุงพลาสติกที่โมเดลเห็นก้ำกึ่งทุก 5 วิ)
DATASET_DUP_FRAC = 0.01
DATASET_MIN_FREE_MB = 1024      # พื้นที่ว่างเหลือน้อยกว่านี้ หยุดเก็บ
DATASET_LABELS = os.path.join(DATASET_DIR, "labels.json")   # ป้ายที่ติดจากหน้าเว็บ
DATASET_PRELABEL_CONF = 0.25    # กรอบจากโมเดลที่ความมั่นใจตั้งแต่เท่านี้ ใส่เป็นกรอบเริ่มต้นในไฟล์ YOLO
DATASET_PAGE = 24               # ภาพต่อหน้าในการ์ดข้อมูลเทรน

# ---------- การยืนยันก่อนสั่งไล่ ----------
# โมเดล COCO ที่ยังไม่ได้เทรนเองมีโอกาสเห็นผิด การบังคับให้เจอติดกันหลายเฟรม
# ช่วยตัดพวกที่โผล่มาเฟรมเดียวแล้วหายไปได้มาก
# ตรวจทุก AUTO_INTERVAL_S ตามปกติ พอเฟรมไหนเจอนก จะถ่ายเฟรมยืนยันตามมาใน CONFIRM_DELAY_S
# ต้องเจอติดกัน CONFIRM_FRAMES เฟรมถึงไล่ เฟรมไหนไม่เจอ = เริ่มนับใหม่
CONFIRM_FRAMES = 2          # ต้องเจอนกติดกันกี่เฟรมถึงจะสั่งไล่ (1 = ไม่ต้องยืนยัน)
CONFIRM_DELAY_S = 3         # เจอเฟรมแรกแล้ว รอกี่วินาทีค่อยถ่ายเฟรมยืนยัน (0 = ถ่ายทันที)
CONFIRM_WINDOW_S = 12       # เฟรมที่เจอห่างกันเกินนี้ ไม่นับว่าติดกัน
# เกณฑ์ของเฟรมถัดจากเฟรมที่เจอนก (เฟรมยืนยัน) ต่ำกว่าเกณฑ์ปกติ: เฟรมแรกต้องมั่นใจจริงก่อน
# แต่พอรู้แล้วว่ามีนก เฟรมยืนยันแค่ต้องเห็นนกตัวนั้นอีก (ซ้อมระบบ: นกตัวเดิมได้ 0.35 แล้ว 0.297
# พอใช้เกณฑ์ 0.30 เท่ากัน เฟรมยืนยันพลาดซ้ำ ๆ จนไม่ได้ไล่เลย)
CONFIRM_CONF = 0.20
# โหมดไล่ทันที: เจอนกเฟรมเดียวก็ไล่ ไม่ต้องรอเฟรมยืนยัน (เปิด/ปิดได้จากหน้าเว็บ จำไว้ใน settings.json)
# เร็วกว่าราว CONFIRM_DELAY_S วิ นกที่แวะแป๊บเดียวไม่หลุด แต่ของที่โมเดลเห็นผิดเฟรมเดียวก็โดนไล่
INSTANT_REPEL_DEFAULT = False

# ---------- ชุดขับไล่ ----------
SERIAL_PORT = os.environ.get("BIRD_SERIAL", "auto")
SERIAL_BAUD = 115200
HW_POLL_SEC = 3.0
HW_POLL_BUSY_SEC = 0.35
HW_LINK_LOST = 10.0

AUTO_REPEL_DEFAULT = True
REPEL_COOLDOWN_S = 10
REPEL_MAX_PER_HOUR = 20     # ไล่อัตโนมัติได้กี่ครั้งต่อชั่วโมง (กดปุ่มเองไม่นับ)
MOTOR_STEP_MAX = 400
# ช่วงที่ตั้งการกวาดตอนไล่ได้ (ต้องตรงกับ REPEL_SWEEP_*/REPEL_SPEED_* ในเฟิร์มแวร์)
SWEEP_DEG_MIN, SWEEP_DEG_MAX = 10, 80
SWEEP_SPD_MIN, SWEEP_SPD_MAX = 10, 100
PUMP_MS_MAX = 8000
PUMP_BUTTON_MS = 3000       # ปุ่มปั๊มบนหน้าเว็บเปิดกี่ ms (บอร์ด v7.9 ขึ้นไป ใช้เวลาเท่ารอบไล่ที่ตั้งจากเว็บ)
REPEL_PUMP_MS_MIN, REPEL_PUMP_MS_MAX = 1000, 8000   # ต้องตรงกับ REPEL_PUMP_MS_* ในเฟิร์มแวร์
STEPS_PER_REV = 1600

# ---------- ความปลอดภัยหน้าเว็บ (v7.0: ล็อกอินด้วยรหัสผ่าน) ----------
# ทุกหน้าต้องล็อกอินก่อน รหัสเก็บเป็น hash ใน auth.json (ไม่เก็บตัวรหัสจริง)
# ตั้งรหัสได้ 3 ทาง:
#   1) เปิดเว็บครั้งแรก จะมีหน้าให้ตั้งรหัส (ใครเปิดก่อนได้ตั้ง จึงควรตั้งทันทีหลังติดตั้ง)
#   2) บน Pi:  python3 raspberry_pi_server.py --set-password   (ใช้ตอนลืมรหัสด้วย)
#   3) env BIRD_PASSWORD=xxxx  ใช้เป็นรหัสเริ่มต้นเมื่อยังไม่มี auth.json (BIRD_PIN รุ่นเก่าก็ได้)
AUTH_FILE = os.path.join(DATA_DIR, "auth.json")
SECRET_FILE = os.path.join(DATA_DIR, "secret.key")   # กุญแจเซ็นคุกกี้ล็อกอิน สร้างเองครั้งแรก
LOGIN_DAYS = 30             # จำการล็อกอินในเบราว์เซอร์กี่วัน
LOGIN_MAX_FAIL = 5          # ใส่ผิดกี่ครั้งติดกันถึงล็อก
LOGIN_LOCK_S = 300          # ล็อกกี่วินาที
PASSWORD_MIN_LEN = 6
ENV_PASSWORD = os.environ.get("BIRD_PASSWORD") or os.environ.get("BIRD_PIN", "")

# ============================ เวอร์ชัน / โหมดการทำงาน ============================
# >>>>>  เลือกเวอร์ชันที่บรรทัดนี้  <<<<<
#   "normal" = ใช้งานปกติ: เจอนก 2 เฟรมติดกันแล้วไล่จริง (ปั๊ม + มอเตอร์)
#              ไม่เก็บภาพเทรน ไม่มีการ์ดติดป้าย (ภาพย้อนหลังที่พบนกและ log ยังเก็บตามปกติ)
#   "train"  = เก็บภาพไปเทรน (ใช้ตอนติดตั้งอุปกรณ์ครบแล้ว วางไว้หน้างานให้เก็บภาพ):
#              เก็บภาพดิบถี่ขึ้น + กรอบจากโมเดล, มีการ์ดติดป้ายและดาวน์โหลด ZIP แบบ YOLO
#              ไม่ไล่อัตโนมัติเป็นค่าเริ่มต้น นกจะได้อยู่ให้ถ่ายหลายท่า (เปิดไล่ได้จากสวิตช์บนหน้าเว็บ)
#   "demo"   = สาธิตบนโต๊ะ: ตีกรอบโชว์ ไม่ไล่เอง ไม่ยืนยันหลายเฟรม ปิด ROI ไม่เก็บภาพเทรน
# หรือเลือกตอนรันโดยไม่แก้ไฟล์:  BIRD_MODE=train python3 raspberry_pi_server.py
#   (ใน bird.service ใส่ Environment=BIRD_MODE=train ใต้ [Service])
# ชื่อโหมดรุ่นเก่ายังใช้ได้: real = normal, field = train
MODE_DEFAULT = "normal"

MODE = os.environ.get("BIRD_MODE", MODE_DEFAULT).strip().lower()
MODE = {"real": "normal", "field": "train"}.get(MODE, MODE)
if MODE not in ("normal", "train", "demo"):
    print(f"[Mode] ไม่รู้จักโหมด '{MODE}' ใช้ normal แทน", flush=True)
    MODE = "normal"
TEST_MODE = MODE == "demo"
TRAIN_MODE = MODE == "train"

if MODE in ("normal", "train"):
    CONF_THRESHOLD = 0.30       # ภาพจริงจากหน้างานเคยได้นกที่ 0.25-0.4 (เทรนโมเดลเองแล้วค่อยขยับขึ้น)
    CONFIRM_FRAMES = 2          # แต่ต้องเจอ 2 เฟรมติดกัน (เฟรมยืนยันถ่ายตามใน 3 วิ) ถึงไล่
    AUTO_REPEL_DEFAULT = True

DATASET_ENABLED = TRAIN_MODE    # เก็บภาพเทรนเฉพาะเวอร์ชัน train
if TRAIN_MODE:
    AUTO_REPEL_DEFAULT = False  # ปล่อยนกอยู่ให้ถ่ายได้หลายภาพ (กดเปิดไล่บนหน้าเว็บได้)
    DATASET_BIRD_MIN_GAP_S = 10     # เจอนกเก็บถี่ขึ้น (ปกติ 30 วิ)
    DATASET_GAP_S = {"lowconf": 30, "pir": 10, "motion": 15}
    DATASET_RANDOM_GAP_S = 600      # ภาพพื้นหลังไม่มีนก สุ่มเก็บทุก 10 นาที (ปกติ 30 นาที)

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

# บรรทัดที่บอร์ดส่งตอนบูต (เฟิร์มแวร์รุ่นเก่าขึ้นต้นด้วย OK) ห้ามนับเป็นคำตอบของคำสั่ง
_BOOT_LINES = ("OK READY", "OK FW ", "OK HW ")

os.makedirs(DATA_DIR, exist_ok=True)
os.makedirs(HIST_DIR, exist_ok=True)

app = Flask(__name__)


def _load_secret():
    try:
        with open(SECRET_FILE, "rb") as f:
            key = f.read()
        if len(key) >= 32:
            return key
    except OSError:
        pass
    key = os.urandom(32)
    atomic_write_bytes(SECRET_FILE, key)
    os.chmod(SECRET_FILE, 0o600)
    return key


app.config.update(SESSION_COOKIE_HTTPONLY=True, SESSION_COOKIE_SAMESITE="Lax",
                  SESSION_COOKIE_NAME="bird_session",
                  PERMANENT_SESSION_LIFETIME=timedelta(days=LOGIN_DAYS))

# ============================ AUTH ============================
# ver เพิ่มทุกครั้งที่เปลี่ยนรหัส คุกกี้ที่ออกก่อนหน้าจะใช้ไม่ได้ทันที (ทุกเครื่องต้องล็อกอินใหม่)
_auth = {"hash": "", "ver": 0, "mtime": None}
_auth_lock = threading.Lock()
_login_fail = {}            # ip -> [ครั้งที่ผิดติดกัน, ล็อกถึงเวลา]


def _auth_refresh():
    """อ่าน auth.json ใหม่ถ้าไฟล์เปลี่ยน (เช่น ตั้งรหัสด้วย --set-password ตอนเซิร์ฟเวอร์รันอยู่)"""
    try:
        mtime = os.path.getmtime(AUTH_FILE)
    except OSError:
        mtime = None
    with _auth_lock:
        if mtime == _auth["mtime"]:
            return
        _auth["mtime"] = mtime
        if mtime is None:
            return
        try:
            with open(AUTH_FILE, encoding="utf-8") as f:
                data = json.load(f)
            _auth["hash"] = str(data.get("hash", ""))
            _auth["ver"] = int(data.get("ver", 0))
        except (OSError, ValueError) as e:
            print(f"[Auth] อ่าน {AUTH_FILE} ไม่ได้: {e}", flush=True)


def set_password(pw):
    with _auth_lock:
        ver = _auth["ver"] + 1
        data = {"hash": generate_password_hash(pw), "ver": ver,
                "changed": datetime.now().strftime("%Y-%m-%d %H:%M:%S")}
        atomic_write_bytes(AUTH_FILE, json.dumps(data).encode("utf-8"))
        os.chmod(AUTH_FILE, 0o600)
        _auth.update(hash=data["hash"], ver=ver, mtime=os.path.getmtime(AUTH_FILE))
    return ver


def password_is_set():
    _auth_refresh()
    return bool(_auth["hash"])


def check_password(pw):
    _auth_refresh()
    h = _auth["hash"]
    return bool(h) and check_password_hash(h, pw)


def logged_in():
    return password_is_set() and session.get("auth") == _auth["ver"]


def _login_session():
    session.clear()
    session.permanent = True
    session["auth"] = _auth["ver"]


def _client_ip():
    return request.remote_addr or "?"


def _lock_left(ip):
    rec = _login_fail.get(ip)
    if not rec:
        return 0
    return max(0, int(rec[1] - time.time() + 0.999))


def _note_fail(ip):
    rec = _login_fail.setdefault(ip, [0, 0.0])
    rec[0] += 1
    if rec[0] >= LOGIN_MAX_FAIL:
        rec[0] = 0
        rec[1] = time.time() + LOGIN_LOCK_S
    if len(_login_fail) > 500:                       # กันตารางโตไม่หยุด
        for k in [k for k, v in _login_fail.items() if v[1] < time.time()][:250]:
            _login_fail.pop(k, None)


def init_auth():
    _auth_refresh()
    if not _auth["hash"] and ENV_PASSWORD:
        if len(ENV_PASSWORD) < 4:
            print("[Auth] BIRD_PASSWORD สั้นเกินไป ไม่ใช้", flush=True)
        else:
            set_password(ENV_PASSWORD)
            print("[Auth] ตั้งรหัสเริ่มต้นจาก BIRD_PASSWORD แล้ว", flush=True)


def cli_set_password():
    import getpass
    print("ตั้งรหัสผ่านหน้าเว็บใหม่ (เครื่องที่ล็อกอินอยู่ทุกเครื่องจะต้องล็อกอินใหม่)")
    while True:
        pw = getpass.getpass("รหัสใหม่: ")
        if len(pw) < PASSWORD_MIN_LEN:
            print(f"ต้องยาวอย่างน้อย {PASSWORD_MIN_LEN} ตัวอักษร")
            continue
        if getpass.getpass("พิมพ์ซ้ำอีกครั้ง: ") != pw:
            print("สองครั้งไม่ตรงกัน ลองใหม่")
            continue
        break
    _auth_refresh()
    set_password(pw)
    print(f"บันทึกแล้วที่ {AUTH_FILE} (ถ้าเซิร์ฟเวอร์รันอยู่ จะใช้รหัสใหม่ทันที)")


class _QuietPoll(logging.Filter):
    def filter(self, record):
        msg = record.getMessage()
        return "/api/state" not in msg


logging.getLogger("werkzeug").addFilter(_QuietPoll())

# ============================ MODEL ============================
_model = None
_model_lock = threading.Lock()
_model_name = "ยังไม่ได้โหลด"


def load_model():
    global _model, _model_name, BIRD_CLASS_ID
    from ultralytics import YOLO

    if os.path.isdir(MODEL_NCNN):
        _model = YOLO(MODEL_NCNN, task="detect")
        _model_name = "yolov8n (NCNN)"
    else:
        _model = YOLO(MODEL_PT)
        _model_name = "yolov8n (PyTorch)"

    dummy = np.zeros((IMG_SIZE, IMG_SIZE, 3), dtype=np.uint8)
    _model.predict(dummy, imgsz=IMG_SIZE, verbose=False)

    # โมเดลที่เทรนเอง (คลาสเดียว "bird") นกเป็นคลาส 0 ไม่ใช่ 14 แบบโมเดล COCO
    names = getattr(_model, "names", None) or {}
    ids = [int(k) for k, v in dict(names).items() if str(v).lower() == "bird"]
    if ids and ids[0] != BIRD_CLASS_ID:
        BIRD_CLASS_ID = ids[0]
        print(f"[Model] คลาส bird ของโมเดลนี้คือ {BIRD_CLASS_ID}", flush=True)
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
# ค่าเริ่มต้น: เปิด ตัดขอบซ้าย-ขวาออก เหลือตรงกลาง (ปรับ/ปิดได้จากหน้าเว็บ)
ROI_DEFAULT = {"enabled": True, "x1": 0.2, "y1": 0.0, "x2": 0.8, "y2": 1.0}
ROI_FILE_VER = 2            # roi.json รุ่นก่อน v6.5 (ไม่มี ver) ไม่โหลด ใช้ค่าเริ่มต้นใหม่แทน
ROI = dict(ROI_DEFAULT)


def load_roi():
    if TEST_MODE:
        # โหมดทดสอบ: ไม่โหลดค่าเก่าจาก roi.json เลย เริ่มแบบปิด ROI เสมอ
        # (ไฟล์เดิมไม่ถูกลบ เปลี่ยนไปโหมดอื่นแล้วค่าเก่ากลับมาเอง)
        ROI["enabled"] = False
        print("[ROI] โหมดทดสอบ -> ปิด ROI", flush=True)
        return
    try:
        with open(ROI_FILE, encoding="utf-8") as f:
            data = json.load(f)
        if data.get("ver") != ROI_FILE_VER:
            print("[ROI] roi.json รุ่นเก่า ใช้ค่าเริ่มต้นใหม่ (ตัดขอบซ้าย-ขวา)", flush=True)
            return
        for k in ROI:
            if k in data:
                ROI[k] = data[k]
        print(f"[ROI] โหลดค่าเดิม: {ROI}", flush=True)
    except (OSError, ValueError):
        pass


def save_roi():
    if TEST_MODE:
        # โหมดทดสอบไม่เขียนทับ roi.json ของจริง ปิดโหมดทดสอบแล้วค่าเดิมยังอยู่ครบ
        return
    try:
        with open(ROI_FILE, "w", encoding="utf-8") as f:
            json.dump(dict(ROI, ver=ROI_FILE_VER), f)
    except OSError:
        pass


SETTINGS = {"instant_repel": INSTANT_REPEL_DEFAULT}


def load_settings():
    try:
        with open(SETTINGS_FILE, encoding="utf-8") as f:
            data = json.load(f)
        if isinstance(data.get("instant_repel"), bool):
            SETTINGS["instant_repel"] = data["instant_repel"]
    except (OSError, ValueError, AttributeError):
        pass


def save_settings():
    try:
        os.makedirs(DATA_DIR, exist_ok=True)
        atomic_write_bytes(SETTINGS_FILE, json.dumps(SETTINGS).encode("utf-8"), durable=True)
    except OSError as e:
        print(f"[IO] บันทึก settings.json ไม่สำเร็จ: {e}", flush=True)


def confirm_frames():
    """ต้องเจอนกติดกันกี่เฟรมถึงไล่ (เปิดโหมดไล่ทันที = 1)"""
    return 1 if SETTINGS["instant_repel"] else CONFIRM_FRAMES


# ============================ HELPERS ============================
def atomic_write_bytes(path, data, durable=False):
    """durable=True รอจนข้อมูลลงการ์ดจริง (fsync) ไฟดับทันทีหลังจากนี้ไฟล์ก็ไม่เสีย
    ใช้กับไฟล์ที่เก็บถาวร (ภาพเทรน ป้าย) ไม่ใช้กับภาพล่าสุดที่เขียนทับทุกไม่กี่วินาที"""
    tmp = f"{path}.{os.getpid()}.part"
    with open(tmp, "wb") as f:
        f.write(data)
        if durable:
            f.flush()
            os.fsync(f.fileno())
    os.replace(tmp, path)


def atomic_write_jpeg(path, image_bgr, quality=85, durable=False):
    if durable:
        ok, buf = cv2.imencode(".jpg", image_bgr, [int(cv2.IMWRITE_JPEG_QUALITY), quality])
        if not ok:
            raise IOError(f"แปลงภาพไม่สำเร็จ: {path}")
        atomic_write_bytes(path, buf.tobytes(), durable=True)
        return
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
_clahe = cv2.createCLAHE(clipLimit=2.0, tileGridSize=(8, 8))


def enhance(image_bgr):
    """เพิ่มคอนทราสต์เฉพาะความสว่าง (สีไม่เพี้ยน) ให้นกที่เป็นเงาดำเห็นรายละเอียดขึ้น"""
    lab = cv2.cvtColor(image_bgr, cv2.COLOR_BGR2LAB)
    lab[:, :, 0] = _clahe.apply(lab[:, :, 0])
    return cv2.cvtColor(lab, cv2.COLOR_LAB2BGR)


def detect_and_draw(image_bgr, threshold=None, boxes_out=None):
    """รัน YOLO เฉพาะคลาสนก กรองด้วย ROI แล้ววาดกรอบลงภาพ

    คืน (จำนวนนกที่นับ, ความมั่นใจสูงสุด, เวลาที่ใช้เป็น ms, ความมั่นใจสูงสุดของกรอบที่ต่ำกว่าเกณฑ์)
    boxes_out: ถ้าส่ง list มา จะได้ทุกกรอบที่โมเดลเห็น [x1, y1, x2, y2, conf] พิกัดภาพเต็ม
               (รวมกรอบอ่อนและกรอบนอก ROI) ไว้เป็นกรอบเริ่มต้นของชุดข้อมูลเทรน
    """
    if _model is None:
        return 0, 0.0, 0, 0.0
    if threshold is None:
        threshold = CONF_THRESHOLD

    h, w = image_bgr.shape[:2]
    roi = dict(ROI)

    # กรอบ ROI เป็นพิกเซล
    rx1 = int(roi["x1"] * w)
    ry1 = int(roi["y1"] * h)
    rx2 = int(roi["x2"] * w)
    ry2 = int(roi["y2"] * h)

    # ตัดภาพเฉพาะ ROI (+ขอบเผื่อ) ก่อนส่งเข้าโมเดล
    # โมเดลย่อภาพให้เหลือ IMG_SIZE เสมอ ภาพที่เล็กลงจึงถูกขยาย นกตัวใหญ่ขึ้นในสายตาโมเดล
    ox, oy, crop = 0, 0, image_bgr
    if roi["enabled"] and ROI_CROP:
        mx = int((rx2 - rx1) * ROI_CROP_MARGIN)
        my = int((ry2 - ry1) * ROI_CROP_MARGIN)
        cx1, cy1 = max(0, rx1 - mx), max(0, ry1 - my)
        cx2, cy2 = min(w, rx2 + mx), min(h, ry2 + my)
        if (cx2 - cx1) * (cy2 - cy1) < 0.8 * w * h:       # กรอบเกือบเต็มภาพ ตัดไปก็ไม่ได้อะไร
            ox, oy, crop = cx1, cy1, image_bgr[cy1:cy2, cx1:cx2]

    model_input = enhance(crop) if DETECT_CLAHE else crop

    t0 = time.perf_counter()
    with _model_lock:
        results = _model.predict(
            model_input,
            imgsz=IMG_SIZE,
            conf=min(threshold, DATASET_LOWCONF) if DATASET_ENABLED else threshold,
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
            x1, x2, y1, y2 = x1 + ox, x2 + ox, y1 + oy, y2 + oy   # กลับเป็นพิกัดภาพเต็ม
            conf = float(b.conf[0])
            if boxes_out is not None:
                boxes_out.append([max(0, x1), max(0, y1), min(w, x2), min(h, y2), round(conf, 3)])

            # กรอบที่ต่ำกว่าเกณฑ์: ไม่นับ ไม่วาด แต่จำไว้ให้ระบบเก็บชุดข้อมูล
            if conf < threshold:
                weak_max = max(weak_max, conf)
                continue

            # กรอง ROI โดยดูจุดกึ่งกลางกล่อง ไม่ใช่ทั้งกล่อง
            # เพราะนกที่เกาะคร่อมขอบกรอบก็ยังควรนับ
            cx = (x1 + x2) // 2
            cy = (y1 + y2) // 2
            inside = (not roi["enabled"]) or (rx1 <= cx <= rx2 and ry1 <= cy <= ry2)

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

    if roi["enabled"]:
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
            "water_fault": False, "pump_duty_left": None,
            "pos_ok": None, "reset_reason": "", "ready": False,
            "bird_lamp": False, "pir_warm": 0,
            "pir_trig": 0, "pir_rep": 0, "pir_stuck": False,
            "pir_glitch": 0, "pir_ignored": 0,
            "vbat": 0.0,
        }
        self.last_error = ""
        self.auto_repel = AUTO_REPEL_DEFAULT
        self.last_repel_at = 0.0
        self.repel_count = 0
        self._auto_repels = deque()          # เวลาที่ไล่อัตโนมัติ ใช้นับโควตาต่อชั่วโมง
        self._streak = 0                     # เจอนกติดกันกี่เฟรมแล้ว
        self._streak_at = 0.0                # เวลาเฟรมล่าสุดที่เจอ
        self.resync = threading.Event()      # บอร์ดรีบูต/ขาดการติดต่อ -> ต้องแจ้งสถานะกล้องใหม่
        self.board_boots = 0
        self.motion_until = 0.0              # เพิ่งสั่งหมุน ให้ถือว่ายังหมุนอยู่จนกว่า STAT จะยืนยัน
        self.last_pir_at = 0.0
        self.pir_count = 0
        self.pir_rep_count = 0
        self.pir_log = deque(maxlen=12)      # แต่ละครั้งที่ OUT เป็น HIGH: เวลา / นานเท่าไหร่ / ทริกไหม

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

            if line.startswith("EVT") or line.startswith(_BOOT_LINES):
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

        up = cmd.strip().upper()
        ok = not line.startswith("ERR")
        if ok and (up.startswith("REPEL") or (up.startswith("MOT") and up not in ("MOT STOP", "MOT ZERO"))):
            # STAT รอบถัดไปอาจยังไม่ทันบอกว่ากำลังหมุน กันไม่ให้ถ่ายภาพตอนแกนยังหมุน
            self.motion_until = time.time() + 1.0
        if not up.startswith("STAT"):
            self._wake.set()
        return ok, line

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
                self.hw["ready"] = vals.get("READY") == "1"
                # ค่าที่มีเฉพาะเฟิร์มแวร์ v7 ขึ้นไป (รุ่นเก่าไม่มี = None)
                self.hw["water_fault"] = vals.get("WFAULT") == "1"
                self.hw["pump_duty_left"] = int(vals["PDUTY"]) if "PDUTY" in vals else None
                self.hw["pos_ok"] = (vals["POSOK"] == "1") if "POSOK" in vals else None
                self.hw["reset_reason"] = vals.get("RST", "")
                self.hw["bird_lamp"] = vals.get("BIRD") == "1"
                self.hw["pir_warm"] = int(vals.get("PIRW", 0))
                self.hw["pir_trig"] = int(vals.get("PIRN", 0))
                self.hw["pir_rep"] = int(vals.get("PIRR", 0))
                self.hw["pir_stuck"] = vals.get("PSTK") == "1"
                self.hw["pir_glitch"] = int(vals.get("PIRG", 0))
                self.hw["pir_ignored"] = int(vals.get("PIRI", 0))
                # v7.7 ขึ้นไป: การกวาดตอนไล่ (รุ่นเก่าไม่มี = None ซ่อนตัวปรับบนเว็บ)
                self.hw["sweep_deg"] = int(vals["SWP"]) if "SWP" in vals else None
                self.hw["sweep_spd"] = int(vals["RSP"]) if "RSP" in vals else None
                # v7.9 ขึ้นไป: เวลาฉีดน้ำต่อรอบไล่ (ms)
                self.hw["repel_pump_ms"] = int(vals["RPM"]) if "RPM" in vals else None
            except ValueError:
                pass

    def _on_event(self, line):
        print(f"[CTRL] {line}", flush=True)
        if line.startswith("EVT BOOT") or line.startswith("OK READY"):
            # บอร์ดรีบูต: สถานะกล้องบนบอร์ดหาย (ไฟเขียวจะดับค้าง) และตำแหน่งมอเตอร์อาจเพี้ยน
            self.board_boots += 1
            self.resync.set()
            if "RST=" in line:
                reason = line.split("RST=", 1)[1].split()[0]
                with self._hw_lock:
                    self.hw["reset_reason"] = reason
                if reason in ("BROWNOUT", "PANIC", "WDT"):
                    self.last_error = f"บอร์ดควบคุมรีบูตเอง ({reason}) ตำแหน่งมอเตอร์อาจเพี้ยน"
        elif line.startswith("EVT SAFE"):
            self.last_error = SAFE_MSG
            self.resync.set()
        elif line.startswith("EVT LINK OK"):
            if self.last_error == SAFE_MSG:       # ล้างเฉพาะข้อความเรื่องขาดการติดต่อ
                self.last_error = ""
            self.resync.set()           # บอร์ดล้างสถานะกล้องตอนขาดการติดต่อ ต้องบอกใหม่
        elif line.startswith("EVT WATER FAULT"):
            self.last_error = "เซนเซอร์ระดับน้ำอ่านไม่ได้ ล็อกปั๊มไว้ก่อน (เช็กสาย HC-SR04)"
        elif line.startswith("EVT WATER SENSOR OK"):
            self.last_error = ""
        elif line.startswith("EVT REPEL FAIL"):
            self.last_error = "รอบไล่ถูกยกเลิก: " + line[len("EVT REPEL FAIL"):].strip()
        elif line.startswith("EVT WATER EMPTY"):
            self.last_error = "น้ำหมด ปั๊มถูกล็อกไว้"
        elif line.startswith("EVT WATER OK"):
            self.last_error = ""
        elif line.startswith("EVT BIRDLAMP"):
            with self._hw_lock:
                self.hw["bird_lamp"] = line.endswith("1")
        elif line.startswith("EVT PLVL"):
            # ระดับ PIR เปลี่ยน (กรองสัญญาณกระตุกแล้ว): "EVT PLVL 1" / "EVT PLVL 0 <HIGH นานกี่ ms>"
            parts = line.split()
            high = len(parts) > 2 and parts[2] == "1"
            with self._hw_lock:
                self.hw["pir"] = high
                if high:
                    self.pir_log.appendleft({"ts": datetime.now().strftime("%H:%M:%S"),
                                             "held": None, "trig": False, "reps": 0, "stuck": False})
                elif self.pir_log and self.pir_log[0]["held"] is None and len(parts) > 3:
                    self.pir_log[0]["held"] = round(int(parts[3]) / 1000, 1)
        elif line.startswith("EVT PREADY"):
            with self._hw_lock:
                self.hw["pir_warm"] = 0
        elif line.startswith("EVT PSTUCK"):
            # OUT ค้าง HIGH นานโดยกล้องไม่เจอนก บอร์ดเลิกทริกซ้ำแล้ว: "EVT PSTUCK 1 <วิ>" / "EVT PSTUCK 0"
            stuck = line.split()[2:3] == ["1"]
            with self._hw_lock:
                self.hw["pir_stuck"] = stuck
                if stuck and self.pir_log:
                    self.pir_log[0]["stuck"] = True
        elif line.strip() in ("EVT PIR", "EVT PIR REP"):
            repeat = line.strip() == "EVT PIR REP"
            self.last_pir_at = time.time()
            if repeat:
                self.pir_rep_count += 1
            else:
                self.pir_count += 1
            with self._hw_lock:
                if not self.pir_log:
                    self.pir_log.appendleft({"ts": datetime.now().strftime("%H:%M:%S"),
                                             "held": None, "trig": False, "reps": 0, "stuck": False})
                if repeat:
                    self.pir_log[0]["reps"] += 1
                else:
                    self.pir_log[0]["trig"] = True
            if PIR_TRIGGER_ENABLED:
                request_detection("pir")

    def snapshot(self):
        with self._hw_lock:
            hw = dict(self.hw)
            hw["pir_log"] = [dict(e) for e in self.pir_log]
        now = time.time()
        left = REPEL_COOLDOWN_S - (now - self.last_repel_at)
        hw.update({
            "link": self.linked,
            "port": self._port,
            "error": self.last_error,
            "auto_repel": self.auto_repel,
            "cooldown_left": round(left, 1) if left > 0 else 0,
            "repel_count": self.repel_count,
            "repel_hour_left": REPEL_MAX_PER_HOUR - self._repels_last_hour(now),
            "steps_per_rev": STEPS_PER_REV,
            "busy": hw["moving"] or hw["repel"] or now < self.motion_until,
            "pir_ago": round(now - self.last_pir_at) if self.last_pir_at else None,
            "pir_count": self.pir_count,
            "pir_rep_count": self.pir_rep_count,
        })
        return hw

    def _repels_last_hour(self, now):
        while self._auto_repels and now - self._auto_repels[0] > 3600:
            self._auto_repels.popleft()
        return len(self._auto_repels)

    def confirm_hits(self):
        """เจอนกติดกันกี่เฟรมแล้ว (เฟรมล่าสุดเก่าเกิน CONFIRM_WINDOW_S = เริ่มใหม่)"""
        if self._streak and time.time() - self._streak_at > CONFIRM_WINDOW_S:
            return 0
        return self._streak

    # ---------- ตรรกะไล่อัตโนมัติ ----------
    def maybe_auto_repel(self, bird_count):
        """เรียกหลังตรวจจับเสร็จ คืนข้อความเหตุผลเพื่อบันทึกลง log"""
        now = time.time()
        if bird_count > 0:
            self._streak = self.confirm_hits() + 1
            self._streak_at = now
        else:
            self._streak = 0                     # เฟรมไหนไม่เจอ ต้องเริ่มนับใหม่
        hits = self._streak
        with _state_lock:
            STATE["confirm_streak"] = hits
        if bird_count <= 0:
            return "ไม่เจอนก"

        # ต้องเจอติดกัน confirm_frames() เฟรม ยังไม่ครบ -> ถ่ายเฟรมยืนยันตามไปเลย ไม่รอรอบ 5 วิ
        need = confirm_frames()
        if hits < need:
            schedule_confirm()
            return (f"รอยืนยัน {hits}/{need} (ถ่ายซ้ำใน {CONFIRM_DELAY_S:g} วิ)"
                    if CONFIRM_DELAY_S > 0 else f"รอยืนยัน {hits}/{need} (ถ่ายซ้ำทันที)")
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
        if self._repels_last_hour(now) >= REPEL_MAX_PER_HOUR:
            return f"ครบโควตาไล่ {REPEL_MAX_PER_HOUR} ครั้ง/ชม. แล้ว"

        ok, line = self.send("REPEL", timeout=5.0)
        if ok:
            self.last_repel_at = now
            self.repel_count += 1
            self._auto_repels.append(now)
            self._streak = 0                     # ไล่แล้วเริ่มนับใหม่
            with _state_lock:
                STATE["confirm_streak"] = 0
            return "สั่งไล่แล้ว"
        return f"สั่งไล่ไม่สำเร็จ: {line}"


SAFE_MSG = "บอร์ดตัดโหลดเองเพราะขาดการติดต่อกับ Pi"
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
        self.is_file = False
        self._prev_small = None      # เฟรมก่อนหน้า (ย่อแล้ว) ไว้เทียบหาเฟรมเสีย
        self._small = None
        self.corrupt_count = 0
        self.last_error = "ยังไม่ได้เปิดกล้อง"

    def _open(self):
        dev = os.path.expanduser(CAM_DEVICE)
        self.is_file = os.path.isfile(dev)            # ไฟล์วิดีโอ (โหมดทดสอบกับคลิป)
        if self.is_file:
            cap = cv2.VideoCapture(dev)
        else:
            dev = dev if os.path.exists(dev) else CAM_DEVICE_FALLBACK
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
            if self.is_file:
                if not ok:                          # คลิปจบ เล่นวนใหม่
                    self._cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
                    continue
                time.sleep(1.0 / CAM_FPS)           # ไฟล์อ่านได้เร็วมาก หน่วงให้เหมือนกล้องจริง
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
            small = _small_gray(frame)
            with self._cond:
                self._prev_small, self._small = self._small, small
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
        """รอจนได้เฟรมที่ถ่ายหลังเวลา t0 คืน (สำเนาภาพ, ภาพย่อ, ภาพย่อของเฟรมก่อนหน้า)
        หรือ (None, None, None) ถ้าหมดเวลา"""
        deadline = time.time() + timeout
        with self._cond:
            while self._frame is None or self._t <= t0:
                left = deadline - time.time()
                if left <= 0:
                    return None, None, None
                self._cond.wait(left)
            return self._frame.copy(), self._small, self._prev_small

    def good_frame_after(self, t0, timeout):
        """เหมือน frame_after แต่ข้ามเฟรมเสีย (ภาพลายจากการส่งผ่าน USB ผิดพลาด)

        เฟรมเสียคือเฟรมที่ต่างจากทั้งเฟรมก่อนหน้าและเฟรมถัดไปมาก ขณะที่สองเฟรมนั้นเหมือนกัน
        ถ้าแค่มีอะไรขยับจริง (คนเดินผ่าน/แสงเปลี่ยน) เฟรมถัดไปก็จะยังต่างจากเฟรมก่อนหน้าด้วย
        """
        deadline = time.time() + timeout
        frame, small, prev = self.frame_after(t0, timeout)
        if frame is None or prev is None or _diff_frac(small, prev) < CORRUPT_DIFF_FRAC:
            return frame
        # ต่างจากเฟรมก่อนหน้ามาก ดูเฟรมถัดไปเพื่อแยกว่าภาพเสียหรือฉากเปลี่ยนจริง
        with self._cond:
            t_frame = self._t
        nxt, nsmall, _ = self.frame_after(t_frame, max(0.2, deadline - time.time()))
        if nxt is None:
            return frame
        if _diff_frac(nsmall, prev) < CORRUPT_DIFF_FRAC <= _diff_frac(nsmall, small):
            self.corrupt_count += 1
            print(f"[Cam] ทิ้งเฟรมเสีย (รวม {self.corrupt_count} เฟรม)", flush=True)
            return nxt
        return frame

    def sharp_frame_after(self, t0, n, timeout):
        """ดู n เฟรมดีที่ถ่ายหลัง t0 แล้วคืนเฟรมที่คมที่สุด (ภาพสั่น/วัตถุขยับจะเบลอ ค่าความคมต่ำ)"""
        deadline = time.time() + timeout
        best, best_score, t = None, -1.0, t0
        for _ in range(max(1, n)):
            left = deadline - time.time()
            if left <= 0:
                break
            frame = self.good_frame_after(t, left)
            if frame is None:
                break
            with self._cond:
                t = self._t
            score = _sharpness(frame)
            if score > best_score:
                best, best_score = frame, score
        return best

    def next_frame(self, last_seq, timeout=2.0):
        """ใช้กับสตรีม รอเฟรมถัดไป คืน (ภาพ, seq)"""
        with self._cond:
            if self._seq == last_seq:
                self._cond.wait(timeout)
            if self._frame is None or self._seq == last_seq:
                return None, last_seq
            return self._frame, self._seq

    def last_frame(self):
        with self._cond:
            return self._frame


def _small_gray(frame):
    g = cv2.cvtColor(cv2.resize(frame, (160, 120), interpolation=cv2.INTER_AREA),
                     cv2.COLOR_BGR2GRAY)
    return cv2.GaussianBlur(g, (5, 5), 0)


def _sharpness(frame):
    """ความคมของภาพ = ความแปรปรวนของ Laplacian (ขอบชัด = ค่าสูง)"""
    g = cv2.cvtColor(cv2.resize(frame, (320, 240), interpolation=cv2.INTER_AREA), cv2.COLOR_BGR2GRAY)
    return float(cv2.Laplacian(g, cv2.CV_64F).var())


def _diff_frac(a, b):
    """สัดส่วนพิกเซลที่ต่างกันแรง ระหว่างภาพย่อสองภาพ"""
    return float((cv2.absdiff(a, b) > 40).mean())


CAM = Webcam()


def camera_online():
    return CAM.online()


def wait_still():
    """รอให้แกนหมุนหยุดจริงก่อนถ่าย ภาพจะได้ไม่เบลอและไม่ใช่มุมเก่าก่อนหมุน"""
    t_wait = time.time()
    while CTRL.snapshot().get("busy") and time.time() - t_wait < CAM_WAIT_STILL_S:
        CTRL._wake.set()                       # ขอ STAT ใหม่ทันที ไม่ต้องรอรอบ 3 วินาที
        time.sleep(0.1)


def capture_frame(n=CAM_SHARP_FRAMES):
    """ถ่ายภาพนิ่งหนึ่งใบจากเว็บแคม คืน (ภาพ BGR, ข้อความผิดพลาด)

    เอาเฟรมที่ถ่ายหลังจากเรียกเท่านั้น ข้ามเฟรมเสีย และเลือกเฟรมที่คมสุดจาก n เฟรม
    """
    frame = CAM.sharp_frame_after(time.time(), n, CAM_FETCH_TIMEOUT)
    if frame is None:
        return None, CAM.last_error or "ไม่ได้รับภาพจากเว็บแคม"
    return frame, ""


# ============================ DATASET ============================
_ds = {"prev_gray": None, "prev_pos": None, "rounds": 0,
       "last": {}, "saved": None, "full_warned": False}
# last  = เวลาที่เก็บล่าสุดแยกตามเหตุผล
# saved = (ภาพย่อ, ตำแหน่งมอเตอร์, กรอบ) ของภาพที่เก็บล่าสุด ไว้เช็กภาพซ้ำ
_ds_lock = threading.Lock()


def _ds_gray(img):
    """ภาพย่อขาวดำ 160x120 เบลอเล็กน้อย ไว้เทียบว่าภาพเปลี่ยนไปแค่ไหน"""
    if img.ndim == 3:
        img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
    img = cv2.resize(img, (160, 120), interpolation=cv2.INTER_AREA)
    return cv2.GaussianBlur(img, (5, 5), 0)


def _box_iou(a, b):
    iw = min(a[2], b[2]) - max(a[0], b[0])
    ih = min(a[3], b[3]) - max(a[1], b[1])
    if iw <= 0 or ih <= 0:
        return 0.0
    inter = iw * ih
    return inter / ((a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter)


def _boxes_same(a, b):
    """กรอบสองชุดอยู่ที่เดียวกันทุกกรอบไหม (มีกรอบใหม่โผล่ เช่นนกตัวเล็กเข้ามา = ไม่เหมือน)"""
    a, b = a or [], b or []
    return (all(any(_box_iou(x, y) >= 0.5 for y in b) for x in a) and
            all(any(_box_iou(x, y) >= 0.5 for y in a) for x in b))


def _is_dup(gray, boxes, prev_gray, prev_boxes):
    return (prev_gray is not None and float((cv2.absdiff(gray, prev_gray) > 25).mean()) < DATASET_DUP_FRAC
            and _boxes_same(boxes, prev_boxes))


def _motion_frac(raw, hw):
    """สัดส่วนพิกเซลที่เปลี่ยนจากรอบก่อน คืน None ถ้าเทียบไม่ได้ (รอบแรก / มอเตอร์เพิ่งหมุน)"""
    gray = _ds_gray(raw)
    pos = hw.get("pos")
    prev, prev_pos = _ds["prev_gray"], _ds["prev_pos"]
    _ds["prev_gray"], _ds["prev_pos"] = gray, pos
    if prev is None or pos != prev_pos or hw.get("moving"):
        return None           # มุมกล้องเปลี่ยน ภาพต่างทั้งเฟรมอยู่แล้ว ไม่นับ
    diff = cv2.absdiff(gray, prev)
    return float((diff > 25).mean())


def dataset_maybe_save(raw, source, count, max_conf, weak_max, hw, boxes=None, repelled=False):
    with _ds_lock:
        _ds["rounds"] += 1
        motion = _motion_frac(raw, hw)
        now = time.time()

        last = _ds["last"]
        reason = None
        if repelled:
            reason = "repel"            # ทุกครั้งที่สั่งไล่ ไว้ตรวจว่าไล่เพราะนกจริงหรือโมเดลเห็นผิด
        elif source == "manual":
            reason = "manual"
        elif count > 0:
            if now - last.get("bird", 0) >= DATASET_BIRD_MIN_GAP_S:
                reason = "bird"
        elif weak_max >= DATASET_LOWCONF:
            reason = "lowconf"
        elif source == "pir":
            reason = "pir"
        elif motion is not None and motion >= DATASET_MOTION_FRAC:
            reason = "motion"
        elif now - last.get("random", 0) >= DATASET_RANDOM_GAP_S:
            reason = "random"
        if reason is None or now - last.get(reason, 0) < DATASET_GAP_S.get(reason, 0):
            return

        gray = _ds_gray(raw)
        if reason not in ("repel", "manual", "bird") and _ds["saved"] is not None:
            prev_gray, prev_pos, prev_boxes = _ds["saved"]
            if prev_pos == hw.get("pos") and _is_dup(gray, boxes, prev_gray, prev_boxes):
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
        atomic_write_jpeg(os.path.join(DATASET_DIR, name), raw, quality=92, durable=True)
        h, w = raw.shape[:2]
        meta = {"w": w, "h": h, "boxes": boxes or [], "source": source}
        atomic_write_bytes(_ds_meta_path(name), json.dumps(meta).encode("utf-8"), durable=True)
        last[reason] = now
        _ds["saved"] = (gray, hw.get("pos"), boxes or [])
        _dataset_count(reason)
        if _dsx["names"] is not None:
            _dsx["names"].append(name)

        new_file = not os.path.exists(DATASET_LOG)
        with open(DATASET_LOG, "a", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            if new_file:
                w.writerow(["file", "time", "reason", "source", "bird_count",
                            "max_conf", "weak_conf", "motion_frac", "motor_deg"])
            w.writerow([name, ts.strftime("%Y-%m-%d %H:%M:%S"), reason, source, count,
                        round(max_conf, 3), round(weak_max, 3),
                        "" if motion is None else round(motion, 4), hw.get("deg", "")])


_ds_counts = None


def _dataset_count(reason):
    """นับเพิ่มในหน่วยความจำ (เรียกภายใต้ _ds_lock)"""
    if _ds_counts is not None:
        _ds_counts["total"] += 1
        _ds_counts[reason] = _ds_counts.get(reason, 0) + 1


def dataset_stats():
    """จำนวนภาพชุดข้อมูลแยกตามเหตุผล

    อ่านโฟลเดอร์แค่ครั้งแรกครั้งเดียว หลังจากนั้นนับเพิ่มเอง
    (เดิม listdir ทุกวินาทีต่อผู้ชมหนึ่งคน พอมีหลายหมื่นไฟล์บน SD การ์ดจะช้ามาก)
    """
    global _ds_counts
    with _ds_lock:
        if _ds_counts is None:
            _ds_load()
            out = {"total": len(_dsx["names"])}
            for f in _dsx["names"]:
                reason = f.rsplit("_", 2)[1]
                out[reason] = out.get(reason, 0) + 1
            _ds_counts = out
        return dict(_ds_counts)


# ---------- ป้ายและไฟล์ส่งออกสำหรับเทรน ----------
_DS_NAME_RE = re.compile(r"^\d{8}_\d{6}_\d{3}_[a-z]+_\d+\.jpg$")
DS_LABELS = ("bird", "none", "fix")     # นก (กรอบถูก) / ไม่มีนก / มีนกแต่ต้องแก้กรอบ
_dsx = {"names": None, "labels": None}  # แคชรายชื่อภาพ (เก่า -> ใหม่) และป้าย ใช้ภายใต้ _ds_lock


def _ds_meta_path(name):
    return os.path.join(DATASET_DIR, name[:-4] + ".json")


def _jpeg_broken(path):
    """ไฟล์ภาพเสียไหม (ว่าง/ถูกตัด เช่นไฟดับตอนกำลังเขียน) ดูแค่หัวกับท้ายไฟล์ เร็วพอจะเช็กทุกไฟล์"""
    try:
        with open(path, "rb") as f:
            if f.read(2) != b"\xff\xd8":
                return True
            f.seek(-2, os.SEEK_END)
            return f.read(2) != b"\xff\xd9"
    except OSError:
        return True


def _ds_load():
    """โหลดรายชื่อภาพและป้ายครั้งแรก (เรียกภายใต้ _ds_lock)

    ครั้งแรกจะลบภาพเสีย (บนเว็บเห็นเป็น ?) และไฟล์ .part ที่ค้างจากไฟดับตอนเขียนด้วย
    """
    if _dsx["names"] is None:
        try:
            files = os.listdir(DATASET_DIR)
        except FileNotFoundError:
            files = []
        names, broken = [], 0
        for f in files:
            path = os.path.join(DATASET_DIR, f)
            if ".part" in f:
                _rm_quiet(path)
            elif _DS_NAME_RE.match(f):
                if _jpeg_broken(path):
                    _rm_quiet(path)
                    _rm_quiet(_ds_meta_path(f))
                    broken += 1
                else:
                    names.append(f)
        if broken:
            print(f"[Dataset] ลบภาพเสีย {broken} ไฟล์ (ไฟดับตอนกำลังเขียน)", flush=True)
        _dsx["names"] = sorted(names)
    if _dsx["labels"] is None:
        try:
            with open(DATASET_LABELS, encoding="utf-8") as f:
                data = json.load(f)
            _dsx["labels"] = {k: v for k, v in data.items() if v in DS_LABELS}
        except (OSError, ValueError):
            _dsx["labels"] = {}


def _rm_quiet(path):
    try:
        os.remove(path)
    except OSError:
        pass


def _ds_save_labels():
    os.makedirs(DATASET_DIR, exist_ok=True)
    atomic_write_bytes(DATASET_LABELS, json.dumps(_dsx["labels"], ensure_ascii=False).encode("utf-8"),
                       durable=True)


def _ds_meta(name):
    try:
        with open(_ds_meta_path(name), encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None                     # ภาพจากเวอร์ชันก่อน v7.0 ไม่มีข้อมูลกรอบ


def _ds_time(name):
    return f"{name[0:4]}-{name[4:6]}-{name[6:8]} {name[9:11]}:{name[11:13]}:{name[13:15]}"


def dataset_page(flt, page):
    with _ds_lock:
        _ds_load()
        names = list(_dsx["names"])
        labels = dict(_dsx["labels"])
    names.reverse()                                     # ใหม่สุดก่อน
    counts = {"all": len(names), "todo": 0, "bird": 0, "none": 0, "fix": 0}
    for n in names:
        counts[labels.get(n, "todo")] += 1
    if flt in DS_LABELS:
        names = [n for n in names if labels.get(n) == flt]
    elif flt == "todo":
        names = [n for n in names if n not in labels]
    start = max(0, page) * DATASET_PAGE
    items, gone = [], []
    for n in names[start:start + DATASET_PAGE]:
        if not os.path.exists(os.path.join(DATASET_DIR, n)):
            gone.append(n)                              # ถูกลบจากเทอร์มินัลขณะเซิร์ฟเวอร์รันอยู่
            continue
        meta = _ds_meta(n) or {}
        reason = n.rsplit("_", 2)[1]
        items.append({"n": n, "t": _ds_time(n), "r": reason, "l": labels.get(n, ""),
                      "w": meta.get("w"), "h": meta.get("h"),
                      "b": meta.get("boxes") if meta else None})
    if gone:
        with _ds_lock:
            for n in gone:
                _ds_remove(n)
            _ds_save_labels()
    try:
        free_gb = round(shutil.disk_usage(DATA_DIR).free / 1e9, 1)
    except OSError:
        free_gb = None
    return {"items": items, "counts": counts, "page": page,
            "more": start + DATASET_PAGE < len(names), "free_gb": free_gb}


def dataset_set_label(name, label):
    with _ds_lock:
        _ds_load()
        if name not in _dsx["names"]:
            return False
        if label:
            _dsx["labels"][name] = label
        else:
            _dsx["labels"].pop(name, None)
        _ds_save_labels()
    return True


def _ds_remove(name):
    """ลบภาพหนึ่งใบออกจากดิสก์และแคช (เรียกภายใต้ _ds_lock หลัง _ds_load ไม่บันทึกป้าย)"""
    _rm_quiet(os.path.join(DATASET_DIR, name))
    _rm_quiet(_ds_meta_path(name))
    try:
        _dsx["names"].remove(name)
    except ValueError:
        return
    _dsx["labels"].pop(name, None)
    if _ds_counts is not None:
        reason = name.rsplit("_", 2)[1]
        _ds_counts["total"] = max(0, _ds_counts["total"] - 1)
        if _ds_counts.get(reason):
            _ds_counts[reason] -= 1


def dataset_delete(name):
    with _ds_lock:
        _ds_load()
        if name not in _dsx["names"]:
            return False
        had_label = name in _dsx["labels"]
        _ds_remove(name)
        if had_label:
            _ds_save_labels()
    return True


def dataset_delete_filter(flt, upto):
    """ลบทุกภาพในหมวด flt ("todo" = ยังไม่ติดป้าย / "none" = ไม่มีนก)
    เฉพาะภาพที่เก่ากว่าหรือเท่ากับ upto (ภาพใหม่สุดที่ผู้ใช้เห็นตอนกด) ภาพที่เข้ามาทีหลังไม่โดนลบ"""
    with _ds_lock:
        _ds_load()
        labels = _dsx["labels"]
        names = [n for n in _dsx["names"] if n <= upto and
                 (n not in labels if flt == "todo" else labels.get(n) == flt)]
        for n in names:
            _ds_remove(n)
        if flt != "todo":
            _ds_save_labels()
    return len(names)


_dedupe_lock = threading.Lock()


def dataset_dedupe():
    """ลบภาพซ้ำ: ไล่จากเก่าไปใหม่ ภาพที่ยังไม่ติดป้ายที่แทบเหมือนภาพที่เก็บไว้ก่อนหน้า
    (พิกเซลต่างไม่ถึง DATASET_DUP_FRAC และกรอบอยู่ที่เดิม) ถูกลบ
    ไม่ลบภาพที่กดถ่ายเองและภาพที่ติดป้ายแล้ว คืน (จำนวนที่ลบ, จำนวนที่ตรวจ)"""
    global _ds_counts
    with _ds_lock:
        _dsx["names"], _ds_counts = None, None          # อ่านรายชื่อจากดิสก์ใหม่ (เผื่อเพิ่ม/ลบไฟล์เอง)
        _ds_load()
        names = list(_dsx["names"])
        labels = dict(_dsx["labels"])
    drop, prev = [], None
    for n in names:                                     # อ่านไฟล์นอกล็อก รอบตรวจจับไม่ต้องรอ
        img = cv2.imread(os.path.join(DATASET_DIR, n), cv2.IMREAD_REDUCED_GRAYSCALE_4)
        if img is None:
            if os.path.exists(os.path.join(DATASET_DIR, n)):
                drop.append(n)                          # เปิดไม่ได้ = ภาพเสีย
            continue
        gray = _ds_gray(img)
        boxes = (_ds_meta(n) or {}).get("boxes") or []
        if (prev is not None and n not in labels and n.rsplit("_", 2)[1] != "manual"
                and _is_dup(gray, boxes, prev[0], prev[1])):
            drop.append(n)
            continue
        prev = (gray, boxes)
    with _ds_lock:
        for n in drop:
            if n not in _dsx["labels"]:                 # เผื่อเพิ่งติดป้ายระหว่างตรวจ
                _ds_remove(n)
    return len(drop), len(names)


def _yolo_lines(meta, min_conf):
    """กรอบจากโมเดลเป็นบรรทัด YOLO: คลาส cx cy w h (สัดส่วน 0-1 ของภาพ)"""
    if not meta or not meta.get("w") or not meta.get("h"):
        return []
    iw, ih = float(meta["w"]), float(meta["h"])
    out = []
    for x1, y1, x2, y2, conf in meta.get("boxes") or []:
        if conf < min_conf or x2 <= x1 or y2 <= y1:
            continue
        cx, cy = (x1 + x2) / 2 / iw, (y1 + y2) / 2 / ih
        bw, bh = (x2 - x1) / iw, (y2 - y1) / ih
        vals = [min(1.0, max(0.0, v)) for v in (cx, cy, bw, bh)]
        out.append("0 " + " ".join(f"{v:.6f}" for v in vals))
    return out


DS_README = """ชุดข้อมูลจากเครื่องไล่นกอัตโนมัติ (รูปแบบ YOLO)
==================================================
images/   ภาพดิบจากกล้อง (ไม่มีกรอบวาดทับ)
labels/   ไฟล์กรอบ 1 ไฟล์ต่อภาพ แต่ละบรรทัด = 0 cx cy w h (สัดส่วน 0-1) คลาส 0 = bird
          ไฟล์ว่าง = ภาพนี้ไม่มีนก (ใช้สอนโมเดลว่าอะไรไม่ใช่นก สำคัญมาก)
data.yaml ใช้เทรนได้ทันที (train กับ val ชี้ที่เดียวกัน ควรแบ่งชุดตรวจสอบเองหรือให้ Roboflow แบ่ง)
review_needed.txt  ภาพที่ต้องเปิดดูและแก้กรอบก่อนเทรน (กรอบในไฟล์มาจากโมเดล อาจผิด)

ป้ายที่ติดจากหน้าเว็บ
  ✓ นก         กรอบจากโมเดล (ความมั่นใจตั้งแต่ {conf}) ถือว่าถูกแล้ว
  ✗ ไม่มีนก     ไฟล์กรอบว่าง
  ✎ ต้องแก้กรอบ  ใส่กรอบจากโมเดลไว้ให้ก่อน และอยู่ใน review_needed.txt
  (ยังไม่ติดป้าย) มีเฉพาะใน ZIP แบบ "ทั้งหมด" ใส่กรอบจากโมเดลไว้ และอยู่ใน review_needed.txt

ขั้นตอนเทรนแบบสั้น
  1. อัปโหลดทั้งโฟลเดอร์เข้า Roboflow (หรือ CVAT / Label Studio) แก้กรอบภาพใน review_needed.txt
  2. เทรนบน Google Colab:  yolo detect train model=yolov8n.pt data=data.yaml epochs=100 imgsz=640
  3. แปลงเป็น NCNN:        yolo export model=runs/detect/train/weights/best.pt format=ncnn
  4. คัดลอกโฟลเดอร์ best_ncnn_model ไปที่ Pi ตั้งชื่อเป็น yolov8n_ncnn_model (สำรองของเดิมไว้ก่อน)
     เซิร์ฟเวอร์จะหาเลขคลาส bird ของโมเดลใหม่ให้เอง
"""


class _ZipPipe:
    """ไฟล์ปลอมสำหรับ zipfile เขียนแล้วดึงออกเป็นก้อนส่งให้เบราว์เซอร์ทันที

    ไม่มี seek() zipfile จึงเขียนแบบสตรีม ไม่ต้องสร้าง ZIP ทั้งก้อนในหน่วยความจำหรือบน SD การ์ด
    """

    def __init__(self):
        self._buf = bytearray()
        self._pos = 0

    def write(self, b):
        self._buf += b
        self._pos += len(b)
        return len(b)

    def tell(self):
        return self._pos

    def flush(self):
        pass

    def take(self):
        out = bytes(self._buf)
        self._buf.clear()
        return out


def dataset_export_stream(scope):
    """สร้าง ZIP แบบ YOLO ทีละไฟล์ (generator) scope = "labeled" หรือ "all" """
    with _ds_lock:
        _ds_load()
        names = list(_dsx["names"])
        labels = dict(_dsx["labels"])
    if scope != "all":
        names = [n for n in names if n in labels]
    pipe = _ZipPipe()
    review = []
    with zipfile.ZipFile(pipe, "w", zipfile.ZIP_STORED) as z:
        z.writestr("data.yaml", "path: .\ntrain: images\nval: images\nnames:\n  0: bird\n")
        z.writestr("classes.txt", "bird\n")
        z.writestr("README.txt", DS_README.replace("{conf}", str(DATASET_PRELABEL_CONF)))
        yield pipe.take()
        for n in names:
            path = os.path.join(DATASET_DIR, n)
            if not os.path.exists(path):
                continue
            label = labels.get(n, "")
            meta = _ds_meta(n)
            lines = [] if label == "none" else _yolo_lines(meta, DATASET_PRELABEL_CONF)
            if label in ("fix", ""):
                review.append(f"{n}\t{'ต้องแก้กรอบ' if label else 'ยังไม่ติดป้าย'}")
            elif label == "bird" and not lines:
                review.append(f"{n}\tติดป้ายว่ามีนก แต่โมเดลไม่ได้ตีกรอบไว้ ต้องตีกรอบเอง")
            z.write(path, "images/" + n)
            z.writestr("labels/" + n[:-4] + ".txt", "\n".join(lines) + ("\n" if lines else ""))
            yield pipe.take()
        z.writestr("review_needed.txt", "\n".join(review) + "\n")
    yield pipe.take()


# ============================ WORKER ============================
_job_q = queue.Queue(maxsize=2)
_manual_q = queue.Queue(maxsize=3)     # ภาพที่กดถ่ายเอง (ถ่ายมาแล้ว) ได้ทำก่อนงานอัตโนมัติ


def request_detection(source="manual"):
    """ขอให้ทำงานตรวจจับหนึ่งรอบ คืน True ถ้ารับคำขอไว้แล้ว

    ถ้าคิวเต็มแปลว่ามีงานค้างอยู่แล้ว ไม่ต้องต่อคิวซ้ำ
    """
    try:
        _job_q.put_nowait((source, time.time()))
        return True
    except queue.Full:
        return False


_confirm_timer = None
_confirm_lock = threading.Lock()


def schedule_confirm():
    """เจอนกเฟรมแรก -> ถ่ายเฟรมยืนยันใน CONFIRM_DELAY_S วินาที (ไม่รอรอบอัตโนมัติถัดไป)

    ถ้าคิวเต็มอยู่แล้วก็ไม่เป็นไร งานที่ค้างในคิวคือเฟรมถัดไปที่ใช้ยืนยันได้เหมือนกัน
    """
    global _confirm_timer
    with _confirm_lock:
        if _confirm_timer is not None and _confirm_timer.is_alive():
            return
        _confirm_timer = threading.Timer(max(0.0, CONFIRM_DELAY_S),
                                         lambda: request_detection("confirm"))
        _confirm_timer.daemon = True
        _confirm_timer.start()


def _process(source, requested_at, frame0=None, shot_ms=0):
    """frame0 = ภาพที่ถ่ายมาแล้ว (ปุ่มถ่ายภาพ) ใช้ภาพนั้นภาพเดียว ไม่ต้องรอแกนหยุด"""
    if frame0 is None:
        wait_still()

    # เพิ่งเจอนกไป (กำลังรอยืนยัน) -> เฟรมนี้ใช้เกณฑ์ยืนยันที่ต่ำกว่า
    confirming = confirm_frames() > 1 and CTRL.confirm_hits() > 0
    threshold = min(CONF_THRESHOLD, CONFIRM_CONF) if confirming else CONF_THRESHOLD

    # ถ่ายรัวหลายเฟรม เจอนกเฟรมไหนก็ใช้เฟรมนั้น (คะแนนนกตัวเดิมแกว่งมากตามท่าทาง
    # ภาพเดียวต่อรอบจึงพลาดบ่อย) เจอแล้วหยุดทันที ไม่เสีย CPU เพิ่มตอนมีนก
    best = None
    err = ""
    shutter_ms = 0
    infer_total = 0
    detect_total = 0
    for i in range(1 if frame0 is not None else max(1, BURST_FRAMES)):
        if i:
            time.sleep(BURST_GAP_S)
        t_shot = time.perf_counter()
        frame, err = (frame0, "") if frame0 is not None else capture_frame()
        if i == 0:
            shutter_ms = shot_ms if frame0 is not None else int((time.perf_counter() - t_shot) * 1000)
        if frame is None:
            break
        if i == 0:
            _write_preview(frame)
            with _state_lock:
                STATE["processing"] = True
        raw = frame.copy()                     # เก็บภาพดิบไว้ก่อนโดนวาดกรอบทับ
        t_det = time.perf_counter()
        boxes = []
        count, max_conf, infer_ms, weak_max = detect_and_draw(frame, threshold, boxes)
        detect_total += int((time.perf_counter() - t_det) * 1000)
        infer_total += infer_ms
        cand = (count, max_conf, weak_max, frame, raw, boxes)
        if best is None or (count, max_conf, weak_max) > best[:3]:
            best = cand
        if count > 0:
            break

    if best is None:
        with _state_lock:
            STATE["status"] = err
            STATE["processing"] = False
        print(f"[Cam] {err}", flush=True)
        return

    count, max_conf, weak_max, img, raw, boxes = best
    infer_ms = infer_total
    detect_ms = detect_total
    ok, buf = cv2.imencode(".jpg", raw, [cv2.IMWRITE_JPEG_QUALITY, 90])
    data = buf.tobytes() if ok else b""

    h, w = img.shape[:2]

    if count > 0 and CTRL.linked:
        # ไฟส้ม = เจอนก ติดทันทีไม่ว่าจะเปิดไล่อัตโนมัติหรือไม่ (บอร์ดดับเองหลัง 5 วินาที)
        CTRL.send("BIRD 1", timeout=2.0)

    repels_before = CTRL.repel_count
    repel_note = CTRL.maybe_auto_repel(count)
    repelled = CTRL.repel_count > repels_before
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
        src = {"pir": "PIR ทริก → ", "manual": "กดถ่ายภาพ → ",
               "confirm": "เฟรมยืนยัน → "}.get(source, "")
        STATE["status"] = src + (f"พบนก {count} ตัว — {repel_note}" if count
                                 else f"ไม่พบนก (ตรวจ {BURST_FRAMES} เฟรม)" if BURST_FRAMES > 1
                                 else "ไม่พบนก")

    append_log([ts.strftime("%Y-%m-%d %H:%M:%S"), source, count,
                round(max_conf, 3), infer_ms, shutter_ms, w, h,
                round(len(data) / 1024, 1), hist_file, repel_note,
                hw.get("water_pct", -1), hw.get("vbat", 0.0)])

    if DATASET_ENABLED:
        try:
            dataset_maybe_save(raw, source, count, max_conf, weak_max, hw, boxes, repelled)
        except Exception as e:                                  # noqa: BLE001
            print(f"[Dataset] บันทึกไม่สำเร็จ: {e}", flush=True)


def _write_preview(frame):
    """เก็บภาพดิบไว้ก่อน หน้าเว็บจะได้เห็นทันทีไม่ต้องรอ YOLO"""
    try:
        ok, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, 85])
        if ok:
            atomic_write_bytes(PREVIEW_PATH, buf.tobytes())
            with _state_lock:
                STATE["preview_seq"] += 1
    except OSError:
        pass


def _next_job():
    """งานถัดไป: ภาพที่กดถ่ายเองก่อน แล้วค่อยงานอัตโนมัติ"""
    while True:
        try:
            return _manual_q.get_nowait()
        except queue.Empty:
            pass
        try:
            return _job_q.get(timeout=0.2)
        except queue.Empty:
            pass


def _worker():
    while True:
        job = _next_job()
        source = job[0]
        try:
            _process(*job)
        except Exception as e:                                  # noqa: BLE001
            print(f"[Worker] ผิดพลาด: {e}", flush=True)
            with _state_lock:
                STATE["processing"] = False
        finally:
            _job_q.task_done()


def _ready_loop():
    """คอยแจ้ง ESP32 ว่ากล้องออนไลน์ไหม เพื่อให้มันตัดสินใจจุดไฟเขียว (พร้อม)

    ส่งใหม่เมื่อ: สถานะกล้องเปลี่ยน / บอร์ดรีบูตหรือเพิ่งกลับมาติดต่อได้ / ทุก 30 วินาทีกันพลาด
    (เดิมส่งเฉพาะตอนเปลี่ยน บอร์ดรีบูตแล้วลืมสถานะกล้อง ไฟเขียวเลยดับค้าง)
    """
    last_sent = None
    last_sent_at = 0.0
    while True:
        try:
            on = camera_online()
            if CTRL.resync.is_set():
                CTRL.resync.clear()
                last_sent = None
            if (on != last_sent or time.time() - last_sent_at > 30) and CTRL.linked:
                ok, _ = CTRL.send(f"CAM {1 if on else 0}", timeout=3.0)
                if ok:
                    if on != last_sent:
                        print(f"[Ready] แจ้งบอร์ดว่ากล้อง{'ออนไลน์' if on else 'หลุด'}",
                              flush=True)
                    last_sent = on
                    last_sent_at = time.time()
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
_PUBLIC = {"login_page", "api_login", "api_setup", "api_health"}
_TRAIN_ONLY = {"api_dataset", "api_dataset_img", "api_dataset_label", "api_dataset_delete",
               "api_dataset_bulk", "api_dataset_export"}


@app.before_request
def guard():
    """ด่านหน้าทุกคำขอ

    1) ทุกคำสั่ง (POST) ต้องเป็น JSON และมี header X-Bird-UI
       เว็บอื่นที่เปิดอยู่ในมือถือจึงยิงฟอร์มมาสั่งปั๊ม/มอเตอร์ไม่ได้ (เบราว์เซอร์ไม่ยอมให้
       เว็บอื่นใส่ header เองโดยไม่ถามเซิร์ฟเวอร์ก่อน และเซิร์ฟเวอร์นี้ไม่อนุญาต)
    2) ทุกหน้า ยกเว้นหน้าล็อกอิน ต้องล็อกอินก่อน รวมภาพสด ภาพ และไฟล์ดาวน์โหลด
    """
    if request.method == "POST" and (request.headers.get("X-Bird-UI") != "1" or not request.is_json):
        return jsonify(ok=False, msg="คำสั่งไม่ได้มาจากหน้าเว็บของระบบ"), 403
    if request.endpoint in _PUBLIC:
        return None
    if logged_in():
        if request.endpoint in _TRAIN_ONLY and not DATASET_ENABLED:
            return jsonify(ok=False, msg="ใช้ได้เฉพาะเวอร์ชันเก็บภาพเทรน (BIRD_MODE=train)"), 403
        return None
    if request.method == "GET" and request.path == "/":
        return redirect("/login")
    return jsonify(ok=False, need_login=True, msg="ต้องล็อกอินก่อน"), 401


@app.route("/login")
def login_page():
    if logged_in():
        return redirect("/")
    page = LOGIN_HTML.replace("__SETUP__", "false" if password_is_set() else "true")
    resp = Response(page, mimetype="text/html; charset=utf-8")
    resp.headers["Cache-Control"] = "no-store"
    return resp


@app.route("/api/login", methods=["POST"])
def api_login():
    ip = _client_ip()
    left = _lock_left(ip)
    if left:
        return jsonify(ok=False, locked=left,
                       msg=f"ใส่รหัสผิดหลายครั้ง ลองใหม่ในอีก {left} วินาที"), 429
    if not password_is_set():
        return jsonify(ok=False, setup=True, msg="ยังไม่ได้ตั้งรหัส"), 409
    pw = str((request.get_json(silent=True) or {}).get("password", ""))
    if not check_password(pw):
        _note_fail(ip)
        time.sleep(0.4)                              # ชะลอการสุ่มเดา
        left = _lock_left(ip)
        return jsonify(ok=False, locked=left or None,
                       msg=(f"ใส่รหัสผิด {LOGIN_MAX_FAIL} ครั้ง ล็อก {left} วินาที" if left
                            else "รหัสไม่ถูกต้อง")), 401
    _login_fail.pop(ip, None)
    _login_session()
    return jsonify(ok=True)


@app.route("/api/setup", methods=["POST"])
def api_setup():
    """ตั้งรหัสครั้งแรก ใช้ได้เฉพาะตอนยังไม่มีรหัสเท่านั้น"""
    if password_is_set():
        return jsonify(ok=False, msg="ตั้งรหัสไว้แล้ว ให้ล็อกอินแทน"), 409
    pw = str((request.get_json(silent=True) or {}).get("password", ""))
    if len(pw) < PASSWORD_MIN_LEN:
        return jsonify(ok=False, msg=f"รหัสต้องยาวอย่างน้อย {PASSWORD_MIN_LEN} ตัวอักษร"), 400
    set_password(pw)
    print(f"[Auth] ตั้งรหัสผ่านหน้าเว็บครั้งแรกจาก {_client_ip()}", flush=True)
    _login_session()
    return jsonify(ok=True)


@app.route("/api/logout", methods=["POST"])
def api_logout():
    session.clear()
    return jsonify(ok=True)


@app.route("/api/password", methods=["POST"])
def api_password():
    data = request.get_json(silent=True) or {}
    ip = _client_ip()
    left = _lock_left(ip)
    if left:
        return jsonify(ok=False, msg=f"ใส่รหัสผิดหลายครั้ง ลองใหม่ในอีก {left} วินาที"), 429
    if not check_password(str(data.get("current", ""))):
        _note_fail(ip)
        return jsonify(ok=False, msg="รหัสปัจจุบันไม่ถูกต้อง"), 400
    new = str(data.get("new", ""))
    if len(new) < PASSWORD_MIN_LEN:
        return jsonify(ok=False, msg=f"รหัสใหม่ต้องยาวอย่างน้อย {PASSWORD_MIN_LEN} ตัวอักษร"), 400
    set_password(new)
    _login_session()                                  # เครื่องนี้ล็อกอินต่อ เครื่องอื่นถูกออก
    return jsonify(ok=True, msg="เปลี่ยนรหัสแล้ว เครื่องอื่นต้องล็อกอินใหม่")


@app.route("/api/capture", methods=["POST"])
def api_capture():
    """ปุ่มถ่ายภาพเดี๋ยวนี้บนหน้าเว็บ

    v7.2: ถ่ายทันทีตอนกด (เลือกเฟรมคมสุดจาก ~0.3 วิ) แล้วค่อยส่งภาพนั้นไปตรวจจับ
    ภาพจึงเป็นจังหวะที่กดจริง ไม่ต้องรอคิวรอบอัตโนมัติหรือรอแกนหยุดหมุน
    """
    t0 = time.perf_counter()
    frame, _ = capture_frame(CAM_SHARP_FRAMES_MANUAL) if camera_online() else (None, "")
    if frame is None:                                   # กล้องหลุด ใช้ทางเดิม (ได้ข้อความผิดพลาดบนเว็บ)
        ok = request_detection("manual")
        return jsonify(ok=ok, msg="รับคำสั่งแล้ว" if ok else "มีงานค้างอยู่ รอสักครู่")
    _write_preview(frame)
    try:
        _manual_q.put_nowait(("manual", time.time(), frame, int((time.perf_counter() - t0) * 1000)))
    except queue.Full:
        return jsonify(ok=False, msg="กดถี่เกินไป รอสักครู่")
    return jsonify(ok=True, msg="ถ่ายแล้ว กำลังตรวจหานก")


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
    s["confirm_frames"] = confirm_frames()
    s["confirm_base"] = CONFIRM_FRAMES             # 1 = โหมดสาธิต ไม่ต้องมีสวิตช์ไล่ทันที
    s["instant_repel"] = SETTINGS["instant_repel"]
    s["confirm_delay"] = CONFIRM_DELAY_S
    s["confirm_streak"] = CTRL.confirm_hits()
    s["pump_button_ms"] = pump_button_ms()
    s["now"] = time.time()
    s["burst_frames"] = BURST_FRAMES
    s["corrupt_frames"] = CAM.corrupt_count
    s["test_mode"] = TEST_MODE
    s["train_mode"] = DATASET_ENABLED
    s["mode"] = MODE
    s["roi"] = dict(ROI)
    s["hw"] = CTRL.snapshot()
    s["history"] = list(HISTORY)[:24]
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
    if data.get("reset"):
        ROI.clear()
        ROI.update(ROI_DEFAULT)
        save_roi()
        return jsonify(ok=True, roi=dict(ROI))
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


@app.route("/api/instant_repel", methods=["POST"])
def api_instant_repel():
    """เปิด/ปิดโหมดไล่ทันทีเมื่อเจอภาพเดียว"""
    data = request.get_json(silent=True) or {}
    SETTINGS["instant_repel"] = bool(data.get("on", False))
    save_settings()
    return jsonify(ok=True, on=SETTINGS["instant_repel"])


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
    elif action == "zero":
        ok, line = CTRL.send("MOT ZERO")
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


def pump_button_ms():
    """ปุ่มปั๊มบนเว็บเปิดนานเท่ารอบไล่ที่ตั้งไว้บนบอร์ด (บอร์ดเก่ากว่า v7.9 ใช้ค่าในไฟล์นี้)"""
    return CTRL.snapshot().get("repel_pump_ms") or PUMP_BUTTON_MS


@app.route("/api/pump", methods=["POST"])
def api_pump():
    data = request.get_json(silent=True) or {}
    try:
        ms = int(data.get("ms", pump_button_ms()))
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


@app.route("/api/sweep", methods=["POST"])
def api_sweep():
    """ตั้งการกวาดตอนไล่ {deg, spd} หรือ {test: true} = กวาดหนึ่งรอบโดยไม่ฉีดน้ำ"""
    data = request.get_json(silent=True) or {}
    if data.get("test"):
        ok, line = CTRL.send("REPEL TEST", timeout=5.0)
        return jsonify(ok=ok, msg="กำลังทดสอบกวาด (ไม่ฉีดน้ำ)" if ok else line)
    try:
        deg = int(data.get("deg"))
        spd = int(data.get("spd"))
    except (TypeError, ValueError):
        return jsonify(ok=False, msg="ค่าไม่ถูกต้อง"), 400
    if not (SWEEP_DEG_MIN <= deg <= SWEEP_DEG_MAX and SWEEP_SPD_MIN <= spd <= SWEEP_SPD_MAX):
        return jsonify(ok=False, msg="ค่าเกินช่วงที่ตั้งได้"), 400
    pms = data.get("pump_ms")
    if pms is not None:
        try:
            pms = int(pms)
        except (TypeError, ValueError):
            return jsonify(ok=False, msg="ค่าไม่ถูกต้อง"), 400
        if not REPEL_PUMP_MS_MIN <= pms <= REPEL_PUMP_MS_MAX:
            return jsonify(ok=False, msg="ค่าเกินช่วงที่ตั้งได้"), 400
    ok, line = CTRL.send(f"RCFG {deg} {spd}" + (f" {pms}" if pms else ""))
    if ok:
        CTRL._wake.set()                                # อ่าน STAT ใหม่ หน้าเว็บเห็นค่าใหม่ทันที
    note = f" ฉีดน้ำ {pms / 1000:g} วิ" if pms else ""
    return jsonify(ok=ok, msg=f"บันทึกการไล่ ±{deg}° ความเร็ว {spd}%{note}" if ok else line)


@app.route("/api/bird_lamp", methods=["POST"])
def api_bird_lamp():
    """ปุ่มทดสอบไฟส้ม"""
    data = request.get_json(silent=True) or {}
    ok, line = CTRL.send(f"BIRD {1 if data.get('on', True) else 0}", timeout=3.0)
    return jsonify(ok=ok, msg=line)


@app.route("/api/abort", methods=["POST"])
def api_abort():
    ok, line = CTRL.send("ABORT", timeout=4.0)
    return jsonify(ok=ok, msg=line)


@app.route("/api/water", methods=["POST"])
def api_water():
    ok, line = CTRL.send("WATER", timeout=4.0)
    return jsonify(ok=ok, msg=line)


_stream_slots = threading.BoundedSemaphore(STREAM_MAX_CLIENTS)


@app.route("/stream")
def api_stream():
    """ภาพสดจากเว็บแคม (MJPEG) เปิดดูได้ทั้งในหน้าเว็บและเบราว์เซอร์ตรง ๆ"""
    if not _stream_slots.acquire(blocking=False):
        return f"เปิดภาพสดพร้อมกันได้ไม่เกิน {STREAM_MAX_CLIENTS} เครื่อง", 503

    def gen():
        seq = -1
        min_gap = 1.0 / STREAM_MAX_FPS
        last_sent = 0.0
        try:
            while True:
                frame, seq_new = CAM.next_frame(seq)
                now = time.time()
                if frame is None:
                    # กล้องหลุด: ส่งภาพเดิมซ้ำทุก 2 วินาที
                    # ถ้าไม่ส่งอะไรเลย เธรดจะไม่รู้ว่าผู้ชมปิดหน้าไปแล้ว และค้างอยู่ตลอดไป
                    frame = CAM.last_frame()
                    if frame is None or now - last_sent < 2.0:
                        continue
                else:
                    seq = seq_new
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
        finally:
            _stream_slots.release()

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


@app.route("/api/dataset")
def api_dataset():
    flt = request.args.get("f", "todo")
    try:
        page = int(request.args.get("p", 0))
    except ValueError:
        page = 0
    return jsonify(dataset_page(flt, page))


def _ds_name_ok(name):
    return isinstance(name, str) and bool(_DS_NAME_RE.match(name))


@app.route("/dataset/img/<name>")
def api_dataset_img(name):
    if not _ds_name_ok(name):
        return "ไม่อนุญาต", 400
    path = os.path.join(DATASET_DIR, name)
    if not os.path.exists(path):
        return "ไม่พบไฟล์", 404
    resp = send_file(path, mimetype="image/jpeg")
    resp.headers["Cache-Control"] = "private, max-age=86400"
    return resp


@app.route("/api/dataset/label", methods=["POST"])
def api_dataset_label():
    data = request.get_json(silent=True) or {}
    name, label = data.get("name"), data.get("label") or ""
    if not _ds_name_ok(name) or (label and label not in DS_LABELS):
        return jsonify(ok=False, msg="ข้อมูลไม่ถูกต้อง"), 400
    if not dataset_set_label(name, label):
        return jsonify(ok=False, msg="ไม่พบภาพนี้แล้ว"), 404
    return jsonify(ok=True)


@app.route("/api/dataset/delete", methods=["POST"])
def api_dataset_delete():
    name = (request.get_json(silent=True) or {}).get("name")
    if not _ds_name_ok(name):
        return jsonify(ok=False, msg="ข้อมูลไม่ถูกต้อง"), 400
    if not dataset_delete(name):
        return jsonify(ok=False, msg="ไม่พบภาพนี้แล้ว"), 404
    return jsonify(ok=True, msg="ลบภาพแล้ว")


@app.route("/api/dataset/bulk", methods=["POST"])
def api_dataset_bulk():
    """{action: "dedupe"} ลบภาพซ้ำ / {action: "delete", f: "todo"|"none", upto: ชื่อภาพ} ลบทั้งแท็บ"""
    data = request.get_json(silent=True) or {}
    act = data.get("action")
    if act == "dedupe":
        if not _dedupe_lock.acquire(blocking=False):
            return jsonify(ok=False, msg="กำลังลบภาพซ้ำอยู่แล้ว รอสักครู่"), 409
        try:
            n, total = dataset_dedupe()
        finally:
            _dedupe_lock.release()
        return jsonify(ok=True, n=n, msg=f"ลบภาพซ้ำ {n} ภาพ (ตรวจทั้งหมด {total} ภาพ)")
    if act == "delete":
        flt, upto = data.get("f"), data.get("upto")
        if flt not in ("todo", "none") or not _ds_name_ok(upto):
            return jsonify(ok=False, msg="ข้อมูลไม่ถูกต้อง"), 400
        n = dataset_delete_filter(flt, upto)
        return jsonify(ok=True, n=n, msg=f"ลบแล้ว {n} ภาพ")
    return jsonify(ok=False, msg="คำสั่งไม่รู้จัก"), 400


@app.route("/dataset/export.zip")
def api_dataset_export():
    scope = "all" if request.args.get("scope") == "all" else "labeled"
    fname = f"bird_dataset_{scope}_{datetime.now().strftime('%Y%m%d_%H%M')}.zip"
    return Response(dataset_export_stream(scope), mimetype="application/zip",
                    headers={"Content-Disposition": f'attachment; filename="{fname}"',
                             "Cache-Control": "no-store"})


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
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#11151c">
<title>Bird Detection Live Monitor</title>
<style>
  :root{
    --bg:#11151c; --card:#1a1f29; --card2:#151a23; --line:#2a3140;
    --txt:#e7ecf3; --muted:#8b97a8; --ok:#3ddc84; --warn:#ffb020; --bad:#ff5d5d;
    --btn:#233044; --btnh:#2c3a52; --pri:#1f6f43; --prib:#2b8a55; --dan:#6f2626; --danb:#8a3030;
  }
  *{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  [hidden]{display:none!important}
  html{scroll-padding-top:120px}
  body{margin:0;background:var(--bg);color:var(--txt);
       font-family:-apple-system,"Segoe UI",Roboto,"Noto Sans Thai",sans-serif;
       -webkit-text-size-adjust:100%}
  .wrap{max-width:1100px;margin:0 auto;padding:12px 16px 40px}

  /* ---------- แถบหัว (ติดด้านบน) ---------- */
  .top{position:sticky;top:0;z-index:20;background:rgba(17,21,28,.92);
       backdrop-filter:blur(8px);-webkit-backdrop-filter:blur(8px);
       border-bottom:1px solid var(--line)}
  .top .in{max-width:1100px;margin:0 auto;padding:10px 16px 8px}
  .topRow{display:flex;align-items:center;gap:8px;justify-content:space-between}
  h1{font-size:18px;margin:0;white-space:nowrap}
  .chips{display:flex;gap:6px;overflow-x:auto;margin-top:8px;padding-bottom:2px;scrollbar-width:none}
  .chips::-webkit-scrollbar{display:none}
  .chip{flex:none;display:inline-flex;align-items:center;gap:5px;font-size:12px;
        padding:4px 9px;border-radius:99px;background:var(--card2);border:1px solid var(--line);color:var(--muted)}
  .chip i{width:8px;height:8px;border-radius:50%;background:var(--muted);display:inline-block}
  .chip.ok i{background:var(--ok)} .chip.ok{color:var(--txt)}
  .chip.wn i{background:var(--warn)} .chip.wn{color:#ffd48a}
  .chip.bad i{background:var(--bad)} .chip.bad{color:#ffb3b3}

  /* ---------- การ์ด ---------- */
  .card{background:var(--card);border:1px solid var(--line);
        border-radius:14px;padding:14px;margin-bottom:14px}
  .ctitle{display:flex;align-items:center;gap:8px;flex-wrap:wrap;
          font-size:14px;font-weight:600;margin-bottom:10px}
  .ctitle .sp{flex:1}
  .sub{font-size:12px;color:var(--muted);font-weight:400}
  .lbl{font-size:12px;color:var(--muted);margin:14px 0 6px}
  .two{display:grid;grid-template-columns:1fr 1fr;gap:14px}
  @media(max-width:760px){.two{grid-template-columns:1fr;gap:0}}

  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:8px}
  .grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
  @media(max-width:520px){.grid4{grid-template-columns:repeat(2,1fr)}}
  .stat{background:var(--card2);border:1px solid var(--line);border-radius:10px;padding:10px}
  .stat .k{font-size:11px;color:var(--muted);margin-bottom:4px}
  .stat .v{font-size:19px;font-weight:700}
  .bar{height:8px;background:#0f131a;border-radius:99px;overflow:hidden;margin-top:6px}
  .bar i{display:block;height:100%;background:var(--ok);transition:width .4s}

  /* ---------- ภาพ ---------- */
  .imgbox{position:relative;border-radius:10px;overflow:hidden;background:#000;aspect-ratio:4/3}
  img.shot{width:100%;height:100%;object-fit:contain;display:block;background:#000}
  img.live{width:100%;display:block;border-radius:10px;background:#000;aspect-ratio:4/3;object-fit:contain}
  .badge{position:absolute;left:8px;top:8px;padding:5px 10px;border-radius:99px;font-size:13px;font-weight:700;
         background:rgba(0,0,0,.6);color:var(--txt)}
  .badge.bird{background:rgba(255,176,32,.92);color:#1a1200}
  .busy{position:absolute;right:8px;top:8px;padding:5px 10px;border-radius:99px;font-size:12px;
        background:rgba(0,0,0,.6);color:var(--txt);display:none}
  .busy.on{display:block;animation:pulse 1s infinite alternate}
  @keyframes pulse{from{opacity:.55}to{opacity:1}}
  .ph{min-height:120px;border-radius:10px;border:1px dashed var(--line);display:flex;align-items:center;
      justify-content:center;text-align:center;color:var(--muted);font-size:13px;padding:20px}

  .cfm{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:13px;color:var(--muted)}
  .dots{display:inline-flex;gap:5px}
  .dots i{width:12px;height:12px;border-radius:50%;border:2px solid var(--warn);display:inline-block}
  .dots i.f{background:var(--warn)}
  .msg{font-size:13px;color:var(--muted);margin-top:6px;min-height:18px;line-height:1.45}

  /* ---------- ปุ่ม ---------- */
  button,.btn{background:var(--btn);color:var(--txt);border:1px solid var(--line);
         border-radius:10px;padding:11px 14px;font-size:15px;cursor:pointer;font-family:inherit;
         min-height:46px;text-decoration:none;display:inline-flex;align-items:center;justify-content:center;gap:6px;
         transition:background .15s,transform .05s}
  button:hover,.btn:hover{background:var(--btnh)}
  button:active,.btn:active{transform:scale(.97)}
  button.pri{background:var(--pri);border-color:var(--prib)}
  button.dan{background:var(--dan);border-color:var(--danb)}
  button:disabled{opacity:.5}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-top:10px}
  .actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:12px}
  .actions .full{grid-column:1/-1}
  .btns{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px}
  .motor{display:grid;grid-template-columns:64px 1fr 64px;gap:8px}
  .motor button{font-size:18px}
  select{background:var(--card2);color:var(--txt);border:1px solid var(--line);border-radius:10px;
         padding:8px;font-size:15px;min-height:46px;font-family:inherit;text-align:center}

  /* ---------- สวิตช์เปิด-ปิด ---------- */
  .toggles{display:flex;flex-direction:column;margin-top:12px;border-top:1px solid var(--line)}
  .tg{display:flex;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid var(--line);
      font-size:15px;cursor:pointer}
  .tg .sp{flex:1}
  input.sw{appearance:none;-webkit-appearance:none;flex:none;width:46px;height:28px;border-radius:99px;
           background:#2a3140;position:relative;cursor:pointer;margin:0;transition:background .2s}
  input.sw::after{content:"";position:absolute;left:3px;top:3px;width:22px;height:22px;border-radius:50%;
           background:#cfd6e0;transition:left .2s}
  input.sw:checked{background:var(--prib)}
  input.sw:checked::after{left:21px;background:#fff}

  .logout{margin-left:auto;flex:none;font-size:11px;padding:4px 9px;border-radius:99px;min-height:0;white-space:nowrap}
  .topRow .pill{white-space:nowrap;flex:none;font-size:11px}
  @media (max-width:420px){ .top h1{font-size:16px} }
  input.txt{width:100%;font-size:16px;padding:10px 12px;border-radius:10px;
            border:1px solid var(--line);background:var(--card2);color:var(--txt)}
  .pwForm{display:grid;gap:8px;max-width:420px;margin-top:6px}
  .pill{display:inline-block;padding:3px 9px;border-radius:99px;font-size:12px;font-weight:400}
  .on{background:rgba(61,220,132,.15);color:var(--ok)}
  .off{background:rgba(255,93,93,.15);color:var(--bad)}
  .wn{background:rgba(255,176,32,.15);color:var(--warn)}

  /* ---------- เตือน ---------- */
  .warns{display:flex;flex-direction:column;gap:8px;margin-bottom:12px}
  .warns:empty{display:none}
  .warn{border-radius:10px;padding:10px 12px;font-size:14px;line-height:1.45}
  .warn.w{background:rgba(255,176,32,.12);border:1px solid rgba(255,176,32,.45);color:#ffd48a}
  .warn.b{background:rgba(255,93,93,.12);border:1px solid rgba(255,93,93,.5);color:#ffb3b3}

  /* ---------- ROI ---------- */
  .roiView{position:relative;border-radius:10px;overflow:hidden;background:#000;aspect-ratio:4/3;margin-top:4px}
  .roiView img{width:100%;height:100%;object-fit:fill;display:block}
  .roiBox{position:absolute;border:2px solid var(--ok);border-radius:4px;
          box-shadow:0 0 0 2000px rgba(0,0,0,.55);transition:all .1s}
  .roiBox.off{border-style:dashed;border-color:var(--muted);box-shadow:none}
  .roiBox span{position:absolute;left:4px;top:2px;font-size:11px;color:var(--ok);font-weight:700}
  .roiBox.off span{color:var(--muted)}
  .sl{display:grid;grid-template-columns:72px 1fr 44px;align-items:center;gap:10px;margin-top:10px;font-size:14px}
  .sl b{font-weight:600;text-align:right;font-size:13px;color:var(--muted)}
  input[type=range]{width:100%;accent-color:var(--ok);height:28px}
  .dirty{color:var(--warn)}

  /* ---------- ภาพย้อนหลัง ---------- */
  .hist{display:grid;grid-template-columns:repeat(auto-fill,minmax(105px,1fr));gap:8px}
  .hi{display:block;padding:0;min-height:0;background:var(--card2);border-radius:10px;overflow:hidden;text-align:left}
  .hi img{width:100%;aspect-ratio:4/3;object-fit:cover;display:block;background:#000}
  .hi span{display:block;font-size:11px;color:var(--muted);padding:5px 6px;line-height:1.35}
  .hi span b{color:var(--warn);font-weight:600}
  .empty{font-size:13px;color:var(--muted);padding:6px 0}

  .lb{position:fixed;inset:0;z-index:50;background:#05070a;display:flex;flex-direction:column;
      padding:env(safe-area-inset-top) 0 env(safe-area-inset-bottom)}
  .lb[hidden]{display:none}
  .lbTop{display:flex;align-items:center;gap:8px;padding:10px 12px;font-size:14px}
  .lbTop span{flex:1}
  .lb img{flex:1;min-height:0;width:100%;object-fit:contain}
  .lbNav{display:grid;grid-template-columns:1fr auto 1fr;gap:8px;padding:10px 12px}

  /* ---------- ข้อมูลสำหรับเทรน ---------- */
  .dsTabs{display:flex;gap:6px;flex-wrap:wrap;margin:10px 0}
  .dsTabs button{min-height:0;padding:6px 10px;font-size:13px;border-radius:99px}
  .dsTabs button b{font-weight:600;color:var(--muted);margin-left:3px}
  .dsTabs button.on{background:var(--pri);border-color:var(--prib)}
  .dsTabs button.on b{color:#d8ffe8}
  .dsGrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:8px}
  .dsi{display:block;padding:0;min-height:0;background:var(--card2);border-radius:10px;overflow:hidden;text-align:left}
  .dsPic{position:relative;width:100%;background:#000}
  .dsPic img{position:absolute;inset:0;width:100%;height:100%;object-fit:fill;display:block}
  .dsBx{position:absolute;border:2px solid #3ddc84;border-radius:2px;pointer-events:none}
  .dsBx.weak{border:2px dashed #ffb020}
  .dsBx i{position:absolute;left:-2px;top:-17px;font-size:10px;font-style:normal;line-height:15px;
          padding:0 3px;background:#3ddc84;color:#06200f;white-space:nowrap}
  .dsBx.weak i{background:#ffb020;color:#2a1a00}
  .dsTag{position:absolute;right:5px;top:5px;font-size:11px;padding:2px 7px;border-radius:99px;font-weight:600}
  .dsTag.bird{background:#1f6f43;color:#d8ffe8} .dsTag.none{background:#5a2a2a;color:#ffd0d0}
  .dsTag.fix{background:#6b4e12;color:#ffe2a6}
  .dsi .meta{display:block;font-size:11px;color:var(--muted);padding:5px 6px;line-height:1.35}
  .dsv{position:fixed;inset:0;z-index:55;background:#05070a;display:flex;flex-direction:column;
       padding:env(safe-area-inset-top) 0 env(safe-area-inset-bottom)}
  .dsv[hidden]{display:none}
  .dsvMid{flex:1;min-height:0;display:flex;align-items:center;justify-content:center;padding:0 8px}
  .dsvMid .dsPic{max-height:100%}
  .dsvBtns{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;padding:10px 12px 4px}
  .dsvBtns button{font-size:15px}
  .dsvBtns .bird{background:#1f6f43;border-color:#2b8a55} .dsvBtns .none{background:#5a2a2a;border-color:#7a3a3a}
  .dsvBtns .fix{background:#6b4e12;border-color:#8a6a1a}
  .dsvBtns button.sel{outline:3px solid #fff;outline-offset:-3px}

  /* ---------- อื่น ๆ ---------- */
  details.card>summary{cursor:pointer;font-size:14px;font-weight:600;list-style:none;display:flex;align-items:center;gap:8px}
  details.card>summary::-webkit-details-marker{display:none}
  details.card>summary::before{content:"▸";color:var(--muted);transition:transform .2s}
  details.card[open]>summary::before{transform:rotate(90deg)}
  details.card[open]>summary{margin-bottom:10px}
  #pirLog{font-size:13px;margin-top:8px;display:flex;flex-direction:column;gap:4px}
  .toast{position:fixed;left:50%;bottom:calc(18px + env(safe-area-inset-bottom));transform:translate(-50%,20px);
         z-index:60;background:#26324a;border:1px solid #3a4a66;color:var(--txt);padding:10px 16px;border-radius:12px;
         font-size:14px;max-width:calc(100% - 32px);box-shadow:0 8px 24px rgba(0,0,0,.4);
         opacity:0;pointer-events:none;transition:opacity .2s,transform .2s}
  .toast.show{opacity:1;transform:translate(-50%,0)}
</style>
</head>
<body>
<header class="top"><div class="in">
  <div class="topRow">
    <h1>🦅 ระบบไล่นกอัตโนมัติ</h1>
    <span class="pill wn" id="modeBadge" hidden></span>
    <button class="logout" id="btnLogout" title="ออกจากระบบ">ออกจากระบบ</button>
  </div>
  <div class="chips">
    <span class="chip" id="cCam"><i></i>กล้อง</span>
    <span class="chip" id="cBrd"><i></i>บอร์ด</span>
    <span class="chip" id="cRdy"><i></i>พร้อม</span>
    <span class="chip" id="cBird"><i></i>ไฟส้ม</span>
    <span class="chip" id="cWat"><i></i>น้ำ</span>
    <span class="chip" id="cAuto"><i></i>ไล่อัตโนมัติ</span>
  </div>
</div></header>

<div class="wrap">
  <div class="warns" id="warns"></div>

  <div class="two">
    <!-- ---------- ผลตรวจล่าสุด + ปุ่มหลัก ---------- -->
    <section class="card">
      <div class="ctitle">ผลตรวจล่าสุด <span class="sp"></span><span class="sub" id="ago"></span></div>
      <div class="imgbox">
        <img class="shot" id="shot" alt="ผลตรวจจับ">
        <div class="badge" id="badge">-</div>
        <div class="busy" id="busy">กำลังตรวจ…</div>
      </div>
      <div class="cfm"><span>ยืนยันก่อนไล่</span><span class="dots" id="dots"></span><span id="cfm">-</span></div>
      <div class="msg" id="msg"></div>
      <div class="actions">
        <button class="pri full" id="btnShot">📷 ถ่ายภาพเดี๋ยวนี้</button>
        <button id="bRepel">🚿 สั่งไล่เดี๋ยวนี้</button>
        <button class="dan" id="bAbort">■ หยุดทุกอย่าง</button>
      </div>
      <div class="toggles">
        <label class="tg"><span class="sp">ตรวจอัตโนมัติ</span><span class="pill" id="pillDet">-</span>
          <input type="checkbox" class="sw" id="autoDet"></label>
        <label class="tg"><span class="sp">ไล่อัตโนมัติเมื่อเจอนก</span><span class="sub" id="cdTxt"></span>
          <input type="checkbox" class="sw" id="autoRep"></label>
        <label class="tg" id="instRow" hidden><span class="sp">ไล่ทันทีเมื่อเจอภาพเดียว
          <span class="sub" style="display:block">ไม่รอเฟรมยืนยัน · เร็วขึ้นราว 3 วิ แต่ไล่ผิดบ่อยขึ้น</span></span>
          <input type="checkbox" class="sw" id="instRep"></label>
      </div>
    </section>

    <!-- ---------- ภาพสด ---------- -->
    <section class="card">
      <div class="ctitle">ภาพสดจากกล้อง <span class="pill" id="pillCam">-</span></div>
      <img class="live" id="live" alt="ภาพสด" style="display:none">
      <div class="ph" id="livePh">ปิดอยู่ — เปิดดูตอนติดตั้ง/เล็งกล้อง<br>(ภาพสดกินเน็ตและแย่ง CPU กับการตรวจจับ)</div>
      <div class="row">
        <button id="btnLive">▶ เปิดภาพสด</button>
        <span class="sub" id="camip"></span>
      </div>
    </section>
  </div>

  <!-- ---------- สรุปตัวเลข ---------- -->
  <section class="card">
    <div class="grid4">
      <div class="stat"><div class="k">นกในภาพล่าสุด</div><div class="v" id="cnt">0</div></div>
      <div class="stat"><div class="k">รอบที่เจอนก</div><div class="v" id="det">0</div></div>
      <div class="stat"><div class="k">ไล่ไปแล้ว</div><div class="v" id="rep">0</div></div>
      <div class="stat"><div class="k">ตรวจไปแล้ว</div><div class="v" id="tot">0</div></div>
    </div>
  </section>

  <!-- ---------- สถานะเครื่อง ---------- -->
  <section class="card">
    <div class="ctitle">สถานะเครื่อง</div>
    <div class="grid">
      <div class="stat">
        <div class="k">ระดับน้ำ</div><div class="v" id="wat">-</div>
        <div class="bar"><i id="watBar" style="width:0%"></i></div>
      </div>
      <div class="stat"><div class="k">บอร์ดควบคุม</div><div class="v" id="brd" style="font-size:16px">-</div></div>
      <div class="stat"><div class="k">ไฟเขียว (พร้อม)</div><div class="v" id="rdy" style="font-size:16px">-</div></div>
      <div class="stat"><div class="k">ไฟส้ม (เจอนก)</div><div class="v" id="blamp" style="font-size:16px">-</div></div>
      <div class="stat"><div class="k">PIR</div><div class="v" id="pir" style="font-size:15px">-</div></div>
      <div class="stat"><div class="k">มุมหัวฉีด</div><div class="v" id="deg">-</div></div>
      <div class="stat"><div class="k">โควตาปั๊ม (10 นาที)</div><div class="v" id="duty">-</div></div>
      <div class="stat"><div class="k">ไล่อัตโนมัติได้อีก (ชม.นี้)</div><div class="v" id="rph">-</div></div>
      <div class="stat"><div class="k">แรงดันแบต</div><div class="v" id="vb">-</div></div>
    </div>
  </section>

  <!-- ---------- ควบคุมด้วยมือ ---------- -->
  <section class="card">
    <div class="ctitle">ควบคุมด้วยมือ</div>
    <div class="lbl" style="margin-top:0">หมุนหัวฉีด</div>
    <div class="motor">
      <button id="mL" aria-label="หมุนซ้าย">◀</button>
      <select id="steps">
        <option value="5">ทีละ 5 สเต็ป</option>
        <option value="10" selected>ทีละ 10 สเต็ป</option>
        <option value="40">ทีละ 40 สเต็ป</option>
        <option value="120">ทีละ 120 สเต็ป</option>
      </select>
      <button id="mR" aria-label="หมุนขวา">▶</button>
    </div>
    <div class="btns" style="margin-top:8px">
      <button id="mH">◆ กลับจุดกลาง</button>
      <button id="mS">■ หยุดหมุน</button>
      <button id="mZ">⌖ ตั้งตรงนี้เป็นจุดกลาง</button>
    </div>
    <div id="swBox" hidden>
      <div class="lbl">การกวาดตอนไล่นก</div>
      <div class="sl"><span>กว้างข้างละ</span><input type="range" id="swDeg" min="10" max="80" step="5"><b id="swDegV"></b></div>
      <div class="sl"><span>ความเร็ว</span><input type="range" id="swSpd" min="10" max="100" step="5"><b id="swSpdV"></b></div>
      <div class="sl" id="swPumpRow" hidden><span>ฉีดน้ำ</span><input type="range" id="swPump" min="1" max="8" step="0.5"><b id="swPumpV"></b></div>
      <div class="row">
        <button class="pri" id="swSave">บันทึกการไล่</button>
        <button id="swTest">↔ ทดสอบกวาด (ไม่ฉีดน้ำ)</button>
      </div>
      <div class="msg" id="swNote">ถ้าหัวฉีดชนขอบ ให้ลดความกว้าง ถ้าเหวี่ยงแรงหรือสะดุด ให้ลดความเร็ว</div>
    </div>
    <div class="lbl">น้ำและไฟ</div>
    <div class="btns">
      <button id="bPump">💧 ปั๊มน้ำ <span id="pumpS">3</span> วินาที</button>
      <button id="bWater">📏 วัดระดับน้ำใหม่</button>
      <button id="bLamp">💡 ทดสอบไฟส้ม</button>
    </div>
  </section>

  <!-- ---------- ROI ---------- -->
  <section class="card" id="roiCard">
    <label class="tg" style="padding-top:0">
      <span class="sp" style="font-weight:600;font-size:14px">ตรวจเฉพาะพื้นที่ (ROI)</span>
      <input type="checkbox" class="sw" id="roiOn">
    </label>
    <div class="msg" style="margin-top:8px">นกนอกกรอบสีเขียวจะไม่นับและไม่สั่งไล่ และภาพในกรอบจะถูกขยายก่อนตรวจ ทำให้เจอนกตัวเล็กง่ายขึ้น</div>
    <div class="roiView"><img id="roiImg" alt="ภาพสำหรับตั้ง ROI"><div class="roiBox" id="roiBox"><span>ROI</span></div></div>
    <div class="sl"><span>ตัดซ้าย</span><input type="range" id="cutL" min="0" max="0.45" step="0.01"><b id="cutLv"></b></div>
    <div class="sl"><span>ตัดขวา</span><input type="range" id="cutR" min="0" max="0.45" step="0.01"><b id="cutRv"></b></div>
    <div class="sl"><span>ตัดบน</span><input type="range" id="cutT" min="0" max="0.45" step="0.01"><b id="cutTv"></b></div>
    <div class="sl"><span>ตัดล่าง</span><input type="range" id="cutB" min="0" max="0.45" step="0.01"><b id="cutBv"></b></div>
    <div class="row">
      <button class="pri" id="roiSave">บันทึก ROI</button>
      <button id="roiReset">↺ ค่าเริ่มต้น (ตัดขอบซ้าย-ขวา)</button>
    </div>
    <div class="msg" id="roiNote"></div>
  </section>

  <!-- ---------- ภาพย้อนหลัง ---------- -->
  <section class="card">
    <div class="ctitle">ภาพย้อนหลังที่พบนก <span class="sub" id="histN"></span></div>
    <div class="hist" id="hist"></div>
    <div class="row"><button id="btnLog">⬇ ดาวน์โหลด log การตรวจจับ (CSV)</button></div>
  </section>

  <!-- ---------- ข้อมูลสำหรับเทรน ---------- -->
  <section class="card" id="dsCard" hidden>
    <div class="ctitle">ข้อมูลสำหรับเทรนโมเดล <span class="sub" id="dsSum"></span></div>
    <div class="msg" style="margin-top:0">แตะภาพเพื่อติดป้าย ✓ มีนก (กรอบถูก) / ✗ ไม่มีนก / ✎ มีนกแต่กรอบผิด
      — ภาพที่ระบบเห็นผิดสำคัญที่สุด กรอบเขียว = โมเดลมั่นใจ กรอบเหลืองประ = ก้ำกึ่ง
      ปุ่ม "ถ่ายภาพเดี๋ยวนี้" เก็บภาพเข้าชุดนี้ด้วย</div>
    <div class="dsTabs" id="dsTabs">
      <button data-f="todo" class="on">ยังไม่ติดป้าย<b></b></button>
      <button data-f="bird">✓ นก<b></b></button>
      <button data-f="none">✗ ไม่มีนก<b></b></button>
      <button data-f="fix">✎ ต้องแก้กรอบ<b></b></button>
      <button data-f="all">ทั้งหมด<b></b></button>
    </div>
    <div class="dsGrid" id="dsGrid"></div>
    <div class="row">
      <button id="dsMore" hidden>โหลดเพิ่ม</button>
      <button id="dsReload">↻ รีเฟรช</button>
    </div>
    <div class="lbl">ลดจำนวนภาพ <span class="sub" id="dsFree"></span></div>
    <div class="row" style="margin-top:0">
      <button id="dsDedupe">🧹 ลบภาพซ้ำ</button>
      <button class="dan" id="dsDelTab" hidden>🗑 ลบทั้งหมดในแท็บนี้</button>
    </div>
    <div class="msg">ลบภาพซ้ำ = ภาพที่แทบเหมือนภาพก่อนหน้าและกรอบอยู่ที่เดิม
      (ไม่ลบภาพที่กดถ่ายเองและภาพที่ติดป้ายแล้ว) · ลบทั้งหมดในแท็บ ใช้ได้กับแท็บ "ยังไม่ติดป้าย" และ "ไม่มีนก"
      ลบแล้วกู้คืนไม่ได้ ถ้าจะเก็บไว้ให้ดาวน์โหลด ZIP ก่อน</div>
    <div class="lbl">ดาวน์โหลดไปเทรน (รูปแบบ YOLO พร้อม data.yaml)</div>
    <div class="row" style="margin-top:0">
      <a class="btn" id="dsZipL" href="/dataset/export.zip?scope=labeled" download>⬇ ZIP เฉพาะที่ติดป้าย</a>
      <a class="btn" id="dsZipA" href="/dataset/export.zip?scope=all" download>⬇ ZIP ทั้งหมด</a>
    </div>
    <div class="msg" id="dsZipNote"></div>
  </section>

  <!-- ---------- รายละเอียดเชิงเทคนิค ---------- -->
  <details class="card" id="tech">
    <summary>รายละเอียดเชิงเทคนิค และบันทึก PIR</summary>
    <div class="grid">
      <div class="stat"><div class="k">ความมั่นใจสูงสุด</div><div class="v" id="conf">-</div></div>
      <div class="stat"><div class="k">เวลาประมวลผล</div><div class="v" id="inf">-</div></div>
      <div class="stat"><div class="k">ดึงภาพจากกล้อง</div><div class="v" id="sh">-</div></div>
      <div class="stat"><div class="k">ขนาดภาพ</div><div class="v" id="sz">-</div></div>
      <div class="stat"><div class="k">ภาพชุดข้อมูล</div><div class="v" id="ds">-</div></div>
    </div>
    <div class="lbl">PIR — ใช้หาสาเหตุ PIR หลอน</div>
    <div class="grid">
      <div class="stat"><div class="k">ทริกใหม่</div><div class="v" id="pirN">-</div></div>
      <div class="stat"><div class="k">ทริกซ้ำ (OUT ค้าง)</div><div class="v" id="pirR">-</div></div>
      <div class="stat"><div class="k">กระตุก (กรองทิ้ง)</div><div class="v" id="pirG">-</div></div>
      <div class="stat"><div class="k">ไม่สนใจ (มอเตอร์หมุน)</div><div class="v" id="pirI">-</div></div>
    </div>
    <div class="msg" id="pirHint"></div>
    <div id="pirLog"></div>
    <div class="lbl">เปลี่ยนรหัสผ่านหน้าเว็บ</div>
    <form class="pwForm" id="pwForm" autocomplete="on">
      <input type="text" name="username" value="bird" autocomplete="username" hidden>
      <input class="txt" type="password" id="pwCur" placeholder="รหัสปัจจุบัน" autocomplete="current-password">
      <input class="txt" type="password" id="pwNew" placeholder="รหัสใหม่ (อย่างน้อย 6 ตัว)" autocomplete="new-password">
      <input class="txt" type="password" id="pwNew2" placeholder="พิมพ์รหัสใหม่ซ้ำ" autocomplete="new-password">
      <button type="submit" id="pwSave">บันทึกรหัสใหม่</button>
      <div class="msg">เปลี่ยนแล้ว มือถือ/คอมเครื่องอื่นที่ล็อกอินไว้ต้องล็อกอินใหม่</div>
    </form>
  </details>
</div>

<!-- ---------- ดูภาพใหญ่ ---------- -->
<div class="lb" id="lb" hidden>
  <div class="lbTop"><span id="lbCap"></span><button id="lbX" aria-label="ปิด">✕</button></div>
  <img id="lbImg" alt="ภาพที่พบนก">
  <div class="lbNav">
    <button id="lbPrev">‹ ใหม่กว่า</button>
    <a class="btn" id="lbDl" download>⬇ บันทึก</a>
    <button id="lbNext">เก่ากว่า ›</button>
  </div>
</div>
<div class="dsv" id="dsv" hidden>
  <div class="lbTop"><span id="dsvCap"></span><button id="dsvX" aria-label="ปิด">✕</button></div>
  <div class="dsvMid" id="dsvMid"></div>
  <div class="dsvBtns">
    <button class="bird" data-l="bird">✓ มีนก</button>
    <button class="none" data-l="none">✗ ไม่มีนก</button>
    <button class="fix" data-l="fix">✎ กรอบผิด</button>
  </div>
  <div class="lbNav">
    <button id="dsvPrev">‹ ก่อนหน้า</button>
    <button class="dan" id="dsvDel">🗑 ลบ</button>
    <button id="dsvNext">ถัดไป ›</button>
  </div>
</div>
<div class="toast" id="toast"></div>

<script>
const $ = id => document.getElementById(id);
let lastFrame = -1, lastPrev = -1, liveOn = false, camIP = null;
let pirStuckSeen = false;

let msgHoldUntil = 0, toastTimer = 0;
function toast(t){
  const el = $('toast');
  el.textContent = t;
  el.classList.add('show');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove('show'), 3000);
}
// ข้อความตอบกลับจากปุ่ม ค้างไว้ 4 วินาที ไม่ให้สถานะตรวจจับทับทันที และเด้งแจ้งที่ล่างจอด้วย
function setMsg(t, hold){
  $('msg').textContent = thaiMsg(t);
  if(hold !== false){ msgHoldUntil = Date.now() + 4000; toast(thaiMsg(t)); }
}

// หมดอายุการล็อกอิน (หรือมีคนเปลี่ยนรหัส) -> กลับไปหน้าล็อกอิน
function toLogin(){ location.replace('/login'); }

// ทุกคำสั่งส่ง header X-Bird-UI (กันเว็บอื่นยิงคำสั่งแทน) คุกกี้ล็อกอินไปเองกับคำขอ
async function post(url, body){
  try{
    const r = await fetch(url, {method:'POST',
      headers:{'Content-Type':'application/json', 'X-Bird-UI':'1'},
      body: JSON.stringify(body||{})});
    const j = await r.json();
    if(r.status === 401 && j.need_login){ toLogin(); return {ok:false, msg:'ต้องล็อกอินใหม่'}; }
    return j;
  }catch(e){ setMsg('ติดต่อเซิร์ฟเวอร์ไม่ได้'); return {ok:false}; }
}

async function run(url, body){
  const r = await post(url, body);
  setMsg(r.msg || (r.ok ? 'ทำแล้ว' : 'สั่งไม่สำเร็จ'));
  return r;
}

// แปลคำตอบ ERR จากบอร์ดเป็นภาษาคน
const ERR_TH = {
  'ERR MOVING':'มอเตอร์ยังหมุนอยู่ รอให้หยุดก่อน (หรือกดหยุดหมุน)',
  'ERR BUSY':'เครื่องกำลังทำงานอยู่',
  'ERR PUMP GAP':'ปั๊มเพิ่งทำงาน ต้องพักอีกสักครู่',
  'ERR PUMP BUSY':'ปั๊มเปิดอยู่แล้ว',
  'ERR PUMP DUTY':'ใช้ปั๊มครบโควตา 10 นาทีแล้ว รอสักพัก',
  'ERR WATER EMPTY':'น้ำหมด (หรือเซนเซอร์น้ำเสีย) ปั๊มถูกล็อก',
  'ERR NO WATER SENSOR':'ยังไม่ได้ต่อเซนเซอร์ระดับน้ำ',
  'ERR UNKNOWN':'บอร์ดไม่รู้จักคำสั่งนี้ (แฟลชเฟิร์มแวร์ v7.6 หรือยัง?)',
};
function thaiMsg(m){ return (m && ERR_TH[m.trim()]) || m || ''; }

let lastWarns = '';
function warnBox(list){
  const key = JSON.stringify(list);
  if(key === lastWarns) return;                 // ไม่สร้างใหม่ทุกวินาที หน้าจะได้ไม่กระตุก
  lastWarns = key;
  $('warns').innerHTML = '';
  for(const [cls, text] of list){
    const d = document.createElement('div');
    d.className = 'warn ' + cls;
    d.textContent = text;
    $('warns').appendChild(d);
  }
}

function pill(el, ok, textOk, textNo, warn){
  el.textContent = ok ? textOk : textNo;
  el.className = 'pill ' + (warn ? 'wn' : (ok ? 'on' : 'off'));
}
function chip(id, cls, text){
  const el = $(id);
  el.className = 'chip ' + cls;
  el.lastChild.textContent = text;
}

async function tick(){
  let s;
  try{
    const r = await fetch('/api/state');
    if(r.status === 401){ toLogin(); return; }
    s = await r.json();
  }
  catch(e){ chip('cBrd', 'bad', 'ติดต่อ Pi ไม่ได้'); return; }

  // ---- แถบหัว ----
  const mb = $('modeBadge');
  mb.hidden = false;
  mb.textContent = s.mode === 'train' ? 'เวอร์ชันเก็บภาพเทรน' : s.mode === 'demo' ? 'โหมดสาธิต' : 'ใช้งานปกติ';
  mb.className = 'pill ' + (s.mode === 'normal' ? 'on' : 'wn');
  $('dsCard').hidden = !s.train_mode;
  if(s.train_mode && !dsStarted){ dsStarted = true; dsLoad(true); }
  chip('cCam', s.cam_online ? 'ok' : 'bad', s.cam_online ? 'กล้อง' : 'กล้องหลุด');
  chip('cBrd', s.hw.link ? 'ok' : 'bad', s.hw.link ? 'บอร์ด' : 'บอร์ดไม่ต่อ');
  const ready = s.hw.link && s.hw.ready;          // อ่านจากบอร์ดจริง = ตรงกับไฟเขียวจริง
  chip('cRdy', ready ? 'ok' : '', ready ? 'พร้อม' : 'กำลังเตรียม');
  const bl = s.hw.link && s.hw.bird_lamp;
  chip('cBird', bl ? 'wn' : '', bl ? 'เจอนก!' : 'ไม่มีนก');
  const wp = s.hw.water_pct;
  chip('cWat', wp < 0 ? '' : s.hw.water_empty ? 'bad' : s.hw.water_low ? 'wn' : 'ok',
       wp < 0 ? 'น้ำ -' : 'น้ำ ' + wp + '%');
  chip('cAuto', s.hw.auto_repel ? 'ok' : '', s.hw.auto_repel ? 'ไล่อัตโนมัติ' : 'ไม่ไล่เอง');

  // ---- ผลตรวจล่าสุด ----
  const n = s.bird_count;
  $('cnt').textContent = n;
  $('badge').textContent = !s.last_update ? 'ยังไม่ได้ตรวจ'
      : n > 0 ? '🐦 พบนก ' + n + ' ตัว · ' + s.max_conf.toFixed(2) : 'ไม่พบนก';
  $('badge').className = 'badge' + (n > 0 ? ' bird' : '');
  $('busy').className = 'busy' + (s.processing ? ' on' : '');
  $('ago').textContent = s.last_update ? Math.max(0, Math.round(s.now - s.last_update)) + ' วิที่แล้ว' : '';
  const cf = s.confirm_frames, st = Math.min(s.confirm_streak, cf);
  $('dots').innerHTML = cf > 1 ? Array.from({length: cf}, (_, i) => '<i' + (i < st ? ' class="f"' : '') + '></i>').join('') : '';
  $('cfm').textContent = cf > 1 ? st + '/' + cf + ' เฟรมติดกัน' : 'ไม่ต้องยืนยัน';

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
  $('duty').textContent = s.hw.pump_duty_left == null ? '-'
                        : (s.hw.pump_duty_left / 1000).toFixed(0) + ' วิ';
  $('rph').textContent = s.hw.repel_hour_left + ' ครั้ง';
  if(s.pump_button_ms) $('pumpS').textContent = s.pump_button_ms / 1000;

  // ---- แถบเตือน ----
  const w = [];
  if(s.mode === 'train') w.push(['w', 'เวอร์ชันเก็บภาพเทรน: เก็บภาพดิบพร้อมกรอบเข้าชุดข้อมูล (ติดป้ายที่การ์ด ' +
                               '"ข้อมูลสำหรับเทรนโมเดล" ท้ายหน้า) — ค่าเริ่มต้นไม่ไล่นกเอง ให้นกอยู่ถ่ายได้หลายภาพ ' +
                               (s.hw.auto_repel ? '(ตอนนี้เปิดไล่อัตโนมัติอยู่)' : 'เปิดไล่ได้ที่สวิตช์ "ไล่อัตโนมัติเมื่อเจอนก"')]);
  if(s.test_mode) w.push(['w', 'โหมดทดสอบเปิดอยู่: ไม่ไล่นกเอง ไม่ต้องยืนยันหลายเฟรม และเกณฑ์คะแนนต่ำกว่าปกติ ' +
                               'ก่อนใช้งานจริงให้แก้ MODE_DEFAULT เป็น normal']);
  if(s.hw.link && s.hw.error) w.push(['b', s.hw.error]);
  if(s.hw.link && ['BROWNOUT','PANIC','WDT'].includes(s.hw.reset_reason) && !s.hw.error)
    w.push(['b', 'บอร์ดควบคุมรีบูตเองครั้งล่าสุด (' + s.hw.reset_reason + ') ' +
                 (s.hw.reset_reason === 'BROWNOUT' ? 'มักเกิดจากไฟตกตอนปั๊ม/มอเตอร์เริ่มทำงาน ' : '') +
                 'ตำแหน่งมอเตอร์อาจเพี้ยน']);
  if(!s.hw.link) w.push(['b', 'ไม่ได้เชื่อมต่อบอร์ดควบคุม' + (s.hw.error ? ' — ' + s.hw.error : '')]);
  if(s.hw.link && s.hw.pos_ok === false)
    w.push(['w', 'ยังไม่ได้ตั้งจุดกลางมอเตอร์หลังเปิดเครื่อง ถ้าหัวฉีดไม่ได้อยู่ตรงกลาง ' +
                 'ให้หมุนจนตรงกลางแล้วกด "ตั้งตรงนี้เป็นจุดกลาง" ไม่งั้นระยะ ±90° จะคลาดไป']);
  if(s.hw.link && s.hw.water_fault) w.push(['b', 'เซนเซอร์ระดับน้ำอ่านไม่ได้ ปั๊มถูกล็อกไว้ก่อน']);
  if(s.hw.link && s.hw.pump_duty_left === 0) w.push(['w', 'ใช้ปั๊มครบโควตา 10 นาทีแล้ว ปั๊มจะกลับมาใช้ได้เอง']);
  if(s.hw.repel_hour_left <= 0) w.push(['w', 'ไล่อัตโนมัติครบโควตาชั่วโมงนี้แล้ว (กันน้ำหมดเพราะโมเดลเห็นผิดซ้ำ ๆ)']);
  if(!s.cam_online) w.push(['b', 'กล้องออฟไลน์ — เช็กสาย USB ของเว็บแคม']);
  if(s.hw.link && s.hw.pir_stuck)
    w.push(['w', 'PIR ค้าง HIGH เกิน 30 วิ และกล้องไม่เจอนก — หยุดสั่งถ่ายซ้ำไว้ก่อน ' +
                 'ลองลดปุ่ม Sx (ความไว) หรือ Tx (หน่วงเวลา) และเช็กว่ามีแดด/ลมร้อน/ใบไม้ไหวหน้าเซนเซอร์ไหม']);
  warnBox(w);

  // ---- สถานะเครื่อง ----
  if(wp < 0){ $('wat').textContent = 'ไม่มีเซนเซอร์'; $('watBar').style.width = '0%'; }
  else{
    $('wat').textContent = wp + '%';
    $('watBar').style.width = wp + '%';
    $('watBar').style.background = s.hw.water_empty ? 'var(--bad)'
                                : (s.hw.water_low ? 'var(--warn)' : 'var(--ok)');
  }
  $('vb').textContent  = s.hw.vbat > 2 ? s.hw.vbat.toFixed(2) + ' V' : '-';
  $('deg').textContent = s.hw.deg + '°' + (s.hw.pos_ok === false ? ' ?' : '');
  $('swBox').hidden = s.hw.sweep_deg == null;
  if(s.hw.sweep_deg != null && !swDirty){
    $('swDeg').value = s.hw.sweep_deg; $('swSpd').value = s.hw.sweep_spd;
    $('swPumpRow').hidden = s.hw.repel_pump_ms == null;
    if(s.hw.repel_pump_ms != null) $('swPump').value = s.hw.repel_pump_ms / 1000;
    swShow();
  }
  $('brd').textContent = s.hw.link ? 'เชื่อมต่อแล้ว' : 'ไม่ได้เชื่อมต่อ';
  $('brd').style.color = s.hw.link ? 'var(--ok)' : 'var(--bad)';
  $('rdy').textContent = ready ? 'ติด (พร้อม)' : 'ดับ (กำลังเตรียม)';
  $('rdy').style.color = ready ? 'var(--ok)' : 'var(--muted)';
  $('blamp').textContent = bl ? 'ติด' : 'ดับ';
  $('blamp').style.color = bl ? 'var(--warn)' : 'var(--muted)';
  $('pir').textContent = !s.hw.link ? '-'
      : s.hw.pir_warm > 0 ? 'อุ่นเครื่อง (ไม่เกิน ' + s.hw.pir_warm + ' วิ)'
      : s.hw.pir_stuck ? 'ค้าง HIGH (หยุดถ่ายซ้ำ)'
      : s.hw.pir ? 'เห็นการเคลื่อนไหว'
      : s.hw.pir_ago != null ? 'ทริกล่าสุด ' + s.hw.pir_ago + ' วิที่แล้ว'
      : 'พร้อม ยังไม่ทริก';
  $('pir').style.color = s.hw.pir ? 'var(--warn)' : '';

  // ---- บันทึก PIR ----
  $('pirN').textContent = s.hw.pir_trig;
  $('pirR').textContent = s.hw.pir_rep;
  $('pirG').textContent = s.hw.pir_glitch;
  $('pirI').textContent = s.hw.pir_ignored;
  const log = s.hw.pir_log || [];
  const holds = log.map(e => e.held).filter(h => h != null);
  let hint = '';
  if(s.hw.pir_glitch >= 5) hint = 'กระตุกบ่อย: สัญญาณรบกวนเข้าสาย PIR — เช็กไฟเลี้ยง 5V / ใส่ตัวเก็บประจุที่ PIR / แยกสายออกจากสายมอเตอร์';
  else if(s.hw.pir_stuck || log.some(e => e.stuck))
    hint = 'เคยค้าง HIGH จนต้องหยุดถ่ายซ้ำ: ลด Sx (ความไว) ทีละนิด ถ้ายังค้างให้หมุน Tx (หน่วงเวลา) ทวนเข็มจนสุด';
  else if(holds.length && Math.max(...holds) > 20) hint = 'HIGH ค้างนาน: หมุนปุ่ม Tx (หน่วงเวลา) บน PIR ทวนเข็มจนสุด';
  $('pirHint').textContent = hint;
  if(s.hw.pir_stuck && !pirStuckSeen) $('tech').open = true;   // ค้างเมื่อไหร่ กางบันทึก PIR ให้เห็นเอง
  pirStuckSeen = !!s.hw.pir_stuck;
  $('pirLog').innerHTML = '';
  for(const e of log){
    const d = document.createElement('div');
    d.textContent = e.ts + ' · ' + (e.held == null ? 'HIGH อยู่' : 'HIGH ' + e.held + ' วิ') +
                    ' · ' + (e.trig ? 'ทริก ✓' : 'ไม่ทริก (ช่วงพัก/มอเตอร์)') +
                    (e.reps ? ' · ซ้ำ ' + e.reps + ' รอบ' : '') +
                    (e.stuck ? ' · ค้าง! หยุดถ่ายซ้ำ' : '');
    d.style.color = e.stuck ? 'var(--warn)' : (e.trig || e.reps ? 'var(--txt)' : 'var(--muted)');
    $('pirLog').appendChild(d);
  }
  if(!log.length) $('pirLog').textContent = 'ยังไม่มีการเคลื่อนไหว';

  pill($('pillDet'), s.auto_detect, 'ทุก ' + s.auto_interval + ' วิ', 'ปิดอยู่');
  pill($('pillCam'), s.cam_online, 'ออนไลน์', 'ออฟไลน์');
  $('cdTxt').textContent = s.hw.cooldown_left > 0 ? 'พักอีก ' + s.hw.cooldown_left + ' วิ' : '';

  if(document.activeElement !== $('autoDet')) $('autoDet').checked = s.auto_detect;
  if(document.activeElement !== $('autoRep')) $('autoRep').checked = s.hw.auto_repel;
  $('instRow').hidden = s.confirm_base <= 1;
  if(document.activeElement !== $('instRep')) $('instRep').checked = s.instant_repel;

  if(s.cam_ip && s.cam_ip !== camIP){
    camIP = s.cam_ip;
    $('camip').textContent = camIP;
    if(liveOn) $('live').src = '/stream?t=' + Date.now();
  }

  // ---- ภาพ ----
  if(s.preview_seq !== lastPrev){ lastPrev = s.preview_seq;
    const u = '/preview?t=' + Date.now();
    $('shot').src = u; $('roiImg').src = u; }
  if(s.frame_seq !== lastFrame){ lastFrame = s.frame_seq;
    $('shot').src = '/image?t=' + Date.now(); }

  if(s.status && Date.now() - msgHoldUntil > 0)
    setMsg(s.status + (s.processing ? ' (กำลังประมวลผล...)' : ''), false);

  // ---- ROI ----
  if(document.activeElement !== $('roiOn')) $('roiOn').checked = s.roi.enabled;
  if(!roiDirty){
    $('cutL').value = s.roi.x1; $('cutR').value = (1 - s.roi.x2).toFixed(2);
    $('cutT').value = s.roi.y1; $('cutB').value = (1 - s.roi.y2).toFixed(2);
  }
  drawRoi();

  // ---- ประวัติ ----
  renderHistory(s.history || []);
}

// ---------- ภาพย้อนหลัง: สร้างใหม่เฉพาะตอนมีภาพเพิ่ม (เดิมสร้างทุกวินาที ภาพเลยกระพริบและกดไม่ได้) ----------
let hist = [], histKey = null, lbFile = null;
function renderHistory(list){
  const key = list.map(h => h.file).join('|');
  if(key === histKey) return;
  histKey = key;
  hist = list;
  $('histN').textContent = list.length ? list.length + ' ภาพล่าสุด' : '';
  if(!list.length){ $('hist').innerHTML = '<div class="empty">ยังไม่เจอนก</div>'; return; }
  $('hist').innerHTML = list.map((h, i) =>
    `<button class="hi" data-f="${h.file}"><img src="/history/${h.file}" loading="lazy" alt="">
     <span><b>${h.count} ตัว</b> · ${h.conf}<br>${h.ts}</span></button>`).join('');
}
$('hist').onclick = e => {
  const b = e.target.closest('.hi');
  if(b) openLb(b.dataset.f);
};
function openLb(file){
  const i = hist.findIndex(h => h.file === file);
  if(i < 0) return;
  const h = hist[i];
  lbFile = file;
  $('lbImg').src = '/history/' + file;
  $('lbDl').href = '/history/' + file;
  $('lbDl').setAttribute('download', file);
  $('lbCap').textContent = h.ts + ' · พบนก ' + h.count + ' ตัว · ความมั่นใจ ' + h.conf +
                           '  (' + (i + 1) + '/' + hist.length + ')';
  $('lbPrev').disabled = i === 0;
  $('lbNext').disabled = i === hist.length - 1;
  $('lb').hidden = false;
  document.body.style.overflow = 'hidden';
}
function lbStep(d){
  const i = hist.findIndex(h => h.file === lbFile);
  const j = i + d;
  if(i >= 0 && j >= 0 && j < hist.length) openLb(hist[j].file);
}
function closeLb(){ $('lb').hidden = true; document.body.style.overflow = ''; }
$('lbX').onclick = closeLb;
$('lbPrev').onclick = () => lbStep(-1);
$('lbNext').onclick = () => lbStep(1);
$('lbImg').onclick = closeLb;
document.addEventListener('keydown', e => {
  if($('lb').hidden) return;
  if(e.key === 'Escape') closeLb();
  if(e.key === 'ArrowLeft') lbStep(-1);
  if(e.key === 'ArrowRight') lbStep(1);
});
let tx0 = null;
$('lb').addEventListener('touchstart', e => { tx0 = e.touches[0].clientX; }, {passive:true});
$('lb').addEventListener('touchend', e => {
  if(tx0 == null) return;
  const dx = e.changedTouches[0].clientX - tx0;
  tx0 = null;
  if(Math.abs(dx) > 60) lbStep(dx > 0 ? -1 : 1);
});

// ---------- ROI ----------
let roiDirty = false;
const CUTS = ['cutL', 'cutR', 'cutT', 'cutB'];
function roiVals(){
  const v = k => parseFloat($(k).value) || 0;
  return {x1: v('cutL'), x2: +(1 - v('cutR')).toFixed(2), y1: v('cutT'), y2: +(1 - v('cutB')).toFixed(2)};
}
function drawRoi(){
  const r = roiVals(), b = $('roiBox');
  b.style.left = r.x1 * 100 + '%';  b.style.width  = (r.x2 - r.x1) * 100 + '%';
  b.style.top  = r.y1 * 100 + '%';  b.style.height = (r.y2 - r.y1) * 100 + '%';
  b.className = 'roiBox' + ($('roiOn').checked ? '' : ' off');
  for(const k of CUTS) $(k + 'v').textContent = Math.round(parseFloat($(k).value) * 100) + '%';
  $('roiNote').textContent = roiDirty ? 'ยังไม่ได้บันทึก — กด "บันทึก ROI" เพื่อใช้ค่านี้' :
      ($('roiOn').checked ? 'ตรวจเฉพาะในกรอบ ' + Math.round((r.x2 - r.x1) * 100) + '% × ' +
                            Math.round((r.y2 - r.y1) * 100) + '% ของภาพ' : 'ปิดอยู่ — ตรวจทั้งภาพ');
  $('roiNote').className = 'msg' + (roiDirty ? ' dirty' : '');
}
for(const k of CUTS) $(k).addEventListener('input', () => { roiDirty = true; drawRoi(); });
// ใช้จากสคริปต์ทดสอบ / คอนโซลได้: setCut('cutL', 0.3)
function setCut(k, v){ $(k).value = v; roiDirty = true; drawRoi(); }

$('roiOn').onchange = async e => {
  drawRoi();
  const r = await post('/api/roi', {enabled: e.target.checked});
  if(r.ok) setMsg(e.target.checked ? 'เปิด ROI แล้ว' : 'ปิด ROI แล้ว (ตรวจทั้งภาพ)');
};
$('roiSave').onclick = async () => {
  const r = await post('/api/roi', Object.assign({enabled: $('roiOn').checked}, roiVals()));
  if(r.ok) roiDirty = false;
  setMsg(r.ok ? 'บันทึก ROI แล้ว' : 'ค่าไม่ถูกต้อง');
  drawRoi();
};
$('roiReset').onclick = async () => {
  const r = await post('/api/roi', {reset: true});
  if(r.ok){
    roiDirty = false;
    $('roiOn').checked = r.roi.enabled;
    $('cutL').value = r.roi.x1; $('cutR').value = (1 - r.roi.x2).toFixed(2);
    $('cutT').value = r.roi.y1; $('cutB').value = (1 - r.roi.y2).toFixed(2);
  }
  setMsg(r.ok ? 'คืนค่า ROI เริ่มต้นแล้ว (ตัดขอบซ้าย-ขวา)' : 'สั่งไม่สำเร็จ');
  drawRoi();
};

// ---------- ปุ่ม ----------
$('btnShot').onclick = async () => {
  setMsg('กำลังดึงภาพจากกล้อง...', false);
  const r = await post('/api/capture');
  if(!r.ok) setMsg(r.msg || 'สั่งไม่สำเร็จ');
};
$('autoDet').onchange = async e => {
  const r = await post('/api/auto_detect', {on: e.target.checked});
  if(r.ok) setMsg(r.on ? 'เปิดตรวจอัตโนมัติ' : 'ปิดตรวจอัตโนมัติ');
};
$('autoRep').onchange = async e => {
  const r = await post('/api/auto_repel', {on: e.target.checked});
  if(r.ok) setMsg(r.on ? 'เปิดไล่อัตโนมัติ — เจอนกแล้วจะฉีดน้ำเอง' : 'ปิดไล่อัตโนมัติ — เจอนกแค่ตีกรอบ/ไฟส้ม');
};
$('instRep').onchange = async e => {
  const r = await post('/api/instant_repel', {on: e.target.checked});
  if(r.ok) setMsg(r.on ? 'ไล่ทันที — เจอนกภาพเดียวก็ฉีดน้ำ (ไม่รอยืนยัน)'
                       : 'กลับไปยืนยันก่อนไล่ — ต้องเจอนกติดกันก่อนฉีดน้ำ');
};
$('btnLog').onclick = () => location.href = '/log';

$('live').onerror = () => {
  if(liveOn) setMsg('เปิดภาพสดไม่ได้ (อาจมีคนเปิดดูพร้อมกันเกินจำนวนที่ตั้งไว้)');
};
$('btnLive').onclick = () => {
  liveOn = !liveOn;
  $('live').style.display = liveOn ? 'block' : 'none';
  $('livePh').style.display = liveOn ? 'none' : 'flex';
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

// ---- การกวาดตอนไล่ ----
let swDirty = false;
function swShow(){
  $('swDegV').textContent = '±' + $('swDeg').value + '°';
  $('swSpdV').textContent = $('swSpd').value + '%';
  $('swPumpV').textContent = $('swPump').value + ' วิ';
}
['swDeg','swSpd','swPump'].forEach(id => $(id).oninput = () => { swDirty = true; swShow(); });
$('swSave').onclick = async () => {
  const body = {deg: +$('swDeg').value, spd: +$('swSpd').value};
  if(!$('swPumpRow').hidden) body.pump_ms = Math.round(+$('swPump').value * 1000);
  const r = await run('/api/sweep', body);
  if(r.ok) swDirty = false;
};
$('swTest').onclick = async () => {
  if(swDirty){ await $('swSave').onclick(); if(swDirty) return; }   // ยังไม่บันทึก = บันทึกก่อนแล้วค่อยทดสอบ
  run('/api/sweep', {test: true});
};
$('mL').onclick = () => run('/api/motor', {action:'left',  steps: steps()});
$('mR').onclick = () => run('/api/motor', {action:'right', steps: steps()});
$('mH').onclick = () => run('/api/motor', {action:'home'});
$('mS').onclick = () => run('/api/motor', {action:'stop'});
$('mZ').onclick = () => {
  if(confirm('หัวฉีดอยู่ตรงกลางจริงแล้วใช่ไหม? ระบบจะถือว่าตำแหน่งนี้คือ 0°'))
    run('/api/motor', {action:'zero'});
};
$('btnLogout').onclick = async () => { await post('/api/logout', {}); toLogin(); };
$('pwForm').onsubmit = async e => {
  e.preventDefault();
  if($('pwNew').value !== $('pwNew2').value){ setMsg('รหัสใหม่สองช่องไม่ตรงกัน'); return; }
  const r = await run('/api/password', {current: $('pwCur').value, new: $('pwNew').value});
  if(r.ok) $('pwForm').reset();
};
$('bPump').onclick  = () => run('/api/pump', {});
$('bRepel').onclick = () => run('/api/repel');
$('bWater').onclick = () => run('/api/water');
$('bAbort').onclick = () => run('/api/abort');
$('bLamp').onclick  = () => run('/api/bird_lamp', {on: true});

// ---------- ข้อมูลสำหรับเทรน ----------
const DS_REASON = {bird:'เจอนก', repel:'สั่งไล่', lowconf:'ก้ำกึ่ง', manual:'กดถ่าย', pir:'PIR',
                   motion:'มีการเคลื่อนไหว', random:'สุ่มเก็บ'};
const DS_TAG = {bird:'✓ นก', none:'✗ ไม่มีนก', fix:'✎ แก้กรอบ'};
const ds = {f:'todo', p:0, items:[], counts:{}, cur:-1, busy:false, free:null};

function dsPic(it){
  // กล่องอัตราส่วนเท่าภาพจริง กรอบวางเป็นเปอร์เซ็นต์จึงตรงกับภาพทุกขนาดจอ
  const w = it.w || 4, h = it.h || 3;
  let bx = '';
  (it.b || []).forEach(b => {
    const weak = b[4] < 0.25;
    bx += '<span class="dsBx' + (weak ? ' weak' : '') + '" style="left:' + (b[0]/w*100) + '%;top:' + (b[1]/h*100) +
          '%;width:' + ((b[2]-b[0])/w*100) + '%;height:' + ((b[3]-b[1])/h*100) + '%"><i>' + b[4].toFixed(2) + '</i></span>';
  });
  const tag = it.l ? '<span class="dsTag ' + it.l + '">' + DS_TAG[it.l] + '</span>' : '';
  return '<div class="dsPic" style="aspect-ratio:' + w + '/' + h + '"><img loading="lazy" alt="" src="/dataset/img/' +
         it.n + '">' + bx + tag + '</div>';
}
function dsRenderCounts(){
  const c = ds.counts;
  document.querySelectorAll('#dsTabs button').forEach(b => {
    b.classList.toggle('on', b.dataset.f === ds.f);
    b.querySelector('b').textContent = c[b.dataset.f] !== undefined ? c[b.dataset.f] : '';
  });
  $('dsSum').textContent = c.all !== undefined ? (c.all + ' ภาพ · ติดป้ายแล้ว ' + (c.all - c.todo)) : '';
  const nTab = c[ds.f] || 0;
  $('dsDelTab').hidden = !(ds.f === 'todo' || ds.f === 'none') || !nTab;
  $('dsDelTab').textContent = '🗑 ลบทั้งหมดในแท็บนี้ (' + nTab + ')';
  $('dsFree').textContent = ds.free != null ? '· การ์ดเหลือว่าง ' + ds.free + ' GB' : '';
  const lab = (c.bird||0) + (c.none||0) + (c.fix||0);
  const mb = n => n ? ' (~' + Math.max(1, Math.round(n * 0.15)) + ' MB)' : '';   // ภาพละ ~150 KB
  $('dsZipNote').textContent = 'ติดป้ายแล้ว ' + lab + ' ภาพ' + mb(lab) + ' · ทั้งหมด ' + (c.all||0) + ' ภาพ' +
    mb(c.all||0) + ' — ไฟล์ใหญ่ ดาวน์โหลดผ่านเน็ตมือถือจะใช้เวลานาน';
}
function dsRenderGrid(){
  if(!ds.items.length){
    $('dsGrid').innerHTML = '<div class="empty">' + (ds.f === 'todo' ? 'ติดป้ายครบแล้ว หรือยังไม่มีภาพ' : 'ไม่มีภาพในหมวดนี้') + '</div>';
    return;
  }
  $('dsGrid').innerHTML = ds.items.map((it, i) =>
    '<button class="dsi" data-i="' + i + '">' + dsPic(it) + '<span class="meta">' + it.t.slice(5, 16) + ' · ' +
    (DS_REASON[it.r] || it.r) + '</span></button>').join('');
}
async function dsLoad(reset){
  if(ds.busy) return;
  ds.busy = true;
  if(reset){ ds.p = 0; ds.items = []; }
  try{
    const r = await fetch('/api/dataset?f=' + ds.f + '&p=' + ds.p);
    if(r.status === 401){ toLogin(); return; }
    const j = await r.json();
    ds.items = ds.items.concat(j.items);
    ds.counts = j.counts;
    ds.free = j.free_gb;
    $('dsMore').hidden = !j.more;
    dsRenderCounts(); dsRenderGrid();
  }catch(e){ $('dsGrid').innerHTML = '<div class="empty">โหลดไม่สำเร็จ</div>'; }
  finally{ ds.busy = false; }
}
$('dsTabs').onclick = e => {
  const b = e.target.closest('button'); if(!b) return;
  ds.f = b.dataset.f; dsLoad(true);
};
$('dsMore').onclick = () => { ds.p++; dsLoad(false); };
$('dsReload').onclick = () => dsLoad(true);
$('dsGrid').onclick = e => {
  const b = e.target.closest('.dsi'); if(b) dsOpen(+b.dataset.i);
};
function dsOpen(i){
  if(i < 0 || i >= ds.items.length) return;
  ds.cur = i;
  const it = ds.items[i];
  const w = it.w || 4, h = it.h || 3;
  $('dsvMid').innerHTML = dsPic(it);
  // กว้างสุดเท่าที่ภาพยังสูงไม่เกินพื้นที่ตรงกลาง
  $('dsvMid').firstChild.style.width = 'min(100%, calc((100vh - 230px) * ' + (w/h) + '))';
  const nb = (it.b || []).filter(b => b[4] >= 0.25).length;
  $('dsvCap').textContent = (i+1) + '/' + ds.items.length + ' · ' + it.t + ' · ' + (DS_REASON[it.r] || it.r) +
    (it.b === null ? ' · ไม่มีข้อมูลกรอบ (ภาพรุ่นเก่า)' : ' · กรอบมั่นใจ ' + nb);
  document.querySelectorAll('.dsvBtns button').forEach(b => b.classList.toggle('sel', b.dataset.l === it.l));
  $('dsvPrev').disabled = i === 0;
  $('dsvNext').disabled = i === ds.items.length - 1 && $('dsMore').hidden;
  $('dsv').hidden = false;
}
function dsClose(){ $('dsv').hidden = true; dsRenderGrid(); }
async function dsNext(){
  if(ds.cur + 1 < ds.items.length){ dsOpen(ds.cur + 1); return; }
  if(!$('dsMore').hidden){ ds.p++; await dsLoad(false); if(ds.cur + 1 < ds.items.length){ dsOpen(ds.cur + 1); return; } }
  dsClose(); toast('ดูครบทุกภาพในหมวดนี้แล้ว');
}
async function dsLabel(label){
  const it = ds.items[ds.cur]; if(!it) return;
  const nl = it.l === label ? '' : label;            // กดป้ายเดิมซ้ำ = ล้างป้าย
  const r = await post('/api/dataset/label', {name: it.n, label: nl});
  if(!r.ok){ toast(r.msg || 'บันทึกป้ายไม่สำเร็จ'); return; }
  ds.counts[it.l || 'todo']--; ds.counts[nl || 'todo']++;
  it.l = nl;
  dsRenderCounts();
  if(nl) dsNext(); else dsOpen(ds.cur);
}
document.querySelectorAll('.dsvBtns button').forEach(b => b.onclick = () => dsLabel(b.dataset.l));
$('dsvX').onclick = dsClose;
$('dsvPrev').onclick = () => dsOpen(ds.cur - 1);
$('dsvNext').onclick = dsNext;
$('dsvDel').onclick = async () => {
  const it = ds.items[ds.cur]; if(!it || !confirm('ลบภาพนี้ออกจากชุดข้อมูล?')) return;
  const r = await post('/api/dataset/delete', {name: it.n});
  if(!r.ok){ toast(r.msg || 'ลบไม่สำเร็จ'); return; }
  ds.counts.all--; ds.counts[it.l || 'todo']--;
  ds.items.splice(ds.cur, 1);
  dsRenderCounts();
  if(!ds.items.length){ dsClose(); return; }
  dsOpen(Math.min(ds.cur, ds.items.length - 1));
};
document.addEventListener('keydown', e => {
  if($('dsv').hidden) return;
  const k = {'1':'bird', '2':'none', '3':'fix'}[e.key];
  if(k) dsLabel(k);
  else if(e.key === 'ArrowRight') dsNext();
  else if(e.key === 'ArrowLeft') dsOpen(ds.cur - 1);
  else if(e.key === 'Escape') dsClose();
});
(() => {   // ปัดซ้าย/ขวาบนมือถือเพื่อเปลี่ยนภาพ
  let x0 = null;
  $('dsvMid').addEventListener('touchstart', e => { x0 = e.touches[0].clientX; }, {passive:true});
  $('dsvMid').addEventListener('touchend', e => {
    if(x0 === null) return;
    const dx = e.changedTouches[0].clientX - x0; x0 = null;
    if(dx < -50) dsNext(); else if(dx > 50) dsOpen(ds.cur - 1);
  });
})();
$('dsDedupe').onclick = async () => {
  if(!confirm('ลบภาพที่แทบเหมือนภาพก่อนหน้า (เฉพาะที่ยังไม่ติดป้าย ไม่รวมภาพที่กดถ่ายเอง)?')) return;
  const b = $('dsDedupe');
  b.disabled = true; b.textContent = '⏳ กำลังตรวจภาพซ้ำ…';
  const r = await post('/api/dataset/bulk', {action: 'dedupe'});
  b.disabled = false; b.textContent = '🧹 ลบภาพซ้ำ';
  toast(r.msg || 'ลบไม่สำเร็จ');
  if(r.ok) dsLoad(true);
};
$('dsDelTab').onclick = async () => {
  const n = ds.counts[ds.f] || 0;
  if(!n || !ds.items.length) return;
  const name = ds.f === 'todo' ? 'ยังไม่ติดป้าย' : 'ไม่มีนก';
  if(!confirm('ลบภาพทั้งหมด ' + n + ' ภาพในแท็บ "' + name + '"?\nกู้คืนไม่ได้')) return;
  const r = await post('/api/dataset/bulk', {action: 'delete', f: ds.f, upto: ds.items[0].n});
  toast(r.msg || 'ลบไม่สำเร็จ');
  if(r.ok) dsLoad(true);
};
let dsStarted = false;           // โหลดการ์ดเทรนครั้งแรกเมื่อรู้ว่าเป็นเวอร์ชัน train

tick();
setInterval(tick, 1000);
</script>
</body>
</html>"""


# ============================ MAIN ============================
# ============================ LOGIN PAGE ============================
LOGIN_HTML = r"""<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#11151c">
<title>เข้าสู่ระบบ — ระบบไล่นกอัตโนมัติ</title>
<style>
  :root{--bg:#11151c;--card:#1a1f29;--line:#2a3140;--txt:#e7ecf3;--muted:#8b97a8;
        --ok:#3ddc84;--bad:#ff5d5d;--warn:#ffb020;--pri:#1f6f43;--prib:#2b8a55;--btn:#233044}
  *{box-sizing:border-box}
  body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;
       background:var(--bg);color:var(--txt);padding:16px;
       font-family:-apple-system,"Segoe UI",Roboto,"Noto Sans Thai",sans-serif}
  .box{width:100%;max-width:380px;background:var(--card);border:1px solid var(--line);
       border-radius:16px;padding:24px 20px}
  h1{font-size:20px;margin:0 0 4px}
  .sub{color:var(--muted);font-size:14px;margin:0 0 18px;line-height:1.5}
  label{display:block;font-size:13px;color:var(--muted);margin:12px 0 6px}
  .pw{position:relative}
  input{width:100%;font-size:17px;padding:12px 44px 12px 12px;border-radius:10px;
        border:1px solid var(--line);background:#11151c;color:var(--txt);outline:none}
  input:focus{border-color:var(--ok)}
  .eye{position:absolute;right:6px;top:50%;transform:translateY(-50%);background:none;border:0;
       color:var(--muted);font-size:18px;padding:6px;cursor:pointer}
  button.go{width:100%;margin-top:18px;padding:13px;font-size:16px;font-weight:600;border-radius:10px;
            border:1px solid var(--prib);background:var(--pri);color:#fff;cursor:pointer}
  button.go:disabled{opacity:.6}
  .msg{min-height:22px;margin-top:12px;font-size:14px;color:var(--bad)}
  .warn{background:#2a2414;border:1px solid #5a4a1a;color:#ffd48a;border-radius:10px;
        padding:10px 12px;font-size:13px;line-height:1.5;margin-bottom:6px}
</style>
</head>
<body>
<form class="box" id="f" autocomplete="on">
  <h1>🦅 ระบบไล่นกอัตโนมัติ</h1>
  <p class="sub" id="sub">ใส่รหัสผ่านเพื่อเข้าดูและสั่งงานเครื่อง</p>
  <div class="warn" id="setupNote" hidden>ยังไม่ได้ตั้งรหัสผ่าน ตั้งตอนนี้เลย
    (ใครเปิดหน้านี้ก่อนจะได้ตั้งรหัส) อย่างน้อย 6 ตัวอักษร</div>
  <input type="text" name="username" value="bird" autocomplete="username" hidden>
  <label for="pw" id="pwLbl">รหัสผ่าน</label>
  <div class="pw"><input type="password" id="pw" autocomplete="current-password" required>
    <button type="button" class="eye" data-for="pw" aria-label="แสดงรหัส">👁</button></div>
  <div id="pw2Wrap" hidden>
    <label for="pw2">พิมพ์รหัสซ้ำอีกครั้ง</label>
    <div class="pw"><input type="password" id="pw2" autocomplete="new-password">
      <button type="button" class="eye" data-for="pw2" aria-label="แสดงรหัส">👁</button></div>
  </div>
  <button class="go" id="go" type="submit">เข้าสู่ระบบ</button>
  <div class="msg" id="msg" role="alert"></div>
</form>
<script>
const SETUP = __SETUP__;
const $ = id => document.getElementById(id);
if(SETUP){
  $('sub').textContent = 'ตั้งรหัสผ่านสำหรับหน้าเว็บนี้';
  $('setupNote').hidden = false;
  $('pw2Wrap').hidden = false;
  $('pwLbl').textContent = 'รหัสผ่านใหม่';
  $('pw').autocomplete = 'new-password';
  $('go').textContent = 'ตั้งรหัสและเข้าสู่ระบบ';
}
document.querySelectorAll('.eye').forEach(b => b.onclick = () => {
  const i = $(b.dataset.for); i.type = i.type === 'password' ? 'text' : 'password';
});
let lockTimer = null;
function lock(sec){
  $('go').disabled = true;
  clearInterval(lockTimer);
  lockTimer = setInterval(() => {
    sec--;
    $('msg').textContent = 'ใส่รหัสผิดหลายครั้ง ลองใหม่ในอีก ' + sec + ' วินาที';
    if(sec <= 0){ clearInterval(lockTimer); $('go').disabled = false; $('msg').textContent = ''; }
  }, 1000);
}
$('f').onsubmit = async e => {
  e.preventDefault();
  const pw = $('pw').value;
  if(SETUP){
    if(pw.length < 6){ $('msg').textContent = 'รหัสต้องยาวอย่างน้อย 6 ตัวอักษร'; return; }
    if(pw !== $('pw2').value){ $('msg').textContent = 'รหัสสองช่องไม่ตรงกัน'; return; }
  }
  $('go').disabled = true; $('msg').textContent = '';
  try{
    const r = await fetch(SETUP ? '/api/setup' : '/api/login', {method:'POST',
      headers:{'Content-Type':'application/json','X-Bird-UI':'1'},
      body: JSON.stringify({password: pw})});
    const j = await r.json();
    if(j.ok){ location.replace('/'); return; }
    if(r.status === 409){ location.reload(); return; }
    $('msg').textContent = j.msg || 'เข้าสู่ระบบไม่สำเร็จ';
    $('pw').select();
    if(j.locked){ lock(j.locked); return; }
  }catch(err){ $('msg').textContent = 'ติดต่อเครื่องไม่ได้'; }
  $('go').disabled = false;
};
$('pw').focus();
</script>
</body>
</html>
"""


def main():
    if "--set-password" in sys.argv[1:]:
        cli_set_password()
        return
    app.secret_key = _load_secret()
    init_auth()
    load_roi()
    load_settings()
    if os.path.isfile(os.path.expanduser(CAM_DEVICE)):
        print(f"[Cam] ใช้ไฟล์วิดีโอแทนเว็บแคม: {CAM_DEVICE}", flush=True)
    print("[Boot] กำลังโหลดโมเดล...", flush=True)
    load_model()

    CAM.start()
    CTRL.start()
    threading.Thread(target=_worker, daemon=True).start()
    threading.Thread(target=_auto_loop, daemon=True).start()
    threading.Thread(target=_ready_loop, daemon=True).start()

    print(f"[Web] เปิดที่ http://0.0.0.0:{os.environ.get('BIRD_PORT', '5000')}", flush=True)
    if TEST_MODE:
        print("[Mode] *** โหมดสาธิต (demo): ปิด ROI / ไม่ยืนยันเฟรม / ไม่ไล่เอง / "
              f"conf={CONF_THRESHOLD} ***", flush=True)
    elif TRAIN_MODE:
        print(f"[Mode] *** เวอร์ชันเก็บภาพเทรน (train): เก็บภาพ + ติดป้ายบนเว็บ / "
              f"ไม่ไล่อัตโนมัติเป็นค่าเริ่มต้น / conf={CONF_THRESHOLD} ***", flush=True)
    else:
        print(f"[Mode] ใช้งานปกติ (normal): ไล่จริง ไม่เก็บภาพเทรน / conf={CONF_THRESHOLD}", flush=True)
    print(f"[Auto] ตรวจจับทุก {AUTO_INTERVAL_S} วินาที รอบละ {BURST_FRAMES} เฟรม "
          + ("(ไล่ทันทีเมื่อเจอภาพเดียว)" if confirm_frames() == 1 else
             f"(เจอนก {CONFIRM_FRAMES} เฟรมติดกันก่อนสั่งไล่ เฟรมยืนยันถ่ายตามใน {CONFIRM_DELAY_S:g} วิ)"),
          flush=True)
    print(f"[Detect] CLAHE={'เปิด' if DETECT_CLAHE else 'ปิด'}  "
          f"ตัดภาพตาม ROI={'เปิด' if ROI_CROP else 'ปิด'}", flush=True)
    if not password_is_set():
        print("[Auth] *** ยังไม่ได้ตั้งรหัสผ่าน — เปิดหน้าเว็บแล้วตั้งรหัสทันที "
              "(หรือ python3 raspberry_pi_server.py --set-password) ***", flush=True)
    if DATASET_ENABLED:
        print(f"[Dataset] เก็บภาพดิบสำหรับเทรนที่ {DATASET_DIR}", flush=True)
    if not PIR_TRIGGER_ENABLED:
        print("[PIR] ปิดการทริกด้วย PIR อยู่ "
              "(แก้ PIR_TRIGGER_ENABLED = True เมื่อเซนเซอร์พร้อม)", flush=True)

    app.run(host="0.0.0.0", port=int(os.environ.get("BIRD_PORT", "5000")),
            threaded=True, debug=False)


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
