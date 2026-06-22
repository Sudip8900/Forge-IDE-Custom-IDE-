@echo off
setlocal enabledelayedexpansion

echo ==========================================
echo code-ZEN MSVC Compilation Bootstrap (Ninja)
echo ==========================================

:: 1. Add Visual Studio's CMake and Ninja to the PATH
set "VS_CMAKE_DIR=D:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
set "VS_NINJA_DIR=D:\Visual Studio\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"

if exist "!VS_CMAKE_DIR!" (
    set "PATH=!VS_CMAKE_DIR!;!PATH!"
    echo Found VS CMake, added to path.
) else (
    echo WARNING: VS CMake path not found. Proceeding with system path.
)

if exist "!VS_NINJA_DIR!" (
    set "PATH=!VS_NINJA_DIR!;!PATH!"
    echo Found VS Ninja, added to path.
) else (
    echo WARNING: VS Ninja path not found. Proceeding with system path.
)

:: 2. Verify CMake availability
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo FATAL ERROR: CMake not found on PATH.
    exit /b 1
)

:: 3. Configure MSVC Compiler Variables (vcvarsall.bat)
set "VCVARS_PATH=D:\Visual Studio\VC\Auxiliary\Build\vcvarsall.bat"
if exist "!VCVARS_PATH!" (
    echo Configuring 64-bit compiler shell environment...
    call "!VCVARS_PATH!" x64
) else (
    echo ERROR: vcvarsall.bat compiler script not found. Compilation might fail.
)

:: 4. Create build directory
if not exist "build" (
    mkdir build
)

:: 5. Run CMake Configuration with Ninja generator
echo Configuring project using CMake and Ninja...
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_UPDATES_DISCONNECTED=ON -S . -B build
if %errorlevel% neq 0 (
    echo FATAL ERROR: CMake configuration failed.
    exit /b %errorlevel%
)

:: Copy compilation database to project root for editor IntelliSense/language-servers
if exist "build\compile_commands.json" (
    copy /Y build\compile_commands.json . >nul
    echo Copied compile_commands.json to workspace root.
)

:: 6. Build the application binary
echo Compiling code-ZEN executable in Release mode using Ninja...
cmake --build build --parallel
if %errorlevel% neq 0 (
    echo FATAL ERROR: Compiler failed to build code-ZEN binary.
    exit /b %errorlevel%
)

:: Ensure build\Release directory exists and copy the executable there for compatibility
if exist "build\code-ZEN.exe" (
    if not exist "build\Release" (
        mkdir build\Release
    )
    copy /Y build\code-ZEN.exe build\Release\code-ZEN.exe >nul
)

echo ==========================================
echo BUILD SUCCESSFUL!
echo Binary path: build\Release\code-ZEN.exe
echo ==========================================
exit /b 0
