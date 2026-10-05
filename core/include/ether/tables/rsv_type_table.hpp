#pragma once
#include <string>
#include <unordered_set>

std::unordered_set<std::string> ReservedTypes = {
  "List",
  "Tuple",
  "Set",
  "Dict",
  "Result",
  "Option",

  "Int",
  "Float",
  "String",
};
