#pragma once

#include "block.hpp"
#include "evaluator.hpp"
#include <string>
#include <type_traits>

namespace gfx {
namespace shader {

using String = std::string;
template <typename T> using UniquePtr = std::unique_ptr<T>;

namespace blocks {
struct Direct : public Block {
	String code;

	Direct(const char *code) : code(code) {}
	void apply(EvaluatorContext &context) const { context.output(code); }
};

struct ConvertibleToBlock {
	UniquePtr<Block> block;
	ConvertibleToBlock(const char *str) { block = std::make_unique<Direct>(str); }
	ConvertibleToBlock(UniquePtr<Block> &&block) : block(std::move(block)) {}
	UniquePtr<Block> operator()() { return std::move(block); }
};

template <typename T, typename T1 = void> struct ConvertToBlock {};
template <typename T> struct ConvertToBlock<T, typename std::enable_if<std::is_base_of_v<Block, T>>::type> {
	UniquePtr<Block> operator()(T &&block) { return std::make_unique<T>(std::move(block)); }
};
template <typename T> struct ConvertToBlock<T, typename std::enable_if<!std::is_base_of_v<Block, T>>::type> {
	UniquePtr<Block> operator()(T &&arg) { return ConvertibleToBlock(std::move(arg))(); }
};

struct Compound : public Block {
	std::vector<UniquePtr<Block>> children;

	template <typename... TArgs> Compound(TArgs... args) {
		(..., children.push_back(ConvertToBlock<TArgs>{}(std::move(args))));
		// TConvertibleToBlock<TArgs>()(args)...
		// }
	}

	void apply(EvaluatorContext &context) const {
		for (auto &c : children)
			c->apply(context);
	}
};

struct WithAttribute : public Block {
	String attributeName;
	UniquePtr<Block> block;

	template <typename T> WithAttribute(const String &attributeName, T &&inner) : attributeName(attributeName), block(ConvertToBlock<T>{}(std::move(inner))) {}
	WithAttribute(WithAttribute &&other) : attributeName(std::move(other.attributeName)), block(std::move(other.block)) {}

	void apply(EvaluatorContext &context) const {
		if (context.hasAttribute(attributeName.c_str())) {
			block->apply(context);
		}
	}
};

// template <typename... TArgs> auto convertToBlock(TArgs&&... args) { return std::make_unique<Compound>(std::move(args)...); };
// template <typename T> auto convertToBlock(T &&arg) { return TConvertibleToBlock<T>()(arg); };
// template <size_t N> auto convertToBlock(const char (&arg)[N]) { return std::make_unique<Direct>(arg); }
// inline auto convertToBlock(const char *arg) { return std::make_unique<Direct>(arg); }

template <typename T> inline auto toBlock(T &&arg) { return ConvertibleToBlock(std::move(arg)); }
template <typename T, typename... TArgs> inline auto makeBlock(TArgs &&...args) { return std::make_unique<T>(std::move(args)...); }
template <typename... TArgs> inline auto makeCompoundBlock(TArgs &&...args) { return std::make_unique<Compound>(std::move(args)...); }

} // namespace blocks
} // namespace shader
} // namespace gfx
