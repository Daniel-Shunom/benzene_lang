#include <string>
namespace mcp {

namespace tools {

struct Construct {
  Construct() = default;
  virtual std::string ToString();
  // Construct(const Construct& other) = delete;
  // Construct& operator=(Construct&&) = delete;
  // Construct(Construct&&) = delete;
};

}

}
