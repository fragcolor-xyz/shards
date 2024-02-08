#include "translator.hpp"
#include "translator_utils.hpp"
#include "../shards_utils.hpp"
#include "spdlog/spdlog.h"
#include <shards/common_types.hpp>
#include <shards/core/foundation.hpp>
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

void TranslationContext::finalize() {
  auto top = takeWGSLTop();
  if(top) {
    const std::string &varName = getUniqueVariableName("unused");
    addNew(blocks::makeCompoundBlock(fmt::format("let {} = ", varName), top->toBlock(), ";\n"));
  }
}

const TranslatedFunction &TranslationContext::processWire(const std::shared_ptr<SHWire> &wire,
                                                          const std::optional<Type> &inputType) {
  SHWire *wirePtr = wire.get();
  auto it = translatedWires.find(wirePtr);
  if (it != translatedWires.end())
    return it->second;

  TranslatedFunction translated = processShards(wire->shards, wire->composeResult.value(), inputType, wire->name);

  return translatedWires.emplace(std::make_pair(wirePtr, std::move(translated))).first->second;
}

TranslatedFunction TranslationContext::processShards(const std::vector<ShardPtr> &shards, const SHComposeResult &composeResult,
                                                     const std::optional<Type> &inputType, const std::string &name) {
  TranslatedFunction translated;
  translated.functionName = getUniqueVariableName(name);
  SPDLOG_LOGGER_TRACE(logger, "Generating shader function for shards {} => {}", name, translated.functionName);

  if (inputType) {
    assert(inputType.has_value());
    translated.inputType = inputType;
  }

  // Save input value
  std::unique_ptr<IWGSLGenerated> savedWgslTop =
      wgslTop ? std::make_unique<WGSLBlock>(wgslTop->getType(), wgslTop->toBlock()) : nullptr;

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
    argsDecl = fmt::format("{} : {}", inputVarName, getWGSLTypeName(translated.inputType.value()));

    // Set the stack value from input
    setWGSLTop<WGSLBlock>(translated.inputType.value(), blocks::makeBlock<blocks::Direct>(inputVarName));
  }

  // Process wire/function contents
  TranslationBlockRef functionScope = TranslationBlockRef::make(functionBody);

  // Setup required variable
  if (composeResult.requiredInfo.len > 0) {
    for (auto &req : composeResult.requiredInfo) {
      tryExpandIntoVariable(req.name);

      const Type *fieldType{};
      const TranslationBlockRef *parent{};
      if (!findVariable(req.name, fieldType, parent)) {
        throw ShaderComposeError(fmt::format("Can not compose shader wire: Requred variable {} does not exist", req.name));
      }

      if (!argsDecl.empty())
        argsDecl += ", ";
      std::string wgslVarName = getUniqueVariableName(req.name);
      argsDecl += fmt::format("{}: {}", wgslVarName, getWGSLTypeName(*fieldType));
      translated.arguments.emplace_back(TranslatedFunctionArgument{*fieldType, wgslVarName, req.name});

      functionScope.variables.mapUniqueVariableName(req.name, wgslVarName);
      functionScope.variables.variables.insert_or_assign(req.name, VariableInfo(*fieldType, true));
    }
  }

  // swap stack becasue variable in a different function can not be accessed
  decltype(stack) savedStack;
  std::swap(savedStack, stack);
  stack.emplace_back(std::move(functionScope));

  for (ShardPtr shard : shards) {
    processShard(shard);
  }
  stack.pop_back();

  // Restore stack
  std::swap(savedStack, stack);

  // Grab return value
  auto returnValue = takeWGSLTop();
  if (composeResult.outputType.basicType != SHType::None) {
    assert(returnValue);
    translated.outputType = returnValue->getType();

    functionBody->appendLine("return ", returnValue->toBlock());

    returnDecl = fmt::format("-> {}", getWGSLTypeName(translated.outputType.value()));
  }

  // Restore input value
  wgslTop = std::move(savedWgslTop);

  // Close function body
  functionBody->appendLine("}");

  // Finalize function signature
  std::string signature = fmt::format("fn {}({}) {}", translated.functionName, argsDecl, returnDecl);
  functionHeader.appendLine(signature + "{");

  SPDLOG_LOGGER_TRACE(logger, "gen(function)> {}", signature);

  // Insert function definition into root so it will be processed first
  // the Header block will insert it's generate code outside & before the shader entry point
  root->children.emplace_back(std::make_unique<blocks::Header>(std::move(functionBody)));

  return translated;
}

bool TranslationContext::findVariableGlobal(const std::string &varName, const Type *&outFieldType) const {
  auto globalIt = globals.variables.find(varName);
  if (globalIt != globals.variables.end()) {
    outFieldType = &globalIt->second.type;
    return true;
  }
  return false;
}

