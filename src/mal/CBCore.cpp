/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "chainblocks.hpp"
#define String MalString
#include "Environment.h"
#include "MAL.h"
#include "StaticList.h"
#include "Types.h"
#undef String
#include "../core/blocks/shared.hpp"
#include "../core/runtime.hpp"
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#ifndef __EMSCRIPTEN__
#include <boost/process/environment.hpp>
#endif
#include <chrono>
#include <filesystem>
#include <set>
#include <thread>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef CB_COMPRESSED_STRINGS
#include "../core/cbccstrings.hpp"
#endif

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

MalString malpath() { return chainblocks::Globals::RootPath; }

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
malCBVarPtr varify(const malValuePtr &arg);

namespace fs = std::filesystem;

namespace chainblocks {
CBlock *createBlockInnerCall();
} // namespace chainblocks

struct Observer;
void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env);

static bool initDoneOnce = false;
static std::map<MalString, malValuePtr> builtIns;
static std::map<malEnv *, std::shared_ptr<Observer>> observers;

void installCBCore(const malEnvPtr &env, const char *exePath,
                   const char *scriptPath) {
  std::shared_ptr<Observer> obs;
  setupObserver(obs, env);

  if (!initDoneOnce) {
    chainblocks::Globals::ExePath = exePath;
    chainblocks::Globals::RootPath = scriptPath;

    LOG(DEBUG) << "Exe path: " << exePath;
    LOG(DEBUG) << "Script path: " << scriptPath;

#ifndef __EMSCRIPTEN__
    chainblocks::installSignalHandlers();
#endif

    cbRegisterAllBlocks();

    initDoneOnce = true;
  }

  // Chain params
  env->set(":Looped", mal::keyword(":Looped"));
  env->set(":Unsafe", mal::keyword(":Unsafe"));
  env->set(":LStack", mal::keyword(":LStack"));
  env->set(":SStack", mal::keyword(":SStack"));
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
  env->set(":Path", mal::keyword(":Path"));
  env->set(":ContextVar", mal::keyword(":ContextVar"));
  env->set(":Image", mal::keyword(":Image"));
  env->set(":Seq", mal::keyword(":Seq"));
  env->set(":Array", mal::keyword(":Array"));
  env->set(":Set", mal::keyword(":Set"));
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
  rep("(defn range [from to] (when (<= from to) (cons from (range (inc from) "
      "to))))",
      env);
  rep("(def || Await)", env);
#if defined(_WIN32)
  rep("(def platform \"windows\")", env);
#elif defined(__EMSCRIPTEN__)
  rep("(def platform \"emscripten\")", env);
#elif defined(__linux__)
  rep("(def platform \"linux\")", env);
#elif defined(__APPLE__)
  rep("(def platform \"apple\")", env);
#endif
  rep("(defmacro! defchain (fn* [name & blocks] `(def! ~(symbol (str name)) "
      "(Chain ~(str name) (chainify (vector ~@blocks))))))",
      env);
  rep("(defmacro! defloop (fn* [name & blocks] `(def! ~(symbol (str name)) "
      "(Chain ~(str name) :Looped (chainify (vector ~@blocks))))))",
      env);
  rep("(defmacro! defnode (fn* [name] `(def ~(symbol (str name)) (Node))))",
      env);
  rep("(defmacro! | (fn* [& blocks] `(Sub (chainify (vector ~@blocks)))))",
      env);
  rep("(defmacro! || (fn* [& blocks] `(Await (chainify (vector ~@blocks)))))",
      env);
  rep("(defmacro! Setup (fn* [& blocks] `(Once (chainify (vector ~@blocks)))))",
      env);
  rep("(defmacro! defblocks (fn* [name args & blocks] `(defn ~(symbol (str "
      "name)) ~args (chainify (vector ~@blocks)))))",
      env);
}

class malRoot {
public:
  void reference(malValue *val) { m_refs.emplace_back(val); }

protected:
  std::vector<malValuePtr> m_refs;
};

class malCBChain : public malValue, public malRoot {
public:
  malCBChain(const MalString &name) {
    LOG(TRACE) << "Created a CBChain - " << name;
    auto chain = CBChain::make(name);
    m_chain = chain->newRef();
    chainblocks::Globals::GlobalChains[name] = chain;
  }

  malCBChain(const malCBChain &that, const malValuePtr &meta) = delete;

  ~malCBChain() {
    // unref all we hold  first
    m_refs.clear();

    // remove from globals TODO some mutex maybe
    auto cp = CBChain::sharedFromRef(m_chain);
    auto it = chainblocks::Globals::GlobalChains.find(cp.get()->name);
    if (it != chainblocks::Globals::GlobalChains.end() && it->second == cp) {
      chainblocks::Globals::GlobalChains.erase(it);
    }

    // delref
    LOG(TRACE) << "Deleting a CBChain - " << cp->name;
    CBChain::deleteRef(m_chain);
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    auto cp = CBChain::sharedFromRef(m_chain);
    stream << "CBChain: " << cp->name;
    return stream.str();
  }

  CBChainRef value() const { return m_chain; }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_chain == static_cast<const malCBChain *>(rhs)->m_chain;
  }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw chainblocks::CBException("Meta not supported on chainblocks chains.");
  }

private:
  CBChainRef m_chain;
};

