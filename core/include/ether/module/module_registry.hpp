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

class ModuleRegistry {
public:
  auto store_module(std::string path, Module&) -> void;
  auto get_module_data(ModuleID) -> ModuleData&;
private:
  std::unordered_map<ModuleID, ModuleData> registry;
};
