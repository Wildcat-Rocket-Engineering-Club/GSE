@echo off
setlocal
pushd "%~dp0"

set "FROM_LAUNCHER=0"
if /I "%~1"=="/from-launcher" set "FROM_LAUNCHER=1"

:: Request Administrator privileges so the Digi driver can be installed.
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if errorlevel 1 goto :request_admin

set VENV_NAME=venv
set DRIVER_URL=https://digi.com
set DRIVER_EXE=%~dp0Digi_USB_RF_Drivers.exe

:: Create the virtual environment.
if not exist "%VENV_NAME%\" (
    echo Creating virtual environment...
    python -m venv %VENV_NAME%
    if errorlevel 1 goto :failed
)

:: Install the pinned Python dependencies.
echo Installing packages...
%VENV_NAME%\Scripts\python.exe -m pip install --upgrade pip
if errorlevel 1 goto :failed
%VENV_NAME%\Scripts\python.exe -m pip install --disable-pip-version-check -r requirements.txt
if errorlevel 1 goto :failed

:: Check for Digi drivers in the Windows Driver Store.
echo Checking for Digi USB RF Drivers in Driver Store...
pnputil /enum-drivers | findstr /I "Digi" >nul
if not errorlevel 1 (
    echo Digi Drivers detected in System Driver Store. Skipping.
) else (
    echo Drivers not detected.
    if not exist "%DRIVER_EXE%" (
        echo Downloading Digi USB RF Drivers...
        powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%DRIVER_URL%' -OutFile '%DRIVER_EXE%'"
        if errorlevel 1 goto :failed
    )

    echo Running installer from: %DRIVER_EXE%
    start /wait "" "%DRIVER_EXE%"
    if errorlevel 1 goto :failed
    echo Driver installation attempt finished.
)

echo.
echo Setup complete!
popd
if "%FROM_LAUNCHER%"=="0" pause
exit /b 0

:request_admin
echo Requesting administrative privileges...
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $p = Start-Process -FilePath '%~f0' -ArgumentList '%~1' -Verb RunAs -Wait -PassThru -ErrorAction Stop; exit $p.ExitCode } catch { Write-Error 'Setup was canceled or could not be elevated.'; exit 1 }"
set "SETUP_EXIT=%ERRORLEVEL%"
popd
exit /b %SETUP_EXIT%

:failed
echo.
echo Setup failed. Check the error above.
popd
if "%FROM_LAUNCHER%"=="0" pause
exit /b 1
