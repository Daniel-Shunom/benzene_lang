#include <mcp_tools/gen_case.hpp>

mcp::tools::Eval mcp::tools::make_eval() {
  return {
    .cases = {},
    .return_value = mcp::tools::Construct(),
  };
}

void mcp::tools::Eval::append_case(mcp::tools::Construct case_condition) {
  this->cases.push_back(case_condition);
}

void mcp::tools::Eval::change_return_value(mcp::tools::Construct return_value) {
  this->return_value = return_value;
}

mcp::tools::Case mcp::tools::make_case() {
  Case case_expr;
  case_expr.conditions = {};
  case_expr.evaluations = {};
  return case_expr;
}

void mcp::tools::Case::append_condition(mcp::tools::Construct case_condition) {
  this->conditions.push_back(case_condition);
}

void mcp::tools::Case::append_eval(mcp::tools::Eval eval) {
  this->evaluations.push_back(eval);
}

std::string mcp::tools::Case::ToString() {
  // todo
}
