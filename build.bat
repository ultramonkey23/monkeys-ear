@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo   BUILDING MONKEY'S EAR // PRODUCTION INSTRUMENT
echo =======================================================

rem Retain the existing WinLibs setup without hardcoding the original
rem developer's Windows username. TOOLCHAIN_BIN may be set by the caller.
if not defined TOOLCHAIN_BIN set "TOOLCHAIN_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if exist "%TOOLCHAIN_BIN%\g++.exe" (
    set "PATH=%TOOLCHAIN_BIN%;%PATH%"
    set "CXX_COMPILER=%TOOLCHAIN_BIN%\g++.exe"
) else (
    where g++.exe >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] No GCC C++ compiler found. Install WinLibs or set TOOLCHAIN_BIN.
        exit /b 2
    )
    set "CXX_COMPILER=g++.exe"
)
where ninja.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Ninja is required for this build. Install it and retry.
    exit /b 2
)
if not exist build mkdir build

echo [CMAKE] Configuring with Ninja and !CXX_COMPILER! ...
cmake -G "Ninja" -DCMAKE_CXX_COMPILER="!CXX_COMPILER!" -B build -S .
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed!
    exit /b %ERRORLEVEL%
)

echo [NINJA] Compiling core, test runner, and standalone VST3 modules...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo [SUCCESS] Monkey's Ear build completed!
echo   - Core Library:  build\libmonkeys_ear_core.a
echo   - Test Runner:   build\monkeys_ear_test_runner.exe
echo   - Full VST3:      build\monkeys_ear.vst3
echo   - Vocal VST3:     build\monkeys_ear_vocal.vst3
echo =======================================================
