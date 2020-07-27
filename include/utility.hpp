/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#ifndef CB_UTILITY_HPP
#define CB_UTILITY_HPP

#include "chainblocks.h"
#include <future>
#include <magic_enum.hpp>
#include <memory>
#include <mutex>
#include <nameof.hpp>
#include <string>
#include <vector>

#ifndef __EMSCRIPTEN__
#include <taskflow/taskflow.hpp>
#endif

namespace chainblocks {
// SFINAE tests
#define CB_HAS_MEMBER_TEST(_name_)                                             \
  template <typename T> class has_##_name_ {                                   \
    typedef char one;                                                          \
    struct two {                                                               \
      char x[2];                                                               \
    };                                                                         \
    template <typename C> static one test(decltype(&C::_name_));               \
    template <typename C> static two test(...);                                \
                                                                               \
  public:                                                                      \
    enum { value = sizeof(test<T>(0)) == sizeof(char) };                       \
  }

template <typename T> class ThreadShared {
public:
  ThreadShared() { addRef(); }

  ~ThreadShared() { decRef(); }

  T &operator()() {
    if (!_tp) {
      // create
      _tp = new T();
      // lock now since _ptrs is shared
      std::unique_lock<std::mutex> lock(_m);
      // store thread local location
      _ptrs.push_back(&_tp);
    }

    return *_tp;
  }

private:
  void addRef() {
    std::unique_lock<std::mutex> lock(_m);

    _refs++;
  }

  void decRef() {
    std::unique_lock<std::mutex> lock(_m);

    _refs--;

    if (_refs == 0) {
      for (auto ptr : _ptrs) {
        // delete the internal object
        delete *ptr;
        // null the thread local
        *ptr = nullptr;
      }
      _ptrs.clear();
    }
  }

  static inline std::mutex _m;
  static inline uint64_t _refs = 0;
  static inline thread_local T *_tp = nullptr;
  static inline std::vector<T **> _ptrs;
};

template <typename T> class Shared {
public:
  Shared() { addRef(); }

  ~Shared() { decRef(); }

  T &operator()() {
    if (!_tp) {
      std::unique_lock<std::mutex> lock(_m);
      // check again as another thread might have locked
      if (!_tp)
        _tp = new T();
    }

    return *_tp;
  }

private:
  void addRef() {
    std::unique_lock<std::mutex> lock(_m);

    _refs++;
  }

  void decRef() {
    std::unique_lock<std::mutex> lock(_m);

    _refs--;

    if (_refs == 0 && _tp) {
      // delete the internal object
      delete _tp;
      // null the thread local
      _tp = nullptr;
    }
  }

  static inline std::mutex _m;
  static inline uint64_t _refs = 0;
  static inline T *_tp = nullptr;
};

template <class CB_CORE> class TParamVar {
private:
  CBVar _v{};
  CBVar *_cp = nullptr;
  CBSeq *_stack = nullptr;

public:
  TParamVar() {}

  explicit TParamVar(CBVar initialValue) {
    CB_CORE::cloneVar(_v, initialValue);
  }

  ~TParamVar() {
    cleanup();
    CB_CORE::destroyVar(_v);
  }

  void warmup(CBContext *ctx) {
    if (_v.valueType == ContextVar) {
      assert(!_cp);
      _cp = CB_CORE::referenceVariable(ctx, _v.payload.stringValue);
    } else {
      _cp = &_v;
    }
    assert(_cp);
  }

  void cleanup() {
    if (_v.valueType == ContextVar) {
      CB_CORE::releaseVariable(_cp);
    }
    _cp = nullptr;
  }

  CBVar &operator=(const CBVar &value) {
    CB_CORE::cloneVar(_v, value);
    cleanup();
    return _v;
  }

  operator CBVar() const { return _v; }
  const CBVar *operator->() const { return &_v; }

  CBVar &get() {
    assert(_cp);
    return *_cp;
  }

  bool isVariable() { return _v.valueType == ContextVar; }

  const char *variableName() {
    if (isVariable())
      return _v.payload.stringValue;
    else
      return nullptr;
  }
};

