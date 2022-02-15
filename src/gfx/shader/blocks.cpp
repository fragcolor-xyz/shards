#include "blocks.hpp"
#include "generator.hpp"
#include <boost/algorithm/string.hpp>

namespace gfx {
namespace shader {
namespace blocks {
void Direct::apply(GeneratorContext &context) const { context.write(code); }

DefaultInterpolation::DefaultInterpolation() {
	matchPrefixes = {
		"texCoord",
	};
}
void DefaultInterpolation::apply(GeneratorContext &context) const {
	for (auto &input : context.inputs) {
		const std::string &name = input.first;
		if (!context.hasOutput(name.c_str())) {
			for (auto &prefix : matchPrefixes) {
				if (boost::algorithm::starts_with(name, prefix)) {
					context.writeOutput(name.c_str(), input.second->type);
					context.write(" = ");
					context.readInput(name.c_str());
					context.write(";\n");
					break;
				}
			}
		}
	}
}
} // namespace blocks
} // namespace shader
} // namespace gfx
