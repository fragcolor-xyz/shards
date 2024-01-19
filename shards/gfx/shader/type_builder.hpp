#include "types.hpp"
#include "utils.hpp"
#include "wgsl_mapping.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/container/small_vector.hpp>
#include <spdlog/fmt/fmt.h>

namespace gfx::shader {

struct TypeBuilder {
  boost::container::flat_map<StructType, String> structTypeNames;
  std::string generatedCode;
  size_t counter{};

  String generateStructName() { return fmt::format("Gen_{}", counter++); }

  void writeStructDefinition(FastString name, const StructType &layout) {
    // Before writing anything, resolve dependent types
    boost::container::small_vector<FastString, 32> resolvedTypeNames;
    for (size_t i = 0; i < layout->entries.size(); i++) {
      resolvedTypeNames.emplace_back(resolveTypeName(layout->entries[i].type));
    }

    generatedCode += fmt::format("struct {} {{\n", name);

    for (size_t i = 0; i < layout->entries.size(); i++) {
      generatedCode += fmt::format("\t{}: {},\n", sanitizeIdentifier(layout->entries[i].name), resolvedTypeNames[i]);
    }

    generatedCode += "};\n";
  }

  String resolveTypeName(StructType type) {
    auto it = structTypeNames.find(type);
    if (it != structTypeNames.end())
      return it->second;

    auto name = generateStructName();
    writeStructDefinition(name, type);
    structTypeNames.emplace(type, name);
    return name;
  }

  String resolveTypeName(ArrayType type) {
    if (type->fixedLength > 0)
      return fmt::format("array<{}, {}>", resolveTypeName(type->elementType), *type->fixedLength);
    else
      return fmt::format("array<{}>", resolveTypeName(type->elementType));
  }
  String resolveTypeName(NumType type) { return getWGSLTypeName(type); }
  String resolveTypeName(SamplerType type) { return getWGSLTypeName(type); }
  String resolveTypeName(TextureType type) { return getWGSLTypeName(type); }

  String resolveTypeName(const Type &type) {
    return std::visit([&](auto &arg) -> String { return resolveTypeName(arg); }, type);
  }
};
} // namespace gfx::shader