@echo off
rem Launches the Benzene language server on the Erlang VM.
rem
rem Neovim (and any other LSP client) should be pointed at this file. It speaks
rem LSP on stdin/stdout, so nothing here may print to stdout.

setlocal enabledelayedexpansion

set "HERE=%~dp0"
set "SHIP=%HERE%..\ether_lsp\build\erlang-shipment"

if not exist "%SHIP%\ether_lsp\ebin" (
  echo ether-lsp: not built yet - run lsp\build.cmd 1>&2
  exit /b 1
)

rem Default to the compiler built from this checkout, so a fresh clone works
rem without touching PATH. An existing ETHER_BIN always wins.
if not defined ETHER_BIN set "ETHER_BIN=%HERE%..\..\bin\ether.exe"

rem erl does not expand wildcards itself, and neither does cmd inside an
rem argument, so each dependency's ebin directory is added explicitly.
set "PA="
for /d %%d in ("%SHIP%\*") do set "PA=!PA! -pa "%%d\ebin""

erl !PA! -noshell -eval "ether_lsp@@main:run(ether_lsp)"
