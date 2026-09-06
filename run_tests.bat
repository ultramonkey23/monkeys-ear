@echo off
setlocal

echo =======================================================
echo   RUNNING MONKEY'S EAR DETERMINISTIC DSP TESTS ^& BENCHMARK
echo =======================================================

set "TOOLCHAIN_BIN=C:\Users\harin\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
set "PATH=%TOOLCHAIN_BIN%;%PATH%"

if not exist build\monkeys_ear_test_runner.exe (
    echo [INFO] Binaries not found, triggering build...
    call build.bat
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)

cd build
monkeys_ear_test_runner.exe
set "RUNNER_ERR=%ERRORLEVEL%"
cd ..

if %RUNNER_ERR% neq 0 (
    echo [FAIL] Deterministic test runner failed with code %RUNNER_ERR%
    exit /b %RUNNER_ERR%
)

echo [PASS] All Monkey's Ear audio and latency tests verified!
