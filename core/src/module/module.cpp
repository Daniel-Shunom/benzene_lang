#include <ether/module/module.hpp>
#include <ether/lexer/lexer.hpp>
#include <ether/parser/parsers.hpp>
#include <cstdio>
#include <utility>

void Module::make_module_ast() {
  this->diag.set_source(this->module_path, this->source_text);

  Lexer lexer(this->source_text, this->diag);
  lexer.scan_tokens();
  auto toks = lexer.get_tokens();

  ParserState state(this->diag);
  state.set_state(toks);
  state.activate_logs();
  auto parent = run_parser(state);

  if (!parent) {
    std::fprintf(stderr, "[ERR] Failed to parse module `%s`\n", this->module_path.data());
    return;
  }

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

void Module::print_errors(std::ostream& out) {
  this->diag.print_all(out);
}

Parent Module::get_ast() {
  return std::move(this->module_root);
}