class malCBlock : public malValue, public malRoot {
public:
  malCBlock(const MalString &name)
      : m_block(chainblocks::createBlock(name.c_str())) {}

  malCBlock(CBlock *block) : m_block(block) {}

  malCBlock(const malCBlock &that, const malValuePtr &meta) = delete;

  ~malCBlock() {
    // unref all we hold  first
    m_refs.clear();

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
  malCBNode() : m_node(CBNode::make()) { LOG(TRACE) << "Created a CBNode"; }

  malCBNode(const malCBNode &that, const malValuePtr &meta) = delete;

  ~malCBNode() {
    // Delete all refs first
    m_refs.clear();
    m_node->terminate();
    LOG(TRACE) << "Deleted a CBNode";
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "CBNode";
    return stream.str();
  }

  CBNode *value() const { return m_node.get(); }

  void schedule(malCBChain *chain) {
    auto cp = CBChain::sharedFromRef(chain->value());
    m_node->schedule(cp);
    reference(chain);
  }

  virtual bool doIsEqualTo(const malValue *rhs) const {
    return m_node == static_cast<const malCBNode *>(rhs)->m_node;
  }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw chainblocks::CBException("Meta not supported on chainblocks nodes.");
  }

private:
  std::shared_ptr<CBNode> m_node;
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
    // unref all we hold  first
    m_refs.clear();

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
  const char *errorMsg;
  CBChain *chain;
};

struct ChainFileWatcher {
  std::atomic_bool running;
  std::thread worker;
  std::string fileName;
  malValuePtr autoexec;
  std::string path;
  boost::lockfree::queue<ChainLoadResult> results;
  std::unordered_map<CBChain *, std::tuple<malEnvPtr, malValuePtr>> liveChains;
  boost::lockfree::queue<CBChain *> garbage;
  std::weak_ptr<CBNode> node;
  std::string localRoot;
  decltype(fs::last_write_time(fs::path())) lastWrite{};
  malEnvPtr rootEnv{new malEnv()};
  CBTypeInfo inputTypeInfo;
  chainblocks::IterableExposedInfo shared;

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  decltype(std::chrono::high_resolution_clock::now()) tStart{
      std::chrono::high_resolution_clock::now()};
#endif

  void checkOnce() {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    // check only every 2 seconds
    auto tnow = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> dt = tnow - tStart;
    if (dt.count() < 2.0) {
      return;
    }
    tStart = tnow;
#endif

    try {
      fs::path p(fileName);
      if (path.size() > 0 && p.is_relative()) {
        // complete path with current path if any
        p = localRoot / p;
      }

      if (!fs::exists(p)) {
        LOG(INFO) << "A ChainLoader loaded script path does not exist: " << p;
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
        auto res = maleval(str.c_str(), env);
        auto var = varify(res);
        if (var->value().valueType != CBType::Chain) {
          LOG(ERROR) << "Script did not return a CBChain";
          return;
        }

        auto chainref = var->value().payload.chainValue;
        auto chain = CBChain::sharedFromRef(chainref);

        CBInstanceData data{};
        data.inputType = inputTypeInfo;
        data.shared = shared;
        data.chain = chain.get();
        data.chain->node = node;

        // run validation to infertypes and specialize
        auto chainValidation = composeChain(
            chain.get(),
            [](const CBlock *errorBlock, const char *errorTxt,
               bool nonfatalWarning, void *userData) {
              if (!nonfatalWarning) {
                auto msg = "RunChain: failed inner chain validation, error: " +
                           std::string(errorTxt);
                throw chainblocks::CBException(msg);
              } else {
                LOG(INFO) << "RunChain: warning during inner chain validation: "
                          << errorTxt;
              }
            },
            nullptr, data);
        chainblocks::arrayFree(chainValidation.exposedInfo);

        liveChains[chain.get()] = std::make_tuple(env, res);

        ChainLoadResult result = {false, "", chain.get()};
        results.push(result);
      }

      // Collect garbage
      if (!garbage.empty()) {
        CBChain *gchain;
        if (garbage.pop(gchain)) {
          auto &data = liveChains[gchain];
          LOG(TRACE) << "Collecting hot chain " << gchain->name
                     << " env refcount: " << std::get<0>(data)->refCount();
          liveChains.erase(gchain);
        }
      }

#if !(defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
      // sleep some, don't run callbacks here tho!
      chainblocks::sleep(2.0, false);
#endif
    } catch (malEmptyInputException &) {
      ChainLoadResult result = {true, "empty input exception", nullptr};
      results.push(result);
    } catch (malValuePtr &mv) {
      LOG(ERROR) << mv->print(true);
      ChainLoadResult result = {true, "script error", nullptr};
      results.push(result);
    } catch (MalString &s) {
      LOG(ERROR) << s;
      ChainLoadResult result = {true, "parse error", nullptr};
      results.push(result);
    } catch (const std::exception &e) {
      LOG(ERROR) << e.what();
      ChainLoadResult result = {true, "exception", nullptr};
      results.push(result);
    } catch (...) {
      ChainLoadResult result = {true, "unknown error (...)", nullptr};
      results.push(result);
    }
  }

