@echo off
echo Running Candle Shader Compile Script...
python "%~dp0\..\Base\CompileShaders.py" %*
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo Shader compilation failed with exit code %RESULT%.
    pause
    exit /b %RESULT%
)

echo Shaders compiled successfully.
pause
exit /b 0
