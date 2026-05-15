#include <mcp_tools/construct.hpp>
#include <string>
#include <vector>

namespace mcp::tools {

struct Eval {
  std::vector<Construct> cases;
  Construct return_value;

  void append_case(Construct case_condition);

  void change_return_value(Construct return_value);

  std::string ToString();
};

struct Case: Construct {
  std::vector<Construct> conditions;
  std::vector<Eval> evaluations;

  void append_condition(Construct condition);

  void append_eval(Eval eval);

  std::string ToString() override;
};

Case make_case();

Eval make_eval();

}