  explicit ChainFileWatcher(const std::string &file, std::string currentPath,
                            const CBInstanceData &data,
                            const malValuePtr &autoexec)
      : running(true), fileName(file), autoexec(autoexec), path(currentPath),
        results(2), garbage(2), inputTypeInfo(data.inputType),
        shared(data.shared) {
    node = data.chain->node;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    localRoot = std::filesystem::path(path).string();
    try {
      malinit(rootEnv, localRoot.c_str(), localRoot.c_str());
      if (this->autoexec != nullptr) {
        EVAL(this->autoexec, rootEnv);
      }
    } catch (malEmptyInputException &) {
      LOG(ERROR) << "empty input exception.";
      throw;
    } catch (malValuePtr &mv) {
      LOG(ERROR) << "script error: " << mv->print(true);
      throw;
    } catch (MalString &s) {
      LOG(ERROR) << "parse error: " << s;
      throw;
    } catch (const std::exception &e) {
      LOG(ERROR) << "Failed to init ChainFileWatcher: " << e.what();
      throw;
    } catch (...) {
      LOG(ERROR) << "Failed to init ChainFileWatcher.";
      throw;
    }
#else
    worker = std::thread([this] {
      localRoot = std::filesystem::path(path).string();
      try {
        malinit(rootEnv, localRoot.c_str(), localRoot.c_str());
        if (this->autoexec != nullptr) {
          EVAL(this->autoexec, rootEnv);
        }
      } catch (malEmptyInputException &) {
        LOG(ERROR) << "empty input exception.";
        throw;
      } catch (malValuePtr &mv) {
        LOG(ERROR) << "script error: " << mv->print(true);
        throw;
      } catch (MalString &s) {
        LOG(ERROR) << "parse error: " << s;
        throw;
      } catch (const std::exception &e) {
        LOG(ERROR) << "Failed to init ChainFileWatcher: " << e.what();
        throw;
      } catch (...) {
        LOG(ERROR) << "Failed to init ChainFileWatcher.";
        throw;
      }

      while (running) {
        checkOnce();
      }

      // Final collect garbage before returing!
      if (!garbage.empty()) {
        CBChain *gchain;
        if (garbage.pop(gchain)) {
          auto &data = liveChains[gchain];
          LOG(TRACE) << "Collecting hot chain " << gchain->name
                     << " env refcount: " << std::get<0>(data)->refCount();
          liveChains.erase(gchain);
        }
      }
    });
#endif
  }

  ~ChainFileWatcher() {
    running = false;
// join is bad under emscripten, will lock the main thread
#ifndef __EMSCRIPTEN__
    worker.join();
#elif defined(__EMSCRIPTEN_PTHREADS__)
    worker.detach();
#endif
  }
};

class malChainProvider : public malValue, public chainblocks::ChainProvider {
public:
  malChainProvider(const MalString &name)
      : chainblocks::ChainProvider(), _filename(name), _watcher(nullptr) {}

  malChainProvider(const MalString &name, const malValuePtr &ast)
      : chainblocks::ChainProvider(), _filename(name), _autoexec(ast),
        _watcher(nullptr) {}

  malChainProvider(const malCBVar &that, const malValuePtr &meta) = delete;

  virtual ~malChainProvider() {}

  MalString print(bool readably) const override { return "ChainProvider"; }

  bool doIsEqualTo(const malValue *rhs) const override { return false; }

  malValuePtr doWithMeta(malValuePtr meta) const override {
    throw chainblocks::CBException(
        "Meta not supported on chainblocks chain providers.");
  }

  void reset() override { _watcher.reset(nullptr); }

  bool ready() override { return bool(_watcher); }

  void setup(const char *path, const CBInstanceData &data) override {
    _watcher.reset(new ChainFileWatcher(_filename, path, data, _autoexec));
  }

  bool updated() override {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    _watcher->checkOnce();
#endif
    return !_watcher->results.empty();
  }

  CBChainProviderUpdate acquire() override {
    assert(!_watcher->results.empty());
    CBChainProviderUpdate update{};
    ChainLoadResult result;
    // consume all of them!
    while (_watcher->results.pop(result)) {
      if (result.hasError) {
        update.chain = nullptr;
        update.error = result.errorMsg;
      } else {
        update.chain = result.chain;
        update.error = nullptr;
      }
    }
    return update;
  }

  void release(CBChain *chain) override { _watcher->garbage.push(chain); }

private:
  MalString _filename;
  malValuePtr _autoexec;
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
  else if (value == ":Set")
    return CBType::Set;
  else
    throw chainblocks::CBException(
        "Could not infer type for special '.' block. Need to return a proper "
        "type keyword!");
}

