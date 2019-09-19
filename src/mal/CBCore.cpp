#define String MalString
#include "Environment.h"
#include "MAL.h"
#include "StaticList.h"
#include "Types.h"
#undef String
#include "../core/blocks/shared.hpp"
#include "../core/runtime.hpp"
#include <algorithm>
#include <set>

#define CHECK_ARGS_IS(expected)                                                \
  checkArgsIs(name.c_str(), expected, std::distance(argsBegin, argsEnd))

#define CHECK_ARGS_BETWEEN(min, max)                                           \
  checkArgsBetween(name.c_str(), min, max, std::distance(argsBegin, argsEnd))

#define CHECK_ARGS_AT_LEAST(expected)                                          \
  checkArgsAtLeast(name.c_str(), expected, std::distance(argsBegin, argsEnd))

#define ARG(type, name) type *name = VALUE_CAST(type, *argsBegin++)

static StaticList<malBuiltIn *> handlers;
#define FUNCNAME(uniq) cb_builtIn##uniq
#define HRECNAME(uniq) cb_handler##uniq
#define BUILTIN_DEF(uniq, symbol)                                              \
  static malBuiltIn::ApplyFunc FUNCNAME(uniq);                                 \
  static StaticList<malBuiltIn *>::Node HRECNAME(uniq)(                        \
      handlers, new malBuiltIn(symbol, FUNCNAME(uniq)));                       \
  malValuePtr FUNCNAME(uniq)(const MalString &name, malValueIter argsBegin,    \
                             malValueIter argsEnd)

#define BUILTIN(symbol) BUILTIN_DEF(__LINE__, symbol)

#define BUILTIN_ISA(symbol, type)                                              \
  BUILTIN(symbol) {                                                            \
    CHECK_ARGS_IS(1);                                                          \
    return mal::boolean(DYNAMIC_CAST(type, *argsBegin));                       \
  }

extern void cbRegisterAllBlocks();

void registerKeywords(malEnvPtr env);

namespace chainblocks {
CBlock *createBlockInnerCall();
}

