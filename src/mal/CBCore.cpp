/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#define String MalString
#include "Environment.h"
#include "MAL.h"
#include "StaticList.h"
#include "Types.h"
#undef String
#include "../core/blocks/shared.hpp"
#include "../core/runtime.hpp"
#include "rigtorp/SPSCQueue.h"
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#include <boost/process/environment.hpp>
#include <filesystem>
#include <set>
#include <thread>

#ifndef _WIN32
#include <dlfcn.h>
#endif

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

class malCBChain;
class malCBlock;
class malCBNode;
class malCBVar;
typedef RefCountedPtr<malCBChain> malCBChainPtr;
typedef RefCountedPtr<malCBlock> malCBlockPtr;
typedef RefCountedPtr<malCBNode> malCBNodePtr;
typedef RefCountedPtr<malCBVar> malCBVarPtr;

void registerKeywords(malEnvPtr env);
malCBVarPtr varify(malCBlock *mblk, const malValuePtr &arg);

namespace fs = std::filesystem;

namespace chainblocks {
CBlock *createBlockInnerCall();
} // namespace chainblocks

struct Observer;
void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env);

static bool initDoneOnce = false;
static std::map<MalString, malValuePtr> builtIns;
static std::map<malEnv *, std::shared_ptr<Observer>> observers;

void installCBCore(const malEnvPtr &env) {
  std::shared_ptr<Observer> obs;
  setupObserver(obs, env);

  if (!initDoneOnce) {
    chainblocks::installSignalHandlers();
    cbRegisterAllBlocks();
    initDoneOnce = true;
  }

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
  env->set(":Bytes", mal::keyword(":Bytes"));

  // inject static handlers
  for (auto it = handlers.begin(), end = handlers.end(); it != end; ++it) {
    malBuiltIn *handler = *it;
    env->set(handler->name(), handler);
  }

  // inject discovered loaded values
  // TODO
  // this is not efficient as it likely will repeat inserting the first time!
  // See Observer down
  for (auto &v : builtIns) {
    env->set(v.first, v.second.ptr());
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
  rep("(defn range [from to] (when (<= from to) (cons from (range (inc from) "
      "to))))",
      env);
#ifdef _WIN32
  rep("(def platform \"windows\")", env);
#elif __linux__
  rep("(def platform \"linux\")", env);
#elif __APPLE__
  rep("(def platform \"apple\")", env);
#endif
}

class malRoot {
public:
  void reference(malValue *val) { m_refs.emplace_back(val); }

private:
  std::vector<malValuePtr> m_refs;
};

class malCBChain : public malValue, public malRoot {
public:
  malCBChain(const MalString &name) : m_chain(new CBChain(name.c_str())) {
    LOG(TRACE) << "Created a CBChain - " << name;
  }

  malCBChain(CBChain *chain) : m_chain(chain) {
    LOG(TRACE) << "Aquired a CBChain - " << chain->name;
  }

  malCBChain(const malCBChain &that, const malValuePtr &meta) = delete;

  ~malCBChain() {
    LOG(TRACE) << "Deleting a CBChain - " << m_chain->name;
    assert(m_chain);
    delete m_chain;
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "CBChain: " << m_chain->name;
    return stream.str();
  }

  CBChain *value() const { return m_chain; }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_chain == static_cast<const malCBChain *>(rhs)->m_chain;
  }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw chainblocks::CBException("Meta not supported on chainblocks chains.");
  }

private:
  CBChain *m_chain;
};

class malCBlock : public malValue, public malRoot {
public:
  malCBlock(const MalString &name)
      : m_block(chainblocks::createBlock(name.c_str())) {}

  malCBlock(CBlock *block) : m_block(block) {}

  malCBlock(const malCBlock &that, const malValuePtr &meta) = delete;

  ~malCBlock() {
    // could be nullptr
    if (m_block) {
      m_block->destroy(m_block);
    }
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "Block: " << m_block->name(m_block);
    return stream.str();
  }

  CBlock *value() const {
    if (!m_block) {
      throw chainblocks::CBException(
          "Attempted to use a null block, blocks are unique, "
          "probably was already used.");
    }
    return m_block;
  }

