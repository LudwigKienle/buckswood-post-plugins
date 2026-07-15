@echo off
setlocal

set "ROOT_DIR=%~dp0.."
set "VENV_DIR=%ROOT_DIR%\.ml-venv"

where py >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set "PYTHON=py -3"
) else (
    set "PYTHON=python"
)

%PYTHON% -m venv "%VENV_DIR%"
if %ERRORLEVEL% NEQ 0 exit /b 1

"%VENV_DIR%\Scripts\python.exe" -m pip install --upgrade pip wheel
"%VENV_DIR%\Scripts\python.exe" -m pip install "numpy>=1.26,<3" "Pillow>=11,<13" "torch>=2.2,<3"
if %ERRORLEVEL% NEQ 0 exit /b 1

echo.
echo Buckswood ML companion is ready:
echo %VENV_DIR%\Scripts\python.exe
echo.
pause
