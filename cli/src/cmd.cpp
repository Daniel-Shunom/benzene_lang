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

auto GetArgs(int argc, char* argv[]) -> Args {
  if (argc < 2) {
    return ArgHelp{};
  }

  std::string_view sub = argv[1];

  if (sub == "create") {

    if (argc < 3) {
      throw std::invalid_argument("usage: ether create <name>");
    }

    return ArgCreate{ .project_name = argv[2] };
  }

  if (sub == "init") {  return ArgInit{}; }

  if (sub == "build") { return ArgBuild{}; }

  if (sub == "run") {   return ArgRun{}; }

  if (sub == "check") {
    ArgCheck arg;
    for (int i = 2; i < argc; ++i) {
      std::string_view tok = argv[i];
      if (tok == "-show-ast") {
        arg.show_ast = true;
      } else if (tok == "-show-types") {
        arg.show_types = true;
      } else if (tok == "-show-constraints") {
        arg.show_constraints = true;
      } else if (tok == "-show-unification") {
        arg.show_unification = true;
      } else if (!tok.empty() && tok.front() == '-') {
        throw std::invalid_argument("unknown check flag: `" + std::string(tok) + "`");
      } else if (arg.path.empty()) {
        arg.path = tok;
      } else {
        throw std::invalid_argument("unexpected positional: `" + std::string(tok) + "`");
      }
    }

    if (arg.path.empty()) {
      throw std::invalid_argument("usage: ether check <file> [-show-ast] [-show-types] [-show-constraints] [-show-unification]");
    }

    return arg;
  }

  if (sub == "help" || sub == "--help" || sub == "-h") {
    return ArgHelp{};
  }

  throw std::invalid_argument("unknown command: `" + std::string(sub) + "`");
}

namespace {
struct Dispatcher {
  auto operator()(const ArgCreate& arg) const -> int { return HandleCreate(arg); }
  auto operator()(const ArgInit&   arg) const -> int { return HandleInit(arg);   }
  auto operator()(const ArgBuild&  arg) const -> int { return HandleBuild(arg);  }
  auto operator()(const ArgRun&    arg) const -> int { return HandleRun(arg);    }
  auto operator()(const ArgCheck&  arg) const -> int { return HandleCheck(arg);  }
  auto operator()(const ArgHelp&   arg) const -> int { return HandleHelp(arg);   }
};
}

auto HandleArgs(const Args& args) -> int {
  return std::visit(Dispatcher{}, args);
}
