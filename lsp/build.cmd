@echo off
rem Compiles the language server into lsp\ether_lsp\build\erlang-shipment.
setlocal
cd /d "%~dp0ether_lsp" || exit /b 1
gleam export erlang-shipment
