@echo off
setlocal
set MSYSTEM=
set MSYSTEM_PREFIX=
set SHELL=
set PYTHONUTF8=1
set IDF_TOOLS_PATH=D:\espbulid\.espressif
set IDF_PATH=D:\espbulid\esp-idf
set MOSAICO_IDF_PYTHON=D:\espbulid\.espressif\python_env\idf6.2_py3.13_env\Scripts\python.exe
cd /d "%~dp0..\..\.."
python mosaico.py game build --project projects/magic_tower
exit /b %ERRORLEVEL%
