@echo off
chcp 65001 >nul
setlocal
REM ============================================================
REM  SomudTick converter
REM  Drag photos or videos onto this file.
REM  Output goes to the folder "SD_card" next to this file.
REM  Copy everything inside "SD_card" to the microSD card.
REM ============================================================

where ffmpeg >nul 2>nul
if errorlevel 1 (
  echo.
  echo  ffmpeg is not installed yet.
  echo  1. Open "PowerShell"
  echo  2. Type:  winget install Gyan.FFmpeg
  echo  3. Close PowerShell, then drag your files here again.
  echo.
  pause
  exit /b
)

if "%~1"=="" (
  echo.
  echo  Drag photos or videos onto this file.
  echo.
  pause
  exit /b
)

set "OUT=%~dp0SD_card"
if not exist "%OUT%\videos" mkdir "%OUT%\videos"
if not exist "%OUT%\photos" mkdir "%OUT%\photos"

REM fit inside 320x240 (wide) or 240x320 (tall), keep the shape
set "FIT=scale=w='if(gt(iw,ih),320,240)':h='if(gt(iw,ih),240,320)':force_original_aspect_ratio=decrease"

:next
if "%~1"=="" goto done
set "EXT=%~x1"
for %%E in (.mp4 .MP4 .mov .MOV .m4v .M4V .avi .AVI .mkv .MKV .3gp .3GP .webm .WEBM) do if "%EXT%"=="%%E" goto video
for %%E in (.jpg .JPG .jpeg .JPEG .png .PNG .bmp .BMP .webp .WEBP .heic .HEIC) do if "%EXT%"=="%%E" goto photo
echo  Skip (not a photo or video): %~nx1
goto skip

:video
echo.
echo  Video: %~nx1
ffmpeg -hide_banner -loglevel error -y -i "%~1" -vf "%FIT%,fps=15" -pix_fmt yuvj420p -q:v 9 -an -f mjpeg "%OUT%\videos\%~n1.mjpeg"
ffmpeg -hide_banner -loglevel quiet -y -i "%~1" -vn -ac 1 -ar 16000 -f s16le "%OUT%\videos\%~n1.pcm"
if exist "%OUT%\videos\%~n1.mjpeg" (echo    OK) else (echo    FAILED)
goto skip

:photo
echo  Photo: %~nx1
ffmpeg -hide_banner -loglevel error -y -i "%~1" -vf "%FIT%" -pix_fmt yuvj420p -q:v 3 -frames:v 1 "%OUT%\photos\%~n1.jpg"
if exist "%OUT%\photos\%~n1.jpg" (echo    OK) else (echo    FAILED - for iPhone HEIC photos, set Camera - Formats - Most Compatible)
goto skip

:skip
shift
goto next

:done
echo.
echo  Done! Copy the "photos" and "videos" folders
echo  from "%OUT%" to the microSD card.
echo.
start "" "%OUT%"
pause
