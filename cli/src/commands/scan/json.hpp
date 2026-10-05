#pragma once
#include <string>

// Minimal JSON emission helpers for the `scan` command. The LSP is the only
// consumer, so this covers exactly what the scan payload needs: escaped
// strings and plain numbers. No parser -- nothing here reads JSON back.
namespace json {

// Escapes per RFC 8259. Control characters below 0x20 take the \u00XX form;
// bytes >= 0x20 pass through untouched, which keeps UTF-8 sequences intact.
inline auto escape(const std::string& in) -> std::string {
  std::string out;
  out.reserve(in.size() + 2);
  for (unsigned char c : in) {
    switch (c) {
      case '"':  out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          constexpr char hex[] = "0123456789abcdef";
          out += "\\u00";
          out += hex[(c >> 4) & 0xF];
          out += hex[c & 0xF];
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

inline auto quote(const std::string& in) -> std::string {
  return "\"" + escape(in) + "\"";
}

}  // namespace json
