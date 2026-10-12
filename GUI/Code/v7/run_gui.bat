@echo off
setlocal
pushd "%~dp0"

set "VENV_PYTHON=venv\Scripts\python.exe"

if not exist "%VENV_PYTHON%" goto :setup_prompt
"%VENV_PYTHON%" -c "import flask, flask_socketio, serial" >nul 2>&1
if errorlevel 1 goto :setup_prompt
goto :launch

:setup_prompt
echo GUI v7 dependencies are missing or incomplete.
choice /C YN /M "Run the Windows dependency and device-driver installer now"
if errorlevel 2 goto :setup_declined

call "windows_setup_venv.bat" /from-launcher
if errorlevel 1 goto :failed

:verify_setup
if not exist "%VENV_PYTHON%" goto :failed
"%VENV_PYTHON%" -c "import flask, flask_socketio, serial" >nul 2>&1
if errorlevel 1 goto :failed

:launch
echo Starting GUI v7...
start "GSE GUI v7" cmd /k ""%CD%\%VENV_PYTHON%" app.py"
timeout /t 2 /nobreak >nul
start "" "http://127.0.0.1:5001"
popd
exit /b 0

:setup_declined
echo.
echo Setup was not run. Run this file again and choose Y to install the dependencies.
popd
pause
exit /b 1

:failed
echo.
echo GUI v7 setup could not complete. Check the error above and confirm Python 3 is installed.
popd
pause
exit /b 1
