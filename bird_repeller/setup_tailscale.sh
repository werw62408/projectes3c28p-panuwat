#!/usr/bin/env bash
# ติดตั้ง Tailscale บน Raspberry Pi เพื่อเปิดหน้าเว็บเครื่องไล่นกจากที่ไหนก็ได้
# (Pi ต้องมีอินเทอร์เน็ต เช่น ต่อฮอตสปอตมือถือ / ไม่ต้องตั้งค่าเราเตอร์หรือเปิดพอร์ต)
#
#   bash setup_tailscale.sh
#
# หลังติดตั้ง: ลงแอป Tailscale บนมือถือ/คอม แล้วล็อกอินบัญชีเดียวกับที่ใช้ตอนรันสคริปต์นี้
# หน้าเว็บยังต้องล็อกอินด้วยรหัสผ่านของระบบเหมือนเดิม
set -euo pipefail

PORT="${BIRD_PORT:-5000}"

if command -v tailscale >/dev/null 2>&1; then
  echo "[1/3] มี Tailscale อยู่แล้ว"
else
  echo "[1/3] กำลังติดตั้ง Tailscale ..."
  curl -fsSL https://tailscale.com/install.sh | sh
fi

echo "[2/3] เชื่อมเข้าบัญชี Tailscale"
echo "      ถ้ามีลิงก์ขึ้นมา ให้เปิดลิงก์นั้นด้วยมือถือหรือคอม แล้วล็อกอิน"
sudo tailscale up

IP="$(tailscale ip -4 | head -n1)"
HOST="$(hostname)"
echo "[3/3] เสร็จแล้ว (Tailscale เริ่มทำงานเองทุกครั้งที่เปิดเครื่อง)"
echo
echo "  เปิดหน้าเว็บ : http://${IP}:${PORT}"
echo "  หรือใช้ชื่อ   : http://${HOST}:${PORT}   (ต้องเปิด MagicDNS ในหน้า admin ของ Tailscale)"
echo "  SSH          : ssh ${USER}@${IP}"
echo
echo "  ให้คนอื่น (เช่น อาจารย์) ดู: หน้า admin ของ Tailscale > Machines > ${HOST} > Share"
