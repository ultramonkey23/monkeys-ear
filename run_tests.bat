@echo off
setlocal EnableExtensions EnableDelayedExpansion

echo =======================================================
echo   RUNNING MONKEY'S EAR DETERMINISTIC DSP + HOST TESTS
echo =======================================================

rem Keep the Windows runtime DLL search path aligned with build.bat.
rem Respect user overrides and default to the current account's WinGet path.
if not defined TOOLCHAIN_BIN set "TOOLCHAIN_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT.LLVM_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
if exist "%TOOLCHAIN_BIN%\g++.exe" set "PATH=%TOOLCHAIN_BIN%;%PATH%"

rem Always rebuild from current source: a passing stale executable is not a
rem valid regression result after edits to the Vocal wrapper or host probe.
echo [INFO] Building the current Monkey's Ear source...
call .\build.bat
if errorlevel 1 (
    set "BUILD_ERR=!errorlevel!"
    echo [FAIL] Monkey's Ear build failed with code !BUILD_ERR!
    exit /b !BUILD_ERR!
)

pushd build
if errorlevel 1 (
    echo [FAIL] Cannot enter the build output directory
    exit /b 2
)

rem Every expected executable is mandatory. The previous "if exist" pattern
rem could silently skip a missing Vocal host probe and still print ALL PASS.
for %%T in (
    monkeys_ear_test_runner
    monkeys_ear_tube_rack_probe
    monkeys_ear_reverb_rt_probe
    monkeys_ear_snapshot_replay_probe
    monkeys_ear_host_probe
    monkeys_ear_vocal_host_probe
    monkeys_ear_vocal_target_chain_probe
    monkeys_ear_vocal_tracker_probe
) do (
    if not exist "%%T.exe" (
        echo [FAIL] Required test executable %%T.exe is missing
        popd
        exit /b 2
    )
    echo [TEST] %%T.exe
    ".\%%T.exe"
    set "TEST_ERR=!errorlevel!"
    if not "!TEST_ERR!"=="0" (
        echo [FAIL] %%T.exe failed with code !TEST_ERR!
        popd
        exit /b !TEST_ERR!
    )
)

popd
echo [PASS] All required Monkey's Ear DSP, Vocal, host, and tracker probes executed successfully.
exit /b 0
