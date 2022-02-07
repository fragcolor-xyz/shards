#pragma once

#include "block.hpp"
#include "generator.hpp"
#include <string>
#include <type_traits>

namespace gfx {
namespace shader {

namespace blocks {

struct Compound : public Block {
	std::vector<UniquePtr<Block>> children;

	template <typename... TArgs> Compound(TArgs... args) { (..., children.push_back(ConvertToBlock<TArgs>{}(std::move(args)))); }

	void apply(GeneratorContext &context) const {
		for (auto &c : children)
			c->apply(context);
	}

	BlockPtr clone() {
		auto result = std::make_unique<Compound>();
		for (auto &child : children)
			result->children.push_back(child->clone());
		return std::move(result);
	}
};
template <typename... TArgs> inline auto makeCompoundBlock(TArgs &&...args) { return std::make_unique<Compound>(std::move(args)...); }

struct WithInput : public Block {
	String name;
	BlockPtr inner;
	BlockPtr innerElse;

	template <typename T> WithInput(const String &name, T &&inner) : name(name), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	template <typename T1, typename T2>
	WithInput(const String &name, T1 &&inner, T2 &&innerElse)
		: name(name), inner(ConvertToBlock<T1>{}(std::move(inner))), innerElse(ConvertToBlock<T2>{}(std::move(innerElse))) {}
	WithInput(WithInput &&other) : name(std::move(other.name)), inner(std::move(other.inner)), innerElse(std::move(other.innerElse)) {}

	void apply(GeneratorContext &context) const {
		if (context.hasInput(name.c_str())) {
			inner->apply(context);
		} else if (innerElse) {
			innerElse->apply(context);
		}
	}

	BlockPtr clone() {
		if (innerElse)
			return std::make_unique<WithInput>(name, inner->clone(), innerElse->clone());
		else
			return std::make_unique<WithInput>(name, inner->clone());
	}
};

struct WriteOutput : public Block {
	String name;
	FieldType type;
	BlockPtr inner;

	template <typename T> WriteOutput(const String &name, FieldType type, T &&inner) : name(name), type(type), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	template <typename... TArgs>
	WriteOutput(const String &name, FieldType type, TArgs &&...inner) : name(name), type(type), inner(makeCompoundBlock(std::move(inner)...)) {}
	WriteOutput(WriteOutput &&other) = default;

	void apply(GeneratorContext &context) const {
		context.writeOutput(name.c_str(), type);
		context.write(" = ");
		inner->apply(context);
		context.write(";\n");
	}

	BlockPtr clone() { return std::make_unique<WriteOutput>(name, type, inner->clone()); }
};

struct ReadInput : public Block {
	String name;

	ReadInput(const String &name) : name(name) {}
	ReadInput(ReadInput &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readInput(name.c_str()); }

	BlockPtr clone() { return std::make_unique<ReadInput>(name); }
};

struct WriteGlobal : public Block {
	String name;
	FieldType type;
	BlockPtr inner;

	template <typename T> WriteGlobal(const String &name, FieldType type, T &&inner) : name(name), type(type), inner(ConvertToBlock<T>{}(std::move(inner))) {}
	template <typename... TArgs>
	WriteGlobal(const String &name, FieldType type, TArgs &&...inner) : name(name), type(type), inner(makeCompoundBlock(std::move(inner)...)) {}
	WriteGlobal(WriteGlobal &&other) = default;

	void apply(GeneratorContext &context) const {
		context.writeGlobal(name.c_str(), type);
		context.write(" = ");
		inner->apply(context);
		context.write(";\n");
	}

	BlockPtr clone() { return std::make_unique<WriteGlobal>(name, type, inner->clone()); }
};

struct ReadGlobal : public Block {
	String name;

	ReadGlobal(const String &name) : name(name) {}
	ReadGlobal(ReadGlobal &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readGlobal(name.c_str()); }

	BlockPtr clone() { return std::make_unique<ReadGlobal>(name); }
};

struct ReadBuffer : public Block {
	String name;

	ReadBuffer(const String &name) : name(name) {}
	ReadBuffer(ReadBuffer &&other) : name(std::move(other.name)) {}

	void apply(GeneratorContext &context) const { context.readBuffer(name.c_str()); }

	BlockPtr clone() { return std::make_unique<ReadBuffer>(name); }
};

template <typename T> inline auto toBlock(T &&arg) { return ConvertibleToBlock(std::move(arg)); }
template <typename T, typename... TArgs> inline auto makeBlock(TArgs &&...args) { return std::make_unique<T>(std::move(args)...); }

} // namespace blocks
} // namespace shader
} // namespace gfx