  void consume() { m_block = nullptr; }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_block == static_cast<const malCBlock *>(rhs)->m_block;
  }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw chainblocks::CBException("Meta not supported on chainblocks blocks.");
  }

private:
  CBlock *m_block;
};

class malCBNode : public malValue, public malRoot {
public:
  malCBNode() : m_node(new CBNode()) { LOG(TRACE) << "Created a CBNode"; }

  malCBNode(const malCBNode &that, const malValuePtr &meta) = delete;

  ~malCBNode() {
    assert(m_node);
    delete m_node;
    LOG(TRACE) << "Deleted a CBNode";
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "CBNode";
    return stream.str();
  }

  CBNode *value() const { return m_node; }

  void schedule(malCBChain *chain) {
    m_node->schedule(chain->value());
    reference(chain);
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_node == static_cast<const malCBNode *>(rhs)->m_node;
  }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw chainblocks::CBException("Meta not supported on chainblocks nodes.");
  }

private:
  CBNode *m_node;
};

class malCBVar : public malValue, public malRoot {
public:
  malCBVar(CBVar &var, bool cloned = false) : m_var(var), m_cloned(cloned) {}

  malCBVar(const malCBVar &that, const malValuePtr &meta)
      : malValue(meta), m_cloned(true) {
    m_var = CBVar();
    chainblocks::cloneVar(m_var, that.m_var);
  }

  ~malCBVar() {
    if (m_cloned)
      chainblocks::destroyVar(m_var);
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << m_var;
    return stream.str();
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_var == static_cast<const malCBVar *>(rhs)->m_var;
  }

  CBVar &value() { return m_var; }

  WITH_META(malCBVar);

private:
  CBVar m_var;
  bool m_cloned;
};

struct ChainLoadResult {
  bool hasError;
  std::string errorMsg;
  CBChain *chain;
};

struct ChainFileWatcher {
  std::atomic_bool running;
  std::thread worker;
  std::string fileName;
  std::string path;
  rigtorp::SPSCQueue<ChainLoadResult> results;
  phmap::node_hash_map<CBChain *, std::tuple<malEnvPtr, malValuePtr>>
      liveChains;
  boost::lockfree::queue<CBChain *> garbage;

  CBTypeInfo inputTypeInfo;
  chainblocks::IterableExposedInfo consumables;

  explicit ChainFileWatcher(std::string &file, std::string currentPath,
                            const CBInstanceData &data)
      : running(true), fileName(file), path(currentPath), results(2),
        garbage(2), inputTypeInfo(data.inputType),
        consumables(data.consumables) {
    worker = std::thread([this] {
      decltype(fs::last_write_time(fs::path())) lastWrite{};
      auto localRoot = std::filesystem::path(path);
      malEnvPtr rootEnv(new malEnv());
      malinit(rootEnv);
      rootEnv->currentPath(path);

      while (running) {
        try {
          fs::path p(fileName);
          if (path.size() > 0 && p.is_relative()) {
            // complete path with current path if any
            p = localRoot / p;
          }

          if (!fs::exists(p)) {
            LOG(INFO) << "A ChainLoader loaded script path does not exist: "
                      << p;
          } else if (fs::is_regular_file(p) &&
                     fs::last_write_time(p) != lastWrite) {
            // make sure to store last write time
            // before any possible error!
            auto writeTime = fs::last_write_time(p);
            lastWrite = writeTime;

            std::ifstream lsp(p.c_str());
            std::string str((std::istreambuf_iterator<char>(lsp)),
                            std::istreambuf_iterator<char>());

            malEnvPtr env(new malEnv(rootEnv));
            env->currentPath(path);
            auto res = maleval(str.c_str(), env);
            auto var = varify(nullptr, res);
            if (var->value().valueType != CBType::Chain) {
              LOG(ERROR) << "Script did not return a CBChain";
              continue;
            }

            auto chain = var->value().payload.chainValue;

            CBInstanceData data{};
            data.inputType = inputTypeInfo;
            data.consumables = consumables;

            // run validation to infertypes and specialize
            auto chainValidation = validateConnections(
                chain,
                [](const CBlock *errorBlock, const char *errorTxt,
                   bool nonfatalWarning, void *userData) {
                  if (!nonfatalWarning) {
                    auto msg =
                        "RunChain: failed inner chain validation, error: " +
                        std::string(errorTxt);
                    throw chainblocks::CBException(msg);
                  } else {
                    LOG(INFO)
                        << "RunChain: warning during inner chain validation: "
                        << errorTxt;
                  }
                },
                nullptr, data);
            chainblocks::arrayFree(chainValidation.exposedInfo);

            liveChains[chain] = std::make_tuple(env, res);

            ChainLoadResult result = {false, "", chain};
            results.push(result);
          }

          // Collect garbage
          if (!garbage.empty()) {
            CBChain *gchain;
            if (garbage.pop(gchain)) {
              liveChains.erase(gchain);
            }
          }

          // sleep some, don't run callbacks here tho!
          chainblocks::sleep(2.0, false);
        } catch (std::exception &e) {
          ChainLoadResult result = {true, e.what(), nullptr};
          results.push(result);
        } catch (...) {
          ChainLoadResult result = {true, "unknown error", nullptr};
          results.push(result);
        }
      }

      // Collect garbage
      if (!garbage.empty()) {
        CBChain *gchain;
        if (garbage.pop(gchain)) {
          liveChains.erase(gchain);
        }
      }
    });
  }

