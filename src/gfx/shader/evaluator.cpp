#include "evaluator.hpp"
#include <spdlog/fmt/fmt.h>

namespace gfx {
namespace shader {

template <typename T> static void formatAttributeReference(T &output, const MeshVertexAttribute &attr) { output += fmt::format("input.{}", attr.name); };

bool EvaluatorContext::hasAttribute(const char *attributeName) { return attributes.find(attributeName) != attributes.end(); }
void EvaluatorContext::outputAttributeReference(const char *attributeName) {
	auto it = attributes.find(attributeName);
	assert(it != attributes.end());
	const MeshVertexAttribute &attr = *it->second;
	formatAttributeReference(result, attr);
}
void EvaluatorContext::output(const StringView &str) { result += str; }

String Evaluator::build(const blocks::Block &input) {
	EvaluatorContext context;
	for (auto &attr : meshFormat.vertexAttributes) {
		context.attributes.insert_or_assign(attr.name, &attr);
	}
	input.apply(context);
	return context.result;
}
} // namespace shader
} // namespace gfx
