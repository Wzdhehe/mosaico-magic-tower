@echo off
setlocal
rem Install magic_tower only, without touching the Vibe Mode base firmware.
rem Use this when the device is already in Vibe Mode with Iris reachable.

set MSYSTEM=
set MSYSTEM_PREFIX=
set SHELL=
set PYTHONUTF8=1
set IDF_TOOLS_PATH=D:\espbulid\.espressif
set IDF_PATH=D:\espbulid\esp-idf
set MOSAICO_IDF_PYTHON=D:\espbulid\.espressif\python_env\idf6.2_py3.13_env\Scripts\python.exe
cd /d "%~dp0..\..\.."

echo.
echo Installing magic_tower via system-update (Vibe Mode is preserved)...
python mosaico.py iris system-update --project projects/magic_tower
exit /b %ERRORLEVEL%