  ~ChainFileWatcher() {
    running = false;
    worker.join();
  }
};

class malChainProvider : public malValue, public chainblocks::ChainProvider {
public:
  malChainProvider(const MalString &name)
      : _filename(name), _watcher(nullptr) {}

  malChainProvider(const malCBVar &that, const malValuePtr &meta) = delete;

  virtual ~malChainProvider() {}

  MalString print(bool readably) const override { return "ChainProvider"; }

  bool doIsEqualTo(const malValue *rhs) const override { return false; }

  malValuePtr doWithMeta(malValuePtr meta) const override {
    throw chainblocks::CBException(
        "Meta not supported on chainblocks chain providers.");
  }

  void reset() override { _watcher.reset(nullptr); }

  bool ready() override { return _watcher.get() != nullptr; }

  void setup(const char *path, const CBInstanceData &data) override {
    _watcher.reset(new ChainFileWatcher(_filename, path, data));
  }

  bool updated() override { return !_watcher->results.empty(); }

  CBChainProviderUpdate acquire() override {
    CBChainProviderUpdate update{};
    auto result = _watcher->results.front();
    if (result->hasError) {
      _errors = result->errorMsg;
      update.error = _errors.c_str();
    } else {
      update.chain = result->chain;
    }
    _watcher->results.pop();
    return update;
  }

  void release(CBChain *chain) override { _watcher->garbage.push(chain); }

private:
  MalString _filename;
  MalString _errors;
  std::unique_ptr<ChainFileWatcher> _watcher;
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
  case Bytes:
    return mal::keyword(":Bytes");
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
  CBVar outputVar{};

  void init(const malValuePtr &infer, const malValuePtr &activate) {
    malInfer = infer;

    auto avec = new malValueVec();
    avec->push_back(activate);

    CBVar empty{};
    innerVar = new malCBVar(empty);
    avec->push_back(malValuePtr(innerVar));

    malActivateList = new malList(avec);
    malActivate = malValuePtr(malActivateList);
  }

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

  CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(CBInstanceData data) {
    // call a fn* [inputTypeKeyword] in place
    auto ivec = new malValueVec();
    ivec->push_back(malInfer);
    ivec->push_back(typeToKeyword(data.inputType.basicType));
    auto res = EVAL(malValuePtr(new malList(ivec)), nullptr);

    auto typeKeyword = VALUE_CAST(malKeyword, res);
    auto returnType = CBTypeInfo();
    returnType.basicType = keywordToType(typeKeyword);

    return returnType;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    cloneVar(innerVar->value(), input);
    auto res = EVAL(malActivate, nullptr);
    auto resStaticVar = STATIC_CAST(
        malCBVar, res); // for perf here we use static, it's dangerous tho!
    cloneVar(outputVar, resStaticVar->value());
    return outputVar;
  }
};

RUNTIME_CORE_BLOCK(InnerCall);
RUNTIME_BLOCK_inputTypes(InnerCall);
RUNTIME_BLOCK_outputTypes(InnerCall);
RUNTIME_BLOCK_compose(InnerCall);
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
  return malValuePtr(new malCBlock(blk));
}