malValuePtr typeToKeyword(CBType type) {
  switch (type) {
  case CBType::EndOfBlittableTypes:
  case CBType::None:
    return mal::keyword(":None");
  case CBType::Any:
    return mal::keyword(":Any");
  case CBType::Object:
    return mal::keyword(":Object");
  case CBType::Enum:
    return mal::keyword(":Enum");
  case CBType::Bool:
    return mal::keyword(":Bool");
  case CBType::Int:
    return mal::keyword(":Int");
  case CBType::Int2:
    return mal::keyword(":Int2");
  case CBType::Int3:
    return mal::keyword(":Int3");
  case CBType::Int4:
    return mal::keyword(":Int4");
  case CBType::Int8:
    return mal::keyword(":Int8");
  case CBType::Int16:
    return mal::keyword(":Int16");
  case CBType::Float:
    return mal::keyword(":Float");
  case CBType::Float2:
    return mal::keyword(":Float2");
  case CBType::Float3:
    return mal::keyword(":Float3");
  case CBType::Float4:
    return mal::keyword(":Float4");
  case CBType::Color:
    return mal::keyword(":Color");
  case CBType::Chain:
    return mal::keyword(":Chain");
  case CBType::Block:
    return mal::keyword(":Block");
  case CBType::String:
    return mal::keyword(":String");
  case CBType::ContextVar:
    return mal::keyword(":ContextVar");
  case CBType::Path:
    return mal::keyword(":Path");
  case CBType::Image:
    return mal::keyword(":Image");
  case CBType::Bytes:
    return mal::keyword(":Bytes");
  case CBType::Seq:
    return mal::keyword(":Seq");
  case CBType::Table:
    return mal::keyword(":Table");
  case CBType::Set:
    return mal::keyword(":Set");
  case CBType::Array:
    return mal::keyword(":Array");
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
  return malValuePtr(node);
}

#define WRAP_TO_CONST(_var_)                                                   \
  auto constBlock = chainblocks::createBlock("Const");                         \
  constBlock->setup(constBlock);                                               \
  constBlock->setParam(constBlock, 0, &_var_);                                 \
  auto mblock = new malCBlock(constBlock);                                     \
  mblock->line = arg->line;                                                    \
  result.emplace_back(mblock);

// Helper to generate const blocks automatically inferring types
std::vector<malCBlockPtr> blockify(const malValuePtr &arg) {
  // blocks clone vars internally so there is no need to keep ref!
  std::vector<malCBlockPtr> result;
  if (arg == mal::nilValue()) {
    // Wrap none into const
    CBVar var{};
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
  } else if (malCBChain *v = DYNAMIC_CAST(malCBChain, arg)) {
    auto val = chainblocks::Var(v->value());
    WRAP_TO_CONST(val);
  } else if (malCBlock *v = DYNAMIC_CAST(malCBlock, arg)) {
    result.emplace_back(v);
  } else if (DYNAMIC_CAST(malVector, arg)) {
    auto cbv = varify(arg);
    WRAP_TO_CONST(cbv->value());
  } else if (const malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    auto count = v->count();
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto blks = blockify(val);
      result.insert(result.end(), blks.begin(), blks.end());
    }
  } else if (const malHash *v = DYNAMIC_CAST(malHash, arg)) {
    CBVar var{};
    chainblocks::CBMap cbMap;
    std::vector<malCBVarPtr> vars;
    for (auto [k, v] : v->m_map) {
      auto cbv = varify(v);
      vars.emplace_back(cbv);
      cbMap[unescape(k)] = cbv->value();
    }
    var.valueType = Table;
    var.payload.tableValue.api = &chainblocks::Globals::TableInterface;
    var.payload.tableValue.opaque = &cbMap;
    WRAP_TO_CONST(var);
  } else if (malAtom *v = DYNAMIC_CAST(malAtom, arg)) {
    return blockify(v->deref());
  }
  // allow anything else, makes more sense, in order to write inline funcs
  // but print a diagnostic message
  else {
    if (malValue *v = DYNAMIC_CAST(malValue, arg)) {
      LOG(INFO) << "Ignoring value inside a chain definition: "
                << v->print(true)
                << " ignore this warning if this was intentional.";
    } else {
      throw chainblocks::CBException("Invalid argument for chain, not a value");
    }
  }
  return result;
}

std::vector<malCBlockPtr> chainify(malValueIter begin, malValueIter end);

