
@echo off
setlocal enabledelayedexpansion

REM Read compile flags from file
set "FLAGS="
for /F "usebackq delims=" %%f in ("compile_flags.txt") do (
  set "FLAGS=!FLAGS! %%f"
)

REM Create output directory
if not exist bin (
  mkdir bin
)

REM Collect source files
set "SOURCES="
for /R src %%f in (*.cpp) do (
  if /I not "%%~nxf"=="main.cpp" (
    set "SOURCES=!SOURCES! %%f"
  )
)

REM compile
echo Compiling with flags: %FLAGS%

g++ src\main.cpp %SOURCES% %FLAGS% -o bin\benzene

echo Build complete: bin\benzene
endlocal
