#pragma once

#include <memory>
#include <unordered_map>
#include "../ast/node_t.hpp"

class Env {
public:
  Env(std::shared_ptr<Env> pt = nullptr):
  parent(pt)
  {};

  void define(std::string name, Value val) {
    this->sym_table[name] = val;
  }

  bool assign(const std::string &name, Value val) {
    if (this->sym_table.find(name) != this->sym_table.end()) {
      this->sym_table[name] = val;
      return true;
    }

    if (this->parent) {
      return this->parent->assign(name, val);
    }

    return false;
  }

  Value get(const std::string &name) {
    if (this->sym_table.find(name) != this->sym_table.end()) {
      return this->sym_table[name];
    }

    if (this->parent) {
      return this->parent->get(name);
    }

    throw std::exception("Invalid argument");
  }

private:
  std::unordered_map<std::string, Value> sym_table;
  std::shared_ptr<Env> parent;
};
