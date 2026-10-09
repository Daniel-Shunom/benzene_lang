@echo off
setlocal

if /I "%~1"=="--help" (
  echo Usage: build.bat [debug^|release] [all^|N]
  echo Defaults: release, one worker to limit memory use.
  echo Higher worker counts increase memory use; use all only if you have enough RAM.
  exit /b 0
)

set "MODE=%~1"
if not defined MODE set "MODE=release"
if /I "%MODE%"=="debug" (
  set "BUILD_TYPE=Debug"
) else if /I "%MODE%"=="release" (
  set "BUILD_TYPE=Release"
) else (
  echo ERROR: Build mode must be debug or release.
  exit /b 1
)

set "JOBS=%~2"
if not defined JOBS set "JOBS=1"
if /I "%JOBS%"=="all" set "JOBS=%NUMBER_OF_PROCESSORS%"
echo(%JOBS%| findstr /R /X "[1-9][0-9]*" >nul
if errorlevel 1 (
  echo ERROR: Worker count must be all or a positive integer.
  exit /b 1
)
if not "%~3"=="" (
  echo ERROR: Usage: build.bat [debug^|release] [all^|N]
  exit /b 1
)

set "BUILD_DIR=%~dp0build\native-%BUILD_TYPE%"
cmake -S "%~dp0." -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DETHER_BUILD_TESTS=OFF -DETHER_BUILD_JOBS=%JOBS%
if errorlevel 1 exit /b 1

echo Building %BUILD_TYPE% with %JOBS% parallel workers...
cmake --build "%BUILD_DIR%" --target ether --parallel %JOBS%
if errorlevel 1 (
  echo Build FAILED.
  exit /b 1
)

echo Build complete: %~dp0bin\ether.exe
endlocal