BUILTIN("Node") {
  auto node = new malCBNode();
  chainblocks::Globals::RootPath = malpath();
  return malValuePtr(node);
}

#define WRAP_TO_CONST(_var_)                                                   \
  auto constBlock = chainblocks::createBlock("Const");                         \
  constBlock->setup(constBlock);                                               \
  constBlock->setParam(constBlock, 0, _var_);                                  \
  auto mblock = new malCBlock(constBlock);                                     \
  result.emplace_back(mblock);

// Helper to generate const blocks automatically inferring types
std::vector<malCBlockPtr> blockify(const malValuePtr &arg) {
  // blocks clone vars internally so there is no need to keep ref!
  std::vector<malCBlockPtr> result;
  if (arg == mal::nilValue()) {
    // Wrap none into const
    CBVar var{};
    var.valueType = None;
    var.payload.chainState = Continue;
    WRAP_TO_CONST(var);
  } else if (const malString *v = DYNAMIC_CAST(malString, arg)) {
    CBVar strVar{};
    strVar.valueType = String;
    auto &s = v->ref();
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
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    WRAP_TO_CONST(var);
  } else if (arg == mal::falseValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    WRAP_TO_CONST(var);
  } else if (malCBVar *v = DYNAMIC_CAST(malCBVar, arg)) {
    WRAP_TO_CONST(v->value());
  } else if (malCBlock *v = DYNAMIC_CAST(malCBlock, arg)) {
    result.emplace_back(v);
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

malCBVarPtr varify(malCBlock *mblk, const malValuePtr &arg) {
  // Returns clones in order to proper cleanup (nested) allocations
  if (arg == mal::nilValue()) {
    CBVar var{};
    var.valueType = None;
    var.payload.chainState = Continue;
    return malCBVarPtr(new malCBVar(var));
  } else if (malString *v = DYNAMIC_CAST(malString, arg)) {
    auto &s = v->ref();
    CBVar var{};
    var.valueType = String;
    var.payload.stringValue = s.c_str();
    auto svar = new malCBVar(var);
    svar->reference(v);
    return malCBVarPtr(svar);
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
    return malCBVarPtr(new malCBVar(var));
  } else if (malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    CBVar tmp{};
    tmp.valueType = Seq;
    tmp.payload.seqValue = {};
    auto count = v->count();
    std::vector<malCBVarPtr> vars;
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto subVar = varify(mblk, val);
      vars.push_back(subVar);
      chainblocks::arrayPush(tmp.payload.seqValue, subVar->value());
    }
    CBVar var{};
    chainblocks::cloneVar(var, tmp);
    chainblocks::arrayFree(tmp.payload.seqValue);
    auto mvar = new malCBVar(var, true);
    for (auto &rvar : vars) {
      mvar->reference(rvar.ptr());
    }
    return malCBVarPtr(mvar);
  } else if (arg == mal::trueValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    return malCBVarPtr(new malCBVar(var));
  } else if (arg == mal::falseValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    return malCBVarPtr(new malCBVar(var));
  } else if (malCBVar *v = DYNAMIC_CAST(malCBVar, arg)) {
    CBVar var{};
    chainblocks::cloneVar(var, v->value());
    return malCBVarPtr(new malCBVar(var, true));
  } else if (malChainProvider *v = DYNAMIC_CAST(malChainProvider, arg)) {
    CBVar var = *v;
    auto providerVar = new malCBVar(var, false);
    providerVar->reference(v);
    return malCBVarPtr(providerVar);
  } else if (malCBlock *v = DYNAMIC_CAST(malCBlock, arg)) {
    auto block = v->value();
    CBVar var{};
    var.valueType = Block;
    var.payload.blockValue = block;
    auto bvar = new malCBVar(var);
    bvar->reference(v);
    v->consume();
    return malCBVarPtr(bvar);
  } else if (malCBChain *v = DYNAMIC_CAST(malCBChain, arg)) {
    auto chain = v->value();
    CBVar var{};
    var.valueType = Chain;
    var.payload.chainValue = chain;
    auto cvar = new malCBVar(var);
    cvar->reference(v);
    return malCBVarPtr(cvar);
  } else {
    throw chainblocks::CBException("Invalid variable");
  }
}

int findParamIndex(const std::string &name, CBParametersInfo params) {
  for (uint32_t i = 0; params.len > i; i++) {
    auto param = params.elements[i];
    if (param.name == name)
      return (int)i;
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

void setBlockParameters(malCBlock *malblock, malValueIter begin,
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
        if (!validateSetParam(block, idx, var->value(), validationCallback,
                              nullptr)) {
          LOG(ERROR) << "Failed parameter: " << paramName;
          throw chainblocks::CBException("Parameter validation failed");
        }
        block->setParam(block, idx, var->value());

        // keep ref
        malblock->reference(var.ptr());
      }
    } else if (pindex == -1) {
      // We expected a keyword! fail
      LOG(ERROR) << "Keyword expected, block: " << block->name(block);
      throw chainblocks::CBException("Keyword expected");
    } else {
      auto var = varify(malblock, arg);
      if (!validateSetParam(block, pindex, var->value(), validationCallback,
                            nullptr)) {
        LOG(ERROR) << "Failed parameter index: " << pindex;
        throw chainblocks::CBException("Parameter validation failed");
      }
      block->setParam(block, pindex, var->value());

      // keep ref
      malblock->reference(var.ptr());

      pindex++;
    }
  }
}

