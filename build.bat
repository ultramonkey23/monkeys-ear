@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   BUILDING MONKEY'S EAR // PRODUCTION INSTRUMENT
echo =======================================================

set "TOOLCHAIN_BIN=C:\Users\harin\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
set "PATH=%TOOLCHAIN_BIN%;%PATH%"

if not exist build mkdir build

echo [CMAKE] Configuring with Ninja and GCC 14.2...
cmake -G "Ninja" -DCMAKE_CXX_COMPILER="%TOOLCHAIN_BIN%\g++.exe" -DCMAKE_C_COMPILER="%TOOLCHAIN_BIN%\gcc.exe" -B build -S .
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo [NINJA] Compiling core, test runner, and VST3 plugin...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Monkey's Ear build completed!
echo   - Core Library:  build\libmonkeys_ear_core.a
echo   - Test Runner:   build\monkeys_ear_test_runner.exe
echo   - VST3 Plugin:   build\monkeys_ear.vst3
echo =======================================================
