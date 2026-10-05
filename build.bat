@echo off
setlocal enabledelayedexpansion

set "MODE=%~1"
if /I "%MODE%"=="debug" (
  set "FLAGS_FILE=compile_flags_debug.txt"
) else (
  set "FLAGS_FILE=compile_flags.txt"
)

if not exist "%FLAGS_FILE%" (
  echo ERROR: Could not find %FLAGS_FILE%
  exit /b 1
)

set "FLAGS="
for /F "usebackq delims=" %%f in ("%FLAGS_FILE%") do (
  set "FLAGS=!FLAGS! %%f"
)

if not exist bin (
  mkdir bin
)

set "SOURCES="
for /R core\src %%f in (*.cpp) do (
  set "SOURCES=!SOURCES! "%%f""
)

for /R cli\src %%f in (*.cpp) do (
  if /I not "%%~nxf"=="main.cpp" (
    set "SOURCES=!SOURCES! "%%f""
  )
)

echo.
echo Building %MODE% executable...
echo Flags: %FLAGS%
echo.

g++ "cli\src\main.cpp" %SOURCES% %FLAGS% -o "bin\ether.exe" -lstdc++exp

if errorlevel 1 (
  echo.
  echo Build FAILED.
  exit /b 1
)

echo.
echo Build complete: bin\ether.exe
endlocal
