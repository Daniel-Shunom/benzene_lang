#include <mcp_tools/gen_let.hpp>
#include <optional>
#include <string>


mcp::tools::Let mcp::tools::make_let() {
  Let let_expr;
  let_expr.identifier = std::string({});
  let_expr.type = std::string({});
  let_expr.value = Construct();
  return let_expr;
}

void mcp::tools::Let::change_identifier(std::string identifier) {
  this->identifier = identifier;
}

void mcp::tools::Let::change_type(std::string type) {
  this->type = type;
}

void mcp::tools::Let::change_value(mcp::tools::Construct value) {
  this->value = value;
}

std::string mcp::tools::Let::ToString() {
  // todo
}
