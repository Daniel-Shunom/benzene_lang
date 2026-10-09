# Installing Benzene

Benzene installs three native commands: `ether` (compiler/front end), `ether-lsp`
(stdio language server), and `ether-mcp` (local HTTP MCP server). The LSP and MCP
are shipped as precompiled Erlang modules. Gleam and the source checkout are
not needed at runtime. Erlang/OTP must remain installed and `erl` must be on PATH,
or `BENZENE_ERL` must point to its executable.

## Windows

To build and install from this checkout:

```powershell
.\install.ps1 -AddToPath
```

The default is `%LOCALAPPDATA%\Programs\Benzene`, a per-user installation that
does not require administrator rights. `-AddToPath` adds its `bin` directory
to your user PATH without replacing existing entries. Open a new terminal after
installation. Omit the flag if you prefer to manage PATH yourself.

Build prerequisites: CMake, Ninja, a C++26 compiler, Gleam, and Erlang/OTP.
The script can detect LLVM installed through Scoop, including its resource
compiler and linker. Clang with the Windows SDK/MSVC toolchain is supported;
MSVC's compiler itself may not support all language features used by this code.
The release build uses a static MSVC runtime (or static MinGW runtime libraries),
so installed executables do not rely on a development compiler's DLL directories.

```powershell
# Another installation prefix (Program Files requires elevation):
.\install.ps1 -Prefix "$env:ProgramFiles\Benzene" -Jobs 2

# Install an already prepared release build:
.\install.ps1 -SkipBuild -AddToPath
```

`-BuildDirectory` selects another CMake build directory. Builds default to one
worker to limit memory use. Production build artifacts live under that build
directory, independently of the development compiler in the checkout's `bin`.

## Linux and macOS

```sh
sh install.sh                  # ~/.local
sh install.sh /opt/benzene     # another prefix; needs appropriate permissions
```

Add `<prefix>/bin` to your shell's PATH. `BENZENE_BUILD_JOBS` controls parallel
builds. These platforms use the same CMake install layout and precompiled
shipments; the Windows path is tested on this development machine, while the
POSIX installer/launchers still need native Linux/macOS verification.

## CMake and portable packages

For an explicit full-toolchain build:

```sh
cmake -S . -B build/install-Release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DETHER_BUILD_TESTS=OFF \
  -DETHER_INSTALL_SERVERS=ON
cmake --build build/install-Release --parallel 1
cmake --install build/install-Release --prefix /path/to/benzene
```

On Windows, prefer `install.ps1` to handle LLVM discovery and the static runtime
options. Compiler-only CMake development builds leave `ETHER_INSTALL_SERVERS`
off, so ordinary C++ builds do not require Gleam. Enable it before creating a
package that includes both servers.

```sh
cpack --config build/install-Release/CPackConfig.cmake -B build/packages
```

This produces a ZIP on Windows or a compressed tar archive on POSIX systems.
Extract it to the desired installation location and add its `bin` directory
to PATH. Keep `bin`, `lib`, and `share` together when moving the installation.
Server launchers find precompiled dependencies and the compiler relative to
themselves, independently of the current working directory. `ETHER_BIN` can
override the installed compiler for either server.

```text
<prefix>/
  bin/ether[.exe]
  bin/ether-lsp[.exe]
  bin/ether-mcp[.exe]
  lib/benzene/lsp/        # production BEAM modules and dependencies
  lib/benzene/mcp/
  share/benzene/editors/nvim/
  share/benzene/examples/mcp/
  share/doc/benzene/      # grammar, guides, server docs and license
```

No server is installed as an automatic background service. To uninstall, stop
the servers, remove the installation folder, and remove its `bin` entry from
your user PATH. The source checkout and generated projects are independent.

## Try the installation

From a new terminal in any directory:

```sh
ether --version
ether new hello_benzene
cd hello_benzene
ether check src/main.bz
ether scan src/main.bz
```

The starter's `main` returns `Nil`. Executable code generation, native printing,
and running Benzene programs are not yet implemented; installation does not
change those language limits.

Start MCP in another terminal:

```sh
ether-mcp
```

Agents connect to `http://127.0.0.1:4000/mcp`; `BENZENE_MCP_PORT` changes the
port. Generated projects include `.mcp.json` and `AGENTS.md` with discovery and
compiler-validation instructions. `ether-mcp --help` explains the launcher.
`ether-lsp` is launched by your editor over stdio; it is not an interactive REPL.

For Neovim 0.11+, add the installed plugin to runtimepath in your configuration,
before plugins are loaded:

```lua
-- Windows default; on POSIX use your prefix's share/benzene/editors/nvim.
local prefix = vim.env.LOCALAPPDATA .. "/Programs/Benzene"
vim.opt.rtp:append(prefix .. "/share/benzene/editors/nvim")
```

The plugin selects `ether-lsp` from PATH, with an adjacent-install fallback.
You can also configure any LSP client to execute `ether-lsp` directly. Editor
configuration is left to you; installation does not edit your Neovim files.

## Verification

```sh
python tests/integration/test_installation.py /path/to/benzene
```

This exercises the installed compiler and project generation, LSP initialization,
diagnostics and hover, MCP discovery, generation and compiler errors, and a
missing-runtime failure. It runs from another directory and repeats the tests
after copying the installation to a path containing spaces. No Gleam commands
or checkout-relative compiler paths are used at runtime.

The deployment build uses [Gleam Erlang shipments](https://gleam.run/documentation/command-line-reference/)
and [CMake installation rules](https://cmake.org/cmake/help/latest/command/install.html).
