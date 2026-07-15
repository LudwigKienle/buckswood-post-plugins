@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%\..\..") do set "PACKAGE_ROOT=%%~fI"
set "SOURCE_DIR=%PACKAGE_ROOT%\dist"
set "TARGET_DIR=%ProgramFiles%\Adobe\Common\Plug-ins\7.0\MediaCore\Buckswood"

echo Installing Buckswood Premiere plugins...
echo Source: %SOURCE_DIR%
echo Target: %TARGET_DIR%
echo.

for %%P in (BuckswoodAIPhotorealizer BuckswoodLensPhysics BuckswoodFakeDiagnostic BuckswoodFilmEmulation BuckswoodFrameDirector BuckswoodRadianceRecover BuckswoodTemporalIntegrity BuckswoodLookDNA) do (
    if not exist "%SOURCE_DIR%\%%P.prm" (
        echo Missing %SOURCE_DIR%\%%P.prm
        pause
        exit /b 1
    )
)

if not exist "%TARGET_DIR%" (
    mkdir "%TARGET_DIR%"
)

for %%P in (BuckswoodAIPhotorealizer BuckswoodLensPhysics BuckswoodFakeDiagnostic BuckswoodFilmEmulation BuckswoodFrameDirector BuckswoodRadianceRecover BuckswoodTemporalIntegrity BuckswoodLookDNA) do (
    copy /Y "%SOURCE_DIR%\%%P.prm" "%TARGET_DIR%\%%P.prm"
    if errorlevel 1 (
        echo.
        echo Install failed. Run this .bat as Administrator, or copy the .prm files manually.
        pause
        exit /b 1
    )
)

echo.
echo Done. Restart Premiere Pro, then search for "Buckswood" in Effects.
pause
