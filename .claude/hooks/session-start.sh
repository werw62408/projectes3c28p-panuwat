#!/bin/bash
# SessionStart hook (Claude Code on the web only): gets the tools SomudTick work needs, so a new session can
# test in the simulator and build the firmware straight away. Safe to run again: every step skips what is there.
#   - simulator: LovyanGFX 1.2.30 + ArduinoJson 7.4.2 built for the PC (tools/sim/setup.sh)
#   - firmware:  arduino-cli + esp32 core 3.3.12 + LovyanGFX 1.2.30, ArduinoJson 7.4.2, IRremoteESP8266 2.9.0
#   - pictures:  Python Pillow (tools/sim/sheet.py)
set -euo pipefail

if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

ROOT="${CLAUDE_PROJECT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}"
BIN="$HOME/.local/bin"
mkdir -p "$BIN"
export PATH="$BIN:$PATH"
[ -n "${CLAUDE_ENV_FILE:-}" ] && echo "export PATH=\"$BIN:\$PATH\"" >> "$CLAUDE_ENV_FILE"

# retry a command (the network can drop for a moment)
retry() { local n=0; until "$@"; do n=$((n + 1)); [ $n -ge 4 ] && return 1; sleep $((2 ** n)); done; }

echo "[1/3] simulator"
retry "$ROOT/tools/sim/setup.sh"

echo "[2/3] Python Pillow"
python3 -c "import PIL" 2>/dev/null || retry pip install -q pillow

echo "[3/3] arduino-cli + esp32 core"
if ! command -v arduino-cli >/dev/null; then
  retry bash -c "curl -fsSL https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Linux_64bit.tar.gz | tar xz -C '$BIN' arduino-cli"
fi
ESP_URL=https://espressif.github.io/arduino-esp32/package_esp32_index.json
if ! arduino-cli core list 2>/dev/null | grep -q "^esp32:esp32 *3.3.12"; then
  arduino-cli config init --overwrite >/dev/null 2>&1 || true
  arduino-cli config set board_manager.additional_urls "$ESP_URL"
  retry arduino-cli core update-index
  retry arduino-cli core install esp32:esp32@3.3.12
  rm -rf "$HOME/.arduino15/staging"   # the downloaded archives (about 2 GB) are not needed after installing
fi
for lib in "LovyanGFX@1.2.30" "ArduinoJson@7.4.2" "IRremoteESP8266@2.9.0"; do
  name="${lib%@*}"; ver="${lib#*@}"
  arduino-cli lib list 2>/dev/null | grep -q "^$name *$ver" || retry arduino-cli lib install "$lib"
done
echo "SomudTick tools ready"
