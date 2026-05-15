#include <mcp_tools/gen_function.hpp>
#include <string>
#include <optional>
#include <vector>

mcp::tools::Func mcp::tools::make_func() {
  Func func_expr;
  func_expr.identifier = std::string({});
  func_expr.return_type = std::nullopt;
  func_expr.param_list = {};
  func_expr.body = {};
  return func_expr;
}

void mcp::tools::Func::change_identifier(std::string identifier) {
  this->identifier = identifier;
}

void mcp::tools::Func::change_return_type(std::optional<std::string> type) {
  this->return_type = type;
}

void mcp::tools::Func::add_to_param_list(std::string func_param) {
  this->param_list.push_back(func_param);
}

void mcp::tools::Func::add_construct_to_body(mcp::tools::Construct construct) {
  this->body.push_back(construct);
}

std::string mcp::tools::Func::ToString() {
  // todo
}
