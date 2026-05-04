#include "cmd.hpp"
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // For legacy windows glyph support
  SetConsoleOutputCP(CP_UTF8);
#endif

  try {
    return HandleArgs(GetArgs(argc, argv));
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ether: %s\n", e.what());
    return 1;
  }
}
