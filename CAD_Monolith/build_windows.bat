@echo off
setlocal EnableDelayedExpansion

color 0A
cls

echo.
echo ========================================================================
echo                    CAD MONOLITH - Build Script
echo                         Windows x64 Release
echo ========================================================================
echo.

REM Check if running in Developer Command Prompt
if "%VisualStudioVersion%"=="" (
    echo [ERROR] Visual Studio environment NOT detected!
    echo.
    echo This script MUST be run from:
    echo   "Developer Command Prompt for VS 2022"
    echo.
    echo How to open it:
    echo   1. Press Windows Key
    echo   2. Type "Developer Command"
    echo   3. Click "Developer Command Prompt for VS 2022"
    echo   4. Drag this build_windows.bat file into that window
    echo   5. Press Enter
    echo.
    echo If you don't have Visual Studio 2022:
    echo   1. Download: https://visualstudio.microsoft.com/downloads/
    echo   2. Install "Desktop development with C++" workload
    echo.
    pause
    exit /b 1
)

echo [OK] Running in Developer Command Prompt
echo [OK] Visual Studio Version: %VisualStudioVersion%
echo.

REM Try auto-detect and load VS environment if not fully loaded
where cl >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [INFO] Compiler not in PATH, attempting auto-detection...
    
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
    if not exist "%VS_PATH%" set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    
    if exist "%VS_PATH%" (
        echo [OK] Found Visual Studio: %VS_PATH%
        call "%VS_PATH%"
    ) else (
        echo [ERROR] Could not locate Visual Studio 2022 installation.
        goto :FAILED
    )
)

echo.
echo ========================================================================
echo                         Starting Build Process
echo ========================================================================
echo.

REM Create directories
echo [1/6] Creating build directories...
if not exist "build" mkdir build
if not exist "build\bin" mkdir build\bin
if not exist "build\obj" mkdir build\obj
if not exist "recovery" mkdir recovery
echo [OK] Directories created
echo.

REM Check CMake
echo [2/6] Checking CMake installation...
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake is not installed or not in PATH!
    echo.
    echo Install CMake from: https://cmake.org/download/
    echo During installation, check "Add CMake to system PATH"
    goto :FAILED
)
cmake --version | findstr /C:"cmake version"
echo [OK] CMake found
echo.

REM Configure
echo [3/6] Configuring project with CMake...
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] CMake configuration failed!
    echo.
    echo Common causes:
    echo   - Missing "Desktop development with C++" workload in VS
    echo   - CMake version too old (need 3.16+)
    echo   - Permission denied creating files
    goto :FAILED
)
echo [OK] CMake configuration successful
echo.
cd ..

REM Build
echo [4/6] Building Release configuration...
cd build
cmake --build . --config Release --verbose
if %ERRORLEVEL% neq 0 (
    echo.
    echo [ERROR] Build compilation failed!
    echo Check the error messages above for details.
    cd ..
    goto :FAILED
)
cd ..
echo [OK] Build compilation successful
echo.

REM Verify executable
echo [5/6] Verifying executable...
if exist "build\bin\CadMonolith.exe" (
    echo [OK] Executable created successfully!
    for %%I in ("build\bin\CadMonolith.exe") do echo [OK] Size: %%~zI bytes
) else (
    echo [WARNING] Executable not found in expected location.
    echo Searching...
    dir /s /b build\*.exe 2>nul
)
echo.

REM Copy recovery folder
echo [6/6] Setting up recovery directory...
if not exist "build\bin\recovery" mkdir "build\bin\recovery"
echo [OK] Recovery directory ready
echo.

echo ========================================================================
echo                      BUILD SUCCESSFUL!
echo ========================================================================
echo.
echo Executable: %CD%\build\bin\CadMonolith.exe
echo.
echo Next steps:
echo   1. Navigate to: build\bin
echo   2. Run: CadMonolith.exe
echo.
echo To create a distributable package:
echo   - Copy the entire 'build\bin' folder
echo   - Include the 'recovery' subfolder
echo   - Distribute as a portable application
echo.
goto :END

:FAILED
echo.
echo ========================================================================
echo                         BUILD FAILED
echo ========================================================================
echo.
echo Troubleshooting steps:
echo   1. Ensure Visual Studio 2022 has "Desktop development with C++"
echo   2. Ensure CMake is installed and in PATH
echo   3. Run as Administrator if permission errors occur
echo   4. Check that no antivirus is blocking compilation
echo.
pause
exit /b 1

:END
echo Build script completed.
echo.
pause
