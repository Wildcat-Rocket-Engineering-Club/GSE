@echo off
setlocal
pushd "%~dp0"

if not exist "venv\Scripts\activate.bat" (
    echo Creating the GUI v7 virtual environment...
    python -m venv venv
    if errorlevel 1 goto :failed
)

call "venv\Scripts\activate.bat"
if errorlevel 1 goto :failed

echo Checking and installing GUI v7 dependencies...
python -m pip install --disable-pip-version-check -r requirements.txt
if errorlevel 1 goto :failed

python -m pip check
if errorlevel 1 goto :failed

echo Starting GUI v7...
start "GSE GUI v7" cmd /k ""%CD%\venv\Scripts\python.exe" app.py"
timeout /t 2 /nobreak >nul
start "" "http://127.0.0.1:5001"
popd
exit /b 0

:failed
echo.
echo GUI v7 could not start. Check the error above and confirm Python is installed.
popd
pause
exit /b 1
