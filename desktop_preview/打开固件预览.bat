@echo off
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
start "Paper S3 Preview" ".pio\build\preview_final\native\program.exe" --scale=1.00
