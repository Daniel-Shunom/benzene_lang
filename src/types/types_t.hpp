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
  Function,
  Undefined
};


typedef struct {
  std::string name;
  Type type;
} Param;

typedef struct {
  std::string name;
  std::vector<Param> params;
  Type return_type;
} FunctionDef;

