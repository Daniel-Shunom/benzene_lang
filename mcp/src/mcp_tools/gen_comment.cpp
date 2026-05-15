#include <mcp_tools/gen_comment.hpp>
#include <string>

mcp::tools::Comment mcp::tools::make_comment() {
  Comment comment;
  comment.type = CommentType::SingleLine;
  comment.data = std::string({});
  return comment;
}

void mcp::tools::Comment::change_type(mcp::tools::CommentType type) {
  this->type = type;
}

void mcp::tools::Comment::append_data(std::string data) {
  this->data += data;
}

std::string mcp::tools::Comment::ToString() {
  // todo
}