bool TranslationContext::findVariable(const std::string &varName, const Type *&outFieldType,
                                      const TranslationBlockRef *&outParent) const {
  for (auto it = stack.crbegin(); it != stack.crend(); ++it) {
    auto &variables = it->variables;
    auto it1 = variables.variables.find(varName);
    if (it1 != variables.variables.end()) {
      outFieldType = &it1->second.type;
      outParent = &*it;
      return true;
    }
  }

  outParent = nullptr;
  return findVariableGlobal(varName, outFieldType);
}

WGSLBlock TranslationContext::assignVariable(const std::string &varName, bool global, bool allowUpdate, bool isMutable,
                                             std::unique_ptr<IWGSLGenerated> &&value) {
  Type valueType = value->getType();

  auto updateStorage = [&](VariableStorage &storage, const std::string &varName, const Type &fieldType,
                           bool forceNewVariable = false) {
    const std::string *outVariableName{};

    auto &variables = storage.variables;
    auto it = variables.find(varName);
    if (forceNewVariable || it == variables.end()) {
      variables.insert_or_assign(varName, fieldType);

      auto tmpName = getUniqueVariableName(varName);
      outVariableName = &storage.mapUniqueVariableName(varName, tmpName);
    } else {
      if (allowUpdate) {
        if (it->second.type != value->getType()) {
          throw ShaderComposeError(
              fmt::format("Can not update \"{}\". Expected: {}, got {} instead", varName, it->second.type, fieldType));
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

    const NumType *numType = std::get_if<NumType>(&valueType);
    if (!numType)
      throw ShaderComposeError(fmt::format("Can not assign {} globally, unsupported type: {}", varName, valueType));

    // Generate a shader source block containing the assignment
    addNew(blocks::makeBlock<blocks::WriteGlobal>(uniqueVariableName, *numType, value->toBlock()));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeBlock<blocks::ReadGlobal>(uniqueVariableName));
  } else {
    const Type *foundFieldType{};
    TranslationBlockRef *parent{};
    bool isNewVariable = true;
    if (allowUpdate) {
      const TranslationBlockRef *foundParent{};
      if (findVariable(varName, foundFieldType, foundParent)) {
        parent = const_cast<decltype(parent)>(foundParent);
        isNewVariable = false;

        // In case of function parameters, show the variables, since they cannot be assigned otherwise
        auto it = parent->variables.variables.find(varName);
        if (it->second.isReadOnly) {
          isNewVariable = true;
          it->second.isReadOnly = false;
        }
      }
    } else {
      parent = &getTop();
    }

    if (!parent)
      throw ShaderComposeError(fmt::format("Can not assign \"{}\", not a valid scope", varName));

    // Store local variable type info & check update
    const std::string &uniqueVariableName = updateStorage(parent->variables, varName, valueType, isNewVariable);

    // Generate a shader source block containing the assignment
    if (isNewVariable) {
      if (isMutable) {
        addNew(blocks::makeCompoundBlock(fmt::format("var {} = ", uniqueVariableName), value->toBlock(), ";\n"));
      } else {
        addNew(blocks::makeCompoundBlock(fmt::format("let {} = ", uniqueVariableName), value->toBlock(), ";\n"));
      }
    } else
      addNew(blocks::makeCompoundBlock(fmt::format("{} = ", uniqueVariableName), value->toBlock(), ";\n"));

    // Push reference to assigned value
    return WGSLBlock(valueType, blocks::makeCompoundBlock(uniqueVariableName));
  }
}

bool TranslationContext::tryExpandIntoVariable(const std::string &varName) {
  auto &top = getTop();
  auto it = top.virtualSequences.find(varName);
  if (it != top.virtualSequences.end()) {
    auto &virtualSeq = it->second;
    NumType type = virtualSeq.elementType;
    type.matrixDimension = virtualSeq.elements.size();

    auto constructor = blocks::makeCompoundBlock();
    constructor->append(fmt::format("{}(", getWGSLTypeName(type)));
    for (size_t i = 0; i < type.matrixDimension; i++) {
      if (i > 0)
        constructor->append(", ");
      constructor->append(virtualSeq.elements[i]->toBlock());
    }
    constructor->append(")");

    assignVariable(varName, false, false, true, std::make_unique<WGSLBlock>(type, std::move(constructor)));

    it = top.virtualSequences.erase(it);
    return true;
  }

  return false;
}

WGSLBlock TranslationContext::reference(const std::string &varName) {
  tryExpandIntoVariable(varName);

  const Type *fieldType{};
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
