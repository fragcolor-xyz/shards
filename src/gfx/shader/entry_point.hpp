#pragma once

#include "block.hpp"
#include <gfx/enums.hpp>
#include <string>
#include <vector>

namespace gfx {
namespace shader {
using BlockPtr = std::unique_ptr<blocks::Block>;

enum class DependencyType { Before, After };
struct NamedDependency {
  std::string name;
  DependencyType type = DependencyType::After;

  NamedDependency() = default;
  NamedDependency(std::string name, DependencyType type = DependencyType::After) : name(name), type(type) {}

  template <typename T> void hashStatic(T &hasher) const {
    hasher(name);
    hasher(type);
  }
};

struct EntryPoint {
  ProgrammableGraphicsStage stage;
  std::string name;
  BlockPtr code;
  std::vector<NamedDependency> dependencies;

  EntryPoint() = default;
  template <typename T>
  EntryPoint(const std::string &name, ProgrammableGraphicsStage stage = ProgrammableGraphicsStage::Fragment,
             T &&code = BlockPtr())
      : stage(stage), name(name), code(blocks::ConvertToBlock<T>{}(std::move(code))) {}

  EntryPoint(EntryPoint &&other) = default;
  EntryPoint &operator=(EntryPoint &&other) = default;

  template <typename T> void hashStatic(T &hasher) const {
    hasher(stage);
    hasher(name);
    hasher(code);
    hasher(dependencies);
  }
};

} // namespace shader
} // namespace gfx
