#pragma once
#include "ether/nodes/node_visitor.hpp"
#include <ether/types/types.hpp>

class TypeChecker;

class TCModule: public Visitor {
public:
  explicit TCModule(TypeChecker& ctx) : context(ctx) {}

protected:
  TypeChecker& context;
};
