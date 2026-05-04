@echo off
setlocal enabledelayedexpansion

set "FLAGS="
for /F "usebackq delims=" %%f in ("compile_flags.txt") do (
  set "FLAGS=!FLAGS! %%f"
)

if not exist bin (
  mkdir bin
)

set "SOURCES="
for /R core\src %%f in (*.cpp) do (
  set "SOURCES=!SOURCES! %%f"
)
for /R cli\src %%f in (*.cpp) do (
  if /I not "%%~nxf"=="main.cpp" (
    set "SOURCES=!SOURCES! %%f"
  )
)

echo Compiling with flags: %FLAGS%

g++ cli\src\main.cpp %SOURCES% %FLAGS% -o bin\ether.exe

echo Build complete: bin\ether.exe
endlocal
