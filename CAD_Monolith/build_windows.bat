@echo off
REM ============================================================================
REM CAD MONOLITH - Windows Build Script
REM Following Voss Protocol for Maximum Stability
REM ============================================================================

echo ============================================
echo   CAD MONOLITH - Build System
echo   Target: Windows x64
echo   Configuration: Release
echo ============================================
echo.

REM Check if Visual Studio is available
where cl >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: Visual Studio compiler (cl.exe) not found in PATH
    echo.
    echo Please run this script from the "Developer Command Prompt for VS"
    echo or ensure Visual Studio 2019/2022 is installed with C++ workload.
    echo.
    pause
    exit /b 1
)

echo [1/5] Checking compiler version...
cl /Bv
echo.

REM Create build directories
echo [2/5] Creating build directories...
if not exist "build" mkdir build
if not exist "build\bin" mkdir build\bin
if not exist "build\obj" mkdir build\obj
if not exist "recovery" mkdir recovery
echo.

REM Set compiler flags for maximum safety
set CXXFLAGS=/std:c++20 /O2 /W4 /WX /permissive- /Zc:__cplusplus
set CXXFLAGS=%CXXFLAGS% /DNOMINMAX /DUNICODE /D_UNICODE
set CXXFLAGS=%CXXFLAGS% /EHsc /MD

REM Linker flags
set LDFLAGS=/OUT:build\bin\CadMonolith.exe
set LDFLAGS=%LDFLAGS% kernel32.lib user32.lib gdi32.lib opengl32.lib glu32.lib comctl32.lib
set LDFLAGS=%LDFLAGS% /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup

echo [3/5] Compiling Core modules...

REM Compile Core sources
cl %CXXFLAGS% /c /Fo:build\obj\Result.obj src\Core\Result.cpp
cl %CXXFLAGS% /c /Fo:build\obj\MemoryGuard.obj src\Core\MemoryGuard.cpp
cl %CXXFLAGS% /c /Fo:build\obj\SessionManager.obj src\Core\SessionManager.cpp
cl %CXXFLAGS% /c /Fo:build\obj\TransactionManager.obj src\Core\TransactionManager.cpp

echo [4/5] Compiling Geometry and Kernel modules...

REM Compile Geometry sources
cl %CXXFLAGS% /c /Fo:build\obj\BRepBody.obj src\Geometry\BRepBody.cpp
cl %CXXFLAGS% /c /Fo:build\obj\NURBS.obj src\Geometry\NURBS.cpp
cl %CXXFLAGS% /c /Fo:build\obj\ConstraintGraph.obj src\Geometry\ConstraintGraph.cpp
cl %CXXFLAGS% /c /Fo:build\obj\GeometrySanitizer.obj src\Geometry\GeometrySanitizer.cpp

REM Compile Kernel sources
cl %CXXFLAGS% /c /Fo:build\obj\HeartbeatMonitor.obj src\Threading\HeartbeatMonitor.cpp
cl %CXXFLAGS% /c /Fo:build\obj\GeometryKernelProxy.obj src\Kernel\GeometryKernelProxy.cpp
cl %CXXFLAGS% /c /Fo:build\obj\WorkerThread.obj src\Kernel\WorkerThread.cpp

REM Compile Persistence sources
cl %CXXFLAGS% /c /Fo:build\obj\PersistenceManager.obj src\Persistence\PersistenceManager.cpp
cl %CXXFLAGS% /c /Fo:build\obj\FileAtomizer.obj src\Persistence\FileAtomizer.cpp

REM Compile UI sources
cl %CXXFLAGS% /c /Fo:build\obj\MainWindow.obj src\UI\MainWindow.cpp
cl %CXXFLAGS% /c /Fo:build\obj\Viewport.obj src\UI\Viewport.cpp
cl %CXXFLAGS% /c /Fo:build\obj\AppController.obj src\UI\AppController.cpp

echo [5/5] Linking executable...

REM Collect all object files
set OBJS=^
build\obj\Result.obj ^
build\obj\MemoryGuard.obj ^
build\obj\SessionManager.obj ^
build\obj\TransactionManager.obj ^
build\obj\BRepBody.obj ^
build\obj\NURBS.obj ^
build\obj\ConstraintGraph.obj ^
build\obj\GeometrySanitizer.obj ^
build\obj\HeartbeatMonitor.obj ^
build\obj\GeometryKernelProxy.obj ^
build\obj\WorkerThread.obj ^
build\obj\PersistenceManager.obj ^
build\obj\FileAtomizer.obj ^
build\obj\MainWindow.obj ^
build\obj\Viewport.obj ^
build\obj\AppController.obj ^
src\main.cpp

REM Link
cl %OBJS% %LDFLAGS%

if %ERRORLEVEL% equ 0 (
    echo.
    echo ============================================
    echo   BUILD SUCCESSFUL!
    echo   Output: build\bin\CadMonolith.exe
    echo ============================================
    echo.
    echo To run the application:
    echo   cd build\bin
    echo   CadMonolith.exe
    echo.
) else (
    echo.
    echo ============================================
    echo   BUILD FAILED
    echo ============================================
    echo.
    pause
    exit /b 1
)

REM Cleanup object files (optional)
REM del build\obj\*.obj

pause
