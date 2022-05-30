#ifndef GFX_SHADER_ENTRY_POINT
#define GFX_SHADER_ENTRY_POINT

#include "shard.hpp"
#include <gfx/enums.hpp>
#include <string>
#include <vector>

namespace gfx {
namespace shader {
using ShardPtr = std::unique_ptr<shards::Shard>;

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
  ShardPtr code;
  std::vector<NamedDependency> dependencies;

  EntryPoint() = default;
  template <typename T>
  EntryPoint(const std::string &name, ProgrammableGraphicsStage stage = ProgrammableGraphicsStage::Fragment,
             T &&code = ShardPtr())
      : stage(stage), name(name), code(shards::ConvertToShard<T>{}(std::move(code))) {}

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

#endif // GFX_SHADER_ENTRY_POINT
