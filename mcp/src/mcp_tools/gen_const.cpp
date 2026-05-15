#include <mcp_tools/gen_const.hpp>
#include <optional>

mcp::tools::Const mcp::tools::make_const() {
  Const const_expr;
  const_expr.identifier = std::string({});
  const_expr.type = std::nullopt;
  const_expr.value = Construct();
  return const_expr;
}

void mcp::tools::Const::change_identifier(std::string identifier) {
  this->identifier =  identifier;
}

void mcp::tools::Const::change_type(std::string type) {
  this->type = type;
}

void mcp::tools::Const::change_value(mcp::tools::Construct value) {
  this->value = value;
}

std::string mcp::tools::Const::ToString() {
  // todo
}
