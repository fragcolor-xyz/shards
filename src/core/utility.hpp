/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "easylogging++.h"
#include "parallel_hashmap/phmap.h"

namespace chainblocks {
template <typename T> class ThreadShared {
public:
  ThreadShared() { addRef(); }

  ~ThreadShared() { decRef(); }

  T &operator()() { return *_tp; }

private:
  void addRef() {
    if (_refs == 0) {
      _tp = new T();
      DLOG(TRACE) << "Created a ThreadShared";
    }
    _refs++;
  }

  void decRef() {
    _refs--;
    if (_refs == 0) {
      delete _tp;
      _tp = nullptr;
      DLOG(TRACE) << "Deleted a ThreadShared";
    }
  }

  static inline thread_local uint64_t _refs = 0;
  static inline thread_local T *_tp = nullptr;
};

template <typename T> class GlobalShared {
public:
  GlobalShared() { addRef(); }

  ~GlobalShared() { decRef(); }

  T &operator()() { return *_tp; }

private:
  void addRef() {
    if (_refs == 0) {
      _tp = new T();
      DLOG(TRACE) << "Created a GlobalShared";
    }
    _refs++;
  }

  void decRef() {
    _refs--;
    if (_refs == 0) {
      delete _tp;
      _tp = nullptr;
      DLOG(TRACE) << "Deleted a GlobalShared";
    }
  }

  static inline uint64_t _refs = 0;
  static inline T *_tp = nullptr;
};
}; // namespace chainblocks
