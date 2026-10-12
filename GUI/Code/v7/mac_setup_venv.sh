#!/bin/bash
VENV_NAME="venv"

# 1. Create the virtual environment
if [ ! -d "$VENV_NAME" ]; then
    echo "Creating virtual environment..."
    python3 -m venv $VENV_NAME
fi

# 2. Upgrade pip and install requirements
echo "Installing packages..."
./$VENV_NAME/bin/pip install --upgrade pip
./$VENV_NAME/bin/pip install -r requirements.txt

echo -e "\nSetup complete! To use it, run: source $VENV_NAME/bin/activate"