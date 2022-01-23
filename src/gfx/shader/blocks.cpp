#include "blocks.hpp"
#include "generator.hpp"

namespace gfx {
namespace shader {
namespace blocks {
void Direct::apply(GeneratorContext &context) const { context.write(code); }
} // namespace blocks
} // namespace shader
} // namespace gfx
