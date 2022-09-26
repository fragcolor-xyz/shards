#include "generator.hpp"
#include "fmt.hpp"

namespace gfx::shader {

void GeneratorContext::write(const StringView &str) { result += str; }
void GeneratorContext::writeHeader(const StringView &str) { header += str; }

void GeneratorContext::readGlobal(const char *name) {
  auto it = globals.find(name);
  if (it == globals.end()) {
    pushError(formatError("Global {} does not exist", name));
  } else {
    result += fmt::format("{}.{}", globalsVariableName, name);
  }
}
void GeneratorContext::beginWriteGlobal(const char *name, const FieldType &type) {
  auto it = globals.find(name);
  if (it == globals.end()) {
    globals.insert_or_assign(name, type);
  } else {
    if (it->second != type) {
      pushError(formatError("Global type doesn't match previously expected type"));
    }
  }
  result += fmt::format("{}.{} = ", globalsVariableName, name);
}
void GeneratorContext::endWriteGlobal() { result += ";\n"; }

bool GeneratorContext::hasInput(const char *name) { return inputs.find(name) != inputs.end(); }

void GeneratorContext::readInput(const char *name) {
  auto it = inputs.find(name);
  const FieldType *fieldType{};
  if (it != inputs.end()) {
    fieldType = &it->second;
  } else {
    fieldType = getOrCreateDynamicInput(name);
  }

  if (!fieldType) {
    pushError(formatError("Input {} does not exist", name));
    return;
  }

  result += fmt::format("{}.{}", inputVariableName, name);
}

const FieldType *GeneratorContext::getOrCreateDynamicInput(const char *name) {
  assert(inputs.find(name) == inputs.end());

  FieldType newField;
  for (auto &h : dynamicHandlers) {
    if (h->createDynamicInput(name, newField)) {
      return &inputs.insert_or_assign(name, newField).first->second;
    }
  }

  return nullptr;
}

bool GeneratorContext::hasOutput(const char *name) { return outputs.find(name) != outputs.end(); }

void GeneratorContext::writeOutput(const char *name, const FieldType &type) {
  auto it = outputs.find(name);
  const FieldType *outputFieldType{};
  if (it != outputs.end()) {
    outputFieldType = &it->second;
  } else {
    outputFieldType = getOrCreateDynamicOutput(name, type);
  }

  if (!outputFieldType) {
    pushError(formatError("Output {} does not exist", name));
    return;
  }

  if (*outputFieldType != type) {
    pushError(formatError("Output type doesn't match previously expected type"));
    return;
  }

  result += fmt::format("{}.{}", outputVariableName, name);
}

const FieldType *GeneratorContext::getOrCreateDynamicOutput(const char *name, FieldType requestedType) {
  assert(outputs.find(name) == outputs.end());

  for (auto &h : dynamicHandlers) {
    if (h->createDynamicOutput(name, requestedType)) {
      return &outputs.insert_or_assign(name, requestedType).first->second;
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
  auto it = textures.find(name);
  if (it == textures.end()) {
    return nullptr;
  } else {
    return &it->second;
  }
}

void GeneratorContext::texture(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    result += texture->variableName;
  } else {
    pushError(formatError("Texture {} does not exist", name));
  }
}

void GeneratorContext::textureDefaultTextureCoordinate(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    if (hasInput(texture->defaultTexcoordVariableName.c_str())) {
      readInput(texture->defaultTexcoordVariableName.c_str());
    } else {
      result += "vec2<f32>(0.0, 0.0)";
    }
  }
}

void GeneratorContext::textureDefaultSampler(const char *name) {
  if (const TextureDefinition *texture = getTexture(name)) {
    result += texture->defaultSamplerVariableName;
  }
}

void GeneratorContext::readBuffer(const char *fieldName, const FieldType &expectedType, const char *bufferName) {
  auto bufferIt = buffers.find(bufferName);
  if (bufferIt == buffers.end()) {
    pushError(formatError("Buffer \"{}\" is not defined", bufferName));
    return;
  }

  const BufferDefinition &buffer = bufferIt->second;

  const UniformLayout *uniform = findUniform(fieldName, buffer);
  if (!uniform) {
    pushError(formatError("Field \"{}\" not found in buffer \"{}\"", fieldName, bufferName));
    return;
  }

  if (expectedType != uniform->type) {
    pushError(formatError("Field \"{}\", shader expected type {} but provided was {}", fieldName, expectedType, uniform->type));
    return;
  }

  if (buffer.indexedBy) {
    result += fmt::format("{}.elements[{}].{}", buffer.variableName, *buffer.indexedBy, fieldName);
  } else {
    result += fmt::format("{}.{}", buffer.variableName, fieldName);
  }
}

const UniformLayout *GeneratorContext::findUniform(const char *fieldName, const BufferDefinition &buffer) {
  for (size_t i = 0; i < buffer.layout.fieldNames.size(); i++) {
    if (buffer.layout.fieldNames[i] == fieldName) {
      return &buffer.layout.items[i];
    }
  }
  return nullptr;
}

void GeneratorContext::pushError(GeneratorError &&error) { errors.emplace_back(std::move(error)); }
} // namespace gfx::shader