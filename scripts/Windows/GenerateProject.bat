@echo off
echo Generating project files with Premake...
python "%~dp0\..\Base\GenerateProject.py" vs2026
set "RESULT=%ERRORLEVEL%"

if not "%RESULT%"=="0" (
    echo Failed to generate project files.
    pause
    exit /b %RESULT%
)

echo Project files generated successfully.
pause
exit /b 0