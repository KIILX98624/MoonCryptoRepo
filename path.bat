@echo off
title MonC Installer

echo ============================
echo       MonC INSTALLER
echo ============================

:: Criar pasta de instalacao
if not exist "C:\Program\MonC" (
    mkdir "C:\Program\MonC"
)

:: Copiar compilador
copy "%~dp0MonCWin10_x64.exe" "C:\Program\MonC\MonCWin10_x64.exe" /Y

:: Adicionar ao PATH do sistema
setx /M PATH "%PATH%;C:\Program\MonC"

echo.
echo MonC instalada com sucesso!
echo.
pause