malCBVarPtr varify(const malValuePtr &arg) {
  // Returns clones in order to proper cleanup (nested) allocations
  if (arg == mal::nilValue()) {
    CBVar var{};
    auto v = new malCBVar(var);
    v->line = arg->line;
    return malCBVarPtr(v);
  } else if (malString *v = DYNAMIC_CAST(malString, arg)) {
    auto &s = v->ref();
    CBVar var{};
    var.valueType = String;
    var.payload.stringValue = s.c_str();
    auto svar = new malCBVar(var);
    svar->reference(v);
    svar->line = arg->line;
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
    auto res = new malCBVar(var);
    res->line = arg->line;
    return malCBVarPtr(res);
  } else if (malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    CBVar tmp{};
    tmp.valueType = Seq;
    tmp.payload.seqValue = {};
    auto count = v->count();
    std::vector<malCBVarPtr> vars;
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto subVar = varify(val);
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
    mvar->line = arg->line;
    return malCBVarPtr(mvar);
  } else if (const malHash *v = DYNAMIC_CAST(malHash, arg)) {
    chainblocks::CBMap cbMap;
    std::vector<malCBVarPtr> vars;
    for (auto [k, v] : v->m_map) {
      auto cbv = varify(v);
      vars.emplace_back(cbv);
      cbMap[unescape(k)] = cbv->value();
    }
    CBVar tmp{};
    tmp.valueType = Table;
    tmp.payload.tableValue.api = &chainblocks::Globals::TableInterface;
    tmp.payload.tableValue.opaque = &cbMap;
    CBVar var{};
    chainblocks::cloneVar(var, tmp);
    auto mvar = new malCBVar(var, true);
    for (auto &rvar : vars) {
      mvar->reference(rvar.ptr());
    }
    mvar->line = arg->line;
    return malCBVarPtr(mvar);
  } else if (arg == mal::trueValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    auto v = new malCBVar(var);
    v->line = arg->line;
    return malCBVarPtr(v);
  } else if (arg == mal::falseValue()) {
    CBVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    auto v = new malCBVar(var);
    v->line = arg->line;
    return malCBVarPtr(v);
  } else if (malCBVar *v = DYNAMIC_CAST(malCBVar, arg)) {
    CBVar var{};
    chainblocks::cloneVar(var, v->value());
    auto res = new malCBVar(var, true);
    res->line = arg->line;
    return malCBVarPtr(res);
  } else if (malChainProvider *v = DYNAMIC_CAST(malChainProvider, arg)) {
    CBVar var = *v;
    auto providerVar = new malCBVar(var, false);
    providerVar->reference(v);
    providerVar->line = arg->line;
    return malCBVarPtr(providerVar);
  } else if (malCBlock *v = DYNAMIC_CAST(malCBlock, arg)) {
    auto block = v->value();
    CBVar var{};
    var.valueType = Block;
    var.payload.blockValue = block;
    auto bvar = new malCBVar(var);
    v->consume();
    bvar->reference(v);
    bvar->line = arg->line;
    return malCBVarPtr(bvar);
  } else if (malCBChain *v = DYNAMIC_CAST(malCBChain, arg)) {
    auto chain = v->value();
    CBVar var{};
    var.valueType = CBType::Chain;
    var.payload.chainValue = chain;
    auto cvar = new malCBVar(var);
    cvar->reference(v);
    cvar->line = arg->line;
    return malCBVarPtr(cvar);
  } else if (malAtom *v = DYNAMIC_CAST(malAtom, arg)) {
    return varify(v->deref());
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
        auto var = varify(value);
        if (!validateSetParam(block, idx, var->value(), validationCallback,
                              nullptr)) {
          LOG(ERROR) << "Failed parameter: " << paramName
                     << ", line: " << value->line;
          throw chainblocks::CBException("Parameter validation failed");
        }
        block->setParam(block, idx, &var->value());

        // keep ref
        malblock->reference(var.ptr());
      }
    } else if (pindex == -1) {
      // We expected a keyword! fail
      LOG(ERROR) << "Keyword expected, block: " << block->name(block);
      throw chainblocks::CBException("Keyword expected");
    } else {
      auto var = varify(arg);
      if (!validateSetParam(block, pindex, var->value(), validationCallback,
                            nullptr)) {
        LOG(ERROR) << "Failed parameter index: " << pindex
                   << ", line: " << arg->line;
        throw chainblocks::CBException("Parameter validation failed");
      }
      block->setParam(block, pindex, &var->value());

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
#ifndef NDEBUG
      // add coverage for parameters setting
      auto val = block->getParam(block, i);
      block->setParam(block, i, &val);
#endif
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

malCBlock *makeVarBlock(malCBVar *v, const char *blockName) {
  auto b = chainblocks::createBlock(blockName);
  b->setup(b);
  b->setParam(b, 0, &v->value());
  auto blk = new malCBlock(b);
  blk->line = v->line;
  return blk;
}

BUILTIN(">>") { return mal::nilValue(); }

BUILTIN(">>!") { return mal::nilValue(); }

BUILTIN("&>") { return mal::nilValue(); }

BUILTIN(">==") { return mal::nilValue(); }

BUILTIN("??") { return mal::nilValue(); }

std::vector<malCBlockPtr> chainify(malValueIter begin, malValueIter end) {
  enum State {
    Get,
    Set,
    SetGlobal,
    Update,
    Ref,
    Push,
    PushNoClear,
    AddGetDefault
  };
  State state = Get;
  std::vector<malCBlockPtr> res;
  while (begin != end) {
    auto next = *begin++;
    if (auto *v = DYNAMIC_CAST(malCBVar, next)) {
      if (v->value().valueType == ContextVar) {
        if (state == Get) {
          res.emplace_back(makeVarBlock(v, "Get"));
        } else if (state == Set) {
          res.emplace_back(makeVarBlock(v, "Set"));
          state = Get;
        } else if (state == SetGlobal) {
          auto blk = makeVarBlock(v, "Set");
          // set :Global true
          auto val = chainblocks::Var(true);
          blk->value()->setParam(blk->value(), 2, &val);
          res.emplace_back(blk);
          state = Get;
        } else if (state == Update) {
          res.emplace_back(makeVarBlock(v, "Update"));
          state = Get;
        } else if (state == Ref) {
          res.emplace_back(makeVarBlock(v, "Ref"));
          state = Get;
        } else if (state == Push) {
          res.emplace_back(makeVarBlock(v, "Push"));
          state = Get;
        } else if (state == PushNoClear) {
          auto blk = makeVarBlock(v, "Push");
          // set :Clear false
          auto val = chainblocks::Var(false);
          blk->value()->setParam(blk->value(), 3, &val);
          res.emplace_back(blk);
          state = Get;
        } else {
          throw chainblocks::CBException("Expected a variable");
        }
      } else {
        auto blks = blockify(next);
        res.insert(res.end(), blks.begin(), blks.end());
      }
    } else if (auto *v = DYNAMIC_CAST(malBuiltIn, next)) {
      if (v->name() == ">=") {
        state = Set;
      } else if (v->name() == ">==") {
        state = SetGlobal;
      } else if (v->name() == ">") {
        state = Update;
      } else if (v->name() == "&>" || v->name() == "=") {
        state = Ref;
      } else if (v->name() == ">>") {
        state = Push;
      } else if (v->name() == ">>!") {
        state = PushNoClear;
      } else if (v->name() == "??") {
        state = AddGetDefault;
      } else {
        throw chainblocks::CBException("Unexpected a token");
      }
    } else {
      if (state == AddGetDefault) {
        auto blk = res.back();
        if (strcmp(blk->value()->name(blk->value()), "Get") != 0) {
          throw chainblocks::CBException(
              "There should be a variable preceeding ||");
        }
        // set :Default
        auto v = varify(next);
        blk->value()->setParam(blk->value(), 4, &v->value());
        state = Get;
      } else {
        auto blks = blockify(next);
        res.insert(res.end(), blks.begin(), blks.end());
      }
    }
  }
  return res;
}

BUILTIN("Chain") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, chainName);
  auto mchain = new malCBChain(chainName->value());
  auto chainref = mchain->value();
  auto chain = CBChain::sharedFromRef(chainref);
  while (argsBegin != argsEnd) {
    auto pbegin = argsBegin;
    auto arg = *argsBegin++;
    // Option keywords or blocks
    if (const malKeyword *v = DYNAMIC_CAST(malKeyword, arg)) {
      if (v->value() == ":Looped") {
        chain->looped = true;
      } else if (v->value() == ":Unsafe") {
        chain->unsafe = true;
      } else if (v->value() == ":LStack") {
        chain->stackSize = 1024 * 1024;     // 1mb
      } else if (v->value() == ":SStack") { // default is 128kb
        chain->stackSize = 32 * 1024;       // 32kb
      }
    } else {
      auto blks = chainify(pbegin, argsEnd);
      for (auto blk : blks) {
        chain->addBlock(blk->value());
        blk->consume();
        mchain->reference(blk.ptr());
      }
      break;
    }
  }
  return malValuePtr(mchain);
}

