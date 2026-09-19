#pragma once

#include <ether/module/module.hpp>
#include <filesystem>
#include <unordered_map>

struct ModuleID {
  std::filesystem::path module_path;
};

struct ModuleData {
  std::filesystem::path src_path;
  ModuleID id;
  Module& module;
};

// All modules, when passed, will be stored in the module registry.
// The module registry is a global dict, responsible for coordinating
// inter-module access via imports and more.
class ModuleRegistry {
public:
  auto store_module(std::string path, Module&) -> void;
private:
  std::unordered_map<ModuleID, ModuleData> registry;
};
