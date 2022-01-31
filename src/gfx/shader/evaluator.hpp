#pragma once

#include "block.hpp"
#include <gfx/mesh.hpp>
#include <map>
#include <string>


namespace gfx {
namespace shader {
using String = std::string;
using StringView = std::string_view;

struct EvaluatorContext {
	std::string result;
	std::map<std::string, MeshVertexAttribute *> attributes;

	bool hasAttribute(const char *attributeName);
	void outputAttributeReference(const char *attributeName);
	void output(const StringView &str);
};

struct Evaluator {
	struct MeshFormat meshFormat;
	String build(const blocks::Block &input);
};
} // namespace shader
} // namespace gfx
