#include "cmd.hpp"

#include "commands/build/build.hpp"
#include "commands/check/check.hpp"
#include "commands/create/create.hpp"
#include "commands/help/help.hpp"
#include "commands/init/init.hpp"
#include "commands/run/run.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

Args GetArgs(int argc, char* argv[]) {
  if (argc < 2) return ArgHelp{};

  std::string_view sub = argv[1];

  if (sub == "create") {
    if (argc < 3) throw std::invalid_argument("usage: ether create <name>");
    return ArgCreate{ .project_name = argv[2] };
  }
  if (sub == "init")  return ArgInit{};
  if (sub == "build") return ArgBuild{};
  if (sub == "run")   return ArgRun{};
  if (sub == "check") {
    ArgCheck a;
    for (int i = 2; i < argc; ++i) {
      std::string_view tok = argv[i];
      if (tok == "-show-ast") {
        a.show_ast = true;
      } else if (!tok.empty() && tok.front() == '-') {
        throw std::invalid_argument("unknown check flag: `" + std::string(tok) + "`");
      } else if (a.path.empty()) {
        a.path = tok;
      } else {
        throw std::invalid_argument("unexpected positional: `" + std::string(tok) + "`");
      }
    }
    if (a.path.empty()) throw std::invalid_argument("usage: ether check <file> [-show-ast]");
    return a;
  }
  if (sub == "help" || sub == "--help" || sub == "-h") return ArgHelp{};

  throw std::invalid_argument("unknown command: `" + std::string(sub) + "`");
}

namespace {
struct Dispatcher {
  int operator()(const ArgCreate& a) const { return HandleCreate(a); }
  int operator()(const ArgInit&   a) const { return HandleInit(a);   }
  int operator()(const ArgBuild&  a) const { return HandleBuild(a);  }
  int operator()(const ArgRun&    a) const { return HandleRun(a);    }
  int operator()(const ArgCheck&  a) const { return HandleCheck(a);  }
  int operator()(const ArgHelp&   a) const { return HandleHelp(a);   }
};
}

int HandleArgs(const Args& args) {
  return std::visit(Dispatcher{}, args);
}