void installCBCore(const malEnvPtr &env) {
  chainblocks::installSignalHandlers();

  cbRegisterAllBlocks();

  registerKeywords(env);

  // Chain params
  env->set(":Looped", mal::keyword(":Looped"));
  env->set(":Unsafe", mal::keyword(":Unsafe"));
  // CBType
  env->set(":None", mal::keyword(":None"));
  env->set(":Any", mal::keyword(":Any"));
  env->set(":Object", mal::keyword(":Object"));
  env->set(":Enum", mal::keyword(":Enum"));
  env->set(":Bool", mal::keyword(":Bool"));
  env->set(":Int", mal::keyword(":Int"));
  env->set(":Int2", mal::keyword(":Int2"));
  env->set(":Int3", mal::keyword(":Int3"));
  env->set(":Int4", mal::keyword(":Int4"));
  env->set(":Int8", mal::keyword(":Int8"));
  env->set(":Int16", mal::keyword(":Int16"));
  env->set(":Float", mal::keyword(":Float"));
  env->set(":Float2", mal::keyword(":Float2"));
  env->set(":Float3", mal::keyword(":Float3"));
  env->set(":Float4", mal::keyword(":Float4"));
  env->set(":Color", mal::keyword(":Color"));
  env->set(":Chain", mal::keyword(":Chain"));
  env->set(":Block", mal::keyword(":Block"));
  env->set(":String", mal::keyword(":String"));
  env->set(":ContextVar", mal::keyword(":ContextVar"));
  env->set(":Image", mal::keyword(":Image"));
  env->set(":Seq", mal::keyword(":Seq"));
  env->set(":Table", mal::keyword(":Table"));

  for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it) {
    malBuiltIn *handler = *it;
    env->set(handler->name(), handler);
  }

  rep("(def! inc (fn* [a] (+ a 1)))", env);
  rep("(def! dec (fn* (a) (- a 1)))", env);
  rep("(def! zero? (fn* (n) (= 0 n)))", env);
  rep("(def! identity (fn* (x) x))", env);
  rep("(def! gensym (let* [counter (atom 0)] (fn* [] (symbol (str \"G__\" "
      "(swap! counter inc))))))",
      env);
  rep("(defmacro! or (fn* [& xs] (if (< (count xs) 2) (first xs) (let* [r "
      "(gensym)] `(let* (~r ~(first xs)) (if ~r ~r (or ~@(rest xs))))))))",
      env);
  rep("(defmacro! and (fn* (& xs) (cond (empty? xs) true (= 1 (count xs)) "
      "(first xs) true (let* (condvar (gensym)) `(let* (~condvar ~(first xs)) "
      "(if ~condvar (and ~@(rest xs)) ~condvar))))))",
      env);
  rep("(def! _alias_add_implicit (fn* [special added] (fn* [x & xs] (list "
      "special x (cons added xs)))))",
      env);
  rep("(defmacro! let  (_alias_add_implicit 'let* 'do))", env);
  rep("(defmacro! when (_alias_add_implicit 'if   'do))", env);
  rep("(defmacro! def  (_alias_add_implicit 'def! 'do))", env);
  rep("(defmacro! fn   (_alias_add_implicit 'fn*  'do))", env);
  rep("(defmacro! defn (_alias_add_implicit 'def! 'fn))", env);
  rep("(def! partial (fn* [pfn & args] (fn* [& args-inner] (apply pfn (concat "
      "args args-inner)))))",
      env);
  rep("(def! Do (fn* [chain] (RunChain chain :Once false :Passthrough false "
      ":Mode RunChainMode.Inline)))",
      env);
  rep("(def! DoOnce (fn* [chain] (RunChain chain :Once true :Passthrough false "
      ":Mode RunChainMode.Inline)))",
      env);
  rep("(def! Dispatch (fn* [chain] (RunChain chain :Once false :Passthrough "
      "true :Mode RunChainMode.Inline)))",
      env);
  rep("(def! DispatchOnce (fn* [chain] (RunChain chain :Once true :Passthrough "
      "true :Mode RunChainMode.Inline)))",
      env);
  rep("(def! Detach (fn* [chain] (RunChain chain :Once false :Passthrough true "
      ":Mode RunChainMode.Detached)))",
      env);
  rep("(def! DetachOnce (fn* [chain] (RunChain chain :Once true :Passthrough "
      "true :Mode RunChainMode.Detached)))",
      env);
  rep("(def! Step (fn* [chain] (RunChain chain :Once false :Passthrough "
      "false :Mode RunChainMode.Stepped)))",
      env);
}

class malCBChain : public malValue {
public:
  malCBChain(const MalString &name) : m_chain(new CBChain(name.c_str())) {}

  malCBChain(CBChain *chain) : m_chain(chain) {}

  malCBChain(const malCBChain &that, const malValuePtr &meta)
      : malValue(meta), m_chain(that.m_chain) {
    // Chains are unique!
    consume();
  }

  ~malCBChain() {
    if (m_chain)
      delete m_chain;
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "CBChain: " << m_chain->name;
    return stream.str();
  }

  CBChain *value() const {
    if (!m_chain)
      throw chainblocks::CBException("Attempted to use a chain that has been "
                                     "consumed. Chains are unique.");
    return m_chain;
  }

  void consume() { m_chain = nullptr; }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_chain == static_cast<const malCBChain *>(rhs)->m_chain;
  }

  WITH_META(malCBChain);

private:
  CBChain *m_chain;
};

class malCBBlock : public malValue {
public:
  malCBBlock(const MalString &name)
      : m_block(chainblocks::createBlock(name.c_str())) {}

  malCBBlock(CBlock *block) : m_block(block) {}

  malCBBlock(const malCBBlock &that, const malValuePtr &meta)
      : malValue(meta), m_block(that.m_block), m_innerRefs(that.m_innerRefs) {
    consume();
  }

  ~malCBBlock() {
    if (m_block)
      m_block->destroy(m_block);
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "Block: " << m_block->name(m_block);
    return stream.str();
  }

  CBlock *value() const {
    if (!m_block)
      throw chainblocks::CBException("Attempted to use a block that has been "
                                     "consumed. Blocks are unique.");
    return m_block;
  }

  void consume() { m_block = nullptr; }

  void addChain(const malCBChain *chain) {
    m_innerRefs.insert(chain->acquire());
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_block == static_cast<const malCBBlock *>(rhs)->m_block;
  }

