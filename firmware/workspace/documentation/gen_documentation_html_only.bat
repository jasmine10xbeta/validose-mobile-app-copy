@echo off
REM Check if Doxyfile exists
if not exist "Doxyfile" (
    echo Doxyfile not found in the current directory.
    pause
    exit /b 1
)

REM Run Doxygen with the specified Doxyfile
echo Running Doxygen...
doxygen Doxyfile
if %errorlevel% neq 0 (
    echo Doxygen encountered an error.
    pause
    exit /b %errorlevel%
)

REM Check if index.html exists
if not exist "docs\html\index.html" (
    echo Documentation generation failed or index.html not found.
    pause
    exit /b 1
)

REM Open the index.html file
echo Opening generated documentation...
start "" "docs\html\index.html"

echo Done!
exit /b 0
