#include "module.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <utility>
#include "../lexer/lexer.hpp"
#include "../parser/parsers.hpp"


std::string Module::extract_contents_from_path() {
  std::ifstream file(this->module_path);

  if (!file.is_open()) {
    return std::string{}; // Idealy we need to better handle this
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  return buffer.str();
}

void Module::make_module_ast() {
  std::string str = this->extract_contents_from_path();
  this->diag.set_source(this->module_path, str);
  auto str_view = std::string_view(str);
  Lexer lexer(str_view, this->diag);
  lexer.scan_tokens();
  auto toks = lexer.get_tokens();

  auto state = ParserState(this->diag);
  state.set_state(toks);
  state.activate_logs();
  auto parent = run_parser(state);

  if (!parent) {
    printf("[ERR]\tFailed to parse module `%s`", this->module_path.data());
    return;
  };

  this->module_root = std::move(parent.value());
}

void Module::attach_visitor(Visitor& visitor) {
  this->module_root.add_visitor(visitor);
}

void Module::generate_ast() {
  this->make_module_ast();
}

void Module::apply_visitors() {
  this->module_root.apply_visitors();
}

void Module::print_errors() {
  this->diag.print_all();
}

Parent Module::get_ast() {
  return std::move(this->module_root);
}