malValuePtr newEnum(int32_t vendor, int32_t type, CBEnum value) {
  CBVar var{};
  var.valueType = Enum;
  var.payload.enumVendorId = vendor;
  var.payload.enumTypeId = type;
  var.payload.enumValue = value;
  return malValuePtr(new malCBVar(var));
}

struct Observer : public chainblocks::RuntimeObserver {
  virtual ~Observer() {}

  malEnvPtr _env;

  void registerBlock(const char *fullName,
                     CBBlockConstructor constructor) override {
    // do some reflection
    auto block = constructor();
    // add params keywords if any
    auto params = block->parameters(block);
    for (uint32_t i = 0; i < params.len; i++) {
      auto param = params.elements[i];
      _env->set(":" + MalString(param.name),
                mal::keyword(":" + MalString(param.name)));
    }
    // define the new built-in
    MalString mname(fullName);
    malBuiltIn::ApplyFunc *func = [](const MalString &name,
                                     malValueIter argsBegin,
                                     malValueIter argsEnd) {
      auto block = chainblocks::createBlock(name.c_str());
      block->setup(block);
      auto malblock = new malCBlock(block);
      setBlockParameters(malblock, argsBegin, argsEnd);
      return malValuePtr(malblock);
    };
    auto bi = new malBuiltIn(mname, func);
    builtIns.emplace(mname, bi);
    _env->set(fullName, bi);
    // destroy our sample block
    block->destroy(block);
  }

  void registerEnumType(int32_t vendorId, int32_t typeId,
                        CBEnumInfo info) override {
    for (uint32_t i = 0; i < info.labels.len; i++) {
      auto enumName =
          MalString(info.name) + "." + MalString(info.labels.elements[i]);
      auto enumValue = newEnum(vendorId, typeId, i);
      builtIns[enumName] = enumValue;
      _env->set(enumName, enumValue);
    }
  }
};

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
      for (auto blk : blks) {
        chain->addBlock(blk->value());
        // chain will manage this block from now on!
        blk->consume();
        mchain->reference(blk.ptr());
      }
    }
  }
  return malValuePtr(mchain);
}

BUILTIN("Chain@") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  return malValuePtr(new malChainProvider(value->ref()));
}

