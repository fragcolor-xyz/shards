/* SPDX-License-Identifier: MPL-2.0 AND BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "shards.hpp"
#define String MalString
#include "Environment.h"
#include "MAL.h"
#include "StaticList.h"
#include "Types.h"
#undef String
#include "../core/shards/shared.hpp"
#include "../core/runtime.hpp"
#include <algorithm>
#include <boost/lockfree/queue.hpp>
#ifdef SHARDS_DESKTOP
#include <boost/process/environment.hpp>
#endif
#include <boost/filesystem.hpp>
#include <chrono>
#include <fstream>
#include <ostream>
#include <set>
#include <thread>

#ifndef _WIN32
#include <dlfcn.h>
#endif

#ifdef SH_COMPRESSED_STRINGS
#include "../core/shccstrings.hpp"
#endif

using namespace shards;

#define ARG(type, name) type *name = VALUE_CAST(type, *argsBegin++)

static StaticList<malBuiltIn *> handlers;
#define FUNCNAME(uniq) sh_builtIn##uniq
#define HRECNAME(uniq) sh_handler##uniq
#define BUILTIN_DEF(uniq, symbol)                                                                         \
  static malBuiltIn::ApplyFunc FUNCNAME(uniq);                                                            \
  static StaticList<malBuiltIn *>::Node HRECNAME(uniq)(handlers, new malBuiltIn(symbol, FUNCNAME(uniq))); \
  malValuePtr FUNCNAME(uniq)(const MalString &name, malValueIter argsBegin, malValueIter argsEnd)

#define BUILTIN(symbol) BUILTIN_DEF(__LINE__, symbol)

#define BUILTIN_ISA(symbol, type)                        \
  BUILTIN(symbol) {                                      \
    CHECK_ARGS_IS(1);                                    \
    return mal::boolean(DYNAMIC_CAST(type, *argsBegin)); \
  }

namespace shards {
extern void setupSpdLog();
}
extern void shRegisterAllShards();

#ifndef SH_CORE_ONLY
extern "C" void runRuntimeTests();
#endif

class malSHWire;
class malShard;
class malSHMesh;
class malSHVar;
typedef RefCountedPtr<malSHWire> malSHWirePtr;
typedef RefCountedPtr<malShard> malShardPtr;
typedef RefCountedPtr<malSHMesh> malSHMeshPtr;
typedef RefCountedPtr<malSHVar> malSHVarPtr;

void registerKeywords(malEnvPtr env);
malSHVarPtr varify(const malValuePtr &arg);

namespace fs = boost::filesystem;

namespace shards {
Shard *createShardInnerCall();
} // namespace shards

struct Observer;
void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env);

static bool initDoneOnce = false;
static std::map<MalString, malValuePtr> builtIns;
static std::mutex observersMutex;
static std::map<malEnv *, std::shared_ptr<Observer>> observers;

void installSHCore(const malEnvPtr &env, const char *exePath, const char *scriptPath) {
  // Setup logging first
  shards::setupSpdLog();

  std::shared_ptr<Observer> obs;
  setupObserver(obs, env);

  std::scoped_lock lock(shards::GetGlobals().GlobalMutex);

  if (!initDoneOnce) {
    shards::GetGlobals().ExePath = exePath;
    shards::GetGlobals().RootPath = scriptPath;

    SHLOG_DEBUG("Exe path: {}", exePath);
    SHLOG_DEBUG("Script path: {}", scriptPath);

#ifndef __EMSCRIPTEN__
    shards::installSignalHandlers();
#endif

    shRegisterAllShards();

    initDoneOnce = true;

#if defined(SHARDS_WITH_RUST_SHARDS) && !defined(NDEBUG)
    // TODO fix running rust tests...
    runRuntimeTests();
#endif
  }

  // Wire params
  env->set(":Looped", mal::keyword(":Looped"));
  env->set(":Unsafe", mal::keyword(":Unsafe"));
  env->set(":LStack", mal::keyword(":LStack"));
  env->set(":SStack", mal::keyword(":SStack"));
  // SHType
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
  env->set(":Wire", mal::keyword(":Wire"));
  env->set(":Shard", mal::keyword(":Shard"));
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
#elif defined(__ANDROID__)
  rep("(def platform \"android\")", env);
#elif defined(__EMSCRIPTEN__)
  rep("(def platform \"emscripten\")", env);
#elif defined(__linux__)
  rep("(def platform \"linux\")", env);
#elif defined(__APPLE__)
  rep("(def platform \"apple\")", env);
#endif
  rep("(defmacro! defwire (fn* [name & shards] `(def! ~(symbol (str name)) "
      "(Wire ~(str name) (wireify (vector ~@shards))))))",
      env);
  rep("(defmacro! deftrait (fn* [name & shards] `(def! ~(symbol (str name)) (hash-map ~@shards))))", env);
  rep("(defmacro! defloop (fn* [name & shards] `(def! ~(symbol (str name)) "
      "(Wire ~(str name) :Looped (wireify (vector ~@shards))))))",
      env);
  rep("(defmacro! defmesh (fn* [name] `(def ~(symbol (str name)) (Mesh))))", env);
  rep("(defmacro! | (fn* [& shards] `(Sub (wireify (vector ~@shards)))))", env);
  rep("(defmacro! |# (fn* [& shards] `(Hashed (wireify (vector ~@shards)))))", env);
  rep("(defmacro! || (fn* [& shards] `(Await (wireify (vector ~@shards)))))", env);
  rep("(defmacro! Setup (fn* [& shards] `(Once (wireify (vector ~@shards)))))", env);
  rep("(defmacro! defshards (fn* [name args & shards] `(defn ~(symbol (str "
      "name)) ~args (wireify (vector ~@shards)))))",
      env);

  // overrides for some built-in types
  rep("(def! Bytes bytes)", env);
  rep("(def! Color color)", env);
  rep("(def! ContextVar context-var)", env);
  rep("(def! Enum enum)", env);
  rep("(def! Float float)", env);
  rep("(def! Float2 float2)", env);
  rep("(def! Float3 float3)", env);
  rep("(def! Float4 float4)", env);
  rep("(def! Int int)", env);
  rep("(def! Int2 int2)", env);
  rep("(def! Int3 int3)", env);
  rep("(def! Int4 int4)", env);
  rep("(def! Path path)", env);
  rep("(def! String string)", env);
}

class malRoot {
public:
  void reference(malValue *val) { m_refs.emplace_back(val); }

protected:
  std::vector<malValuePtr> m_refs;
};

class malSHWire : public malValue, public malRoot {
public:
  malSHWire(const MalString &name) {
    SHLOG_TRACE("Created a SHWire - {}", name);
    auto wire = SHWire::make(name);
    m_wire = wire->newRef();
    {
      std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
      shards::GetGlobals().GlobalWires[name] = wire;
    }
  }

  malSHWire(const std::shared_ptr<SHWire> &wire) : m_wire(wire->newRef()) {
    SHLOG_TRACE("Loaded a SHWire - {}", wire->name);
  }

  malSHWire(const malSHWire &that, const malValuePtr &meta) = delete;

  ~malSHWire() {
    // unref all we hold  first
    m_refs.clear();

    // remove from globals TODO some mutex maybe
    auto cp = SHWire::sharedFromRef(m_wire);
    {
      std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
      auto it = shards::GetGlobals().GlobalWires.find(cp.get()->name);
      if (it != shards::GetGlobals().GlobalWires.end() && it->second == cp) {
        shards::GetGlobals().GlobalWires.erase(it);
      }
    }

    // delref
    SHLOG_TRACE("Deleting a SHWire - {}", cp->name);
    SHWire::deleteRef(m_wire);
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    auto cp = SHWire::sharedFromRef(m_wire);
    stream << "SHWire: " << cp->name;
    return stream.str();
  }

  SHWireRef value() const { return m_wire; }

  virtual bool doIsEqualTo(const malValue *rhs) const { return m_wire == static_cast<const malSHWire *>(rhs)->m_wire; }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw shards::SHException("Meta not supported on shards wires.");
  }

private:
  SHWireRef m_wire;
};

class malShard : public malValue, public malRoot {
public:
  malShard(const MalString &name) : m_shard(shards::createShard(name.c_str())) {}

  malShard(Shard *shard) : m_shard(shard) {}

  malShard(const malShard &that, const malValuePtr &meta) = delete;

  malShard(const malShard &that) = delete;

  malShard(malShard &&that) = delete;

  ~malShard() {
    // unref all we hold  first
    m_refs.clear();

    // could be nullptr
    if (m_shard) {
      SHLOG_DEBUG("Deleting a non-consumed Shard - {}", m_shard->name(m_shard));
      m_shard->destroy(m_shard);
    }
  }

  virtual MalString print(bool readably) const {
    assert(m_shard);
    std::ostringstream stream;
    stream << "Shard: " << m_shard->name(m_shard);
    return stream.str();
  }

  Shard *value() const {
    if (!m_shard) {
      throw shards::SHException("Attempted to use a null shard, shards are unique, "
                                     "probably was already used.");
    }
    return m_shard;
  }

  void consume() { m_shard = nullptr; }

  virtual bool doIsEqualTo(const malValue *rhs) const { return m_shard == static_cast<const malShard *>(rhs)->m_shard; }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw shards::SHException("Meta not supported on shards shards.");
  }

private:
  Shard *m_shard;
};

class malSHMesh : public malValue, public malRoot {
public:
  malSHMesh() : m_mesh(SHMesh::make()) { SHLOG_TRACE("Created a Mesh"); }

  malSHMesh(const malSHMesh &that, const malValuePtr &meta) = delete;

  ~malSHMesh() {
    // Delete all refs first
    m_refs.clear();
    m_mesh->terminate();
    SHLOG_TRACE("Deleted a Mesh");
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << "Mesh";
    return stream.str();
  }

  SHMesh *value() const { return m_mesh.get(); }

  void schedule(malSHWire *wire) {
    auto cp = SHWire::sharedFromRef(wire->value());
    m_mesh->schedule(cp);
    reference(wire);
  }

  virtual bool doIsEqualTo(const malValue *rhs) const { return m_mesh == static_cast<const malSHMesh *>(rhs)->m_mesh; }

  virtual malValuePtr doWithMeta(malValuePtr meta) const {
    throw shards::SHException("Meta not supported on shards meshs.");
  }

private:
  std::shared_ptr<SHMesh> m_mesh;
};

class malSHVar : public malValue, public malRoot {
public:
  malSHVar(const SHVar &var, bool cloned) : m_cloned(cloned) {
    if (cloned) {
      memset(&m_var, 0, sizeof(m_var));
      shards::cloneVar(m_var, var);
    } else {
      m_var = var;
    }
  }

  malSHVar(const malSHVar &that, const malValuePtr &meta) : malValue(meta), m_cloned(true) {
    m_var = SHVar();
    shards::cloneVar(m_var, that.m_var);
  }

  ~malSHVar() {
    // unref all we hold  first
    m_refs.clear();

    if (m_cloned)
      shards::destroyVar(m_var);
  }

  virtual MalString print(bool readably) const {
    std::ostringstream stream;
    stream << m_var;
    return stream.str();
  }

  virtual bool doIsEqualTo(const malValue *rhs) const { return m_var == static_cast<const malSHVar *>(rhs)->m_var; }

  SHVar &value() { return m_var; }

  WITH_META(malSHVar);

private:
  SHVar m_var;
  bool m_cloned;
};

struct WireLoadResult {
  bool hasError;
  const char *errorMsg;
  SHWire *wire;
};

struct WireFileWatcher {
  std::atomic_bool running;
  std::thread worker;
  std::string fileName;
  malValuePtr autoexec;
  std::string path;
  boost::lockfree::queue<WireLoadResult> results;
  std::unordered_map<SHWire *, std::tuple<malEnvPtr, malValuePtr>> liveWires;
  boost::lockfree::queue<SHWire *> garbage;
  std::weak_ptr<SHMesh> mesh;
  std::string localRoot;
  decltype(fs::last_write_time(fs::path())) lastWrite{};
  malEnvPtr rootEnv{new malEnv()};
  SHTypeInfo inputTypeInfo;
  shards::IterableExposedInfo shared;

#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
  decltype(std::chrono::high_resolution_clock::now()) tStart{std::chrono::high_resolution_clock::now()};
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
      if (!fs::exists(p)) {
        SHLOG_INFO("A WireLoader loaded script path does not exist: {}", p);
      } else if (fs::is_regular_file(p) && fs::last_write_time(p) != lastWrite) {
        // make sure to store last write time
        // before any possible error!
        auto writeTime = fs::last_write_time(p);
        lastWrite = writeTime;

        std::ifstream lsp(p.c_str());
        std::string str((std::istreambuf_iterator<char>(lsp)), std::istreambuf_iterator<char>());

        auto fileNameOnly = p.filename();
        str = "(Wire \"" + fileNameOnly.string() + "\" " + str + ")";

        SHLOG_DEBUG("Processing {}", fileNameOnly.string());

        malEnvPtr env(new malEnv(rootEnv));
        auto res = maleval(str.c_str(), env);
        auto var = varify(res);
        if (var->value().valueType != SHType::Wire) {
          SHLOG_ERROR("Script did not return a SHWire");
          return;
        }

        auto wireref = var->value().payload.wireValue;
        auto wire = SHWire::sharedFromRef(wireref);

        SHInstanceData data{};
        data.inputType = inputTypeInfo;
        data.shared = shared;
        data.wire = wire.get();
        data.wire->mesh = mesh;

        SHLOG_TRACE("Processed {}", fileNameOnly.string());

        // run validation to infertypes and specialize
        auto wireValidation = composeWire(
            wire.get(),
            [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
              if (!nonfatalWarning) {
                auto msg = "RunWire: failed inner wire validation, error: " + std::string(errorTxt);
                throw shards::SHException(msg);
              } else {
                SHLOG_INFO("RunWire: warning during inner wire validation: {}", errorTxt);
              }
            },
            nullptr, data);
        shards::arrayFree(wireValidation.exposedInfo);

        SHLOG_TRACE("Validated {}", fileNameOnly.string());

        liveWires[wire.get()] = std::make_tuple(env, res);

        WireLoadResult result = {false, "", wire.get()};
        results.push(result);
      }

      // Collect garbage
      if (!garbage.empty()) {
        SHWire *gwire;
        if (garbage.pop(gwire)) {
          auto &data = liveWires[gwire];
          SHLOG_TRACE("Collecting hot wire {} env refcount: {}", gwire->name, std::get<0>(data)->refCount());
          liveWires.erase(gwire);
        }
      }

#if !(defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__))
      // sleep some, don't run callbacks here tho!
      shards::sleep(2.0, false);
#endif
    } catch (malEmptyInputException &) {
      WireLoadResult result = {true, "empty input exception", nullptr};
      results.push(result);
    } catch (malValuePtr &mv) {
      SHLOG_ERROR(mv->print(true));
      WireLoadResult result = {true, "script error", nullptr};
      results.push(result);
    } catch (MalString &s) {
      SHLOG_ERROR(s);
      WireLoadResult result = {true, "parse error", nullptr};
      results.push(result);
    } catch (const std::exception &e) {
      SHLOG_ERROR(e.what());
      WireLoadResult result = {true, "exception", nullptr};
      results.push(result);
    } catch (...) {
      WireLoadResult result = {true, "unknown error (...)", nullptr};
      results.push(result);
    }
  }

  explicit WireFileWatcher(const std::string &file, std::string currentPath, const SHInstanceData &data,
                            const malValuePtr &autoexec)
      : running(true), fileName(file), autoexec(autoexec), path(currentPath), results(2), garbage(2),
        inputTypeInfo(data.inputType), shared(data.shared) {
    mesh = data.wire->mesh;
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    localRoot = fs::path(path).string();
    try {
      malinit(rootEnv, localRoot.c_str(), localRoot.c_str());
      if (this->autoexec != nullptr) {
        EVAL(this->autoexec, rootEnv);
      }
    } catch (malEmptyInputException &) {
      SHLOG_ERROR("empty input exception.");
      throw;
    } catch (malValuePtr &mv) {
      SHLOG_ERROR("script error: {}", mv->print(true));
      throw;
    } catch (MalString &s) {
      SHLOG_ERROR("parse error: {}", s);
      throw;
    } catch (const std::exception &e) {
      SHLOG_ERROR("Failed to init WireFileWatcher: {}", e.what());
      throw;
    } catch (...) {
      SHLOG_ERROR("Failed to init WireFileWatcher.");
      throw;
    }
#else
    worker = std::thread([this] {
      localRoot = fs::path(path).string();
      try {
        malinit(rootEnv, localRoot.c_str(), localRoot.c_str());
        if (this->autoexec != nullptr) {
          EVAL(this->autoexec, rootEnv);
        }
      } catch (malEmptyInputException &) {
        SHLOG_ERROR("empty input exception.");
        throw;
      } catch (malValuePtr &mv) {
        SHLOG_ERROR("script error: {}", mv->print(true));
        throw;
      } catch (MalString &s) {
        SHLOG_ERROR("parse error: {}", s);
        throw;
      } catch (const std::exception &e) {
        SHLOG_ERROR("Failed to init WireFileWatcher: {}", e.what());
        throw;
      } catch (...) {
        SHLOG_ERROR("Failed to init WireFileWatcher.");
        throw;
      }

      while (running) {
        checkOnce();
      }

      // Final collect garbage before returing!
      if (!garbage.empty()) {
        SHWire *gwire;
        if (garbage.pop(gwire)) {
          auto &data = liveWires[gwire];
          SHLOG_TRACE("Collecting hot wire {} env refcount: {}", gwire->name, std::get<0>(data)->refCount());
          liveWires.erase(gwire);
        }
      }
    });
#endif
  }

  ~WireFileWatcher() {
    running = false;
// join is bad under emscripten, will lock the main thread
#ifndef __EMSCRIPTEN__
    worker.join();
#elif defined(__EMSCRIPTEN_PTHREADS__)
    worker.detach();
#endif
  }
};

class malWireProvider : public malValue, public shards::WireProvider {
public:
  malWireProvider(const MalString &name) : shards::WireProvider(), _filename(name), _watcher(nullptr) {}

  malWireProvider(const MalString &name, const malValuePtr &ast)
      : shards::WireProvider(), _filename(name), _autoexec(ast), _watcher(nullptr) {}

  malWireProvider(const malSHVar &that, const malValuePtr &meta) = delete;

  virtual ~malWireProvider() {}

  MalString print(bool readably) const override { return "WireProvider"; }

  bool doIsEqualTo(const malValue *rhs) const override { return false; }

  malValuePtr doWithMeta(malValuePtr meta) const override {
    throw shards::SHException("Meta not supported on shards wire providers.");
  }

  void reset() override { _watcher.reset(nullptr); }

  bool ready() override { return bool(_watcher); }

  void setup(const char *path, const SHInstanceData &data) override {
    _watcher.reset(new WireFileWatcher(_filename, path, data, _autoexec));
  }

  bool updated() override {
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)
    _watcher->checkOnce();
#endif
    return !_watcher->results.empty();
  }

  SHWireProviderUpdate acquire() override {
    assert(!_watcher->results.empty());
    SHWireProviderUpdate update{};
    WireLoadResult result;
    // consume all of them!
    while (_watcher->results.pop(result)) {
      if (result.hasError) {
        update.wire = nullptr;
        update.error = result.errorMsg;
      } else {
        update.wire = result.wire;
        update.error = nullptr;
      }
    }
    return update;
  }

  void release(SHWire *wire) override { _watcher->garbage.push(wire); }

private:
  MalString _filename;
  malValuePtr _autoexec;
  std::unique_ptr<WireFileWatcher> _watcher;
};

SHType keywordToType(malKeyword *typeKeyword) {
  auto value = typeKeyword->value();
  if (value == ":None")
    return SHType::None;
  else if (value == ":Any")
    return SHType::Any;
  else if (value == ":Enum")
    return SHType::Enum;
  else if (value == ":Bool")
    return SHType::Bool;
  else if (value == ":Int")
    return SHType::Int;
  else if (value == ":Int2")
    return SHType::Int2;
  else if (value == ":Int3")
    return SHType::Int3;
  else if (value == ":Int4")
    return SHType::Int4;
  else if (value == ":Int8")
    return SHType::Int8;
  else if (value == ":Int16")
    return SHType::Int16;
  else if (value == ":Float")
    return SHType::Float;
  else if (value == ":Float2")
    return SHType::Float2;
  else if (value == ":Float3")
    return SHType::Float3;
  else if (value == ":Float4")
    return SHType::Float4;
  else if (value == ":Color")
    return SHType::Color;
  else if (value == ":Wire")
    return SHType::Wire;
  else if (value == ":Shard")
    return SHType::ShardRef;
  else if (value == ":String")
    return SHType::String;
  else if (value == ":ContextVar")
    return SHType::ContextVar;
  else if (value == ":Image")
    return SHType::Image;
  else if (value == ":Seq")
    return SHType::Seq;
  else if (value == ":Table")
    return SHType::Table;
  else if (value == ":Set")
    return SHType::Set;
  else
    throw shards::SHException("Could not infer type for special '.' shard. Need to return a proper "
                                   "type keyword!");
}

malValuePtr typeToKeyword(SHType type) {
  switch (type) {
  case SHType::EndOfBlittableTypes:
  case SHType::None:
    return mal::keyword(":None");
  case SHType::Any:
    return mal::keyword(":Any");
  case SHType::Object:
    return mal::keyword(":Object");
  case SHType::Enum:
    return mal::keyword(":Enum");
  case SHType::Bool:
    return mal::keyword(":Bool");
  case SHType::Int:
    return mal::keyword(":Int");
  case SHType::Int2:
    return mal::keyword(":Int2");
  case SHType::Int3:
    return mal::keyword(":Int3");
  case SHType::Int4:
    return mal::keyword(":Int4");
  case SHType::Int8:
    return mal::keyword(":Int8");
  case SHType::Int16:
    return mal::keyword(":Int16");
  case SHType::Float:
    return mal::keyword(":Float");
  case SHType::Float2:
    return mal::keyword(":Float2");
  case SHType::Float3:
    return mal::keyword(":Float3");
  case SHType::Float4:
    return mal::keyword(":Float4");
  case SHType::Color:
    return mal::keyword(":Color");
  case SHType::Wire:
    return mal::keyword(":Wire");
  case SHType::ShardRef:
    return mal::keyword(":Shard");
  case SHType::String:
    return mal::keyword(":String");
  case SHType::ContextVar:
    return mal::keyword(":ContextVar");
  case SHType::Path:
    return mal::keyword(":Path");
  case SHType::Image:
    return mal::keyword(":Image");
  case SHType::Audio:
    return mal::keyword(":Audio");
  case SHType::Bytes:
    return mal::keyword(":Bytes");
  case SHType::Seq:
    return mal::keyword(":Seq");
  case SHType::Table:
    return mal::keyword(":Table");
  case SHType::Set:
    return mal::keyword(":Set");
  case SHType::Array:
    return mal::keyword(":Array");
  };
  return mal::keyword(":None");
}

namespace shards {
struct InnerCall {
  malValuePtr malInfer; // a fn* [inputType]
  malList *malActivateList;
  malValuePtr malActivate; // a fn* [input]
  malSHVar *innerVar;
  SHVar outputVar{};

  void init(const malValuePtr &infer, const malValuePtr &activate) {
    malInfer = infer;

    auto avec = new malValueVec();
    avec->push_back(activate);

    SHVar empty{};
    innerVar = new malSHVar(empty, false);
    avec->push_back(malValuePtr(innerVar));

    malActivateList = new malList(avec);
    malActivate = malValuePtr(malActivateList);
  }

  SHTypesInfo inputTypes() { return CoreInfo::AnyType; }

  SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(SHInstanceData data) {
    // call a fn* [inputTypeKeyword] in place
    auto ivec = new malValueVec();
    ivec->push_back(malInfer);
    ivec->push_back(typeToKeyword(data.inputType.basicType));
    auto res = EVAL(malValuePtr(new malList(ivec)), nullptr);

    auto typeKeyword = VALUE_CAST(malKeyword, res);
    auto returnType = SHTypeInfo();
    returnType.basicType = keywordToType(typeKeyword);

    return returnType;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    cloneVar(innerVar->value(), input);
    auto res = EVAL(malActivate, nullptr);
    auto resStaticVar = STATIC_CAST(malSHVar, res); // for perf here we use static, it's dangerous tho!
    cloneVar(outputVar, resStaticVar->value());
    return outputVar;
  }
};

RUNTIME_CORE_SHARD(InnerCall);
RUNTIME_SHARD_inputTypes(InnerCall);
RUNTIME_SHARD_outputTypes(InnerCall);
RUNTIME_SHARD_compose(InnerCall);
RUNTIME_SHARD_activate(InnerCall);
RUNTIME_SHARD_END(InnerCall);
}; // namespace shards

BUILTIN(".") {
  CHECK_ARGS_IS(2);
  ARG(malLambda, infer);
  ARG(malLambda, activate);
  auto blk = shards::createShardInnerCall();
  auto inner = reinterpret_cast<shards::InnerCallRuntime *>(blk);
  inner->core.init(infer, activate);
  return malValuePtr(new malShard(blk));
}

BUILTIN("Mesh") {
  auto mesh = new malSHMesh();
  return malValuePtr(mesh);
}

#define WRAP_TO_CONST(_var_)                           \
  auto constShard = shards::createShard("Const"); \
  constShard->setup(constShard);                       \
  constShard->setParam(constShard, 0, &_var_);         \
  auto mshard = new malShard(constShard);             \
  mshard->line = arg->line;                            \
  result.emplace_back(mshard);

// Helper to generate const shards automatically inferring types
std::vector<malShardPtr> shardify(const malValuePtr &arg) {
  // shards clone vars internally so there is no need to keep ref!
  std::vector<malShardPtr> result;
  if (arg == mal::nilValue()) {
    // Wrap none into const
    SHVar var{};
    WRAP_TO_CONST(var);
  } else if (const malString *v = DYNAMIC_CAST(malString, arg)) {
    SHVar strVar{};
    strVar.valueType = String;
    auto &s = v->ref();
    strVar.payload.stringValue = s.c_str();
    WRAP_TO_CONST(strVar);
  } else if (const malNumber *v = DYNAMIC_CAST(malNumber, arg)) {
    auto value = v->value();
    SHVar var{};
    if (v->isInteger()) {
      var.valueType = Int;
      var.payload.intValue = value;
    } else {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    WRAP_TO_CONST(var);
  } else if (arg == mal::trueValue()) {
    SHVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    WRAP_TO_CONST(var);
  } else if (arg == mal::falseValue()) {
    SHVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    WRAP_TO_CONST(var);
  } else if (malSHVar *v = DYNAMIC_CAST(malSHVar, arg)) {
    WRAP_TO_CONST(v->value());
  } else if (malSHWire *v = DYNAMIC_CAST(malSHWire, arg)) {
    auto val = shards::Var(v->value());
    WRAP_TO_CONST(val);
  } else if (malShard *v = DYNAMIC_CAST(malShard, arg)) {
    result.emplace_back(v);
  } else if (DYNAMIC_CAST(malVector, arg)) {
    auto shv = varify(arg);
    WRAP_TO_CONST(shv->value());
  } else if (const malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    auto count = v->count();
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto blks = shardify(val);
      result.insert(result.end(), blks.begin(), blks.end());
    }
  } else if (const malHash *v = DYNAMIC_CAST(malHash, arg)) {
    SHVar var{};
    shards::SHMap shMap;
    std::vector<malSHVarPtr> vars;
    for (auto [k, v] : v->m_map) {
      auto shv = varify(v);
      vars.emplace_back(shv);
      // key is either a quoted string or a keyword (starting with ':')
      shMap[k[0] == '"' ? unescape(k) : k.substr(1)] = shv->value();
    }
    var.valueType = Table;
    var.payload.tableValue.api = &shards::GetGlobals().TableInterface;
    var.payload.tableValue.opaque = &shMap;
    WRAP_TO_CONST(var);
  } else if (malAtom *v = DYNAMIC_CAST(malAtom, arg)) {
    return shardify(v->deref());
  }
  // allow anything else, makes more sense, in order to write inline funcs
  // but print a diagnostic message
  else {
    if (malValue *v = DYNAMIC_CAST(malValue, arg)) {
      SHLOG_INFO("Ignoring value inside a wire definition: {} ignore this "
                 "warning if this was intentional.",
                 v->print(true));
    } else {
      throw shards::SHException("Invalid argument for wire, not a value");
    }
  }
  return result;
}

std::vector<malShardPtr> wireify(malValueIter begin, malValueIter end);

malSHVarPtr varify(const malValuePtr &arg) {
  // Returns clones in order to proper cleanup (nested) allocations
  if (arg == mal::nilValue()) {
    SHVar var{};
    auto v = new malSHVar(var, false);
    v->line = arg->line;
    return malSHVarPtr(v);
  } else if (malString *v = DYNAMIC_CAST(malString, arg)) {
    auto &s = v->ref();
    SHVar var{};
    var.valueType = String;
    var.payload.stringValue = s.c_str();
    // notice, we don't clone in this case
    auto svar = new malSHVar(var, false);
    svar->reference(v);
    svar->line = arg->line;
    return malSHVarPtr(svar);
  } else if (const malNumber *v = DYNAMIC_CAST(malNumber, arg)) {
    auto value = v->value();
    SHVar var{};
    if (v->isInteger()) {
      var.valueType = Int;
      var.payload.intValue = value;
    } else {
      var.valueType = Float;
      var.payload.floatValue = value;
    }
    auto res = new malSHVar(var, false);
    res->line = arg->line;
    return malSHVarPtr(res);
  } else if (malSequence *v = DYNAMIC_CAST(malSequence, arg)) {
    SHVar tmp{};
    tmp.valueType = Seq;
    tmp.payload.seqValue = {};
    auto count = v->count();
    std::vector<malSHVarPtr> vars;
    for (auto i = 0; i < count; i++) {
      auto val = v->item(i);
      auto subVar = varify(val);
      vars.push_back(subVar);
      shards::arrayPush(tmp.payload.seqValue, subVar->value());
    }
    SHVar var{};
    shards::cloneVar(var, tmp);
    shards::arrayFree(tmp.payload.seqValue);
    auto mvar = new malSHVar(var, true);
    for (auto &rvar : vars) {
      mvar->reference(rvar.ptr());
    }
    mvar->line = arg->line;
    return malSHVarPtr(mvar);
  } else if (const malHash *v = DYNAMIC_CAST(malHash, arg)) {
    shards::SHMap shMap;
    std::vector<malSHVarPtr> vars;
    for (auto [k, v] : v->m_map) {
      auto shv = varify(v);
      vars.emplace_back(shv);
      // key is either a quoted string or a keyword (starting with ':')
      shMap[k[0] == '"' ? unescape(k) : k.substr(1)] = shv->value();
    }
    SHVar tmp{};
    tmp.valueType = Table;
    tmp.payload.tableValue.api = &shards::GetGlobals().TableInterface;
    tmp.payload.tableValue.opaque = &shMap;
    SHVar var{};
    shards::cloneVar(var, tmp);
    auto mvar = new malSHVar(var, true);
    for (auto &rvar : vars) {
      mvar->reference(rvar.ptr());
    }
    mvar->line = arg->line;
    return malSHVarPtr(mvar);
  } else if (arg == mal::trueValue()) {
    SHVar var{};
    var.valueType = Bool;
    var.payload.boolValue = true;
    auto v = new malSHVar(var, false);
    v->line = arg->line;
    return malSHVarPtr(v);
  } else if (arg == mal::falseValue()) {
    SHVar var{};
    var.valueType = Bool;
    var.payload.boolValue = false;
    auto v = new malSHVar(var, false);
    v->line = arg->line;
    return malSHVarPtr(v);
  } else if (malSHVar *v = DYNAMIC_CAST(malSHVar, arg)) {
    SHVar var{};
    shards::cloneVar(var, v->value());
    auto res = new malSHVar(var, true);
    res->line = arg->line;
    return malSHVarPtr(res);
  } else if (malWireProvider *v = DYNAMIC_CAST(malWireProvider, arg)) {
    SHVar var = *v;
    auto providerVar = new malSHVar(var, false);
    providerVar->reference(v);
    providerVar->line = arg->line;
    return malSHVarPtr(providerVar);
  } else if (malShard *v = DYNAMIC_CAST(malShard, arg)) {
    auto shard = v->value();
    SHVar var{};
    var.valueType = ShardRef;
    var.payload.shardValue = shard;
    auto bvar = new malSHVar(var, false);
    v->consume();
    bvar->reference(v);
    bvar->line = arg->line;
    return malSHVarPtr(bvar);
  } else if (malSHWire *v = DYNAMIC_CAST(malSHWire, arg)) {
    auto wire = v->value();
    SHVar var{};
    var.valueType = SHType::Wire;
    var.payload.wireValue = wire;
    auto cvar = new malSHVar(var, false);
    cvar->reference(v);
    cvar->line = arg->line;
    return malSHVarPtr(cvar);
  } else if (malAtom *v = DYNAMIC_CAST(malAtom, arg)) {
    return varify(v->deref());
  } else {
    throw shards::SHException("Invalid variable");
  }
}

int findParamIndex(const std::string &name, SHParametersInfo params) {
  for (uint32_t i = 0; params.len > i; i++) {
    auto param = params.elements[i];
    if (param.name == name)
      return (int)i;
  }
  return -1;
}

void validationCallback(const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
  auto shard = const_cast<Shard *>(errorShard);
  if (!nonfatalWarning) {
    SHLOG_ERROR("Parameter validation failed: {} shard: {}", errorTxt, shard->name(shard));
  } else {
    SHLOG_INFO("Parameter validation warning: {} shard: {}", errorTxt, shard->name(shard));
  }
}

void setShardParameters(malShard *malshard, malValueIter begin, malValueIter end) {
  auto shard = malshard->value();
  auto argsBegin = begin;
  auto argsEnd = end;
  auto params = shard->parameters(shard);
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
        SHLOG_ERROR("Parameter not found: {} shard: {}", paramName, shard->name(shard));
        throw shards::SHException("Parameter not found");
      } else {
        auto var = varify(value);
        if (!validateSetParam(shard, idx, var->value(), validationCallback, nullptr)) {
          SHLOG_ERROR("Failed parameter: {} line: {}", paramName, value->line);
          throw shards::SHException("Parameter validation failed");
        }
        shard->setParam(shard, idx, &var->value());

        // keep ref
        malshard->reference(var.ptr());
      }
    } else if (pindex == -1) {
      // We expected a keyword! fail
      SHLOG_ERROR("Keyword expected, shard: {}", shard->name(shard));
      throw shards::SHException("Keyword expected");
    } else {
      auto var = varify(arg);
      if (!validateSetParam(shard, pindex, var->value(), validationCallback, nullptr)) {
        SHLOG_ERROR("Failed parameter index: {} line: {}", pindex, arg->line);
        throw shards::SHException("Parameter validation failed");
      }
      shard->setParam(shard, pindex, &var->value());

      // keep ref
      malshard->reference(var.ptr());

      pindex++;
    }
  }
}

malValuePtr newEnum(int32_t vendor, int32_t type, SHEnum value) {
  SHVar var{};
  var.valueType = Enum;
  var.payload.enumVendorId = vendor;
  var.payload.enumTypeId = type;
  var.payload.enumValue = value;
  return malValuePtr(new malSHVar(var, false));
}

struct Observer : public shards::RuntimeObserver {
  virtual ~Observer() {}

  malEnvPtr _env;

  void registerShard(const char *fullName, SHShardConstructor constructor) override {
    // do some reflection
    auto shard = constructor();
    // add params keywords if any
    auto params = shard->parameters(shard);
    for (uint32_t i = 0; i < params.len; i++) {
      auto param = params.elements[i];
      _env->set(":" + MalString(param.name), mal::keyword(":" + MalString(param.name)));
#ifndef NDEBUG
      // add coverage for parameters setting
      auto val = shard->getParam(shard, i);
      shard->setParam(shard, i, &val);
#endif
    }
    // define the new built-in
    MalString mname(fullName);
    malBuiltIn::ApplyFunc *func = [](const MalString &name, malValueIter argsBegin, malValueIter argsEnd) {
      auto shard = shards::createShard(name.c_str());
      shard->setup(shard);
      auto malshard = new malShard(shard);
      setShardParameters(malshard, argsBegin, argsEnd);
      return malValuePtr(malshard);
    };
    auto bi = new malBuiltIn(mname, func);
    builtIns.emplace(mname, bi);
    _env->set(fullName, bi);
    // destroy our sample shard
    shard->destroy(shard);
  }

  void registerEnumType(int32_t vendorId, int32_t typeId, SHEnumInfo info) override {
    for (uint32_t i = 0; i < info.labels.len; i++) {
      auto enumName = MalString(info.name) + "." + MalString(info.labels.elements[i]);
      auto enumValue = newEnum(vendorId, typeId, info.values.elements[i]);
      builtIns[enumName] = enumValue;
      _env->set(enumName, enumValue);
    }
  }
};

malShard *makeVarShard(malSHVar *v, const char *shardName) {
  auto b = shards::createShard(shardName);
  b->setup(b);
  b->setParam(b, 0, &v->value());
  auto blk = new malShard(b);
  blk->line = v->line;
  return blk;
}

BUILTIN(">>") { return mal::nilValue(); }

BUILTIN(">>!") { return mal::nilValue(); }

BUILTIN("&>") { return mal::nilValue(); }

BUILTIN(">==") { return mal::nilValue(); }

BUILTIN("??") { return mal::nilValue(); }

std::vector<malShardPtr> wireify(malValueIter begin, malValueIter end) {
  enum State { Get, Set, SetGlobal, Update, Ref, Push, PushNoClear, AddGetDefault };
  State state = Get;
  std::vector<malShardPtr> res;
  while (begin != end) {
    auto next = *begin++;
    if (auto *v = DYNAMIC_CAST(malSHVar, next)) {
      if (v->value().valueType == ContextVar) {
        if (state == Get) {
          res.emplace_back(makeVarShard(v, "Get"));
        } else if (state == Set) {
          res.emplace_back(makeVarShard(v, "Set"));
          state = Get;
        } else if (state == SetGlobal) {
          auto blk = makeVarShard(v, "Set");
          // set :Global true
          auto val = shards::Var(true);
          blk->value()->setParam(blk->value(), 2, &val);
          res.emplace_back(blk);
          state = Get;
        } else if (state == Update) {
          res.emplace_back(makeVarShard(v, "Update"));
          state = Get;
        } else if (state == Ref) {
          res.emplace_back(makeVarShard(v, "Ref"));
          state = Get;
        } else if (state == Push) {
          res.emplace_back(makeVarShard(v, "Push"));
          state = Get;
        } else if (state == PushNoClear) {
          auto blk = makeVarShard(v, "Push");
          // set :Clear false
          auto val = shards::Var(false);
          blk->value()->setParam(blk->value(), 3, &val);
          res.emplace_back(blk);
          state = Get;
        } else {
          throw shards::SHException("Expected a variable");
        }
      } else {
        auto blks = shardify(next);
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
        throw shards::SHException("Unexpected a token");
      }
    } else {
      if (state == AddGetDefault) {
        auto blk = res.back();
        if (strcmp(blk->value()->name(blk->value()), "Get") != 0) {
          throw shards::SHException("There should be a variable preceeding ||");
        }
        // set :Default
        auto v = varify(next);
        blk->value()->setParam(blk->value(), 3, &v->value());
        state = Get;
      } else {
        auto blks = shardify(next);
        res.insert(res.end(), blks.begin(), blks.end());
      }
    }
  }
  return res;
}

BUILTIN("Wire") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, wireName);
  auto mwire = new malSHWire(wireName->value());
  auto wireref = mwire->value();
  auto wire = SHWire::sharedFromRef(wireref);
  while (argsBegin != argsEnd) {
    auto pbegin = argsBegin;
    auto arg = *argsBegin++;
    // Option keywords or shards
    if (const malKeyword *v = DYNAMIC_CAST(malKeyword, arg)) {
      if (v->value() == ":Looped") {
        wire->looped = true;
      } else if (v->value() == ":Unsafe") {
        wire->unsafe = true;
      } else if (v->value() == ":LStack") {
        wire->stackSize = 4 * 1024 * 1024; // 4mb
      } else if (v->value() == ":SStack") { // default is 128kb
        wire->stackSize = 32 * 1024;       // 32kb
      }
    } else {
      auto blks = wireify(pbegin, argsEnd);
      for (auto blk : blks) {
        wire->addShard(blk->value());
        blk->consume();
        mwire->reference(blk.ptr());
      }
      break;
    }
  }
  return malValuePtr(mwire);
}

static MalString printValues(malValueIter begin, malValueIter end, const MalString &sep, bool readably) {
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
  SHLOG_INFO(printValues(argsBegin, argsEnd, " ", false));
  return mal::nilValue();
}

BUILTIN("Wire@") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, value);
  if (argsBegin != argsEnd) {
    return malValuePtr(new malWireProvider(value->ref(), *argsBegin));
  } else {
    return malValuePtr(new malWireProvider(value->ref()));
  }
}

BUILTIN("Wire*") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, value);
  if (argsBegin != argsEnd) {
    return malValuePtr(new malWireProvider(value->ref(), *argsBegin));
  } else {
    return malValuePtr(new malWireProvider(value->ref()));
  }
}

BUILTIN("eval-wire") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malString, value);

  malEnvPtr env(new malEnv());
  auto res = maleval(value->ref().c_str(), env);
  auto var = varify(res);
  if (var->value().valueType != SHType::Wire) {
    SHLOG_ERROR("Script did not return a SHWire");
    return mal::nilValue();
  }

  auto wireref = var->value().payload.wireValue;
  auto wire = SHWire::sharedFromRef(wireref);

  return malValuePtr(new malSHWire(wire));
}

static malValuePtr readVar(const SHVar &v) {
  if (v.valueType == SHType::String) {
    auto sv = SHSTRVIEW(v);
    MalString s(sv);
    return mal::string(s);
  } else if (v.valueType == SHType::Int) {
    return mal::number(double(v.payload.intValue), true);
  } else if (v.valueType == SHType::Float) {
    return mal::number(v.payload.floatValue, false);
  } else if (v.valueType == SHType::Seq) {
    auto vec = new malValueVec();
    for (auto &sv : v) {
      vec->emplace_back(readVar(sv));
    }
    return malValuePtr(new malList(vec));
  } else if (v.valueType == SHType::Table) {
    malHash::Map map;
    auto &t = v.payload.tableValue;
    SHTableIterator tit;
    t.api->tableGetIterator(t, &tit);
    SHString k;
    SHVar v;
    while (t.api->tableNext(t, &tit, &k, &v)) {
      map[escape(k)] = readVar(v);
    }
    return mal::hash(map);
  } else {
    // just return a variable in this case, but a copy, not cloning inner
    return malValuePtr(new malSHVar(v, false));
  }
}

BUILTIN("read-var") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malSHVar, value);
  auto &v = value->value();
  return readVar(v);
}

BUILTIN("->") {
  auto vec = new malValueVec();
  auto blks = wireify(argsBegin, argsEnd);
  for (auto blk : blks) {
    malShard *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("wireify") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, value);
  auto vec = new malValueVec();
  auto blks = wireify(value->begin(), value->end());
  for (auto blk : blks) {
    malShard *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("unquote") {
  CHECK_ARGS_IS(1);
  ARG(malSequence, value);
  auto vec = new malValueVec();
  auto blks = wireify(value->begin(), value->end());
  for (auto blk : blks) {
    malShard *pblk = blk.ptr();
    vec->emplace_back(pblk);
  }
  return malValuePtr(new malList(vec));
}

BUILTIN("schedule") {
  CHECK_ARGS_IS(2);
  ARG(malSHMesh, mesh);
  ARG(malSHWire, wire);
  mesh->schedule(wire);
  return mal::nilValue();
}

// used in wires without a mesh (manually prepared etc)
thread_local std::shared_ptr<SHMesh> TLSRootSHMesh{SHMesh::make()};

BUILTIN("prepare") {
  CHECK_ARGS_IS(1);
  ARG(malSHWire, wirevar);
  auto wire = SHWire::sharedFromRef(wirevar->value());
  SHInstanceData data{};
  data.wire = wire.get();
  wire->mesh = TLSRootSHMesh->shared_from_this();
  auto wireValidation = composeWire(
      wire.get(),
      [](const Shard *errorShard, const char *errorTxt, bool nonfatalWarning, void *userData) {
        if (!nonfatalWarning) {
          auto msg = "RunWire: failed inner wire validation, error: " + std::string(errorTxt);
          throw shards::SHException(msg);
        } else {
          SHLOG_INFO("RunWire: warning during inner wire validation: {}", errorTxt);
        }
      },
      nullptr, data);
  wire->composedHash = Var(1, 1); // just to mark as composed
  shards::arrayFree(wireValidation.exposedInfo);
  shards::prepare(wire.get(), nullptr);
  return mal::nilValue();
}

BUILTIN("set-global-wire") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malSHWire *v = DYNAMIC_CAST(malSHWire, first)) {
    auto wire = SHWire::sharedFromRef(v->value());
    std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
    shards::GetGlobals().GlobalWires[wire->name] = wire;
    return mal::trueValue();
  }
  return mal::falseValue();
}

BUILTIN("unset-global-wire") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malSHWire *v = DYNAMIC_CAST(malSHWire, first)) {
    auto wire = SHWire::sharedFromRef(v->value());
    std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
    auto it = shards::GetGlobals().GlobalWires.find(wire->name);
    if (it != shards::GetGlobals().GlobalWires.end()) {
      shards::GetGlobals().GlobalWires.erase(it);
      return mal::trueValue();
    }
  }
  return mal::falseValue();
}

BUILTIN("run-empty-forever") {
  while (true) {
    shards::sleep(1.0, false);
  }
}

BUILTIN("set-var") {
  CHECK_ARGS_IS(3);
  ARG(malSHWire, wirevar);
  ARG(malString, varName);
  auto last = *argsBegin;
  auto wire = SHWire::sharedFromRef(wirevar->value());
  auto var = varify(last);
  auto clonedVar = new malSHVar(var->value(), true);
  clonedVar->value().flags |= SHVAR_FLAGS_EXTERNAL;
  wire->externalVariables[varName->ref().c_str()] = &clonedVar->value();
  wirevar->reference(clonedVar); // keep alive until wire is destroyed
  return malValuePtr(clonedVar);
}

BUILTIN("start") {
  CHECK_ARGS_AT_LEAST(1);
  ARG(malSHWire, wirevar);
  auto wire = SHWire::sharedFromRef(wirevar->value());
  if (argsBegin != argsEnd) {
    ARG(malSHVar, inputVar);
    shards::start(wire.get(), inputVar->value());
  } else {
    shards::start(wire.get());
  }
  return mal::nilValue();
}

BUILTIN("tick") {
  CHECK_ARGS_IS(1);
  auto first = *argsBegin;
  if (const malSHWire *v = DYNAMIC_CAST(malSHWire, first)) {
    auto wire = SHWire::sharedFromRef(v->value());
    SHDuration now = SHClock::now().time_since_epoch();
    auto ticked = shards::tick(wire.get(), now);
    return mal::boolean(ticked);
  } else if (const malSHMesh *v = DYNAMIC_CAST(malSHMesh, first)) {
    auto noErrors = v->value()->tick();
    return mal::boolean(noErrors);
  } else {
    throw shards::SHException("tick Expected Mesh or Wire");
  }
  return mal::nilValue();
}

BUILTIN("stop") {
  CHECK_ARGS_IS(1);
  ARG(malSHWire, wirevar);
  auto wire = SHWire::sharedFromRef(wirevar->value());
  SHVar res{};
  shards::stop(wire.get(), &res);
  return malValuePtr(new malSHVar(res, true));
}

BUILTIN("run") {
  CHECK_ARGS_AT_LEAST(1);
  SHMesh *mesh = nullptr;
  SHWire *wire = nullptr;
  auto first = *argsBegin++;
  if (const malSHWire *v = DYNAMIC_CAST(malSHWire, first)) {
    auto cs = SHWire::sharedFromRef(v->value());
    wire = cs.get();
  } else if (const malSHMesh *v = DYNAMIC_CAST(malSHMesh, first)) {
    mesh = v->value();
  } else {
    throw shards::SHException("tick Expected Mesh or Wire");
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

  SHDuration dsleep(sleepTime);

  if (mesh) {
    auto now = SHClock::now();
    auto next = now + dsleep;
    while (!mesh->empty()) {
      const auto noErrors = mesh->tick();

      // other wires might be not in error tho...
      // so return only if empty
      if (!noErrors && mesh->empty()) {
        return mal::boolean(false);
      }

      if (dec) {
        times--;
        if (times == 0) {
          mesh->terminate();
          break;
        }
      }
      // We on purpose run terminate (evenutally)
      // before sleep
      // cos during sleep some shards
      // swap states and invalidate stuff
      if (sleepTime <= 0.0) {
        shards::sleep(-1.0);
      } else {
        // remove the time we took to tick from sleep
        now = SHClock::now();
        SHDuration realSleepTime = next - now;
        if (unlikely(realSleepTime.count() <= 0.0)) {
          // tick took too long!!!
          // TODO warn sometimes and skip sleeping, skipping callbacks too
          next = now + dsleep;
        } else {
          next = now + realSleepTime;
          shards::sleep(realSleepTime.count());
        }
      }
    }
  } else {
    SHDuration now = SHClock::now().time_since_epoch();
    while (!shards::tick(wire, now)) {
      if (dec) {
        times--;
        if (times == 0) {
          shards::stop(wire);
          break;
        }
      }
      shards::sleep(sleepTime);
      now = SHClock::now().time_since_epoch();
    }
  }

  spdlog::default_logger()->flush();

  return mal::boolean(true);
}

BUILTIN("sleep") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, sleepTime);
  shards::sleep(sleepTime->value());
  return mal::nilValue();
}

BUILTIN("override-root-path") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);

  std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
  shards::GetGlobals().RootPath = value->ref();
  fs::current_path(value->ref());
  return mal::nilValue();
}

BUILTIN("path") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  auto &s = value->ref();
  SHVar var{};
  var.valueType = Path;
  var.payload.stringValue = s.c_str();
  auto mvar = new malSHVar(var, false);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("enum") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  SHVar var{};
  var.valueType = Enum;
  var.payload.enumVendorId = static_cast<int32_t>(value0->value());
  var.payload.enumTypeId = static_cast<int32_t>(value1->value());
  var.payload.enumValue = static_cast<SHEnum>(value2->value());
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("string") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  SHVar var{};
  var.valueType = String;
  auto &s = value->ref();
  var.payload.stringValue = s.c_str();
  auto mvar = new malSHVar(var, false);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("bytes") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  SHVar var{};
  var.valueType = Bytes;
  auto &s = value->ref();
  var.payload.bytesValue = (uint8_t *)s.data();
  var.payload.bytesSize = s.size();
  auto mvar = new malSHVar(var, false);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("context-var") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  SHVar var{};
  var.valueType = ContextVar;
  auto &s = value->ref();
  var.payload.stringValue = s.c_str();
  auto mvar = new malSHVar(var, false);
  mvar->reference(value);
  return malValuePtr(mvar);
}

BUILTIN("int") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  SHVar var{};
  var.valueType = Int;
  var.payload.intValue = value->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("int2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  SHVar var{};
  var.valueType = Int2;
  var.payload.int2Value[0] = value0->value();
  var.payload.int2Value[1] = value1->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("int3") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  SHVar var{};
  var.valueType = Int3;
  var.payload.int3Value[0] = value0->value();
  var.payload.int3Value[1] = value1->value();
  var.payload.int3Value[2] = value2->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("int4") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  SHVar var{};
  var.valueType = Int4;
  var.payload.int4Value[0] = value0->value();
  var.payload.int4Value[1] = value1->value();
  var.payload.int4Value[2] = value2->value();
  var.payload.int4Value[3] = value3->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("color") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  SHVar var{};
  var.valueType = Color;
  var.payload.colorValue.r = static_cast<uint8_t>(value0->value());
  var.payload.colorValue.g = static_cast<uint8_t>(value1->value());
  var.payload.colorValue.b = static_cast<uint8_t>(value2->value());
  var.payload.colorValue.a = static_cast<uint8_t>(value3->value());
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("float") {
  CHECK_ARGS_IS(1);
  ARG(malNumber, value);
  SHVar var{};
  var.valueType = Float;
  var.payload.floatValue = value->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("float2") {
  CHECK_ARGS_IS(2);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  SHVar var{};
  var.valueType = Float2;
  var.payload.float2Value[0] = value0->value();
  var.payload.float2Value[1] = value1->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("float3") {
  CHECK_ARGS_IS(3);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  SHVar var{};
  var.valueType = Float3;
  var.payload.float3Value[0] = value0->value();
  var.payload.float3Value[1] = value1->value();
  var.payload.float3Value[2] = value2->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("float4") {
  CHECK_ARGS_IS(4);
  ARG(malNumber, value0);
  ARG(malNumber, value1);
  ARG(malNumber, value2);
  ARG(malNumber, value3);
  SHVar var{};
  var.valueType = Float4;
  var.payload.float4Value[0] = value0->value();
  var.payload.float4Value[1] = value1->value();
  var.payload.float4Value[2] = value2->value();
  var.payload.float4Value[3] = value3->value();
  return malValuePtr(new malSHVar(var, false));
}

BUILTIN("import") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);

  auto filepath = fs::path(value->value());

  auto lib_name_str = filepath.string();
  auto lib_name = lib_name_str.c_str();

  // FIXME we are leaking libs
#if _WIN32
  SHLOG_INFO("Importing DLL: {}", lib_name);
  auto handle = LoadLibraryA(lib_name);
  if (!handle) {
    SHLOG_ERROR("LoadLibrary failed.");
  }
#elif defined(__linux__) || defined(__APPLE__)
  SHLOG_INFO("Importing Shared Library: {}", lib_name);
  auto handle = dlopen(lib_name, RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    SHLOG_ERROR("dlerror: {}", dlerror());
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
#ifdef SHARDS_DESKTOP
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
#ifdef SHARDS_DESKTOP
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

BUILTIN("settings-try-set-min") {
  std::scoped_lock lock(GetGlobals().GlobalMutex);
  CHECK_ARGS_IS(2);
  ARG(malString, key);
  ARG(malSHVar, value);
  auto &current = GetGlobals().Settings[key->value()];
  if (current.valueType != SHType::None) {
    if (value->value() < current) {
      current = value->value();
      return mal::trueValue();
    } else {
      return mal::falseValue();
    }
  } else {
    current = value->value();
    return mal::trueValue();
  }
}

BUILTIN("settings-try-set-max") {
  std::scoped_lock lock(GetGlobals().GlobalMutex);
  CHECK_ARGS_IS(2);
  ARG(malString, key);
  ARG(malSHVar, value);
  auto &current = GetGlobals().Settings[key->value()];
  if (current.valueType != SHType::None) {
    if (value->value() > current) {
      current = value->value();
      return mal::trueValue();
    } else {
      return mal::falseValue();
    }
  } else {
    current = value->value();
    return mal::trueValue();
  }
}

BUILTIN("hasShard?") {
  CHECK_ARGS_IS(1);
  ARG(malString, value);
  if (builtIns.find(value->value()) != builtIns.end()) {
    return mal::trueValue();
  } else {
    return mal::falseValue();
  }
}

BUILTIN("shards") {
  std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
  malValueVec v;
  for (auto [name, _] : shards::GetGlobals().ShardsRegister) {
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
    auto shard = createShard(blkname->ref().c_str());
    DEFER(shard->destroy(shard));

    {
      auto help = shard->help(shard);
      map[":help"] = mal::string(help.string ? help.string : getString(help.crc));
    }

    auto params = shard->parameters(shard);
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
      {
        std::ostringstream ss;
        auto param = shard->getParam(shard, (int)i);
        if (param.valueType == SHType::String)
          ss << "\"" << param.payload.stringValue << "\"";
        else
          ss << param;
        pmap[":default"] = mal::string(ss.str());
      }
      pvec.emplace_back(mal::hash(pmap));
    }
    map[":parameters"] = mal::list(pvec.begin(), pvec.end());

    {
      std::stringstream ss;
      ss << shard->inputTypes(shard);
      map[":inputTypes"] = mal::string(ss.str());
      auto help = shard->inputHelp(shard);
      map[":inputHelp"] = mal::string(help.string ? help.string : getString(help.crc));
    }

    {
      std::stringstream ss;
      ss << shard->outputTypes(shard);
      map[":outputTypes"] = mal::string(ss.str());
      auto help = shard->outputHelp(shard);
      map[":outputHelp"] = mal::string(help.string ? help.string : getString(help.crc));
    }

    {
      auto properties = shard->properties(shard);
      if (unlikely(properties != nullptr)) {
        malHash::Map pmap;
        ForEach(*properties, [&](auto &key, auto &val) { pmap[escape(key)] = readVar(val); });
        map[":properties"] = mal::hash(pmap);
      }
    }

    return mal::hash(map);
  }
}

#ifndef SH_COMPRESSED_STRINGS
BUILTIN("export-strings") {
  assert(shards::GetGlobals().CompressedStrings);
  std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
  malValueVec strs;
  for (auto &[crc, str] : *shards::GetGlobals().CompressedStrings) {
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
#ifdef SH_COMPRESSED_STRINGS
  std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
  if (!shards::GetGlobals().CompressedStrings) {
    throw shards::SHException("String storage was null");
  }
  // run the script to populate compressed strings
  auto bytes = Var(__shards_compressed_strings);
  auto wire = ::shards::Wire("decompress strings").let(bytes).shard("Brotli.Decompress").shard("FromBytes");
  auto mesh = SHMesh::make();
  mesh->schedule(wire);
  mesh->tick();
  if (wire->finishedOutput.valueType != SHType::Seq) {
    throw shards::SHException("Failed to decompress strings!");
  }

  for (uint32_t i = 0; i < wire->finishedOutput.payload.seqValue.len; i++) {
    auto pair = wire->finishedOutput.payload.seqValue.elements[i];
    if (pair.valueType != SHType::Seq || pair.payload.seqValue.len != 2) {
      throw shards::SHException("Failed to decompress strings!");
    }
    auto crc = pair.payload.seqValue.elements[0];
    auto str = pair.payload.seqValue.elements[1];
    if (crc.valueType != SHType::Int || str.valueType != SHType::String) {
      throw shards::SHException("Failed to decompress strings!");
    }
    auto emplaced = strings_storage.emplace(uint32_t(crc.payload.intValue), str.payload.stringValue);
    auto &s = emplaced.first->second;
    auto &val = (*shards::GetGlobals().CompressedStrings)[uint32_t(crc.payload.intValue)];
    val.string = s.c_str();
    val.crc = uint32_t(crc.payload.intValue);
  }
#endif
  return mal::nilValue();
}

BUILTIN_ISA("Var?", malSHVar);
BUILTIN_ISA("Mesh?", malSHMesh);
BUILTIN_ISA("Wire?", malSHWire);
BUILTIN_ISA("Shard?", malShard);

extern "C" {
SHARDS_API __cdecl void *shLispCreate(const char *path) {
  try {
    auto env = new malEnvPtr(new malEnv);
    malinit(*env, path, path);
    return (void *)env;
  } catch (std::exception &ex) {
    SHLOG_FATAL("Failed to create lisp environment: {}", ex.what());
    return nullptr;
  }
}

SHARDS_API __cdecl void *shLispCreateSub(void *parent) {
  assert(parent != nullptr);
  auto env = new malEnvPtr(new malEnv(*reinterpret_cast<malEnvPtr *>(parent)));
  return (void *)env;
}

SHARDS_API __cdecl void shLispDestroy(void *env) {
  auto penv = (malEnvPtr *)env;
  {
    std::scoped_lock obsLock(observersMutex);
    observers.erase((malEnv *)penv->ptr());
  }
  delete penv;
}

SHARDS_API __cdecl SHBool shLispEval(void *env, const char *str, SHVar *output = nullptr) {
  auto penv = (malEnvPtr *)env;
  try {
    malValuePtr res;
    if (penv) {
      res = maleval(str, *penv);
    } else {
      auto cenv = malenv();
      res = maleval(str, cenv);
    }
    if (output) {
      auto mvar = varify(res);
      auto scriptVal = mvar->value();
      shards::cloneVar(*output, scriptVal);
    }
    return true;
  } catch (...) {
    return false;
  }
}
};

void setupObserver(std::shared_ptr<Observer> &obs, const malEnvPtr &env) {
  obs = std::make_shared<Observer>();
  obs->_env = env;
  {
    std::scoped_lock lock(shards::GetGlobals().GlobalMutex);
    shards::GetGlobals().Observers.emplace_back(obs);
  }
  {
    std::scoped_lock obsLock(observersMutex);
    observers[env.ptr()] = obs;
  }
}

namespace mal {
malValuePtr contextVar(const MalString &token) {
  SHVar tmp{}, v{};
  tmp.valueType = SHType::ContextVar;
  tmp.payload.stringValue = token.c_str();
  shards::cloneVar(v, tmp);
  return malValuePtr(new malSHVar(v, true));
}
} // namespace mal
