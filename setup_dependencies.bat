@echo off
setlocal
cd /d "%~dp0"

echo ============================================================
echo CDP Experiment - Dependency Setup
echo ============================================================
echo.
echo This downloads the official 64-bit Windows CSPICE toolkit and
echo the NAIF kernel files used by the experiment.
echo.

powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup_dependencies.ps1"

if errorlevel 1 (
    echo.
    echo Setup failed. Read the error above.
    pause
    exit /b 1
)

echo.
echo Setup finished successfully.
echo You can now open CDP-Experiment.slnx in Visual Studio.
pause
