#include "interface.hpp"
#include "debugger.hpp"
#include "log.hpp"
#include <semaphore>
#include <shards/core/foundation.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/assert.hpp>
#include <shards/utility.hpp>
#include <cstdlib>

namespace shards::dbg {

struct File {
  std::string path;
};

struct FileBreakpoint {
  int line;
  uint32_t fileId;
  std::shared_ptr<File> file;
};

enum class StepMode {
  None,
  Shard,
  Thread,
  In,
  Over,
};

struct ThreadState {
  std::thread::id id = std::this_thread::get_id();

  static ThreadState &get() {
    static thread_local ThreadState ts;
    return ts;
  }
  static std::thread::id getId() { return get().id; }
};

static inline std::atomic_uint64_t nextStackFrameId;

struct ContextTracking {
  SHContext *context;
  ContextTracking *parent;
  struct StackEntry {
    Shard *shard;
    // The start of the scope that was entered
    Shard **start;
    SHWire *wire;
    const SHVar **input;
    const SHVar **output;
    uint64_t stackFrameId = nextStackFrameId++;
    bool breakOnStep{};
    bool breakOnStepOut{};
    bool breakOnStepIn{};
  };
  std::vector<StackEntry> stack;
  uint64_t rootWireId;
  uint64_t threadId;

  // shortcut to check if any breakOnStep is set on any stack frame
  bool breakOnStepAny{};

  void clearBreakOnStep() {
    for (auto &s : stack) {
      s.breakOnStep = false;
      s.breakOnStepOut = false;
      s.breakOnStepIn = false;
    }
    breakOnStepAny = false;
    if (parent) {
      parent->clearBreakOnStep();
    }
  }
};

enum class VariableScopeType {
  Wire,
  WireInputs,
  Variable,
  Mesh,
};
enum class CachedVarType {
  Local,
  External,
  Global,
};

struct CachedScopeVar {
  CachedVarType type;
  std::string name;
  SHVar *var;
};
struct VariableScope {
  VariableScopeType type;
  uint64_t id;
  union {
    const ContextTracking::StackEntry *scope;
    const SHWire *wire;
    const SHVar *var;
    const SHMesh *mesh;
  };
  std::vector<CachedScopeVar> cache;
};

struct State {
  std::shared_ptr<DAPServer> server;
  std::optional<std::thread> serverThread;
  std::shared_mutex mtx;

  // The active paused location
  std::counting_semaphore<1> breakSema;

  std::map<std::string, std::shared_ptr<File>> files;
  std::vector<FileBreakpoint> fileBreakpoints;

  std::shared_mutex contextMtx;
  std::unordered_map<SHContext *, ContextTracking> contextTracking;
  std::unordered_map<uint64_t, SHContext *> contextThreadMap; // Maps root wire debug id to context
  uint64_t threadIdCounter{};

  std::shared_mutex stackFrameMtx;
  std::unordered_map<uint64_t, ContextTracking *> stackFrameMap;

  std::shared_mutex exclusiveMtx;
  void (State::*shardHook)(SHContext *context, Shard *blk){};

  std::atomic_int pauseQueue;

  std::optional<Command> command;

  uint64_t variableScopeCounter = 1;
  std::unordered_map<uint64_t, VariableScope> variableScopes;
  std::unordered_map<const SHMesh *, uint64_t> meshVariableScopeMap;
  std::unordered_map<const SHWire *, uint64_t> wireVariableScopeMap;
  std::unordered_map<const SHVar *, uint64_t> varVariableScopeMap;
  std::unordered_map<const SHWire *, uint64_t> wireInputScopeMap;

  // Used to wait for the client to connect
  std::atomic_bool initialClientConnected;
  uint32_t debuggerWaitMode = 0;

  // Updated when
  uint32_t configCounter;

