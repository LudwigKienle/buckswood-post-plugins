@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%\..\..") do set "PACKAGE_ROOT=%%~fI"
set "SOURCE_DIR=%PACKAGE_ROOT%\dist"
set "TARGET_DIR=%ProgramFiles%\Adobe\Common\Plugins\7.0\MediaCore\Buckswood"

echo Installing Buckswood Premiere plugins...
echo Source: %SOURCE_DIR%
echo Target: %TARGET_DIR%
echo.

if not exist "%SOURCE_DIR%\BuckswoodAIPhotorealizer.prm" (
    echo Missing %SOURCE_DIR%\BuckswoodAIPhotorealizer.prm
    pause
    exit /b 1
)

if not exist "%SOURCE_DIR%\BuckswoodLensPhysics.prm" (
    echo Missing %SOURCE_DIR%\BuckswoodLensPhysics.prm
    pause
    exit /b 1
)

if not exist "%TARGET_DIR%" (
    mkdir "%TARGET_DIR%"
)

copy /Y "%SOURCE_DIR%\BuckswoodAIPhotorealizer.prm" "%TARGET_DIR%\BuckswoodAIPhotorealizer.prm"
if errorlevel 1 (
    echo.
    echo Install failed. Run this .bat as Administrator, or copy the .prm files manually.
    pause
    exit /b 1
)

copy /Y "%SOURCE_DIR%\BuckswoodLensPhysics.prm" "%TARGET_DIR%\BuckswoodLensPhysics.prm"
if errorlevel 1 (
    echo.
    echo Install failed. Run this .bat as Administrator, or copy the .prm files manually.
    pause
    exit /b 1
)

echo.
echo Done. Restart Premiere Pro, then search for "Buckswood" in Effects.
pause
