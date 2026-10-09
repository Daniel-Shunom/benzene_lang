<# Install the compiler and production LSP/MCP shipments for the current user.
   Example: .\install.ps1 -AddToPath
   A machine-wide install can use -Prefix "$env:ProgramFiles\Benzene" from
   an elevated shell. No registry/file associations are otherwise changed. #>
[CmdletBinding()]
param(
    [string]$Prefix = (Join-Path $env:LOCALAPPDATA 'Programs\Benzene'),
    [string]$BuildDirectory = '',
    [ValidateRange(1, 64)][int]$Jobs = 1,
    [switch]$AddToPath,
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
if (-not $BuildDirectory) {
    $BuildDirectory = Join-Path $PSScriptRoot 'build\install-Release'
}
$Prefix = [System.IO.Path]::GetFullPath($Prefix)
$BuildDirectory = [System.IO.Path]::GetFullPath($BuildDirectory)
foreach ($tool in @('cmake', 'erl')) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool is required and must be on PATH."
    }
}
if (-not $SkipBuild) {
    foreach ($tool in @('gleam', 'ninja')) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool is required to build the installation."
        }
    }
    $configure = @('-S', $PSScriptRoot, '-B', $BuildDirectory, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DETHER_BUILD_TESTS=OFF',
        '-DETHER_INSTALL_SERVERS=ON', "-DETHER_BUILD_JOBS=$Jobs",
        "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY:PATH=$($BuildDirectory.Replace('\', '/'))/bin",
        '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded')
    # A configured build already knows its compiler. For a fresh build prefer
    # Clang (including Scoop's installed LLVM) before relying on CMake discovery.
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    $compilerPath = ''
    if (Test-Path -LiteralPath $cache) {
        $cachedCompiler = Select-String -LiteralPath $cache -Pattern '^CMAKE_CXX_COMPILER:[^=]+=(.+)$'
        if ($cachedCompiler) { $compilerPath = $cachedCompiler.Matches[0].Groups[1].Value }
    } else {
        $compiler = Get-Command 'clang++.exe' -ErrorAction SilentlyContinue
        if ($compiler) {
            $compilerPath = $compiler.Source
        } else {
            $scoopClang = Join-Path $env:USERPROFILE 'scoop\apps\llvm\current\bin\clang++.exe'
            if (Test-Path -LiteralPath $scoopClang) {
                $compilerPath = $scoopClang
            }
        }
        if ($compilerPath) { $configure += "-DCMAKE_CXX_COMPILER:FILEPATH=$($compilerPath.Replace('\', '/'))" }
    }
    if ($compilerPath -and (Split-Path -Leaf $compilerPath) -match '^clang') {
        $llvmDirectory = Split-Path -Parent $compilerPath
        foreach ($entry in @(@('CMAKE_RC_COMPILER', 'llvm-rc.exe'), @('CMAKE_LINKER', 'lld-link.exe'))) {
            $toolPath = Join-Path $llvmDirectory $entry[1]
            if (Test-Path -LiteralPath $toolPath) {
                $configure += "-D$($entry[0]):FILEPATH=$($toolPath.Replace('\', '/'))"
            }
        }
    }
    & cmake @configure
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & cmake --build $BuildDirectory --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Installation build failed.' }
}

& cmake --install $BuildDirectory --prefix $Prefix --config Release
if ($LASTEXITCODE -ne 0) { throw 'Installation failed.' }
$binDirectory = Join-Path $Prefix 'bin'
foreach ($name in @('ether.exe', 'ether-lsp.exe', 'ether-mcp.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $binDirectory $name))) {
        throw "Installation is incomplete: $name is missing."
    }
}

if ($AddToPath) {
    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $entries = @($userPath -split ';' | Where-Object { $_ })
    $present = $entries | Where-Object {
        [Environment]::ExpandEnvironmentVariables($_).TrimEnd('\') -ieq $binDirectory.TrimEnd('\')
    }
    if (-not $present) {
        [Environment]::SetEnvironmentVariable('Path', (($entries + $binDirectory) -join ';'), 'User')
    }
    # This affects this process only; newly launched shells read the user PATH.
    if (-not (($env:Path -split ';') -contains $binDirectory)) {
        $env:Path = "$binDirectory;$env:Path"
    }
    # Notify Explorer so newly opened terminals inherit the updated environment.
    if (-not ('BenzeneEnvironment' -as [type])) {
        Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class BenzeneEnvironment {
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern IntPtr SendMessageTimeout(IntPtr window, uint message,
      UIntPtr param, string value, uint flags, uint timeout, out UIntPtr result);
}
'@
    }
    $notification = [UIntPtr]::Zero
    [void][BenzeneEnvironment]::SendMessageTimeout([IntPtr]0xffff, 0x1a,
        [UIntPtr]::Zero, 'Environment', 2, 3000, [ref]$notification)
}

Write-Host "Installed Benzene to $Prefix"
Write-Host "Commands: ether, ether-lsp, ether-mcp"
if ($AddToPath) {
    Write-Host 'Added bin to your user PATH. Open a new terminal to use it.'
} else {
    Write-Host "Add $binDirectory to PATH, or rerun with -AddToPath."
}
