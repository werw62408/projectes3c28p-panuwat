#!/bin/bash
# release_check.sh <version> [zip output folder]
# Checks a SomudTick version is ready to hand over, and builds what can be built:
#   folder, version on the About page, README "what's new" on top, test file, simulator tests,
#   firmware build + merged bin, partition table unchanged, preview pictures, zip.
# Prints one line per check (PASS / FAIL / WARN) and ends with RELEASE READY or NOT READY.
# Example: .claude/skills/somudtick-release/scripts/release_check.sh 11.5 /tmp/out
set -uo pipefail
V="${1:?usage: release_check.sh <version like 11.5> [zip folder]}"
OUTZ="${2:-}"
ROOT="$(cd "$(dirname "$0")/../../../.." && pwd)"
SK="$ROOT/SomudTick_v$V/SomudTick"
bad=0
ok()   { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; bad=1; }
warn() { echo "WARN  $*"; }

# 1. folder
[ -d "$SK" ] || { fail "folder SomudTick_v$V/SomudTick missing"; echo "NOT READY"; exit 1; }
ok "folder SomudTick_v$V/SomudTick"

# the version before this one (for the partition check)
PREV=$(ls -d "$ROOT"/SomudTick_v*/ 2>/dev/null | sed 's|.*/SomudTick_v||; s|/$||' | sort -V | awk -v v="$V" '$0==v{print p; exit} {p=$0}')

# 2. version on the About page
if grep -q "line(\"Version\", \"v$V\"" "$SK/screen_settings.h"; then ok "About page shows v$V"
else fail "About page version is $(grep -o 'line("Version", "v[0-9.]*"' "$SK/screen_settings.h" | grep -o 'v[0-9.]*') (should be v$V)"; fi

# 3. README: title + "what's new" of this version at the top, with checks
R="$SK/README.md"
if [ -f "$R" ]; then
  head -1 "$R" | grep -q "v$V" && ok "README title says v$V" || fail "README title does not say v$V"
  first=$(grep -m1 -n '^## ของใหม่ใน' "$R" | cut -d: -f2-)
  echo "$first" | grep -q "v$V" && ok "README starts with 'ของใหม่ใน v$V'" || fail "first 'ของใหม่ใน' section is not v$V: $first"
  sec=$(awk -v v="v$V" '/^## ของใหม่ใน/{n++} n==1' "$R")
  echo "$sec" | grep -q '✅' && ok "README new section has ✅ checks" || fail "README new section has no ✅ checks"
  echo "$sec" | grep -q '❌' && ok "README new section has ❌ steps" || fail "README new section has no ❌ steps"
  grep -q '^## ตารางคำศัพท์' "$R" && ok "README has the glossary" || fail "README glossary missing"
else fail "README.md missing"; fi

# 4. simulator test file for this version + all tests
[ -f "$ROOT/tools/sim/test_v$V.cpp" ] && ok "tools/sim/test_v$V.cpp exists" || fail "tools/sim/test_v$V.cpp missing (every version adds its tests)"
if out=$("$ROOT/tools/sim/run_tests.sh" "$SK" 2>&1); then
  ok "simulator: $(echo "$out" | grep -c " PASS$") checks passed, ALL TESTS PASSED"
else
  fail "simulator tests failed:"; echo "$out" | grep -E "FAIL|BUILD FAILED" | sed 's/^/        /'
fi

# 5. firmware build + merged bin
if command -v arduino-cli >/dev/null; then
  if bout=$("$ROOT/tools/build_firmware.sh" "$SK" 2>&1) && [ -f "$SK/SomudTick_merged.bin" ]; then
    ok "firmware built: $(echo "$bout" | grep -o 'Sketch uses [0-9]* bytes ([0-9]*%)')"
    echo "$bout" | grep -qi " error" && fail "compiler printed errors" 
  else fail "firmware build failed:"; echo "$bout" | tail -15 | sed 's/^/        /'; fi
else
  if [ -f "$SK/SomudTick_merged.bin" ]; then warn "arduino-cli not found: using the SomudTick_merged.bin already there (not rebuilt)"
  else fail "arduino-cli not found and no SomudTick_merged.bin (run .claude/hooks/session-start.sh)"; fi
fi

# 6. partition table must stay the same (user logs live in a partition)
B="$SK/SomudTick_merged.bin"
if [ -f "$B" ] && [ -n "$PREV" ] && [ -f "$ROOT/SomudTick_v$PREV/SomudTick/SomudTick_merged.bin" ]; then
  a=$(dd if="$B" bs=1 skip=$((0x8000)) count=3072 2>/dev/null | md5sum)
  b=$(dd if="$ROOT/SomudTick_v$PREV/SomudTick/SomudTick_merged.bin" bs=1 skip=$((0x8000)) count=3072 2>/dev/null | md5sum)
  [ "$a" = "$b" ] && ok "partition table same as v$PREV (logs stay)" || fail "PARTITION TABLE CHANGED from v$PREV: flashing loses the logs unless planned"
  size=$(stat -c %s "$B"); [ "$size" -lt 8000000 ] && ok "bin is not padded ($size bytes, flash at 0x0)" || fail "bin is $size bytes: padded to 16 MB would overwrite the logs"
else warn "partition check skipped (no bin, or no previous version found)"; fi

# 7. preview pictures
ls "$SK"/preview_v$V*.png >/dev/null 2>&1 && ok "preview pictures: $(cd "$SK" && ls preview_v$V*.png | tr '\n' ' ')" || fail "no preview_v$V*.png"

# 8. zip
if [ -n "$OUTZ" ]; then
  mkdir -p "$OUTZ"; Z="$OUTZ/SomudTick_v$V.zip"; rm -f "$Z"
  (cd "$ROOT" && zip -qr "$Z" "SomudTick_v$V") && ok "zip: $Z ($(du -h "$Z" | cut -f1))" || fail "zip failed"
fi

[ $bad = 0 ] && echo "RELEASE READY" || echo "NOT READY"
exit $bad
