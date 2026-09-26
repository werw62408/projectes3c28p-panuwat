---
name: somudtick-release
description: Packages and ships a new SomudTick firmware version for the ES3C28P board. Covers the new SomudTick_vX folder, the About page version, the Thai README ("ของใหม่ใน vX" with ✅/❌ checks and glossary), simulator tests, the merged .bin, the partition-table check, preview pictures, zip, commit and push. Use it every time a SomudTick version is finished or about to be handed over. Triggers include "ปล่อยเวอร์ชัน", "ส่งงาน", "ทำ v11.5", "release", "ship", "/somudtick-release", and the end of any coding round on SomudTick, even if the user doesn't say "release".
---

# SomudTick release

The owner flashes each version to the board they carry every day. Anything missed costs them a flash, a test on the real board, and a message back: a wrong version on the About page, a README without checks, or a partition change that wipes their logs. This skill makes every hand-over the same, so nothing gets missed.

Read `CLAUDE.md` at the project root first. It has the language rules (Thai to the owner, English on screen and in code, Thai README), the pocket-touch rules and the pins.

## Before you start
- The owner must have said **"เริ่ม"** for this round of work. This skill packages finished work. It does not replace the talk-first rule.
- The tools come from the session hook (`.claude/hooks/session-start.sh`). If `arduino-cli` or `tools/sim/deps` is missing, run the hook script: `CLAUDE_CODE_REMOTE=true .claude/hooks/session-start.sh`.

## Steps

### 1. New folder (at the start of the coding round)
- `cp -r SomudTick_v<prev> SomudTick_v<new>`
- Delete the old `SomudTick_merged.bin` and `preview_v<prev>*.png` in the new folder, so stale files can't be shipped.
- Never edit an older version's folder. The owner may need to flash back to it.

### 2. Version on the About page
- In `screen_settings.h`, set `line("Version", "v<new>", false);`
- v11.3 shipped still saying v11.2. The check script catches this.

### 3. Tests in the simulator
- Add `tools/sim/test_v<new>.cpp` with one PASS/FAIL line per new behaviour and per bug fixed.
- Copy the pattern of `test_v114.cpp`: helpers `push`, `press`, `run`, `sweep`.
- Test what the owner would do with a finger or the stick:
  - a real sequence of pushes and presses
  - the screen-off (pocket) case
  - both screen directions when layout changed
- For network data (news, weather), retry the download inside the test. The live internet sometimes fails.
- If the phone web page changed, run `node tools/web/test_video.js SomudTick_v<new>/SomudTick/webpage.h`, or write a similar Playwright check.
- Screenshots:
  - take one of every changed screen and look at each image yourself
  - clipped or overflowing text is a bug; shorten the English text
  - keep the good ones for the preview

### 4. README (Thai)
- Title line: `# SomudTick (สมุดติ๊ก) v<new>: firmware สำหรับบอร์ด ES3C28P`
- New section `## ของใหม่ใน v<new> (...)` right under the title, above older sections.
- Start the section with the flashing note:
  - `SomudTick_merged.bin` at `0x0`
  - data is kept because the partition table is unchanged
- One `###` part per change. Each part has:
  - what the owner will see
  - **✅ เช็ค:** exact steps on the real board, with what should happen
  - **❌** what to do if it doesn't happen
- Last part:
  - which files changed and why
  - how many simulator tests passed (new / total)
  - what can only be tested on the real board
- Glossary (`## ตารางคำศัพท์`):
  - add every new on-screen English phrase with its Thai meaning and `(v<new>)`
  - update renamed ones and note the old name
- Fix references to files that are not in the new folder, such as old preview names.

### 5. Preview pictures
- Build one sheet from the screenshots with `python3 tools/sim/sheet.py SomudTick_v<new>/SomudTick/preview_v<new>.png 4 <pngs...>`.
- Use clear file names so the labels explain themselves, such as `1_weather_scroll.png`.

### 6. Build, check, zip
- Run the check script. It rebuilds the firmware and checks everything above:
  ```
  .claude/skills/somudtick-release/scripts/release_check.sh <new> <scratchpad folder>
  ```
- Every line must be PASS and the last line must be `RELEASE READY`.
- Fix any FAIL and run it again.
- **A partition-table FAIL is serious.** The owner's logs live in a partition, so do not ship. The exception is a change that was planned and discussed, with a backup-and-restore plan.

### 7. Commit and push
- Stage only the new version folder and the test and tool files you touched.
- Commit with author `werw62408 <kinashyy@gmail.com>`:
  ```
  git -c user.name=werw62408 -c user.email=kinashyy@gmail.com commit
  ```
- Message:
  - first line: `Add SomudTick v<new>: <main changes>`
  - body: a short bullet list
  - end with the `Co-Authored-By:` and `Claude-Session:` lines from the session's attribution instructions
- Push with `git push -u origin somudtick-v8-relax-games`. If the network fails, retry up to 4 times with waits of 2, 4, 8 and 16 seconds.

### 8. Hand over (in Thai)
- Send the zip and the preview picture with the file tool.
- Write a short summary:
  - what changed
  - bugs found during testing
  - test counts
  - a numbered list of what to try on the real board
- End by asking how it went. Then wait: the next round starts with talk, not code.

## Why the partition table matters
The flash holds the app (3 MB, `huge_app` scheme) and a 12 MB `logs` LittleFS partition with years of the owner's records.

The merged bin is written from `0x0` and **not padded to 16 MB**, so flashing never touches the logs. If the partition table changes, the logs partition can move and the data reads as empty.

That is why the script compares the 3 KB at `0x8000` with the previous version.
