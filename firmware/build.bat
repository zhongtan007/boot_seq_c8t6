@echo off
rem ==========================================================================
rem  boot_seq_c8t6 build script  (STM32F103C8T6 power sequence firmware)
rem
rem  Usage: double-click, or run from the firmware directory:  build.bat
rem         add -q to skip the final pause (for scripted use)
rem
rem  Output: build\boot_seq_c8t6.elf / .hex / .bin
rem          build\hex\boot_seq_c8t6.hex   (for flashing)
rem
rem  After a successful build, intermediate files (*.o *.d *.elf *.map)
rem  are removed automatically -- only the flashing artifacts (*.hex *.bin)
rem  and the archived hex are kept.
rem
rem  On build failure the intermediates are kept for troubleshooting.
rem
rem  Toolchain: machine-specific paths are intentionally kept OUT of this file
rem  so the repository carries no local environment info. Provide them via:
rem    1) build.local.bat  (git-ignored; recommended)
rem         set "MAKE=<path>\make.exe"
rem         set "GCC_PATH=<path>\tools\bin"
rem    2) the MAKE / GCC_PATH environment variables.
rem ==========================================================================

rem --- load machine-specific toolchain paths (this file is git-ignored) ---
if exist "%~dp0build.local.bat" call "%~dp0build.local.bat"

if not defined MAKE set "MAKE=make.exe"
if not defined GCC_PATH (
  echo [ERROR] GCC_PATH is not set.
  echo         Create firmware\build.local.bat containing:
  echo             set "MAKE=^<path to make.exe^>"
  echo             set "GCC_PATH=^<path to arm-none-eabi bin dir^>"
  echo         or define the MAKE / GCC_PATH environment variables.
  if /i not "%~1"=="-q" pause
  exit /b 1
)

rem switch to the script directory (firmware) so Makefile relative paths work
cd /d "%~dp0"

if not exist "%MAKE%" (
  echo [ERROR] make.exe not found
  echo         check MAKE in build.local.bat: %MAKE%
  if /i not "%~1"=="-q" pause
  exit /b 1
)
if not exist "%GCC_PATH%\arm-none-eabi-gcc.exe" (
  echo [ERROR] arm-none-eabi-gcc.exe not found
  echo         check GCC_PATH in build.local.bat: %GCC_PATH%
  if /i not "%~1"=="-q" pause
  exit /b 1
)

echo ============================================================
echo  Building boot_seq_c8t6 (STM32F103C8T6) ...
echo ============================================================
"%MAKE%" -j8 GCC_PATH=%GCC_PATH%
if errorlevel 1 (
  echo.
  echo [ERROR] build failed - intermediate files kept for troubleshooting
  if /i not "%~1"=="-q" pause
  exit /b 1
)

rem --- archive hex (for flashing) ---
if not exist build\hex mkdir build\hex
copy /y build\boot_seq_c8t6.hex build\hex\boot_seq_c8t6.hex >nul

rem --- clean intermediates: keep only *.hex / *.bin (+ hex\ archive) ---
echo.
echo Cleaning intermediate files ...
powershell -NoProfile -ExecutionPolicy Bypass -Command "$t=@(); foreach($e in @('o','d','lst','elf','map')){$t+=@(Get-ChildItem -File -Filter ('*.'+$e) -Path 'build' -ErrorAction SilentlyContinue)}; $t=@($t|Sort-Object FullName -Unique); if($t.Count -eq 0){Write-Host '[CLEAN] nothing to clean'} else {$mb=[math]::Round((($t|Measure-Object Length -Sum).Sum)/1MB,1); $t | Remove-Item -Force -ErrorAction SilentlyContinue; Write-Host ('[CLEAN] removed '+$t.Count+' files, freed ~'+$mb+' MB')}"
if errorlevel 1 echo [WARN] cleanup step failed, but build output is still valid

echo.
echo ============================================================
echo  Build OK
echo    build\boot_seq_c8t6.hex
echo    build\boot_seq_c8t6.bin
echo    build\hex\boot_seq_c8t6.hex  ^(for flashing^)
echo ============================================================
if /i not "%~1"=="-q" pause
exit /b 0
