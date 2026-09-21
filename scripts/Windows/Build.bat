@echo off
echo Running Candle Build Script...
python "%~dp0\..\Base\Build.py" %*
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo Build failed.
    pause
    exit /b %RESULT%
)

echo Build script commands completed successfully.
pause
exit /b 0
