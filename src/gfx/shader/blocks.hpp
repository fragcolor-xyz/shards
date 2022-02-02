#pragma once

#include "block.hpp"
#include "generator.hpp"
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
	void apply(GeneratorContext &context) const { context.write(code); }
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

	template <typename... TArgs> Compound(TArgs... args) { (..., children.push_back(ConvertToBlock<TArgs>{}(std::move(args)))); }

	void apply(GeneratorContext &context) const {
		for (auto &c : children)
			c->apply(context);
	}
};
template <typename... TArgs> inline auto makeCompoundBlock(TArgs &&...args) { return std::make_unique<Compound>(std::move(args)...); }

struct WithInput : public Block {
	String name;
	BlockPtr inner;

	template <typename T> WithInput(const String &name, T &&inner) : name(name), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	WithInput(WithInput &&other) : name(std::move(other.name)), inner(std::move(other.inner)) {}

	void apply(GeneratorContext &context) const {
		if (context.hasInput(name.c_str())) {
			inner->apply(context);
		}
	}
};

struct WriteOutput : public Block {
	String name;
	BlockPtr inner;

	template <typename T> WriteOutput(const String &name, T &&inner) : name(name), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	template <typename... TArgs> WriteOutput(const String &name, TArgs &&...inner) : name(name), inner(makeCompoundBlock(std::move(inner)...)) {}
	WriteOutput(WriteOutput &&other) : name(std::move(other.name)), inner(std::move(other.inner)) {}

	void apply(GeneratorContext &context) const {
		context.writeOutput(name.c_str());
		context.write(" = ");
		inner->apply(context);
		context.write(";\n");
	}
};

struct ReadInput : public Block {
	String name;

	ReadInput(const String &name) : name(name) {}
	ReadInput(ReadInput &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readInput(name.c_str()); }
};

struct WriteGlobal : public Block {
	String name;
	FieldType type;
	BlockPtr inner;

	template <typename T> WriteGlobal(const String &name, FieldType type, T &&inner) : name(name), type(type), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	WriteGlobal(WriteGlobal &&other) : name(std::move(other.name)), type(std::move(other.type)), inner(std::move(other.inner)) {}

	void apply(GeneratorContext &context) const {
		context.writeGlobal(name.c_str(), type);
		context.write(" = ");
		inner->apply(context);
		context.write(";\n");
	}
};

struct ReadGlobal : public Block {
	String name;

	ReadGlobal(const String &name) : name(name) {}
	ReadGlobal(ReadGlobal &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readGlobal(name.c_str()); }
};

struct ReadBuffer : public Block {
	String name;

	ReadBuffer(const String &name) : name(name) {}
	ReadBuffer(ReadBuffer &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readBuffer(name.c_str()); }
};

template <typename T> inline auto toBlock(T &&arg) { return ConvertibleToBlock(std::move(arg)); }
template <typename T, typename... TArgs> inline auto makeBlock(TArgs &&...args) { return std::make_unique<T>(std::move(args)...); }

} // namespace blocks
} // namespace shader
} // namespace gfx