template <class CB_CORE, typename E> class TEnumInfo {
private:
  static constexpr auto eseq = magic_enum::enum_names<E>();
  CBEnumInfo info;
  std::vector<std::string> labels;
  std::vector<CBString> clabels;

public:
  TEnumInfo(const char *name, int32_t vendorId, int32_t enumId) {
    info.name = name;
    for (auto &view : eseq) {
      labels.emplace_back(view);
    }
    for (auto &s : labels) {
      clabels.emplace_back(s.c_str());
    }
    info.labels.elements = &clabels[0];
    info.labels.len = labels.size();
    CB_CORE::registerEnumType(vendorId, enumId, info);
  }
};

template <class CB_CORE> class TBlocksVar {
private:
  CBVar _blocksParam{}; // param cache
  CBlocks _blocks{};    // var wrapper we pass to validate and activate
  std::vector<CBlockPtr> _blocksArray; // blocks actual storage
  CBComposeResult _chainValidation{};

  void destroy() {
    for (auto it = _blocksArray.rbegin(); it != _blocksArray.rend(); ++it) {
      auto blk = *it;
      blk->cleanup(blk);
      blk->destroy(blk);
    }
    _blocksArray.clear();
  }

public:
  ~TBlocksVar() {
    destroy();
    CB_CORE::destroyVar(_blocksParam);
    CB_CORE::expTypesFree(_chainValidation.exposedInfo);
    CB_CORE::expTypesFree(_chainValidation.requiredInfo);
  }

  void cleanup() {
    for (auto it = _blocksArray.rbegin(); it != _blocksArray.rend(); ++it) {
      auto blk = *it;
      blk->cleanup(blk);
    }
  }

  void warmup(CBContext *context) {
    for (auto &blk : _blocksArray) {
      if (blk->warmup)
        blk->warmup(blk, context);
    }
  }

  CBVar &operator=(const CBVar &value) {
    cbassert(value.valueType == None || value.valueType == Block ||
             value.valueType == Seq);

    CB_CORE::cloneVar(_blocksParam, value);

    destroy();
    if (_blocksParam.valueType == Block) {
      assert(!_blocksParam.payload.blockValue->owned);
      _blocksParam.payload.blockValue->owned = true;
      _blocksArray.push_back(_blocksParam.payload.blockValue);
    } else {
      for (uint32_t i = 0; i < _blocksParam.payload.seqValue.len; i++) {
        auto blk = _blocksParam.payload.seqValue.elements[i].payload.blockValue;
        assert(!blk->owned);
        blk->owned = true;
        _blocksArray.push_back(blk);
      }
    }

    // We want to avoid copies in hot paths
    // So we write here the var we pass to CORE
    _blocks.elements = &_blocksArray[0];
    _blocks.len = _blocksArray.size();

    return _blocksParam;
  }

  operator CBVar() const { return _blocksParam; }

  CBComposeResult compose(const CBInstanceData &data) {
    // Free any previous result!
    CB_CORE::expTypesFree(_chainValidation.exposedInfo);
    CB_CORE::expTypesFree(_chainValidation.requiredInfo);

    _chainValidation = CB_CORE::composeBlocks(
        _blocks,
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            auto msg =
                "Error during inner chain validation: " + std::string(errorTxt);
            CB_CORE::log(msg.c_str());
            throw chainblocks::ComposeError("Failed inner chain validation.");
          } else {
            auto msg = "Warning during inner chain validation: " +
                       std::string(errorTxt);
            CB_CORE::log(msg.c_str());
          }
        },
        this, data);
    return _chainValidation;
  }

  CBChainState activate(CBContext *context, const CBVar &input, CBVar &output,
                        const bool handleReturn = false) {
    return CB_CORE::runBlocks(_blocks, context, input, &output, handleReturn);
  }

  operator bool() const { return _blocksArray.size() > 0; }

  const CBlocks &blocks() const { return _blocks; }
};