  State() : breakSema(1) {

    if (const char *wait = std::getenv("SHARDS_DEBUGGER_WAIT")) {
      debuggerWaitMode = atoi(wait);
      if (debuggerWaitMode > 0) {
        // Break on startup
        shardHook = &State::hookWaitForDebugger;
      }
    }

    server = std::make_shared<DAPServer>();
    // Set up the onStarted callback before starting the server
    server->onStarted = [](const std::string &instanceName, int actualPort) {
      SPDLOG_LOGGER_INFO(logger, "DAP server '{}' is ready and listening on port {}", instanceName, actualPort);
    };

    serverThread.emplace([=, server = this->server]() {
      SPDLOG_LOGGER_INFO(logger, "DAP server starting");
      server->start();
    });
    server->handleCommand = [this](const Command &cmd) {
      if (cmd.type == CommandType::Pause) {
        shardHook = &State::hookPause;
        pauseQueue++;
      }
      command = cmd;
    };
    server->requestThreads = [this](std::vector<Thread> &threads) {
      std::shared_lock l(contextMtx);
      for (auto &p : contextTracking) {
        auto &ctx = p.first;
        threads.push_back({ctx->currentWire()->name, p.second.threadId});
      }
    };
    server->requestCallStack = [this](uint64_t threadId, std::vector<StackFrame> &frames) {
      std::shared_lock l1(contextMtx);
      auto it = contextThreadMap.find(threadId);
      if (it != contextThreadMap.end()) {
        auto &ctxTracking = contextTracking[it->second];
        auto appendStack = [&](ContextTracking &ctxTracking, std::optional<std::string> presentationHint) {
          // auto wire = it->second;
          // SHContext *context = wire->context;
          for (auto &frame : std::ranges::reverse_view(ctxTracking.stack)) {
            auto &sf = frames.emplace_back();
            auto &s = frame.shard;
            sf.line = s->line;
            sf.column = s->column;
            sf.name = s->name(frame.shard);
            if (sf.name == "Do" || sf.name == "Step" || sf.name == "SwitchTo" || sf.name == "WireRunner") {
              auto arg = s->getParam(s, 0);
              if (arg.valueType == SHType::Wire) {
                auto wire = SHWire::sharedFromRef(arg.payload.wireValue);
                if (wire) {
                  sf.name = fmt::format("{}({})", sf.name, wire->name);
                }
              }
            }
            sf.id = frame.stackFrameId;
            sf.presentationHint = presentationHint;
            if (frame.shard->file != 0) {
              auto &src = sf.source.emplace();
              src.path = toStringView(InternalCore::getSourceFileName(frame.shard->file));
            }
          }
        };
        appendStack(ctxTracking, std::nullopt);
        auto p = ctxTracking.parent;
        while (p) {
          auto &e = frames.emplace_back();
          e.name = fmt::format("--- {} ---", p->context->currentWire()->name);
          e.presentationHint = "label";
          appendStack(*p, "deemphasize");
          p = p->parent;
        }
      } else {
        SPDLOG_LOGGER_ERROR(logger, "Could not find thread: {}", threadId);
      }
    };
    server->configurationDone = [this]() {
      if (!initialClientConnected) {
        if (debuggerWaitMode == 2) {
          shardHook = &State::hookPause;
          pauseQueue++;
        } else {
          shardHook = nullptr;
        }
        initialClientConnected = true;
      }
    };
    server->setBreakpoints = [this](const Source &source, std::vector<BreakpointRequest> &breakpoints) {
      if (!source.path) {
        SPDLOG_LOGGER_ERROR(logger, "Ignoring breakpoints, no path for source");
        return;
      }
      exclusive([&]() {
        auto file = getOrCreateFile(*source.path);

        // Clear existing
        fileBreakpoints.erase(std::remove_if(fileBreakpoints.begin(), fileBreakpoints.end(),
                                             [&](const FileBreakpoint &bp) { return bp.file == file; }),
                              fileBreakpoints.end());

        // Add new
        for (auto &bp : breakpoints) {
          FileBreakpoint fileBp;
          fileBp.file = file;
          fileBp.line = bp.line;
          fileBp.fileId = InternalCore::getSourceFileId(toSWL(file->path));
          fileBreakpoints.push_back(fileBp);
        }
        breakpoints.insert(breakpoints.end(), breakpoints.begin(), breakpoints.end());
      });
    };
    server->requestScopes = [this](const ScopesArguments &args, std::vector<Scope> &scopes) {
      auto frameId = args.frameId;

      // Find the context tracking for this frame
      std::shared_lock<std::shared_mutex> lock(stackFrameMtx);
      auto it = stackFrameMap.find(frameId);
      if (it != stackFrameMap.end()) {
        auto ctxTracking = it->second;
        // Add scopes for this context tracking
        auto frameIt =
            std::find_if(ctxTracking->stack.begin(), ctxTracking->stack.end(),
                         [frameId](const ContextTracking::StackEntry &entry) { return entry.stackFrameId == frameId; });

        std::set<uint64_t> uniqueScopes;

        if (frameIt != ctxTracking->stack.end()) {
          auto wire = frameIt->wire;
          auto inputFrame = wireInputScopeMap.find(wire);
          if (inputFrame == wireInputScopeMap.end()) {
            auto res = wireInputScopeMap.emplace(wire, variableScopeCounter++);
            variableScopes.emplace(
                res.first->second,
                VariableScope{.type = VariableScopeType::WireInputs, .id = res.first->second, .scope = &*frameIt});
            inputFrame = res.first;
          }
          scopes.emplace_back(Scope{
              .name = "Inputs",
              .presentationHint = "returnValue",
              .variablesReference = inputFrame->second,
              .namedVariables = 1,
          });
        }

        auto mesh = ctxTracking->context->rootWire()->mesh.lock();
        if (mesh) {
          uint64_t scopeId;
          auto scopeIt = meshVariableScopeMap.find(mesh.get());
          if (scopeIt == meshVariableScopeMap.end()) {
            auto res = meshVariableScopeMap.emplace(mesh.get(), variableScopeCounter++);
            auto &scope =
                variableScopes
                    .emplace(res.first->second,
                             VariableScope{.type = VariableScopeType::Mesh, .id = res.first->second, .mesh = mesh.get()})
                    .first->second;
            scopeId = res.first->second;

            // Populate the variable cache
            for (auto &v : mesh->getVariables()) {
              scope.cache.emplace_back(
                  CachedScopeVar{.type = CachedVarType::Global, .name = fmt::format("{}", v.first), .var = &v.second});
            }
          } else {
            scopeId = scopeIt->second;
          }
          scopes.emplace_back(Scope{
              .name = fmt::format("{} (mesh)", mesh->getLabel()),
              .presentationHint = "locals",
              .variablesReference = scopeId,
          });
        }

        if (frameIt != ctxTracking->stack.end()) {
          auto &ws = ctxTracking->context->wireStack;
          // Iterate in reverse to add unique wires to scope mapping
          for (int i = ws.size() - 1; i >= 0; --i) {
            SHWire *wire = ws[i];

            uint64_t scopeId;
            auto scopeIt = wireVariableScopeMap.find(wire);
            if (scopeIt != wireVariableScopeMap.end()) {
              scopeId = scopeIt->second;
            } else {
              auto res = wireVariableScopeMap.emplace(wire, variableScopeCounter++);
              auto &scope = variableScopes
                                .emplace(res.first->second,
                                         VariableScope{.type = VariableScopeType::Wire, .id = res.first->second, .wire = wire})
                                .first->second;
              scopeId = res.first->second;

              // Populate the variable cache
              scope.cache.reserve(512);
              for (auto &[k, v] : wire->getVariables()) {
                scope.cache.emplace_back(CachedScopeVar{.type = CachedVarType::Local, .name = fmt::format("{}", k), .var = &v});
              }
              for (auto &[k, v] : wire->getExternalVariables()) {
                scope.cache.emplace_back(
                    CachedScopeVar{.type = CachedVarType::External, .name = fmt::format("{}", k), .var = v.var});
              }
            }

            if (uniqueScopes.insert(scopeId).second) {
              // Wire was newly inserted, add it to scopes
              auto &scope = scopes.emplace_back();
              scope.name = fmt::format("{} (wire)", wire->name);
              scope.variablesReference = scopeId;
              scope.presentationHint = "locals";
            }
          }
        }
      }
    };

    server->requestVariables = [=, this](const VariablesArguments &args, std::vector<Variable> &variables) {
      auto scopeIt = variableScopes.find(args.variablesReference);
      if (scopeIt != variableScopes.end()) {
        auto &scope = scopeIt->second;
        if (scope.type == VariableScopeType::Mesh || scope.type == VariableScopeType::Wire) {
          auto start = args.start.value_or(0);
          if (start > scope.cache.size())
            return;
          auto max = scope.cache.size() - start;
          auto end = args.count.value_or(max);
          if (end == 0)
            end = max;
          if (end > max)
            end = max;
          for (uint32_t i = start; i < end; i++) {
            auto &var = scope.cache[i];
            auto &v = variables.emplace_back();
            v.name = var.name;
            fetchVariable(*var.var, v);
          }
        } else if (scope.type == VariableScopeType::Variable) {
          auto &var = scope.var;
          fetchVariableChildren(args, *var, variables);
        } else if (scope.type == VariableScopeType::WireInputs) {
          auto scope1 = scope.scope;
          auto &v = variables.emplace_back();
          v.name = "<input>";
          if (*scope1->input) {
            fetchVariable(**scope1->input, v);
          } else {
            v.value = "<failed to read input>";
          }
        }
      }
    };
  }

