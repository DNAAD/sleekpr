@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0rebuild.ps1" %*