template <class CB_CORE> struct TOwnedVar : public CBVar {
  TOwnedVar() : CBVar() {}
  TOwnedVar(TOwnedVar &&source) { *this = source; }
  TOwnedVar(const TOwnedVar &source) : CBVar() {
    CB_CORE::cloneVar(*this, source);
  }
  TOwnedVar(const CBVar &source) : CBVar() { CB_CORE::cloneVar(*this, source); }
  TOwnedVar &operator=(const CBVar &other) {
    CB_CORE::cloneVar(*this, other);
    return *this;
  }
  TOwnedVar &operator=(const TOwnedVar &other) {
    CB_CORE::cloneVar(*this, other);
    return *this;
  }
  TOwnedVar &operator=(TOwnedVar &&other) {
    CB_CORE::destroyVar(*this);
    *this = other;
    return *this;
  }
  ~TOwnedVar() { CB_CORE::destroyVar(*this); }
};

// https://godbolt.org/z/I72ctd
template <class Function> struct Defer {
  Function _f;
  Defer(Function &&f) : _f(f) {}
  ~Defer() { _f(); }
};

#define DEFER_NAME(uniq) _defer##uniq
#define DEFER_DEF(uniq, body)                                                  \
  ::chainblocks::Defer DEFER_NAME(uniq)([&]() { body; })
#define DEFER(body) DEFER_DEF(__LINE__, body)

#ifndef __EMSCRIPTEN__
template <class CB_CORE> struct AsyncOp {
  AsyncOp(CBContext *context) : _context(context) {}

  // template <class Function, class... Args>
  // CBVar operator()(Function &&f, Args &&... args) {
  //   auto asyncRes = std::async(std::launch::async, f, args...);
  //   // Wait suspending!
  //   while (true) {
  //     auto state = asyncRes.wait_for(std::chrono::seconds(0));
  //     if (state == std::future_status::ready ||
  //         CB_CORE::suspend(_context, 0) != CBChainState::Continue)
  //       break;
  //   }
  //   // This should also throw if we had exceptions
  //   return asyncRes.get();
  // }

  CBVar operator()(std::future<CBVar> &fut) {
    while (true) {
      auto state = fut.wait_for(std::chrono::seconds(0));
      if (state == std::future_status::ready ||
          CB_CORE::suspend(_context, 0) != CBChainState::Continue)
        break;
    }
    // This should also throw if we had exceptions
    return fut.get();
  }

  void operator()(std::future<void> &fut) {
    while (true) {
      auto state = fut.wait_for(std::chrono::seconds(0));
      if (state == std::future_status::ready)
        break;
      // notice right now we cannot really abort a chain (suspend returns `don't
      // continue`) this would set flow out of scope...
      // TODO fix this
      CB_CORE::suspend(_context, 0);
    }
    // This should also throw if we had exceptions
    fut.get();
  }

  template <class Function> void sidechain(tf::Executor &exec, Function &&f) {
    tf::Taskflow flow;
    std::exception_ptr p = nullptr;

    // wrap into a call to catch exceptions
    auto call = [&]() {
      try {
        f();
      } catch (...) {
        p = std::current_exception();
      }
    };

    flow.emplace(call);
    auto fut = exec.run(flow);
    // ensure flow runs
    // to ensure determinism and no leakage
    DEFER({
      if (fut.valid())
        fut.wait();
    });

    while (true) {
      auto state = fut.wait_for(std::chrono::seconds(0));
      if (state == std::future_status::ready)
        break;
      // notice right now we cannot really abort a chain (suspend returns `don't
      // continue`) this would set flow out of scope...
      // TODO fix this
      CB_CORE::suspend(_context, 0);
    }

    if (p)
      std::rethrow_exception(p);
  }

  template <typename Result, class Function>
  Result sidechain(tf::Executor &exec, Function &&f) {
    tf::Taskflow flow;
    std::exception_ptr p = nullptr;
    Result res;

    // wrap into a call to catch exceptions
    auto call = [&]() {
      try {
        res = f();
      } catch (...) {
        p = std::current_exception();
      }
    };

    flow.emplace(call);
    auto fut = exec.run(flow);

    // ensure flow runs
    // to ensure determinism and no leakage
    DEFER({
      if (fut.valid())
        fut.wait();
    });

    while (true) {
      auto state = fut.wait_for(std::chrono::seconds(0));
      if (state == std::future_status::ready ||
          CB_CORE::suspend(_context, 0) != CBChainState::Continue)
        break;
    }

    if (p)
      std::rethrow_exception(p);

    return res;
  }

private:
  CBContext *_context;
};
#endif
}; // namespace chainblocks

#endif
