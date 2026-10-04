@echo off
echo Running Candle Profile Script...
python "%~dp0\..\Base\Profile.py" %*
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo Profile script failed with exit code %RESULT%.
    pause
    exit /b %RESULT%
)

exit /b 0
