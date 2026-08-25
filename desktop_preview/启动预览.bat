@echo off
cd /d "%~dp0"
set "PATH=C:\msys64\ucrt64\bin;%PATH%"
start "Paper S3 Preview" "PaperS3Preview.exe" --scale=1.00