  ~State() {
    if (serverThread) {
      server->stop();
      if (serverThread->joinable())
        serverThread->join();
    }
  }

  void fetchVariable(const SHVar &var, Variable &outVar) {
    outVar.type = type2Name(var.valueType);

    bool needSubScope{};
    if (var.valueType == SHType::Seq) {
      outVar.indexedVariables = var.payload.seqValue.len;
      if (*outVar.indexedVariables > 0) {
        needSubScope = true;
        outVar.value = fmt::format("[..., size = {}]", var.payload.seqValue.len);
      } else {
        outVar.value = "[]";
      }
    } else if (var.valueType == SHType::Table) {
      SHMap *table = static_cast<shards::SHMap *>(var.payload.tableValue.opaque);
      outVar.namedVariables = table->size();
      if (*outVar.namedVariables > 0) {
        needSubScope = true;
        outVar.value = fmt::format("{{..., size = {}}}", table->size());
      } else {
        outVar.value = "{}";
      }
    } else {
      outVar.value = fmt::format("{}", var);
    }

    // Populate child scope if needed
    if (needSubScope) {
      auto it = varVariableScopeMap.find(&var);
      if (it == varVariableScopeMap.end()) {
        auto it2 =
            variableScopes.emplace(variableScopeCounter++, VariableScope{.type = VariableScopeType::Variable, .var = &var}).first;
        auto &scope = it2->second;
        scope.id = it2->first;
        varVariableScopeMap.emplace(&var, scope.id);

        // Populate scope
        outVar.variablesReference = scope.id;
      } else {
        outVar.variablesReference = it->second;
      }
    }
  }
  void fetchVariableChildren(const VariablesArguments &args, const SHVar &var, std::vector<Variable> &outVars) {
    switch (var.valueType) {
    case SHType::Seq: {
      size_t i = args.start.value_or(0);
      size_t max = var.payload.seqValue.len;
      size_t end = args.count.value_or(max);
      if (end == 0)
        end = max;
      if (end > max)
        end = max;
      for (; i < end; i++) {
        auto &outVar = outVars.emplace_back();
        outVar.name = fmt::format("[{}]", i);
        fetchVariable(var.payload.seqValue.elements[i], outVar);
      }
    } break;
    case SHType::Table: {
      SHMap *table = static_cast<shards::SHMap *>(var.payload.tableValue.opaque);
      size_t max = table->size();
      size_t i = args.start.value_or(0);
      size_t end = args.count.value_or(max);
      if (end == 0)
        end = max;
      if (end > max)
        end = max;
      for (; i < end; i++) {
        auto it = table->begin() + i;
        auto &outVar = outVars.emplace_back();
        outVar.name = fmt::format("{}", it->first);
        fetchVariable(it->second, outVar);
      }
    } break;
    default:
      break;
    }
  }

