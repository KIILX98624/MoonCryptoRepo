
@echo off
title MoonCrypto Launcher
chcp 65001 >nul

cls

echo ==========================
echo      MOONCRYPTO LAUNCHER
echo ==========================

pause

echo.
echo [1/3] Executando Python...
python "1ainput.py"

echo.
echo [2/3] Executando MoonCrypto...
mooncyt "3main.mncyt"

pause

echo.
echo [3/3] Executando outro programa...
python "5printer.py"

echo.
