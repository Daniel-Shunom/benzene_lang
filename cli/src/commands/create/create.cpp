#include "create.hpp"
#include <cstdio>

int HandleCreate(const ArgCreate& a) {
  std::fprintf(stderr, "ether: `create %s` not yet implemented\n",
               a.project_name.c_str());
  return 1;
}