  // This data is temporarily available until the debugger resumes
  void resetVariableScopes() {
    variableScopeCounter = 1;
    variableScopes.clear();
    meshVariableScopeMap.clear();
    wireVariableScopeMap.clear();
    varVariableScopeMap.clear();
    wireInputScopeMap.clear();
  }

  std::shared_ptr<File> getOrCreateFile(const std::string &path) {
    auto it = files.find(path);
    if (it == files.end()) {
      auto file = std::make_shared<File>();
      file->path = path;
      it = files.emplace(path, file).first;
    }
    return it->second;
  }

  template <typename F> void exclusive(F &&cb) {
    std::unique_lock<std::shared_mutex> lock(exclusiveMtx);
    auto shardHookPrev = shardHook;
    shardHook = &State::hookExclusiveLock;
    std::this_thread::yield();
    cb();
    shardHook = shardHookPrev;
  }

  bool isDebuggerPaused() {
    if (!breakSema.try_acquire())
      return true;
    breakSema.release();
    return false;
  }

  void hookPause(SHContext *context, Shard *blk) {
    if (pauseQueue.exchange(0) > 0) {
      // Clear hook
      shardHook = nullptr;

      // Enter pause loop
      breakInto(context, blk);
    }
  }

  void hookWaitForDebugger(SHContext *context, Shard *blk) {
    SPDLOG_LOGGER_INFO(logger, "Waiting for debugger to connect");
    while (true) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      if (initialClientConnected) {
        break;
      }
    }
    if (debuggerWaitMode == 2) {
      hookPause(context, blk);
    }
  }

