#pragma once
#include <string>
#include <variant>

struct ArgCreate { std::string project_name; };
struct ArgInit   {};
struct ArgBuild  {};
struct ArgRun    {};
struct ArgCheck  {
  std::string path;
  bool show_ast = false;
  bool show_types = false;
};
struct ArgHelp   {};

using Args = std::variant<
  ArgInit,
  ArgBuild,
  ArgRun,
  ArgCheck,
  ArgCreate,
  ArgHelp
>;

Args GetArgs(int argc, char* argv[]);

int HandleArgs(const Args&);
