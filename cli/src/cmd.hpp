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
  bool show_constraints = false;
  bool show_unification = false;
};
struct ArgScan   {
  std::string path;
  // When set, the source is read from stdin as a length-prefixed payload
  // instead of from `path`, so the editor can check an unsaved buffer. `path`
  // is still the identity reported back in diagnostics.
  bool use_stdin = false;
};
struct ArgHelp   {};

using Args = std::variant<
  ArgInit,
  ArgBuild,
  ArgRun,
  ArgCheck,
  ArgScan,
  ArgCreate,
  ArgHelp
>;

Args GetArgs(int argc, char* argv[]);

int HandleArgs(const Args&);
