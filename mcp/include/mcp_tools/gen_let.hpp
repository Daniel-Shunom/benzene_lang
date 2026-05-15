#include <optional>
#include <ether/ast/print/print.hpp>
#include <string>
#include <mcp_tools/construct.hpp>

namespace mcp::tools {

struct Let: Construct {
  std::string identifier;
  std::optional<std::string> type;
  Construct value;

  void change_identifier(std::string identifier);

  void change_type(std::string type);

  void change_value(Construct value);

  std::string ToString() override;
};

Let make_let();

}