  WITH_META(malCBBlock);

private:
  CBlock *m_block;
  std::set<const RefCounted *> m_innerRefs;
};

class malCBNode : public malValue {
public:
  malCBNode() : m_node(new CBNode()) {}

  malCBNode(const malCBNode &that, const malValuePtr &meta)
      : malValue(meta), m_node(that.m_node), m_innerRefs(that.m_innerRefs) {
    consume();
  }

  ~malCBNode() { delete m_node; }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "CBNode";
    return stream.str();
  }

  CBNode *value() const {
    if (!m_node)
      throw chainblocks::CBException(
          "Attempted to use a node that has been consumed. Nodes are unique.");
    return m_node;
  }

  void consume() { m_node = nullptr; }

  void schedule(malCBChain *chain) {
    m_node->schedule(chain->value());
    m_innerRefs.insert(chain->acquire());
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_node == static_cast<const malCBNode *>(rhs)->m_node;
  }

  WITH_META(malCBNode);

private:
  CBNode *m_node;
  std::set<const RefCounted *> m_innerRefs;
};

class malCBVar : public malValue {
public:
  malCBVar(CBVar var) : m_var(var) {}

  malCBVar(const malCBVar &that, const malValuePtr &meta) : malValue(meta) {
    m_var = CBVar();
    chainblocks::cloneVar(m_var, that.m_var);
  }

  ~malCBVar() { chainblocks::destroyVar(m_var); }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << m_var;
    return stream.str();
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_var == static_cast<const malCBVar *>(rhs)->m_var;
  }

  CBVar value() const { return m_var; }

  WITH_META(malCBVar);

  CBVar m_var;
};

CBType keywordToType(malKeyword *typeKeyword) {
  auto value = typeKeyword->value();
  if (value == ":None")
    return CBType::None;
  else if (value == ":Any")
    return CBType::Any;
  else if (value == ":Enum")
    return CBType::Enum;
  else if (value == ":Bool")
    return CBType::Bool;
  else if (value == ":Int")
    return CBType::Int;
  else if (value == ":Int2")
    return CBType::Int2;
  else if (value == ":Int3")
    return CBType::Int3;
  else if (value == ":Int4")
    return CBType::Int4;
  else if (value == ":Int8")
    return CBType::Int8;
  else if (value == ":Int16")
    return CBType::Int16;
  else if (value == ":Float")
    return CBType::Float;
  else if (value == ":Float2")
    return CBType::Float2;
  else if (value == ":Float3")
    return CBType::Float3;
  else if (value == ":Float4")
    return CBType::Float4;
  else if (value == ":Color")
    return CBType::Color;
  else if (value == ":Chain")
    return CBType::Chain;
  else if (value == ":Block")
    return CBType::Block;
  else if (value == ":String")
    return CBType::String;
  else if (value == ":ContextVar")
    return CBType::ContextVar;
  else if (value == ":Image")
    return CBType::Image;
  else if (value == ":Seq")
    return CBType::Seq;
  else if (value == ":Table")
    return CBType::Table;
  else
    throw chainblocks::CBException(
        "Could not infer type for special '.' block. Need to return a proper "
        "type keyword!");
}

malValuePtr typeToKeyword(CBType type) {
  switch (type) {
  case EndOfBlittableTypes:
  case None:
    return mal::keyword(":None");
  case Any:
    return mal::keyword(":Any");
  case Object:
    return mal::keyword(":Object");
  case Enum:
    return mal::keyword(":Enum");
  case Bool:
    return mal::keyword(":Bool");
  case Int:
    return mal::keyword(":Int");
  case Int2:
    return mal::keyword(":Int2");
  case Int3:
    return mal::keyword(":Int3");
  case Int4:
    return mal::keyword(":Int4");
  case Int8:
    return mal::keyword(":Int8");
  case Int16:
    return mal::keyword(":Int16");
  case Float:
    return mal::keyword(":Float");
  case Float2:
    return mal::keyword(":Float2");
  case Float3:
    return mal::keyword(":Float3");
  case Float4:
    return mal::keyword(":Float4");
  case Color:
    return mal::keyword(":Color");
  case Chain:
    return mal::keyword(":Chain");
  case Block:
    return mal::keyword(":Block");
  case String:
    return mal::keyword(":String");
  case ContextVar:
    return mal::keyword(":ContextVar");
  case Image:
    return mal::keyword(":Image");
  case Seq:
    return mal::keyword(":Seq");
  case Table:
    return mal::keyword(":Table");
  };
  return mal::keyword(":None");
}

