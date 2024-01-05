#ifndef GFX_SHADER_ENTRY_POINT
#define GFX_SHADER_ENTRY_POINT

#include "../fwd.hpp"
#include "block.hpp"
#include <gfx/enums.hpp>
#include <string>
#include <vector>

namespace gfx {
namespace shader {
using BlockPtr = std::unique_ptr<blocks::Block>;

enum class DependencyType { Before, After };
struct NamedDependency {
  FastString name;
  DependencyType type = DependencyType::After;

  NamedDependency() = default;
  NamedDependency(FastString name, DependencyType type = DependencyType::After) : name(name), type(type) {}

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(name);
    hasher(type);
  }
};

struct EntryPoint {
  ProgrammableGraphicsStage stage;
  FastString name;
  BlockPtr code;
  std::vector<NamedDependency> dependencies;

  EntryPoint() = default;
  template <typename T>
  EntryPoint(FastString name, ProgrammableGraphicsStage stage = ProgrammableGraphicsStage::Fragment,
             T &&code = BlockPtr())
      : stage(stage), name(name), code(blocks::ConvertToBlock<T>{}(std::forward<T>(code))) {}

  EntryPoint(EntryPoint &&other) = default;
  EntryPoint &operator=(EntryPoint &&other) = default;

  template <typename T> void getPipelineHash(T &hasher) const {
    hasher(stage);
    hasher(name);
    hasher(code);
    hasher(dependencies);
  }
};

} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_ENTRY_POINT
