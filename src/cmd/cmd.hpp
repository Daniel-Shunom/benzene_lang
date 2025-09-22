#pragma once
#include <stdexcept>
#include <string>

typedef struct {
  std::string file_path;
} Args;

inline Args GetArgs(int argc, char* argv[]) {
  if (argc < 2) {
    throw std::invalid_argument("insufficient argument. must provide file path");
  }

  return (Args) {
    .file_path = argv[1],
  };
}