static MalString printValues(malValueIter begin, malValueIter end,
                             const MalString &sep, bool readably) {
  MalString out;

  if (begin != end) {
    out += (*begin)->print(readably);
    ++begin;
  }

  for (; begin != end; ++begin) {
    out += sep;
    out += (*begin)->print(readably);
  }

  return out;
}

BUILTIN("LOG") {
  LOG(INFO) << printValues(argsBegin, argsEnd, " ", false);
  return mal::nilValue();
}

BUILTIN("Chain@") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, value);
  if (argsBegin != argsEnd) {
    return malValuePtr(new malChainProvider(value->ref(), *argsBegin));
  } else {
    return malValuePtr(new malChainProvider(value->ref()));
  }
}

BUILTIN("Chain*") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, value);
  if (argsBegin != argsEnd) {
    return malValuePtr(new malChainProvider(value->ref(), *argsBegin));
  } else {
    return malValuePtr(new malChainProvider(value->ref()));
  }
}

BUILTIN("-->") {
  auto vec = new malValueVec();
  auto blks = chainify(argsBegin, argsEnd);
  for (auto blk : blks) {
    malCBlock *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("->") {
  auto vec = new malValueVec();
  auto blks = chainify(argsBegin, argsEnd);
  for (auto blk : blks) {
    malCBlock *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("chainify") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, value);
  auto vec = new malValueVec();
  auto blks = chainify(value->begin(), value->end());
  for (auto blk : blks) {
    malCBlock *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("unquote") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, value);
  auto vec = new malValueVec();
  auto blks = chainify(value->begin(), value->end());
  for (auto blk : blks) {
    malCBlock *pblk = blk.ptr();
    vec->emplace_back(pblk);
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
thread_local std::shared_ptr<CBNode> TLSRootNode{CBNode::make()};

BUILTIN("prepare") {
  CHECK_ARGS_IS(1);
  ARG(malCBChain, chainvar);
  auto chain = CBChain::sharedFromRef(chainvar->value());
  CBInstanceData data{};
  data.chain = chain.get();
  chain->node = TLSRootNode->shared_from_this();
  auto chainValidation = composeChain(
      chain.get(),
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
      nullptr, data);
  chain->composedHash = 1; // just to mark as composed
  chainblocks::arrayFree(chainValidation.exposedInfo);
  chainblocks::prepare(chain.get(), nullptr);
  return mal::nilValue();
}

BUILTIN("set-global-chain") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    auto chain = CBChain::sharedFromRef(v->value());
    chainblocks::Globals::GlobalChains[chain->name] = chain;
    return mal::trueValue();
  }
  return mal::falseValue();
}

BUILTIN("unset-global-chain") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    auto chain = CBChain::sharedFromRef(v->value());
    auto it = chainblocks::Globals::GlobalChains.find(chain->name);
    if (it != chainblocks::Globals::GlobalChains.end()) {
      chainblocks::Globals::GlobalChains.erase(it);
      return mal::trueValue();
    }
  }
  return mal::falseValue();
}

BUILTIN("run-empty-forever") {
  while (true) {
    chainblocks::sleep(1.0, false);
  }
}

