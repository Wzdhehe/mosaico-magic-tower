@echo off
setlocal
if /i "%~1"=="--no-pause" set "NOPAUSE=1"
rem One-click official install for magic_tower.
rem Prerequisite: device is in ROM Download Mode (COM5 / VID_303A PID_0020).
rem This replaces the current ESP-Claw firmware. A full ESP-Claw backup exists at
rem D:\espbulid\espclaw_backup\ if you need to restore it later.

set MSYSTEM=
set MSYSTEM_PREFIX=
set SHELL=
set PYTHONUTF8=1
set IDF_TOOLS_PATH=D:\espbulid\.espressif
set IDF_PATH=D:\espbulid\esp-idf
set MOSAICO_IDF_PYTHON=D:\espbulid\.espressif\python_env\idf6.2_py3.13_env\Scripts\python.exe
cd /d "%~dp0..\..\.."

echo.
echo [1/2] Writing Mosaico Vibe Mode base firmware (recover)...
rem Do not pass --hardware-mac here: that makes recover run an extra esptool
rem read-mac probe first, and this board allows only one esptool session per
rem power cycle. With a single registered ROM port recover selects COM5 directly
rem and the flash write is the first (and only) esptool session.
python mosaico.py recover --project projects/magic_tower --timeout 600
if errorlevel 1 goto fail

echo.
echo [2/2] Installing magic_tower via system-update...
python mosaico.py iris system-update --project projects/magic_tower
if errorlevel 1 goto fail

echo.
echo Done. Device should now run magic_tower.
echo Verify with:
echo   python mosaico.py iris screenshot D:\magic_tower.png --project projects/magic_tower --json
if not defined NOPAUSE pause
exit /b 0

:fail
echo.
echo FAILED. Read the messages above. Do not unplug or erase flash.
echo The device may still be in ROM or Vibe Mode; report the last lines.
if not defined NOPAUSE pause
exit /b 1
