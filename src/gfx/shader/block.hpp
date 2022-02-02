#pragma once

namespace gfx {
namespace shader {
struct GeneratorContext;
namespace blocks {
struct Block {
	virtual ~Block() = default;
	virtual void apply(GeneratorContext &context) const = 0;
};
} // namespace blocks
} // namespace shader
} // namespace gfx