BUILTIN("start") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malCBChain, chainvar);
  auto chain = CBChain::sharedFromRef(chainvar->value());
  if (argsBegin != argsEnd) {
    ARG(malCBVar, inputVar);
    chainblocks::start(chain.get(), inputVar->value());
  } else {
    chainblocks::start(chain.get());
  }
  return mal::nilValue();
}

BUILTIN("tick") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    auto chain = CBChain::sharedFromRef(v->value());
    CBDuration now = CBClock::now().time_since_epoch();
    auto ticked = chainblocks::tick(chain.get(), now);
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
  ARG(malCBChain, chainvar);
  auto chain = CBChain::sharedFromRef(chainvar->value());
  CBVar res{};
  chainblocks::stop(chain.get(), &res);
  return malValuePtr(new malCBVar(res, true));
}

BUILTIN("run") {
  CHECK_ARGS_AT_LEAST(1);
  CBNode *node = nullptr;
  CBChain *chain = nullptr;
  auto first = *argsBegin++;
  if (const malCBChain *v = DYNAMIC_CAST(malCBChain, first)) {
    auto cs = CBChain::sharedFromRef(v->value());
    chain = cs.get();
  } else if (const malCBNode *v = DYNAMIC_CAST(malCBNode, first)) {
    node = v->value();
  } else {
    throw chainblocks::CBException("tick Expected Node or Chain");
  }

  auto sleepTime = -1.0;
  if (argsBegin != argsEnd) {
    ARG(malNumber, argSleepTime);
    sleepTime = argSleepTime->value();
  }

  int times = 1;
  bool dec = false;
  if (argsBegin != argsEnd) {
    ARG(malNumber, argTimes);
    times = argTimes->value();
    dec = true;
  }

  if (node) {
    while (!node->empty()) {
      const auto pre = CBClock::now();
      const auto noErrors = node->tick();
      const auto elapsed = CBClock::now() - pre;

      // other chains might be not in error tho...
      // so return only if empty
      if (!noErrors && node->empty()) {
        return mal::boolean(false);
      }

      if (dec) {
        times--;
        if (times == 0) {
          node->terminate();
          break;
        }
      }
      // We on purpose run terminate (evenutally)
      // before sleep
      // cos during sleep some blocks
      // swap states and invalidate stuff
      if (sleepTime <= 0.0) {
        chainblocks::sleep(-1.0);
      } else {
        CBDuration dsleep(sleepTime);
        // remove the time we took to tick from sleep
        CBDuration realSleepTime = dsleep - elapsed;
        if (realSleepTime.count() <= 0.0) {
          // tick took too long!!!
          // warn sometimes and skip sleeping, skipping callbacks too
          LOG_EVERY_N(100, INFO)
              << "Skipping frame sleep and callbacks, ticking nodes is taking "
                 "more time than the wanted sleeptime! Consider using Await/|| "
                 "or reduce complexity.";
        } else {
          chainblocks::sleep(realSleepTime.count());
        }
      }
    }
  } else {
    CBDuration now = CBClock::now().time_since_epoch();
    while (!chainblocks::tick(chain, now)) {
      if (dec) {
        times--;
        if (times == 0) {
          chainblocks::stop(chain);
          break;
        }
      }
      chainblocks::sleep(sleepTime);
      now = CBClock::now().time_since_epoch();
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

BUILTIN("override-root-path") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  chainblocks::Globals::RootPath = value->ref();
  return mal::nilValue();
}

BUILTIN("Path") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  auto &s = value->ref();
  CBVar var{};
  var.valueType = Path;
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

BUILTIN("ContextVar") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  CBVar var{};
  var.valueType = ContextVar;
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

  // FIXME we are leaking libs
#if _WIN32
  LOG(INFO) << "Importing DLL: " << lib_name;
  auto handle = LoadLibraryA(lib_name);
  if (!handle) {
    LOG(ERROR) << "LoadLibrary failed.";
  }
#elif defined(__linux__) || defined(__APPLE__)
  LOG(INFO) << "Importing Shared Library: " << lib_name;
  auto handle = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    LOG(ERROR) << "dlerror: " << dlerror();
  }
#else
  void *handle = nullptr;
#endif

  if (!handle) {
    return mal::falseValue();
  }
  return mal::trueValue();
}

BUILTIN("getenv") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
#ifndef __EMSCRIPTEN__
  auto envs = boost::this_process::environment();
  auto env_value = envs[value->value()].to_string();
  return mal::string(env_value);
#else
  return mal::string(getenv(value->ref().c_str()));
#endif
}

BUILTIN("setenv") {
  CHECK_ARGS_IS(2);
  ARG(malString, key);
  ARG(malString, value);
#ifndef __EMSCRIPTEN__
  auto envs = boost::this_process::environment();
  envs[key->value()] = value->value();
  return mal::trueValue();
#else
  if (!setenv(key->ref().c_str(), value->ref().c_str(), 1))
    return mal::trueValue();
  else
    return mal::falseValue();
#endif
}

BUILTIN("hasBlock?") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  if (builtIns.find(value->value()) != builtIns.end()) {
    return mal::trueValue();
  } else {
    return mal::falseValue();
  }
}

