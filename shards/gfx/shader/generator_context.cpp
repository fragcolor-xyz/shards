#include "generator.hpp"
#include "fmt.hpp"

namespace gfx::shader {

void GeneratorContext::write(const StringView &str) { getOutput() += str; }

void GeneratorContext::pushHeaderScope() {
  size_t headerIndex = headers.size();
  headers.emplace_back();
  headerStack.push_back(headerIndex);
}

void GeneratorContext::popHeaderScope() {
  assert(headerStack.size() > 0);
  headerStack.pop_back();
}

std::string &GeneratorContext::getOutput() {
  if (!headerStack.empty()) {
    return headers[headerStack.back()];
  } else {
    return result;
  }
}

void GeneratorContext::readGlobal(const char *name) {
  auto it = definitions.globals.find(name);
  if (it == definitions.globals.end()) {
    pushError(formatError("Global {} does not exist", name));
  } else {
    getOutput() += fmt::format("{}.{}", globalsVariableName, name);
  }
}
void GeneratorContext::beginWriteGlobal(const char *name, const NumFieldType &type) {
  auto it = definitions.globals.find(name);
  if (it == definitions.globals.end()) {
    definitions.globals.insert_or_assign(name, type);
  } else {
    if (it->second != type) {
      pushError(formatError("Global type doesn't match previously expected type"));
    }
  }
  getOutput() += fmt::format("{}.{} = ", globalsVariableName, name);
}
void GeneratorContext::endWriteGlobal() { getOutput() += ";\n"; }

bool GeneratorContext::hasInput(const char *name) { return definitions.inputs.find(name) != definitions.inputs.end(); }

void GeneratorContext::readInput(const char *name) {
  auto it = definitions.inputs.find(name);
  const NumFieldType *fieldType{};
  if (it != definitions.inputs.end()) {
    fieldType = &it->second;
  } else {
    fieldType = getOrCreateDynamicInput(name);
  }

  if (!fieldType) {
    pushError(formatError("Input {} does not exist", name));
    return;
  }

  getOutput() += fmt::format("{}.{}", inputVariableName, name);
}

const NumFieldType *GeneratorContext::getOrCreateDynamicInput(const char *name) {
  assert(definitions.inputs.find(name) == definitions.inputs.end());

  NumFieldType newField;
  for (auto &h : dynamicHandlers) {
    if (h->createDynamicInput(name, newField)) {
      return &definitions.inputs.insert_or_assign(name, newField).first->second;
    }
  }

  return nullptr;
}

bool GeneratorContext::hasOutput(const char *name) { return definitions.outputs.find(name) != definitions.outputs.end(); }

void GeneratorContext::writeOutput(const char *name, const NumFieldType &type) {
  auto it = definitions.outputs.find(name);
  const NumFieldType *outputFieldType{};
  if (it != definitions.outputs.end()) {
    outputFieldType = &it->second;
  } else {
    outputFieldType = getOrCreateDynamicOutput(name, type);
  }

  if (!outputFieldType) {
    pushError(formatError("Output {} does not exist", name));
    return;
  }

  if (*outputFieldType != type) {
    pushError(formatError("Output {} ({}) doesn't match previously expected type {}", name, *outputFieldType, type));
    return;
  }

  getOutput() += fmt::format("{}.{}", outputVariableName, name);
}

const NumFieldType *GeneratorContext::getOrCreateDynamicOutput(const char *name, NumFieldType requestedType) {
  assert(definitions.outputs.find(name) == definitions.outputs.end());

  for (auto &h : dynamicHandlers) {
    if (h->createDynamicOutput(name, requestedType)) {
      return &definitions.outputs.insert_or_assign(name, requestedType).first->second;
    }
  }

  return nullptr;
}

bool GeneratorContext::hasTexture(const char *name, bool defaultTexcoordRequired) {
  auto texture = getTexture(name);
  if (!texture)
    return false;
  if (defaultTexcoordRequired && !hasInput(texture->defaultTexcoordVariableName.c_str()))
    return false;
  return true;
}

const TextureDefinition *GeneratorContext::getTexture(const char *name) {
  auto it = definitions.textures.find(name);
  if (it == definitions.textures.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

void GeneratorContext::texture(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    getOutput() += texture->variableName;
  } else {
    pushError(formatError("Texture {} does not exist", name));
  }
}

void GeneratorContext::textureDefaultTextureCoordinate(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    if (hasInput(texture->defaultTexcoordVariableName.c_str())) {
      readInput(texture->defaultTexcoordVariableName.c_str());
    } else {
      getOutput() += "vec2<f32>(0.0, 0.0)";
    }
  }
}

void GeneratorContext::textureDefaultSampler(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    getOutput() += texture->defaultSamplerVariableName;
  }
}

void GeneratorContext::readBuffer(const char *fieldName, const NumFieldType &expectedType, const char *bufferName,
                                  const Function<void(IGeneratorContext &ctx)> &index) {
  auto bufferIt = definitions.buffers.find(bufferName);
  if (bufferIt == definitions.buffers.end()) {
    pushError(formatError("Buffer \"{}\" is not defined", bufferName));
    return;
  }

  const BufferDefinition &buffer = bufferIt->second;

  const UniformLayout *field = buffer.findField(fieldName);
  if (!field) {
    pushError(formatError("Field \"{}\" not found in buffer \"{}\"", fieldName, bufferName));
    return;
  }

  if (expectedType != field->type) {
    pushError(formatError("Field \"{}\", shader expected type {} but provided was {}", fieldName, expectedType, field->type));
    return;
  }

  std::visit(
      [&](auto &&arg) {
        using T1 = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T1, dim::One>) {
          getOutput() += fmt::format("{}.{}", buffer.variableName, fieldName);
        } else if constexpr (std::is_same_v<T1, dim::PerInstance>) {
          getOutput() += fmt::format("{}.elements[{}].{}", buffer.variableName, "u_instanceIndex", fieldName);
        } else {
          if (!index)
            pushError(formatError("Can not access buffer \"{}\" without index since it's an array, which requires an index",
                                  bufferName));
          getOutput() += fmt::format("{}.elements[", buffer.variableName);
          index(*this);
          getOutput() += fmt::format("].{}", fieldName);
        }
      },
      buffer.dimension);
}

void GeneratorContext::pushError(GeneratorError &&error) { errors.emplace_back(std::move(error)); }
} // namespace gfx::shader