namespace chainblocks {
struct InnerCall {
  malValuePtr malInfer; // a fn* [inputType]
  malList *malActivateList;
  malValuePtr malActivate; // a fn* [input]
  malCBVar *innerVar;
  CBVar outputVar;

  void init(const malValuePtr &infer, const malValuePtr &activate) {
    malInfer = infer;

    auto avec = new malValueVec();
    avec->push_back(activate);

    innerVar = new malCBVar(CBVar());
    avec->push_back(malValuePtr(innerVar));

    malActivateList = new malList(avec);
    malActivate = malValuePtr(malActivateList);
  }

  CBTypesInfo inputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypesInfo outputTypes() { return CBTypesInfo(SharedTypes::anyInfo); }

  CBTypeInfo inferTypes(CBTypeInfo inputType,
                        CBExposedTypesInfo consumableVariables) {
    // call a fn* [inputTypeKeyword] in place
    auto ivec = new malValueVec();
    ivec->push_back(malInfer);
    ivec->push_back(typeToKeyword(inputType.basicType));
    auto res = EVAL(malValuePtr(new malList(ivec)), nullptr);

    auto typeKeyword = VALUE_CAST(malKeyword, res);
    auto returnType = CBTypeInfo();
    returnType.basicType = keywordToType(typeKeyword);

    return returnType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cloneVar(innerVar->m_var, input);
    auto res = EVAL(malActivate, nullptr);
    auto resStaticVar = STATIC_CAST(
        malCBVar, res); // for perf here we use static, it's dangerous tho!
    cloneVar(outputVar, resStaticVar->m_var);
    return outputVar;
  }
};

RUNTIME_CORE_BLOCK(InnerCall);
RUNTIME_BLOCK_inputTypes(InnerCall);
RUNTIME_BLOCK_outputTypes(InnerCall);
RUNTIME_BLOCK_inferTypes(InnerCall);
RUNTIME_BLOCK_activate(InnerCall);
RUNTIME_BLOCK_END(InnerCall);
}; // namespace chainblocks

BUILTIN(".") {
  CHECK_ARGS_IS(2);
  ARG(malLambda, infer);
  ARG(malLambda, activate);
  auto blk = chainblocks::createBlockInnerCall();
  auto inner = reinterpret_cast<chainblocks::InnerCallRuntime *>(blk);
  inner->core.init(infer, activate);
  return malValuePtr(new malCBBlock(blk));
}

BUILTIN("Node") { return malValuePtr(new malCBNode()); }

#define WRAP_TO_CONST(_var_)                                                   \
  auto constBlock = chainblocks::createBlock("Const");                         \
  constBlock->setup(constBlock);                                               \
  constBlock->setParam(constBlock, 0, _var_);                                  \
  result.push_back(constBlock)

// Helper to generate const blocks automatically inferring types
std::vector<CBlock *> blockify(const malValuePtr &arg) {
  std::vector<CBlock *> result;

  if (arg == mal::nilValue()) {
    // Wrap none into const
    CBVar var;
    var.valueType = None;
    var.payload.chainState = Continue;
    WRAP_TO_CONST(var);
  } else if (const malString *v = DYNAMIC_CAST(malString, arg)) {
    CBVar strVar;
    strVar.valueType = String;
    auto s = v->value();
    strVar.payload.stringValue = s.c_str();
    WRAP_TO_CONST(strVar);
  } else if (const malNumber *v = DYNAMIC_CAST(malNumber, arg)) {
    auto value = v->value();
    CBVar var{};
    if (v->isInteger()) {
      var.valueType = Int;
      var.payload.intValue = value;
    } else {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    WRAP_TO_CONST(var);
  } else if (arg == mal::trueValue()) {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = true;
    WRAP_TO_CONST(var);
  } else if (arg == mal::falseValue()) {
    CBVar var;
    var.valueType = Bool;
    var.payload.boolValue = false;
    WRAP_TO_CONST(var);
  } else if (const malCBVar *v = DYNAMIC_CAST(malCBVar, arg)) {
    WRAP_TO_CONST(v->value());
  } else if (const malCBBlock *v = DYNAMIC_CAST(malCBBlock, arg)) {
    auto block = v->value();
    // Blocks are unique, consume before use, assume goes inside another block
    const_cast<malCBBlock *>(v)->consume();
    result.push_back(block);
  } else if (const malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    auto count = v->count();
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto blks = blockify(val);
      result.insert(result.end(), blks.begin(), blks.end());
    }
  } else {
    throw chainblocks::CBException("Invalid argument for chain");
  }
  return result;
}