BUILTIN("-->") {
  auto vec = new malValueVec();
  while (argsBegin != argsEnd) {
    auto arg = *argsBegin++;
    auto blks = blockify(arg);
    for (auto blk : blks) {
      malCBlock *pblk = blk.ptr();
      vec->emplace_back(pblk);
    }
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

// used in chains without a node (manually prepared etc)
thread_local CBNode TLSRootNode;

BUILTIN("prepare") {
  CHECK_ARGS_IS(1);
  ARG(malCBChain, chain);
  auto chainValidation = validateConnections(
      chain->value(),
      [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
         void *userData) {
        if (!nonfatalWarning) {
          auto msg = "RunChain: failed inner chain validation, error: " +
                     std::string(errorTxt);
          throw chainblocks::CBException(msg);
        } else {
          LOG(INFO) << "RunChain: warning during inner chain validation: "
                    << errorTxt;
        }
      },
      nullptr);
  chainblocks::arrayFree(chainValidation.exposedInfo);
  chain->value()->node = &TLSRootNode;
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
  auto &s = value->ref();
  CBVar var{};
  var.valueType = ContextVar;
  var.payload.stringValue = s.c_str();
  auto mvar = new malCBVar(var);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("Enum") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  CBVar var{};
  var.valueType = Enum;
  var.payload.enumVendorId = static_cast<int32_t>(value0->value());
  var.payload.enumTypeId = static_cast<int32_t>(value1->value());
  var.payload.enumValue = static_cast<CBEnum>(value2->value());
  return malValuePtr(new malCBVar(var));
}

BUILTIN("String") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  CBVar var{};
  var.valueType = String;
  auto &s = value->ref();
  var.payload.stringValue = s.c_str();
  auto mvar = new malCBVar(var);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("Int") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  CBVar var{};
  var.valueType = Int;
  var.payload.intValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Int2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var{};
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
  CBVar var{};
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
  CBVar var{};
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
  CBVar var{};
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
  CBVar var{};
  var.valueType = Float;
  var.payload.floatValue = value->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("Float2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  CBVar var{};
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
  CBVar var{};
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
  CBVar var{};
  var.valueType = Float4;
  var.payload.float4Value[0] = value0->value();
  var.payload.float4Value[1] = value1->value();
  var.payload.float4Value[2] = value2->value();
  var.payload.float4Value[3] = value3->value();
  return malValuePtr(new malCBVar(var));
}

BUILTIN("import") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);

  auto filepath = std::filesystem::path(value->value());
  auto currentPath = malpath();
  if (currentPath.size() > 0 && filepath.is_relative()) {
    filepath = std::filesystem::path(currentPath) / filepath;
  }

  auto lib_name_str = filepath.string();
  auto lib_name = lib_name_str.c_str();

#if _WIN32
  LOG(INFO) << "Importing DLL: " << lib_name;
  auto handle = LoadLibraryA(lib_name);
  if (!handle) {
    LOG(ERROR) << "LoadLibrary failed.";
  }
#elif defined(__linux__) || defined(__APPLE__)
  LOG(INFO) << "Importing Shared Library: " << lib_name;
  auto handle = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
  if (!handle) {
    LOG(ERROR) << "dlerror: " << dlerror();
  }
#endif

  if (!handle) {
    return mal::falseValue();
  }
  return mal::trueValue();
}

BUILTIN("getenv") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  auto envs = boost::this_process::environment();
  auto env_value = envs[value->value()].to_string();
  return mal::string(env_value);
}

BUILTIN_ISA("Var?", malCBVar);
BUILTIN_ISA("Node?", malCBNode);
BUILTIN_ISA("Chain?", malCBChain);
BUILTIN_ISA("Block?", malCBlock);

extern "C" {
EXPORTED __cdecl void *cbLispCreate(const char *path) {
  auto env = new malEnvPtr(new malEnv);
  malinit(*env);
  if (path) {
    MalString mpath(path);
    malsetpath(mpath);
  }
  return (void *)env;
}

EXPORTED __cdecl void cbLispDestroy(void *env) {
  auto penv = (malEnvPtr *)env;
  observers.erase((malEnv *)penv->ptr());
  delete penv;
}

EXPORTED __cdecl CBVar cbLispEval(void *env, const char *str) {
  auto penv = (malEnvPtr *)env;
  try {
    auto res = maleval(str, *penv);
    auto mvar = varify(nullptr, res);
    // hack, increase count to not loose contents...
    // TODO, improve as in the end this leaks basically
    std::size_t sh = std::hash<const char *>{}(str);
    (*penv)->set(std::to_string(sh), malValuePtr(mvar.ptr()));
    return mvar->value();
  } catch (...) {
    return chainblocks::Empty;
  }
}
};

void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env) {
  obs = std::make_shared<Observer>();
  obs->_env = env;
  chainblocks::Globals::Observers.emplace_back(obs);
  observers[env.ptr()] = obs;
}
