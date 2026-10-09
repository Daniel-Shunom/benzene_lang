#include "create.hpp"

#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <vector>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

bool valid_name(std::string_view name) {
  constexpr std::string_view allowed =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-";
  if (name.empty() || name.front() == '-' ||
      name.find_first_not_of(allowed) != std::string_view::npos) return false;

  // Keep project names portable, including Windows device names.
  std::string upper(name);
  for (char& c : upper) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL")
    return false;
  if (upper.size() == 4 && (upper.starts_with("COM") || upper.starts_with("LPT"))
      && upper[3] >= '1' && upper[3] <= '9') return false;
  return true;
}

void write_file(const fs::path& path, std::string_view content) {
  std::ofstream file;
  file.exceptions(std::ios::failbit | std::ios::badbit);
  try {
    file.open(path, std::ios::binary | std::ios::out);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.close();
  } catch (const std::ios_base::failure&) {
    throw std::runtime_error("could not write `" + path.string() +
                             "`; the new project may be incomplete");
  }
}

enum class GitResult { Initialized, Unavailable, Failed };

GitResult initialize_git(const fs::path& path) {
  // Execute Git directly, without a shell or interpolated shell commands.
#ifdef _WIN32
  std::vector<wchar_t> executable(32768);
  const DWORD length = SearchPathW(nullptr, L"git.exe", nullptr,
      static_cast<DWORD>(executable.size()), executable.data(), nullptr);
  if (length == 0) return GitResult::Unavailable;
  if (length >= executable.size()) return GitResult::Failed;
  // Windows paths cannot contain quotes; the absolute directory has no
  // trailing separator, so quoting it preserves spaces in parent paths.
  std::wstring command = L"\"" + std::wstring(executable.data()) +
      L"\" -C \"" + path.native() + L"\" init --quiet";
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(executable.data(), command.data(), nullptr, nullptr,
                      FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process))
    return GitResult::Failed;
  const DWORD wait = WaitForSingleObject(process.hProcess, INFINITE);
  DWORD status = 1;
  const bool exited = wait == WAIT_OBJECT_0 && GetExitCodeProcess(process.hProcess, &status);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return exited && status == 0 ? GitResult::Initialized : GitResult::Failed;
#else
  const pid_t child = fork();
  if (child < 0) return GitResult::Failed;
  if (child == 0) {
    execlp("git", "git", "-C", path.c_str(), "init", "--quiet", nullptr);
    _exit(errno == ENOENT ? 127 : 126);
  }
  int status = 0;
  pid_t waited;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  if (waited < 0 || !WIFEXITED(status)) return GitResult::Failed;
  if (WEXITSTATUS(status) == 127) return GitResult::Unavailable;
  return WEXITSTATUS(status) == 0 ? GitResult::Initialized : GitResult::Failed;
#endif
}

constexpr std::string_view main_source = "func main() :> Nil\n  Nil\nend\n";

constexpr std::string_view mcp_config = R"json({
  "mcpServers": {
    "benzene": {
      "type": "http",
      "url": "http://127.0.0.1:4000/mcp"
    }
  }
}
)json";

constexpr std::string_view agent_instructions = R"md(# Benzene agent entrypoint

Use the Benzene MCP server to construct code and validate it against the real
compiler. Do not guess syntax, invent native APIs, or treat successful source
generation alone as proof that a program type-checks.

## Connect and discover

Read `.mcp.json` for the Benzene server URL. If your agent client does not load
this configuration format, register the same HTTP endpoint in its MCP settings.
The MCP server must already be running; this project does not install or start it.
With Benzene installed, run:

```sh
ether-mcp
```

For a source checkout, run from `mcp/ether_mcp` instead:

```sh
gleam run -m mcp/server
```

Set `ETHER_BIN` on the server to the intended `ether` compiler executable when
needed. The default endpoint is `http://127.0.0.1:4000/mcp`; change `.mcp.json`
if the server uses another port. Never substitute an unrelated tool server.

