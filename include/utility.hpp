/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_UTILITY_HPP
#define CB_UTILITY_HPP

#include "chainblocks.h"
#include <magic_enum.hpp>
#include <nameof.hpp>
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

public:
  TParamVar() {}

  explicit TParamVar(CBVar initialValue) {
    // notice, no cloning here!, purely utility
    _v = initialValue;
  }

  explicit TParamVar(CBVar initialValue, bool clone) {
    if (clone) {
      CB_CORE::cloneVar(_v, initialValue);
    } else {
      _v = initialValue;
    }
  }

  ~TParamVar() { CB_CORE::destroyVar(_v); }

  void reset() {
    // reset is useful specially
    // if we swap nodes
    _cp = nullptr;
  }

  CBVar &operator=(const CBVar &value) {
    CB_CORE::cloneVar(_v, value);
    _cp = nullptr; // reset this!
    return _v;
  }

  operator CBVar() const { return _v; }

  CBVar &operator()(CBContext *ctx) {
    if (_v.valueType == ContextVar) {
      if (unlikely(!_cp)) {
        _cp = CB_CORE::findVariable(ctx, _v.payload.stringValue);
        // Do some type checking once - here!
        cb_debug_only({
          if (_v.payload.variableInfo) {
            bool valid = false;
            for (auto i = 0; i < stbds_arrlen(_v.payload.variableInfo); i++) {
              if (_cp->valueType == _v.payload.variableInfo[i].basicType) {
                // todo check other stuff like enum/object
                valid = true;
                break;
              }
            }
            assert(valid);
          } else {
            auto msg = "WARNING: Context variable without type info! " +
                       std::string(_v.payload.stringValue);
            CB_CORE::log(msg.c_str());
          }
        });
        return *_cp;
      } else {
        return *_cp;
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
  static constexpr auto ename = NAMEOF_TYPE(E);
  CBEnumInfo info;
  std::vector<std::string> labels;

public:
  TEnumInfo(int32_t vendorId, int32_t enumId) {
    info.name = ename.c_str();
    for (auto &view : eseq) {
      labels.emplace_back(view);
    }
    for (auto &label : labels) {
      stbds_arrpush(info.labels, label.c_str());
    }
    CB_CORE::registerEnumType(vendorId, enumId, info);
  }

  ~TEnumInfo() { stbds_arrfree(info.labels); }
};
}; // namespace chainblocks

#endif
