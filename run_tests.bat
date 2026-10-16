@echo off
setlocal

echo =======================================================
echo   RUNNING MONKEY'S EAR DETERMINISTIC DSP TESTS ^& BENCHMARK
echo =======================================================

set "TOOLCHAIN_BIN=C:\Users\harin\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
set "PATH=%TOOLCHAIN_BIN%;%PATH%"

if not exist build\monkeys_ear_test_runner.exe (
    echo [INFO] Binaries not found, triggering build...
    call .\build.bat
    if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%
)

cd build
.\monkeys_ear_test_runner.exe
set "RUNNER_ERR=%ERRORLEVEL%"
if %RUNNER_ERR% neq 0 (
    cd ..
    echo [FAIL] Deterministic test runner failed with code %RUNNER_ERR%
    exit /b %RUNNER_ERR%
)

if exist monkeys_ear_host_probe.exe (
    .\monkeys_ear_host_probe.exe
    if %ERRORLEVEL% neq 0 (
        cd ..
        echo [FAIL] VST3 host probe failed with code %ERRORLEVEL%
        exit /b %ERRORLEVEL%
    )
)

if exist monkeys_ear_vocal_host_probe.exe (
    .\monkeys_ear_vocal_host_probe.exe
    if %ERRORLEVEL% neq 0 (
        cd ..
        echo [FAIL] Vocal VST3 host probe failed with code %ERRORLEVEL%
        exit /b %ERRORLEVEL%
    )
)

if exist monkeys_ear_vocal_target_chain_probe.exe (
    .\monkeys_ear_vocal_target_chain_probe.exe
    if %ERRORLEVEL% neq 0 (
        cd ..
        echo [FAIL] Vocal target chain probe failed with code %ERRORLEVEL%
        exit /b %ERRORLEVEL%
    )
)

cd ..

echo [PASS] All Monkey's Ear audio, host, and latency tests verified!