Connect through your client's MCP transport, initialize the connection, and
call `tools/list`. Read the returned tool descriptions and recursive input
schemas before forming requests. The server supports protocol `2025-11-25`.
For direct HTTP requests, POST JSON-RPC 2.0 to the configured endpoint with
`Content-Type: application/json` and `Accept: application/json, text/event-stream`.
Initialize with `protocolVersion`, `capabilities`, and `clientInfo` (`name`,
`version`), send `notifications/initialized`, and use the negotiated version in
the `MCP-Protocol-Version` header. Give each request a unique string or integer id.

## Generate and validate

1. Discover `generate_program` and `check_program` using `tools/list`.
2. Describe the program using the discovered structured constructs, not raw
   Benzene source strings. Use `generate_program` with `construct`, a logical
   `path` such as `src/main.bz`, and `validate: true`.
3. Read `structuredContent` (or parse the JSON in text `content`). Accept the
   result only when `isError` is false, `validated` is true, `valid` is true,
   and `analysis.diagnostics` contains no errors. Inspect inferred types in
   `analysis.index` where useful. Correct errors and regenerate before writing.
4. Save the returned `source` to the intended project file. The MCP tools do not
   write files for you. Preserve unrelated code and user changes.
5. After editing existing source, call `check_program` with its complete `source`
   and `path`. Apply the same validation checks and fix any diagnostics.

For fragments, `validate: false` checks formation only. Assemble and validate a
complete module before treating the result as finished. A missing compiler or
MCP connection means validation has not happened; report it rather than claiming
success. If MCP is unavailable, `ether scan src/main.bz` can produce compiler
diagnostics locally, but it does not replace MCP construct generation.

## Starter program and current limits

`src/main.bz` contains a `main` function returning `Nil`. Printing, native file
I/O, HTTP runtime adapters, compilation to an executable, and execution are not
implemented yet. Model hypothetical effects as typed callback dependencies;
clearly distinguish validated application logic from executable functionality.
Do not add a pretend `print` or `Hello, world!` implementation.
)md";

std::string readme(const std::string& name) {
  return "# " + name + R"md(

A Benzene project generated by `ether new`.

The starter source is `src/main.bz`. Its `main` function returns `Nil`.

```sh
ether check src/main.bz
ether scan src/main.bz
```

`check` displays diagnostics; `scan` returns JSON diagnostics and inferred
symbol types. Check the diagnostics to establish correctness. Executable
code generation, running programs, and printing are not implemented yet.

## Agent-assisted development

Start with [AGENTS.md](AGENTS.md). [.mcp.json](.mcp.json) describes the local
Benzene MCP endpoint. Clients that do not automatically read that format need
the same endpoint configured in their own MCP settings. The MCP server is a
separate service; this scaffold does not bundle or start it.

Agents discover tools with `tools/list`, create structured constructs with
`generate_program`, and validate source with `check_program`. Accept generated
code only after successful syntax, symbol resolution and type checking.
)md";
}
} // namespace

int HandleCreate(const ArgCreate& arg) {
  if (!valid_name(arg.project_name))
    throw std::invalid_argument("project name must contain only letters, digits, underscores "
                                "or hyphens, must not start with a hyphen, and must not be a reserved device name");
  const fs::path target = fs::current_path() / arg.project_name;
  // An existing destination is never reused or overwritten.
  if (!fs::create_directory(target))
    throw std::runtime_error("`" + target.string() + "` already exists; choose a new project name");
  fs::create_directory(target / "src");
  write_file(target / "src" / "main.bz", main_source);
  write_file(target / "README.md", readme(arg.project_name));
  write_file(target / "AGENTS.md", agent_instructions);
  write_file(target / ".mcp.json", mcp_config);
  write_file(target / ".gitignore", "bin/\nbuild/\n");

  switch (initialize_git(target)) {
    case GitResult::Initialized:
      std::printf("Initialized Git repository.\n");
      break;
    case GitResult::Unavailable:
      std::printf("Git is not available; skipped repository initialization.\n");
      break;
    case GitResult::Failed:
      std::fprintf(stderr, "ether: project files were created, but Git initialization failed; "
                           "run git init inside the project to retry\n");
      return 1;
  }
  std::printf("Created Benzene project `%s`.\n"
              "  cd %s\n"
              "  ether check src/main.bz\n"
              "Agents: read AGENTS.md and configure the MCP endpoint in .mcp.json.\n",
              arg.project_name.c_str(), arg.project_name.c_str());
  return 0;
}
