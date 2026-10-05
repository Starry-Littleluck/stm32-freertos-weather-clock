@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
echo Available serial ports:
powershell -NoProfile -Command "[System.IO.Ports.SerialPort]::GetPortNames()"
set /p "COM_PORT=Serial port (for example COM3 or 3): "
set /p "FONT_FILE=Font file path, leave empty to select from the menu: "

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%upload_fonts.ps1" -Port "%COM_PORT%" -FontFile "%FONT_FILE%"
if errorlevel 1 goto download_failed
echo Font download completed.
goto download_end

:download_failed
echo Font download failed.

:download_end
pause
endlocal
