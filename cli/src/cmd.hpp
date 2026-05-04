#pragma once
#include <stdexcept>
#include <string>

struct Args {
  std::string file_path;
  std::string logs_enabled;

  bool is_logs_enabled() {
    return logs_enabled == "--wlogs";
  }
};

inline Args GetArgs(int argc, char* argv[]) {
  if (argc < 2) {
    throw std::invalid_argument("insufficient argument. must provide file path");
  }

  return (Args) {
    .file_path = argv[1],
    .logs_enabled = argv[2] ? argv[2] : std::string{}
  };
}