CBVar varify(malCBBlock *mblk, const malValuePtr &arg) {
  // Returns clones in order to proper cleanup (nested) allocations
  if (arg == mal::nilValue()) {
    CBVar var{};
    var.valueType = None;
    var.payload.chainState = Continue;
    return var;
  } else if (const malString *v = DYNAMIC_CAST(malString, arg)) {
    CBVar tmp{};
    tmp.valueType = String;
    auto s = v->value();
    tmp.payload.stringValue = s.c_str();
    CBVar var{};
    chainblocks::cloneVar(var, tmp);
    return var;
  } else if (const malNumber *v = DYNAMIC_CAST(malNumber, arg)) {
    auto value = v->value();
    CBVar var{};
    if (v->isInteger()) {
      var.valueType = Int;
      var.payload.intValue = value;
    } else {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    return var;
  } else if (const malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    CBVar tmp{};
    tmp.valueType = Seq;
    tmp.payload.seqValue = nullptr;
    tmp.payload.seqLen = -1;
    auto count = v->count();
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto subVar = varify(mblk, val);
      stbds_arrpush(tmp.payload.seqValue, subVar);
    }
    CBVar var{};
    chainblocks::cloneVar(var, tmp);
    stbds_arrfree(tmp.payload.seqValue);
    return var;
  } else if (arg == mal::trueValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    return var;
  } else if (arg == mal::falseValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    return var;
  } else if (const malCBVar *v = DYNAMIC_CAST(malCBVar, arg)) {
    CBVar var{};
    chainblocks::cloneVar(var, v->value());
    return var;
  } else if (const malCBBlock *v = DYNAMIC_CAST(malCBBlock, arg)) {
    auto block = v->value();
    // Blocks are unique, consume before use, assume goes inside another block
    const_cast<malCBBlock *>(v)->consume();
    CBVar var{};
    var.valueType = Block;
    var.payload.blockValue = block;
    return var;
  } else if (malCBChain *v = DYNAMIC_CAST(malCBChain, arg)) {
    auto chain = v->value();
    CBVar var{};
    var.valueType = Chain;
    var.payload.chainValue = chain;
    if (mblk)
      mblk->addChain(v);
    else
      v->consume();
    return var;
  } else {
    throw chainblocks::CBException("Invalid variable");
  }
}

int findParamIndex(const std::string &name, CBParametersInfo params) {
  for (auto i = 0; stbds_arrlen(params) > i; i++) {
    auto param = params[i];
    if (param.name == name)
      return i;
  }
  return -1;
}

void validationCallback(const CBlock *errorBlock, const char *errorTxt,
                        bool nonfatalWarning, void *userData) {
  auto block = const_cast<CBlock *>(errorBlock);
  if (!nonfatalWarning) {
    LOG(ERROR) << "Parameter validation failed: " << errorTxt
               << ", block: " << block->name(block);
  } else {
    LOG(INFO) << "Parameter validation warning: " << errorTxt
              << ", block: " << block->name(block);
  }
}