  void hookPauseOther(SHContext *context, Shard *blk) {
    // When any other thread is paused, this will hang this thread, while others are being debugged
    breakSema.acquire();
    breakSema.release();
  }

  void hookExclusiveLock(SHContext *context, Shard *blk) {
    SPDLOG_LOGGER_DEBUG(logger, "Enter hookExclusiveLock");
    std::shared_lock lock(exclusiveMtx);
    SPDLOG_LOGGER_DEBUG(logger, "Leave hookExclusiveLock");
  }

  bool shouldBreak(SHContext *ctx, Shard *blk) {
    if (shardHook) {
      (this->*shardHook)(ctx, blk);
    }

    if (ctx->debugContextTracking->breakOnStepAny) {
      auto &s = ctx->debugContextTracking->stack.back();
      if (s.breakOnStep) {
        ctx->debugContextTracking->clearBreakOnStep();
        return true;
      }
    }

    for (const auto &bp : fileBreakpoints) {
      if (bp.fileId == blk->file && bp.line == blk->line) {
        return true;
      }
    }

    return false;
  }

  void breakInto(SHContext *context, Shard *blk) {
    SPDLOG_LOGGER_INFO(logger, "Breaking into {}, {}", blk->name(blk), formatShardSourceLocation(blk));
    breakSema.acquire();
    DEFER({ breakSema.release(); });

    // Make sure to freeze all the other threads
    shardHook = &State::hookPauseOther;

    uint64_t threadId = context->debugContextTracking->threadId;

    server->sendStoppedEvent("pause", threadId, "Execution paused by user");

    auto debugCtx = context->debugContextTracking;
    debugCtx->clearBreakOnStep();

    bool continue_ = false;
    do {
      if (command) {
        auto cmd = command->type;
        if (cmd == CommandType::Continue) {
          continue_ = true;
          command.reset();
        } else if (cmd == CommandType::StepIn) {
          debugCtx->stack.back().breakOnStepIn = true;
          debugCtx->stack.back().breakOnStep = true;
          debugCtx->breakOnStepAny = true;
          continue_ = true;
          command.reset();
        } else if (cmd == CommandType::StepOut) {
          debugCtx->stack.back().breakOnStepOut = true;
          debugCtx->breakOnStepAny = true;
          continue_ = true;
          command.reset();
        } else if (cmd == CommandType::StepOver) {
          debugCtx->stack.back().breakOnStep = true;
          debugCtx->breakOnStepAny = true;
          continue_ = true;
          command.reset();
        } else if (cmd == CommandType::Pause) {
          server->sendStoppedEvent("pause", threadId, "Execution paused by user");
          command.reset();
        } else if (cmd == CommandType::Stop) {
          continue_ = true;
          command.reset();
        }
      }
      if (!continue_)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!continue_);

    resetVariableScopes();
    server->sendContinuedEvent();

    // Reset the hook
    shardHook = nullptr;
  }

