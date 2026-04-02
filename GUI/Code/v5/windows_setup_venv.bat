@echo off
set VENV_NAME=venv

:: 1. Create the virtual environment if it doesn't exist
if not exist "%VENV_NAME%\" (
    echo Creating virtual environment...
    python -m venv %VENV_NAME%
)

:: 2. Upgrade pip and install requirements
echo Installing packages from requirements.txt...
%VENV_NAME%\Scripts\python.exe -m pip install --upgrade pip
%VENV_NAME%\Scripts\pip.exe install -r requirements.txt

echo.
echo Setup complete! To use it, type: %VENV_NAME%\Scripts\activate
pause