void setBlockParameters(malCBBlock *malblock, malValueIter begin,
                        malValueIter end) {
  auto block = malblock->value();
  auto argsBegin = begin;
  auto argsEnd = end;
  auto params = block->parameters(block);
  auto pindex = 0;
  while (argsBegin != argsEnd) {
    auto arg = *argsBegin++;
    if (const malKeyword *v = DYNAMIC_CAST(malKeyword, arg)) {
      // In this case it's a keyworded call from now on
      pindex = -1;

      // Jump to next value straight, expecting a value
      auto value = *argsBegin++;
      auto paramName = v->value().substr(1);
      auto idx = findParamIndex(paramName, params);
      if (unlikely(idx == -1)) {
        LOG(ERROR) << "Parameter not found: " << paramName
                   << " block: " << block->name(block);
        throw chainblocks::CBException("Parameter not found");
      } else {
        auto var = varify(malblock, value);
        if (!validateSetParam(block, idx, var, validationCallback, nullptr)) {
          LOG(ERROR) << "Failed parameter: " << paramName;
          throw chainblocks::CBException("Parameter validation failed");
        }
        block->setParam(block, idx, var);
        chainblocks::destroyVar(var);
      }
    } else if (pindex == -1) {
      // We expected a keyword! fail
      LOG(ERROR) << "Keyword expected, block: " << block->name(block);
      throw chainblocks::CBException("Keyword expected");
    } else {
      auto var = varify(malblock, arg);
      if (!validateSetParam(block, pindex, var, validationCallback, nullptr)) {
        LOG(ERROR) << "Failed parameter index: " << pindex;
        throw chainblocks::CBException("Parameter validation failed");
      }
      block->setParam(block, pindex, var);
      chainblocks::destroyVar(var);

      pindex++;
    }
  }
}

BUILTIN("Chain") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, chainName);
  auto mchain = new malCBChain(chainName->value());
  auto chain = mchain->value();
  while (argsBegin != argsEnd) {
    auto arg = *argsBegin++;
    if (const malKeyword *v = DYNAMIC_CAST(malKeyword, arg)) // Options!
    {
      if (v->value() == ":Looped") {
        chain->looped = true;
      } else if (v->value() == ":Unsafe") {
        chain->unsafe = true;
      }
    } else {
      auto blks = blockify(arg);
      for (auto blk : blks)
        chain->addBlock(blk);
    }
  }
  return malValuePtr(mchain);
}

BUILTIN("-->") {
  auto vec = new malValueVec();
  while (argsBegin != argsEnd) {
    auto arg = *argsBegin++;
    auto blks = blockify(arg);
    for (auto blk : blks)
      vec->push_back(malValuePtr(new malCBBlock(blk)));
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("schedule") {
  CHECK_ARGS_IS(2);
  ARG(malCBNode, node);
  ARG(malCBChain, chain);
  node->schedule(chain);
  return mal::nilValue();
}

BUILTIN("prepare") {
  CHECK_ARGS_IS(1);
  ARG(malCBChain, chain);
  chainblocks::prepare(chain->value());
  return mal::nilValue();
}

BUILTIN("start") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malCBChain, chain);
  if (argsBegin != argsEnd) {
    ARG(malCBVar, inputVar);
    chainblocks::start(chain->value(), inputVar->value());
  } else {
    chainblocks::start(chain->value());
  }
  return mal::nilValue();
}

BUILTIN("tick") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    auto ticked = chainblocks::tick(v->value());
    return mal::boolean(ticked);
  } else if (const malCBNode *v = DYNAMIC_CAST(malCBNode, first)) {
    auto noErrors = v->value()->tick();
    return mal::boolean(noErrors);
  } else {
    throw chainblocks::CBException("tick Expected Node or Chain");
  }
  return mal::nilValue();
}

BUILTIN("stop") {
  CHECK_ARGS_IS(1);
  ARG(malCBChain, chain);
  CBVar res{};
  chainblocks::stop(chain->value(), &res);
  return malValuePtr(new malCBVar(res));
}

BUILTIN("run") {
  CHECK_ARGS_AT_LEAST(1);
  CBNode *node = nullptr;
  CBChain *chain = nullptr;
  auto first = *argsBegin++;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    chain = v->value();
  } else if (const malCBNode *v = DYNAMIC_CAST(malCBNode, first)) {
    node = v->value();
  } else {
    throw chainblocks::CBException("tick Expected Node or Chain");
  }

  auto sleepTime = 0.0;
  if (argsBegin != argsEnd) {
    ARG(malNumber, argSleepTime);
    sleepTime = argSleepTime->value();
  }

  if (node) {
    while (!node->empty()) {
      auto noErrors = node->tick();
      if (!noErrors)
        return mal::boolean(false);
      chainblocks::sleep(sleepTime);
    }
  } else {
    while (!chainblocks::tick(chain)) {
      chainblocks::sleep(sleepTime);
    }
  }

  return mal::boolean(true);
}