  ContextTracking *getOrCreateContextTracking(SHContext *context) {
    std::shared_lock<std::shared_mutex> lock(contextMtx);
    auto it = contextTracking.find(context);
    if (it == contextTracking.end()) {
      uint64_t threadId = threadIdCounter++;
      lock.unlock();
      std::unique_lock<std::shared_mutex> lock2(contextMtx);
      auto res = contextTracking.emplace(context, ContextTracking{.context = context, .threadId = threadId});
      it = res.first;
      if (res.second) {
        auto &dbgCtx = it->second;
        dbgCtx.rootWireId = context->rootWire()->debuggerId;
        contextThreadMap.emplace(dbgCtx.threadId, context);
      }
    }
    return &it->second;
  }

  void removeContextTracking(SHContext *context) {
    std::unique_lock<std::shared_mutex> lock(contextMtx);
    auto it = contextTracking.find(context);
    if (it != contextTracking.end()) {
      contextThreadMap.erase(it->second.threadId);
      contextTracking.erase(it);
    }
  }

  static std::shared_ptr<State> &instancePtr() {
    static std::shared_ptr<State> state_;
    return state_;
  }
  static State &instance() {
    if (!instancePtr()) {
      instancePtr() = std::make_shared<State>();
    }
    return *instancePtr();
  }
  static void resetInstance() { instancePtr().reset(); }
};

void onShard(SHContext *context, Shard *where) {
  if (auto ct = State::instance().getOrCreateContextTracking(context)) {
    ct->stack.back().shard = where;
  }

  // SPDLOG_LOGGER_TRACE(logger, "onShard: {}, {}", where->name(where), formatShardSourceLocation(where));
  if (State::instance().shouldBreak(context, where)) {
    State::instance().breakInto(context, where);
  }
}
void onError(SHContext *context, Shard *blk, const std::string &err) {}

void onWireRunStart(SHContext *ctx) {
  auto &i = State::instance();

  auto wire = ctx->currentWire();
  SPDLOG_LOGGER_TRACE(logger, "onWireRunStart {} ({})", wire->name, wire->debuggerId);

  auto ctxTracking = State::instance().getOrCreateContextTracking(ctx);
  ctx->debugContextTracking = ctxTracking;
}

void onWireRunEnd(SHContext *ctx) {
  auto &i = State::instance();

  // auto &ctx = wire->context;
  ctx->debugContextTracking = nullptr;

  State::instance().removeContextTracking(ctx);
}

