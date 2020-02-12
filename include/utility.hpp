/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_UTILITY_HPP
#define CB_UTILITY_HPP

#include "chainblocks.h"
#include <magic_enum.hpp>
#include <nameof.hpp>
#include <string>
#include <vector>

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
      _tp = new T();
      // store thread local location
      _ptrs.push_back(&_tp);
    }

    return *_tp;
  }

private:
  void addRef() { _refs++; }

  void decRef() {
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

  static inline uint64_t _refs = 0;
  static inline thread_local T *_tp = nullptr;
  static inline std::vector<T **> _ptrs;
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
    reset();
    CB_CORE::destroyVar(_v);
  }

  void reset() {
    if (_cp) {
      CB_CORE::releaseVariable(_cp);
      _cp = nullptr;
    }
    _stack = nullptr;
  }

  CBVar &operator=(const CBVar &value) {
    CB_CORE::cloneVar(_v, value);
    reset();
    return _v;
  }

  operator CBVar() const { return _v; }

  CBVar &operator()(CBContext *ctx) {
    if (_v.valueType == ContextVar) {
      if (_v.payload.stringValue) {
        if (unlikely(!_cp)) {
          _cp = CB_CORE::referenceVariable(ctx, _v.payload.stringValue);
        }
        return *_cp;
      } else {
        if (unlikely(!_stack)) {
          _stack = CB_CORE::getStack(ctx);
        }
        return _stack->elements[(_stack->len - 1) - _v.payload.stackPosition];
      }
    } else {
      return _v;
    }
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
  CBVar _blocks{};
  std::vector<CBlockPtr> _blocksArray;
  CBValidationResult _chainValidation{};

  void destroy() {
    for (auto it = _blocksArray.rbegin(); it != _blocksArray.rend(); ++it) {
      auto blk = *it;
      blk->cleanup(blk);
      blk->destroy(blk);
    }
    _blocksArray.clear();
  }

  void cleanup() {
    for (auto it = _blocksArray.rbegin(); it != _blocksArray.rend(); ++it) {
      auto blk = *it;
      blk->cleanup(blk);
    }
  }

public:
  ~TBlocksVar() {
    destroy();
    CB_CORE::destroyVar(_blocks);
    CB_CORE::expTypesFree(_chainValidation.exposedInfo);
  }

  void reset() { cleanup(); }

  CBVar &operator=(const CBVar &value) {
    cbassert(value.valueType == None || value.valueType == Block ||
             value.valueType == Seq);

    CB_CORE::cloneVar(_blocks, value);

    destroy();
    if (_blocks.valueType == Block) {
      _blocksArray.push_back(_blocks.payload.blockValue);
    } else {
      for (uint32_t i = 0; i < _blocks.payload.seqValue.len; i++) {
        _blocksArray.push_back(
            _blocks.payload.seqValue.elements[i].payload.blockValue);
      }
    }

    return _blocks;
  }

  operator CBVar() const { return _blocks; }

  CBValidationResult validate(const CBInstanceData &data) {
    // Free any previous result!
    CB_CORE::expTypesFree(_chainValidation.exposedInfo);

    CBlocks blocks{};
    blocks.elements = &_blocksArray[0];
    blocks.len = _blocksArray.size();
    _chainValidation = CB_CORE::validateBlocks(
        blocks,
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            auto msg =
                "Error during inner chain validation: " + std::string(errorTxt);
            CB_CORE::log(msg.c_str());
            CB_CORE::throwException("Failed inner chain validation.");
          } else {
            auto msg = "Warning during inner chain validation: " +
                       std::string(errorTxt);
            CB_CORE::log(msg.c_str());
          }
        },
        this, data);
    return _chainValidation;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    CBlocks blocks{};
    blocks.elements = &_blocksArray[0];
    blocks.len = _blocksArray.size();
    return CB_CORE::runBlocks(blocks, context, input);
  }

  operator bool() const { return _blocksArray.size() > 0; }
};
}; // namespace chainblocks

#endif
