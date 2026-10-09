// Relocatable native launchers: run installed BEAM code without Gleam or a
// source checkout. stdout belongs exclusively to the LSP when launching it.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static fs::path executable_path() {
#ifdef _WIN32
  std::vector<wchar_t> path(32768);
  const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length >= path.size()) throw std::runtime_error("cannot locate launcher");
  return fs::path(std::wstring(path.data(), length));
#elif defined(__APPLE__)
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> path(size);
  if (_NSGetExecutablePath(path.data(), &size) != 0) throw std::runtime_error("cannot locate launcher");
  return fs::canonical(path.data());
#else
  return fs::canonical("/proc/self/exe");
#endif
}

#ifdef _WIN32
static std::wstring environment(const wchar_t* name) {
  const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
  if (size == 0) return {};
  std::vector<wchar_t> value(size);
  const DWORD length = GetEnvironmentVariableW(name, value.data(), size);
  if (length == 0 || length >= size) return {};
  return std::wstring(value.data(), length);
}

static std::wstring quote(const std::wstring& value) {
  std::wstring result = L"\"";
  size_t slashes = 0;
  for (const auto c : value) {
    if (c == L'\\') { ++slashes; continue; }
    result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    result += c;
    slashes = 0;
  }
  result.append(slashes * 2, L'\\');
  return result + L'"';
}

static int run(const std::vector<std::wstring>& args) {
  std::vector<wchar_t> runtime(32768);
  const auto override = environment(L"BENZENE_ERL");
  const std::wstring requested = override.empty() ? L"erl.exe" : override;
  const DWORD size = SearchPathW(nullptr, requested.c_str(), nullptr,
      static_cast<DWORD>(runtime.size()), runtime.data(), nullptr);
  if (size == 0 || size >= runtime.size())
    throw std::runtime_error("Erlang/OTP not found; put erl on PATH or set BENZENE_ERL");
  std::wstring command = quote(runtime.data());
  for (const auto& arg : args) command += L" " + quote(arg);
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  // A job keeps the VM and its compiler subprocesses from outliving a killed
  // launcher (for example when the editor exits or stops the LSP client).
  const HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (!job) throw std::runtime_error("cannot create server process job");
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
    CloseHandle(job);
    throw std::runtime_error("cannot configure server process job");
  }
  PROCESS_INFORMATION child{};
  if (!CreateProcessW(runtime.data(), command.data(), nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &child)) {
    CloseHandle(job);
    throw std::runtime_error("could not start Erlang/OTP");
  }
  if (!AssignProcessToJobObject(job, child.hProcess)) {
    TerminateProcess(child.hProcess, 1);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    CloseHandle(job);
    throw std::runtime_error("could not attach Erlang/OTP to server process job");
  }
  ResumeThread(child.hThread);
  WaitForSingleObject(child.hProcess, INFINITE);
  DWORD status = 1;
  GetExitCodeProcess(child.hProcess, &status);
  CloseHandle(child.hThread);
  CloseHandle(child.hProcess);
  CloseHandle(job);
  return static_cast<int>(status);
}
#endif

int main(int argc, char* argv[]) {
  try {
    const std::string server = ETHER_SERVER;
    if (argc == 2 && std::string(argv[1]) == "--version") {
      std::printf("ether-%s %s\n", ETHER_SERVER, ETHER_VERSION);
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::printf("ether-%s: installed Benzene %s server\n"
                  "Requires Erlang/OTP on PATH (or BENZENE_ERL).\n"
                  "ETHER_BIN overrides the bundled compiler.\n", ETHER_SERVER, ETHER_SERVER);
      if (server == "mcp") std::printf("HTTP endpoint: http://127.0.0.1:4000/mcp; BENZENE_MCP_PORT selects the port.\n");
      return 0;
    }
    if (argc != 1) throw std::runtime_error("unexpected arguments; use --help");
    const auto bin = executable_path().parent_path();
    const auto shipment = bin.parent_path() / ETHER_LIBDIR / "benzene" / server;
    if (!fs::is_directory(shipment / ("ether_" + server) / "ebin"))
      throw std::runtime_error("precompiled server not found beside this installation");
#ifdef _WIN32
    const auto compiler = bin / "ether.exe";
    if (environment(L"ETHER_BIN").empty())
      SetEnvironmentVariableW(L"ETHER_BIN", compiler.c_str());
    std::vector<std::wstring> args;
#else
    const auto compiler = bin / "ether";
    if (!std::getenv("ETHER_BIN") || !*std::getenv("ETHER_BIN"))
      setenv("ETHER_BIN", compiler.c_str(), 1);
    std::vector<std::string> args;
#endif
    std::vector<fs::path> ebins;
    for (const auto& entry : fs::directory_iterator(shipment)) {
      const auto ebin = entry.path() / "ebin";
      if (fs::is_directory(ebin)) ebins.push_back(ebin);
    }
    std::sort(ebins.begin(), ebins.end());
    for (const auto& path : ebins) {
#ifdef _WIN32
      args.push_back(L"-pa");
      args.push_back(path.native());
#else
      args.push_back("-pa");
      args.push_back(path.native());
#endif
    }
#ifdef _WIN32
    args.push_back(L"-noshell");
    args.push_back(L"-eval");
    args.push_back(server == "lsp" ? L"'ether_lsp@@main':run(ether_lsp)." : L"'ether_mcp@@main':run('mcp@server').");
    return run(args);
#else
    args.push_back("-noshell");
    args.push_back("-eval");
    args.push_back(server == "lsp" ? "'ether_lsp@@main':run(ether_lsp)." : "'ether_mcp@@main':run('mcp@server').");
    const char* override = std::getenv("BENZENE_ERL");
    std::string runtime = override && *override ? override : "erl";
    std::vector<char*> command{runtime.data()};
    for (auto& arg : args) command.push_back(arg.data());
    command.push_back(nullptr);
    execvp(runtime.c_str(), command.data());
    throw std::runtime_error("Erlang/OTP not found; put erl on PATH or set BENZENE_ERL");
#endif
  } catch (const std::exception& error) {
    std::fprintf(stderr, "ether-%s: %s\n", ETHER_SERVER, error.what());
    return 1;
  }
}
