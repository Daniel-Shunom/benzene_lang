#include <mcp_tools/construct.hpp>
#include <optional>
#include <string>

namespace mcp::tools {

struct Const: Construct {
  std::string identifier;
  std::optional<std::string> type;
  Construct value;

  void change_identifier(std::string identifer);

  void change_type(std::string type);

  void change_value(Construct value);

  std::string ToString() override;
};

Const make_const();

}
