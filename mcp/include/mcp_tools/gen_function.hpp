#include <mcp_tools/construct.hpp>
#include <optional>
#include <string>
#include <vector>

namespace mcp::tools {

struct Func: Construct {
  std::string identifier;
  std::optional<std::string> return_type;
  std::vector<std::string> param_list;
  std::vector<Construct> body;

  void change_identifier(std::string identifier);

  void change_return_type(std::optional<std::string> type);

  void add_to_param_list(std::string func_param);

  void add_construct_to_body(Construct construct);

  std::string ToString() override;
};

Func make_func();

}