void onEnterActivation(SHContext *context, const SHVar **input, const SHVar **output, Shard **start, size_t stride, size_t len) {
  if (len == 0)
    return;

  auto &i = State::instance();
  auto &dbgCtx = context->debugContextTracking;

  auto &entry = dbgCtx->stack.emplace_back();
  entry.start = start;
  entry.shard = start[0];
  entry.wire = context->currentWire();
  entry.input = input;
  entry.output = output;

  std::unique_lock<std::shared_mutex> lock(i.stackFrameMtx);
  i.stackFrameMap.emplace(entry.stackFrameId, dbgCtx);

  if (dbgCtx->breakOnStepAny) {
    if (dbgCtx->stack.size() > 1) {
      auto &s0 = dbgCtx->stack[dbgCtx->stack.size() - 2];
      auto &s1 = dbgCtx->stack.back();
      if (s0.breakOnStepIn) {
        s1.breakOnStep = true;
        s1.breakOnStepIn = true;
      }
      SPDLOG_LOGGER_TRACE(logger, "StepInto?: {} => {}", s0.shard->name(s0.shard), s1.shard->name(s1.shard));
    } else if (dbgCtx->stack.size() == 1) {
      // Assume we looped but didn't break, put a breakpoint on the first line
      auto &s1 = dbgCtx->stack.back();
      if (!s1.breakOnStep && !s1.breakOnStepIn && !s1.breakOnStepOut) {
        s1.breakOnStep = true;
      }
    }
  }

  if (context->parent) {
    auto &parentDbg = context->parent->debugContextTracking;
    dbgCtx->parent = parentDbg;
    shassert(parentDbg);
    if (parentDbg->breakOnStepAny) {
      auto &s0 = parentDbg->stack.back();
      auto &s1 = dbgCtx->stack.back();
      if (s0.breakOnStepIn) {
        s1.breakOnStep = true;
        s1.breakOnStepIn = true;
        dbgCtx->breakOnStepAny = true;
        SPDLOG_LOGGER_TRACE(logger, "StepIntoCtx?: {} ({}) => {} ({})", s0.shard->name(s0.shard),
                            dbgCtx->context->currentWire()->name, s1.shard->name(s1.shard), context->currentWire()->name);
      }
    }
  }
}

void onExitActivation(SHContext *context, Shard **start, size_t stride, size_t len) {
  if (len == 0)
    return;

  auto &i = State::instance();
  auto &dbgCtx = context->debugContextTracking;
  auto &stack = dbgCtx->stack;

  if (dbgCtx->breakOnStepAny) {
    if (dbgCtx->stack.size() > 1) {
      auto &s0 = dbgCtx->stack[dbgCtx->stack.size() - 2];
      auto &s1 = dbgCtx->stack.back();
      if (s1.breakOnStepOut || s1.breakOnStep) {
        s0.breakOnStep = true;
        s0.breakOnStepOut = true;
        SPDLOG_LOGGER_TRACE(logger, "StepOut?: {} => {}", s0.shard->name(s0.shard), s1.shard->name(s1.shard));
      }
    } else if (dbgCtx->stack.size() == 1) {
      auto &s1 = dbgCtx->stack.back();
      if ((s1.breakOnStepOut || s1.breakOnStep) && dbgCtx->parent) {
        auto &parentBack = dbgCtx->parent->stack.back();
        parentBack.breakOnStep = true;
        parentBack.breakOnStepOut = true;
        dbgCtx->parent->breakOnStepAny = true;
        dbgCtx->breakOnStepAny = false;
        SPDLOG_LOGGER_TRACE(logger, "StepOut?: {} => {}", s1.shard->name(s1.shard), parentBack.shard->name(parentBack.shard));
      }
    }
  }

  std::unique_lock<std::shared_mutex> lock(i.stackFrameMtx);
  {
    auto idx = stack.back().stackFrameId;
    i.stackFrameMap.erase(idx);
  }

  stack.pop_back();
}
void unload() { State::resetInstance(); }
} // namespace shards::dbg