BUILTIN("sleep") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, sleepTime);
  chainblocks::sleep(sleepTime->value());
  return mal::nilValue();
}

BUILTIN("#") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  CBVar tmp;
  tmp.valueType = ContextVar;
  auto s = value->value();
  tmp.payload.stringValue = s.c_str();
  auto var = CBVar();
  chainblocks::cloneVar(var, tmp);
  return malValuePtr(new malCBVar(var));
}

malValuePtr newEnum(int32_t vendor, int32_t type, CBEnum value) {
  CBVar var;
  var.valueType = Enum;
  var.payload.enumVendorId = vendor;
  var.payload.enumTypeId = type;
  var.payload.enumValue = value;
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Enum") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Enum;
  var.payload.enumVendorId = static_cast<int32_t>(value0->value());
  var.payload.enumTypeId = static_cast<int32_t>(value1->value());
  var.payload.enumValue = static_cast<CBEnum>(value2->value());
  return malValuePtr(new malCBVar(var));
}

BUILTIN("String") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);

  CBVar tmp;
  tmp.valueType = String;
  auto s = value->value();
  tmp.payload.stringValue = s.c_str();
  auto var = CBVar();
  chainblocks::cloneVar(var, tmp);

  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  CBVar var;
  var.valueType = Int;
  var.payload.intValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var;
  var.valueType = Int2;
  var.payload.int2Value[0] = value0->value();
  var.payload.int2Value[1] = value1->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int3") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Int3;
  var.payload.int3Value[0] = value0->value();
  var.payload.int3Value[1] = value1->value();
  var.payload.int3Value[2] = value2->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int4") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Int4;
  var.payload.int4Value[0] = value0->value();
  var.payload.int4Value[1] = value1->value();
  var.payload.int4Value[2] = value2->value();
  var.payload.int4Value[3] = value3->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Color") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Color;
  var.payload.colorValue.r = static_cast<uint8_t>(value0->value());
  var.payload.colorValue.g = static_cast<uint8_t>(value1->value());
  var.payload.colorValue.b = static_cast<uint8_t>(value2->value());
  var.payload.colorValue.a = static_cast<uint8_t>(value3->value());
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  CBVar var;
  var.valueType = Float;
  var.payload.floatValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var;
  var.valueType = Float2;
  var.payload.float2Value[0] = value0->value();
  var.payload.float2Value[1] = value1->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float3") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var;
  var.valueType = Float3;
  var.payload.float3Value[0] = value0->value();
  var.payload.float3Value[1] = value1->value();
  var.payload.float3Value[2] = value2->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float4") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  CBVar var;
  var.valueType = Float4;
  var.payload.float4Value[0] = value0->value();
  var.payload.float4Value[1] = value1->value();
  var.payload.float4Value[2] = value2->value();
  var.payload.float4Value[3] = value3->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("json") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    json jchain = v->value();
    return malValuePtr(mal::string(jchain.dump()));
  } else if (const malCBVar *v = DYNAMIC_CAST(malCBVar, first)) {
    json jchain = v->value();
    return malValuePtr(mal::string(jchain.dump()));
  }
  return mal::nilValue();
}

BUILTIN("ChainJson") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  try {
    auto jchain = json::parse(value->value());
    CBChain *chain = jchain.get<CBChain *>();
    return malValuePtr(new malCBChain(chain));
  } catch (json::exception &e) {
    LOG(ERROR) << "Failed to parse a json chain, " << e.what();
    return mal::nilValue();
  }
}

BUILTIN_ISA("Var?", malCBVar);
BUILTIN_ISA("Node?", malCBNode);
BUILTIN_ISA("Chain?", malCBChain);
BUILTIN_ISA("Block?", malCBBlock);

extern "C" {
EXPORTED __cdecl void cbLispInit() { malinit(); }

EXPORTED __cdecl CBVar cbLispEval(const char *str) {
  try {
    auto res = maleval(str);
    return varify(nullptr, res);
  } catch (...) {
    return chainblocks::Empty;
  }
}
};

#ifdef HAS_CB_GENERATED
#include "CBGenerated.hpp"
#endif