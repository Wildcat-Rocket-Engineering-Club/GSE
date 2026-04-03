@echo off
:: 1. Request Administrator privileges
>nul 2>&1 "%SYSTEMROOT%\system32\cacls.exe" "%SYSTEMROOT%\system32\config\system"
if '%errorlevel%' NEQ '0' (
    echo Requesting administrative privileges...
    goto UACPrompt
) else ( goto gotAdmin )

:UACPrompt
    echo Set UAC = CreateObject^("Shell.Application"^) > "%temp%\getadmin.vbs"
    echo UAC.ShellExecute "%~s0", "", "", "runas", 1 >> "%temp%\getadmin.vbs"
    "%temp%\getadmin.vbs"
    exit /B

:gotAdmin
    if exist "%temp%\getadmin.vbs" ( del "%temp%\getadmin.vbs" )
    :: CRITICAL: Set the working directory to where the script is located
    pushd "%~dp0"

set VENV_NAME=venv
set DRIVER_URL=https://digi.com
set DRIVER_EXE=%~dp0Digi_USB_RF_Drivers.exe

:: 2. Create the virtual environment
if not exist "%VENV_NAME%\" (
    echo Creating virtual environment...
    python -m venv %VENV_NAME%
)

:: 3. Upgrade pip and install requirements
echo Installing packages...
%VENV_NAME%\Scripts\python.exe -m pip install --upgrade pip
%VENV_NAME%\Scripts\pip.exe install -r requirements.txt

:: 4. Check for Drivers using PnPUtil (Looks for the actual driver files)
echo Checking for Digi USB RF Drivers in Driver Store...
pnputil /enum-drivers | findstr /I "Digi" >nul
if %ERRORLEVEL% equ 0 (
    echo Digi Drivers detected in System Driver Store. Skipping.
) else (
    echo Drivers not detected.
    if not exist "%DRIVER_EXE%" (
        echo Downloading Digi USB RF Drivers...
        powershell -Command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest -Uri '%DRIVER_URL%' -OutFile '%DRIVER_EXE%'"
    )
    
    echo Running installer from: %DRIVER_EXE%
    :: Using absolute path to prevent "cannot execute" error
    start /wait "" "%DRIVER_EXE%"
    
    :: Optional: Clean up
    :: del "%DRIVER_EXE%"
    echo Driver installation attempt finished.
)

echo.
echo Setup complete!
pause
