
@echo off
title MonC - Environment Verification
setlocal

echo ========================================
echo       MonC - ENVIRONMENT VERIFICATION
echo ========================================
echo.

set "MSYS_MISSING=0"
set "PYTHON_MISSING=0"

:: ========================================
:: MSYS2 / C++
:: ========================================

echo [1/2] Checking MSYS2 / C++...

where g++ >nul 2>&1

if %errorlevel% equ 0 (
    echo [OK] G++ compiler found!
    g++ --version | findstr /i "g++"
) else (
    echo [ERROR] G++ compiler not found!
    set "MSYS_MISSING=1"
)

echo.

:: ========================================
:: Python 3.13
:: ========================================

echo [2/2] Checking Python 3.13...

py -3.13 --version >nul 2>&1

if %errorlevel% equ 0 (
    echo [OK] Python 3.13 found!
    py -3.13 --version
) else (
    echo [ERROR] Python 3.13 not found!
    set "PYTHON_MISSING=1"
)

echo.
echo ========================================
echo          MISSING DEPENDENCIES
echo ========================================
echo.

:: ========================================
:: Download MSYS2
:: ========================================

if "%MSYS_MISSING%"=="1" (
    echo MSYS2 / G++ is missing.
    set /p "DOWNLOAD_MSYS=Do you want to download MSYS2? (Y/N): "

    if /i "%DOWNLOAD_MSYS%"=="Y" (
        start "" "https://www.msys2.org/"
    )
    echo.
)

:: ========================================
:: Download Python
:: ========================================

if "%PYTHON_MISSING%"=="1" (
    echo Python 3.13 is missing.
    set /p "DOWNLOAD_PYTHON=Do you want to download Python 3.13? (Y/N): "

    if /i "%DOWNLOAD_PYTHON%"=="Y" (
        start "" "https://www.python.org/downloads/release/python-3130/"
    )
    echo.
)

:: ========================================
:: Final Result
:: ========================================

echo ========================================
echo              FINAL RESULT
echo ========================================

if "%MSYS_MISSING%"=="0" if "%PYTHON_MISSING%"=="0" (
    echo [SUCCESS] All dependencies are installed!
) else (
    echo [WARNING] Some dependencies are missing.
)

echo ========================================

pause
