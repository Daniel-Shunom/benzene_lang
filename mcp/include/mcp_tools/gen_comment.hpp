#include <cstdint>
#include <mcp_tools/construct.hpp>
#include <string>

namespace mcp::tools {

enum class CommentType {
  SingleLine,
  MultiLine
};

struct Comment: Construct {
  CommentType type;
  std::string data;

  void change_type(CommentType type);

  void append_data(std::string data);

  std::string ToString() override;

private:
  uint16_t max_width{80};
};

Comment make_comment();

}
