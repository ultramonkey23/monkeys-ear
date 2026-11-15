@echo off
setlocal
set "REAPER_EXE=%ProgramFiles%\REAPER (x64)\reaper.exe"
if not exist "%REAPER_EXE%" set "REAPER_EXE=%ProgramFiles%\REAPER\reaper.exe"
if not exist "%REAPER_EXE%" (
  echo REAPER was not found in its usual location. Open REAPER and run tools\reaper_audition.lua from Actions.
  exit /b 1
)
start "" "%REAPER_EXE%" "%~dp0tools\reaper_audition.lua"
