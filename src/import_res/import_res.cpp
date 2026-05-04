#include "import_res.hpp"
#include <algorithm>
#include <cctype>
#include <string_view>

bool is_valid_import_path(Token tok) {
  if (tok.token_type != TokenType::ImportModule) {
    return false;
  }

  std::string_view val = tok.token_value;

  bool is_val = std::ranges::all_of(val, [](auto const& c) {
    return std::isalpha(c)
      || std::islower(c)
      || c == '_'
      || c == '.';
  });


  return is_val;
}
