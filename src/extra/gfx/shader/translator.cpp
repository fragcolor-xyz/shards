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

const TranslatedWire &TranslationContext::processWire(const std::shared_ptr<SHWire> &wire,
                                                      const std::optional<FieldType> &inputType) {
  SHWire *wirePtr = wire.get();
  auto it = translatedWires.find(wirePtr);
  if (it != translatedWires.end())
    return it->second;

  TranslatedWire translated;
  translated.functionName = getUniqueVariableName(wire->name);

  SPDLOG_LOGGER_INFO(logger, "Generating shader function for wire {} => {}", wire->name, translated.functionName);

  if (wire->inputType->basicType != SHType::None) {
    assert(inputType.has_value());
    translated.inputType = inputType;
  }

  std::string inputVarName;
  std::string argsDecl;
  std::string returnDecl;

  auto functionBody = blocks::makeCompoundBlock();

  // Allocate header signature
  auto functionHeaderBlock = blocks::makeCompoundBlock();
  auto &functionHeader = *functionHeaderBlock.get();
  functionBody->children.emplace_back(std::move(functionHeaderBlock));

  // Setup input
  if (translated.inputType) {
    inputVarName = getUniqueVariableName("arg");
    argsDecl = fmt::format("{} : {}", inputVarName, getFieldWGSLTypeName(translated.inputType.value()));

    // Set the stack value from input
    setWGSLTop<WGSLBlock>(translated.inputType.value(), blocks::makeBlock<blocks::Direct>(inputVarName));
  }

  // Process wire/function contents
  TranslationBlockRef functionScope = TranslationBlockRef::make(functionBody);

  // Setup required variable
  if (!wire->pure) {

    auto &compositionResult = wire->composeResult.value();
    for (auto &req : compositionResult.requiredInfo) {
      tryExpandIntoVariable(req.name);

      const FieldType *fieldType{};
      const TranslationBlockRef *parent{};
      if (!findVariable(req.name, fieldType, parent)) {
        throw ShaderComposeError(fmt::format("Can not compose shader wire: Requred variable {} does not exist", req.name));
      }

      if (!argsDecl.empty())
        argsDecl += ", ";
      std::string wgslVarName = getUniqueVariableName(req.name);
      argsDecl += fmt::format("{}: {}", wgslVarName, getFieldWGSLTypeName(*fieldType));
      translated.arguments.emplace_back(TranslatedFunctionArgument{*fieldType, wgslVarName, req.name});

      functionScope.variables.mapUniqueVariableName(req.name, wgslVarName);
      functionScope.variables.types.insert_or_assign(req.name, *fieldType);
    }
  }

  // swap stack becasue variable in a different function can not be accessed
  decltype(stack) savedStack;
  std::swap(savedStack, stack);
  stack.emplace_back(std::move(functionScope));

  for (auto shard : wire->shards) {
    processShard(shard);
  }
  stack.pop_back();

  // Restore stack
  std::swap(savedStack, stack);

  // Grab return value
  auto returnValue = takeWGSLTop();
  if (wire->outputType.basicType != SHType::None) {
    assert(returnValue);
    translated.outputType = returnValue->getType();

    functionBody->appendLine("return ", returnValue->toBlock());

    returnDecl = fmt::format("-> {}", getFieldWGSLTypeName(translated.outputType.value()));
  }

  // Close function body
  functionBody->appendLine("}");

  // Finalize function signature
  std::string signature = fmt::format("fn {}({}) {}", translated.functionName, argsDecl, returnDecl);
  functionHeader.appendLine(signature + "{");

  SPDLOG_LOGGER_INFO(logger, "gen(wire)> {}", signature);

  // Add the definition
  // the Header block will insert it's generate code outside & before the current function
  addNew(std::make_unique<blocks::Header>(std::move(functionBody)));

  return translatedWires.emplace(std::make_pair(wirePtr, std::move(translated))).first->second;
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
    const std::string &uniqueVariableName = updateStorage(globals, varName, valueType);

    // Generate a shader source block containing the assignment
    addNew(blocks::makeBlock<blocks::WriteGlobal>(uniqueVariableName, valueType, value->toBlock()));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeBlock<blocks::ReadGlobal>(uniqueVariableName));
  } else {
    TranslationBlockRef &parent = getTop();

    // Store local variable type info & check update
    const std::string &uniqueVariableName = updateStorage(parent.variables, varName, valueType);

    // Generate a shader source block containing the assignment
    addNew(blocks::makeCompoundBlock(fmt::format("let {} = ", uniqueVariableName), value->toBlock(), ";\n"));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeCompoundBlock(uniqueVariableName));
  }
}

bool TranslationContext::tryExpandIntoVariable(const std::string &varName) {
  auto &top = getTop();
  auto it = top.virtualSequences.find(varName);
  if (it != top.virtualSequences.end()) {
    auto &virtualSeq = it->second;
    FieldType type = virtualSeq.elementType;
    type.matrixDimension = virtualSeq.elements.size();

    auto constructor = blocks::makeCompoundBlock();
    constructor->append(fmt::format("{}(", getFieldWGSLTypeName(type)));
    for (size_t i = 0; i < type.matrixDimension; i++) {
      if (i > 0)
        constructor->append(", ");
      constructor->append(virtualSeq.elements[i]->toBlock());
    }
    constructor->append(")");

    assignVariable(varName, false, false, std::make_unique<WGSLBlock>(type, std::move(constructor)));

    it = top.virtualSequences.erase(it);
    return true;
  }

  return false;
}

WGSLBlock TranslationContext::reference(const std::string &varName) {
  tryExpandIntoVariable(varName);

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
void registerWireShards();
void registerFlowShards();
void registerTranslatorShards() {
  registerCoreShards();
  registerMathShards();
  registerLinalgShards();
  registerWireShards();
  registerFlowShards();
}

} // namespace shader
} // namespace gfx
