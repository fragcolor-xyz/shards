#pragma once

namespace gfx {
namespace shader {
struct EvaluatorContext;
namespace blocks {
struct Block {
	virtual ~Block() = default;
	virtual void apply(EvaluatorContext &context) const = 0;
};
} // namespace blocks
} // namespace shader
} // namespace gfx
