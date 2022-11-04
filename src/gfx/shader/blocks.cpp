#include "blocks.hpp"
#include "generator.hpp"
#include <boost/algorithm/string.hpp>

namespace gfx {
namespace shader {
namespace blocks {
void Direct::apply(IGeneratorContext &context) const { context.write(code); }
void Header::apply(IGeneratorContext &context) const {
  context.pushHeaderScope();
  inner->apply(context);
  context.popHeaderScope();
  }

DefaultInterpolation::DefaultInterpolation() {
  matchPrefixes = {
      "texCoord",
  };
}

void DefaultInterpolation::apply(IGeneratorContext &context) const {
  for (auto &input : context.getDefinitions().inputs) {
    const std::string &name = input.first;
    if (!context.hasOutput(name.c_str())) {
      for (auto &prefix : matchPrefixes) {
        if (boost::algorithm::starts_with(name, prefix)) {
          context.writeOutput(name.c_str(), input.second);
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
