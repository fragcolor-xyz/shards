#include "translator.hpp"
#include "translator_utils.hpp"
#include "../shards_utils.hpp"
#include "spdlog/spdlog.h"
#include <common_types.hpp>
#include <foundation.hpp>
#include <gfx/shader/log.hpp>
#include <gfx/shader/generator.hpp>
#include <gfx/shader/fmt.hpp>

using namespace shards;
namespace gfx {
namespace shader {
TranslationRegistry &getTranslationRegistry() {
  static TranslationRegistry instance;
  return instance;
}

TranslationContext::TranslationContext() : translationRegistry(getTranslationRegistry()) {
  logger = shader::getLogger();

  root = std::make_unique<blocks::Compound>();
  stack.push_back(TranslationBlockRef::make(root));
}

void TranslationContext::processShard(ShardPtr shard) {
  ITranslationHandler *handler = translationRegistry.resolve(shard);
  if (!handler) {
    throw ShaderComposeError(fmt::format("No shader translation available for shard {}", shard->name(shard)), shard);
  }

  handler->translate(shard, *this);
}

bool TranslationContext::findVariableGlobal(const std::string &varName, const FieldType *&outFieldType) const {
  auto globalIt = globals.types.find(varName);
  if (globalIt != globals.types.end()) {
    outFieldType = &globalIt->second;
    return true;
  }
  return false;
}

bool TranslationContext::findVariable(const std::string &varName, const FieldType *&outFieldType,
                                      const TranslationBlockRef *&outParent) const {
  for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
    auto &variables = it->variables;
    auto it1 = variables.types.find(varName);
    if (it1 != variables.types.end()) {
      outFieldType = &it1->second;
      outParent = &*it;
      return true;
    }
  }

  outParent = nullptr;
  return findVariableGlobal(varName, outFieldType);
}

WGSLBlock TranslationContext::assignVariable(const std::string &varName, bool global, bool allowUpdate,
                                             std::unique_ptr<IWGSLGenerated> &&value) {
  FieldType valueType = value->getType();

  auto updateStorage = [&](VariableStorage &storage, const std::string &varName, const FieldType &fieldType) {
    const std::string *outVariableName{};

    auto &types = storage.types;
    auto it = types.find(varName);
    if (it == types.end()) {
      types.insert_or_assign(varName, fieldType);

      auto tmpName = getUniqueVariableName(varName);
      outVariableName = &storage.mapUniqueVariableName(varName, tmpName);
    } else {
      if (allowUpdate) {
        if (it->second != value->getType()) {
          throw ShaderComposeError(
              fmt::format("Can not update \"{}\". Expected: {}, got {} instead", varName, it->second, fieldType));
        }
        outVariableName = &storage.resolveUniqueVariableName(varName);
      } else {
        throw ShaderComposeError(fmt::format("Can not set \"{}\", it's already set", varName));
      }
    }

    return *outVariableName;
  };

  if (global) {
    // Store global variable type info & check update
    const std::string& uniqueVariableName = updateStorage(globals, varName, valueType);

    // Generate a shader source block containing the assignment
    addNew(blocks::makeBlock<blocks::WriteGlobal>(uniqueVariableName, valueType, value->toBlock()));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeBlock<blocks::ReadGlobal>(uniqueVariableName));
  } else {
    TranslationBlockRef &parent = getTop();

    // Store local variable type info & check update
    const std::string& uniqueVariableName = updateStorage(parent.variables, varName, valueType);

    // Generate a shader source block containing the assignment
    addNew(blocks::makeCompoundBlock(fmt::format("let {} = ", uniqueVariableName), value->toBlock(), ";\n"));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeCompoundBlock(uniqueVariableName));
  }
}

WGSLBlock TranslationContext::reference(const std::string &varName) {
  const FieldType *fieldType{};
  const TranslationBlockRef *parent{};
  if (!findVariable(varName, fieldType, parent)) {
    throw ShaderComposeError(fmt::format("Can not get/ref: Variable {} does not exist here", varName));
  }

  if (parent) {
    return WGSLBlock(*fieldType, blocks::makeCompoundBlock(parent->variables.resolveUniqueVariableName(varName)));
  } else {
    return WGSLBlock(*fieldType, blocks::makeBlock<blocks::ReadGlobal>(globals.resolveUniqueVariableName(varName)));
  }
}

void TranslationRegistry::registerHandler(const char *blockName, ITranslationHandler *translateable) {
  handlers.insert_or_assign(blockName, translateable);
}

ITranslationHandler *TranslationRegistry::resolve(ShardPtr shard) {
  auto it = handlers.find((const char *)shard->name(shard));
  if (it != handlers.end())
    return it->second;
  return nullptr;
}

void registerCoreShards();
void registerMathShards();
void registerLinalgShards();
void registerTranslatorShards() {
  registerCoreShards();
  registerMathShards();
  registerLinalgShards();
}

} // namespace shader
} // namespace gfx
