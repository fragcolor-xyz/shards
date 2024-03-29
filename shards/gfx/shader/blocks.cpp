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
    FastString name = input.first;
    if (!context.hasOutput(name)) {
      for (auto &prefix : matchPrefixes) {
        if (boost::algorithm::starts_with(name.str(), prefix)) {
          context.writeOutput(name, input.second);
          context.write(" = ");
          context.readInput(name);
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
