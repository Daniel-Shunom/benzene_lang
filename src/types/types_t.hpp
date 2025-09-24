#pragma once

#include <string>
#include <vector>
#include <memory>

struct FuncBody;

enum class Type {
  Integer,
  Float,
  String,
  Bool,
  Nil,
  Function
};

typedef struct {
  /*
   * This is actually so cool.
   * So since in the declaration of the function, the
   * paramters are symbols, with values, we can just,
   * look at the corresponding scoped symbol table and
   * retrieve the values from there.
  */
  std::vector<std::string> params;
  std::vector<std::unique_ptr<FuncBody>> body;
} Function;
