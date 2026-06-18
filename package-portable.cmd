@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0package-portable.ps1" %*
