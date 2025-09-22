#include <fstream>
#include <sstream>
#include <string>

inline std::string FileToString(const std::string &file_path) {
  std::ifstream file(file_path);

  if (!file.is_open()) {
    std::printf("Error: failed to open file path");
    return "";
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return buffer.str();
}