BUILTIN("blocks") {
  malValueVec v;
  for (auto [name, _] : chainblocks::Globals::BlocksRegister) {
    v.emplace_back(mal::string(std::string(name)));
  }
  malValueVec *items = new malValueVec(v);
  return mal::list(items);
}

BUILTIN("info") {
  CHECK_ARGS_IS(1);
  ARG(malString, blkname);
  const auto blkIt = builtIns.find(blkname->ref());
  if (blkIt == builtIns.end()) {
    return mal::nilValue();
  } else {
    malHash::Map map;
    auto block = createBlock(blkname->ref().c_str());
    DEFER(block->destroy(block));

    auto help = block->help(block);
    map[":help"] = mal::string(help.string ? help.string : getString(help.crc));

    auto params = block->parameters(block);
    malValueVec pvec;
    for (uint32_t i = 0; i < params.len; i++) {
      malHash::Map pmap;
      pmap[":name"] = mal::string(params.elements[i].name);
      if (params.elements[i].help.string)
        pmap[":help"] = mal::string(params.elements[i].help.string);
      else
        pmap[":help"] = mal::string(getString(params.elements[i].help.crc));
      std::stringstream ss;
      ss << params.elements[i].valueTypes;
      pmap[":types"] = mal::string(ss.str());
      pvec.emplace_back(mal::hash(pmap));
    }
    map[":parameters"] = mal::list(pvec.begin(), pvec.end());

    {
      std::stringstream ss;
      ss << block->inputTypes(block);
      map[":inputTypes"] = mal::string(ss.str());
    }

    {
      std::stringstream ss;
      ss << block->outputTypes(block);
      map[":outputTypes"] = mal::string(ss.str());
    }

    return mal::hash(map);
  }
}

#ifndef CB_COMPRESSED_STRINGS
BUILTIN("export-strings") {
  assert(chainblocks::Globals::CompressedStrings);
  malValueVec strs;
  for (auto &[crc, str] : *chainblocks::Globals::CompressedStrings) {
    if (crc != 0) {
      malValueVec record;
      record.emplace_back(mal::number(crc, true));
      record.emplace_back(mal::string(str.string));
      strs.emplace_back(mal::list(record.begin(), record.end()));
    }
  }
  return mal::list(strs.begin(), strs.end());
}
#else
static std::unordered_map<uint32_t, std::string> strings_storage;
#endif

BUILTIN("decompress-strings") {
#ifdef CB_COMPRESSED_STRINGS
  if (!chainblocks::Globals::CompressedStrings) {
    throw chainblocks::CBException("String storage was null");
  }
  // run the script to populate compressed strings
  auto bytes = Var(__chainblocks_compressed_strings);
  auto chain = ::chainblocks::Chain("decompress strings")
                   .let(bytes)
                   .block("Brotli.Decompress")
                   .block("FromBytes");
  auto node = CBNode::make();
  node->schedule(chain);
  node->tick();
  if (chain->finishedOutput.valueType != CBType::Seq) {
    throw chainblocks::CBException("Failed to decompress strings!");
  }

  for (uint32_t i = 0; i < chain->finishedOutput.payload.seqValue.len; i++) {
    auto pair = chain->finishedOutput.payload.seqValue.elements[i];
    if (pair.valueType != CBType::Seq || pair.payload.seqValue.len != 2) {
      throw chainblocks::CBException("Failed to decompress strings!");
    }
    auto crc = pair.payload.seqValue.elements[0];
    auto str = pair.payload.seqValue.elements[1];
    if (crc.valueType != CBType::Int || str.valueType != CBType::String) {
      throw chainblocks::CBException("Failed to decompress strings!");
    }
    auto emplaced = strings_storage.emplace(uint32_t(crc.payload.intValue),
                                            str.payload.stringValue);
    auto &s = emplaced.first->second;
    auto &val = (*chainblocks::Globals::CompressedStrings)[uint32_t(
        crc.payload.intValue)];
    val.string = s.c_str();
    val.crc = uint32_t(crc.payload.intValue);
  }
#endif
  return mal::nilValue();
}

BUILTIN_ISA("Var?", malCBVar);
BUILTIN_ISA("Node?", malCBNode);
BUILTIN_ISA("Chain?", malCBChain);
BUILTIN_ISA("Block?", malCBlock);

extern "C" {
EXPORTED __cdecl void *cbLispCreate(const char *path) {
  auto env = new malEnvPtr(new malEnv);
  malinit(*env, path, path);
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
    auto mvar = varify(res);
    // hack, increase count to not loose contents...
    // TODO, improve as in the end this leaks basically
    std::size_t sh = std::hash<const char *>{}(str);
    (*penv)->set(std::to_string(sh), malValuePtr(mvar.ptr()));
    return mvar->value();
  } catch (...) {
    return chainblocks::Var::Empty;
  }
}
};

void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env) {
  obs = std::make_shared<Observer>();
  obs->_env = env;
  chainblocks::Globals::Observers.emplace_back(obs);
  observers[env.ptr()] = obs;
}

namespace mal {
malValuePtr contextVar(const MalString &token) {
  CBVar tmp{}, v{};
  tmp.valueType = CBType::ContextVar;
  tmp.payload.stringValue = token.c_str();
  chainblocks::cloneVar(v, tmp);
  return malValuePtr(new malCBVar(v, true));
}
} // namespace mal
