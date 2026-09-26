@echo off
echo Running Candle Build Script...
python "%~dp0\..\Base\Build.py" %*
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo Build script failed with exit code %RESULT% ^(build or test^).
    pause
    exit /b %RESULT%
)

echo Build script commands completed successfully.
pause
exit /b 0
