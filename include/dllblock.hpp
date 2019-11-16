/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_DLLBLOCK_HPP
#define CB_DLLBLOCK_HPP

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif
#include <cassert>

#include "blockwrapper.hpp"
#include "chainblocks.h"

namespace chainblocks {
// this must be defined in the external
extern void registerBlocks();

struct CoreLoader {
  CBCore _core;

  CoreLoader() {
    CBChainblocksInterface ifaceproc;
#ifdef _WIN32
    auto handle = GetModuleHandle(NULL);
    ifaceproc =
        (CBChainblocksInterface)GetProcAddress(handle, "chainblocksInterface");
#else
    auto handle = dlopen(NULL, RTLD_NOW);
    ifaceproc = (CBChainblocksInterface)dlsym(handle, "chainblocksInterface");
#endif

    if (!ifaceproc) {
      // try again.. see if we are libcb
#ifdef _WIN32
      handle = GetModuleHandleA("libcb.dll");
      if (handle)
        ifaceproc = (CBChainblocksInterface)GetProcAddress(
            handle, "chainblocksInterface");
#else
      handle = dlopen("libcb.so", RTLD_NOW);
      if (handle)
        ifaceproc =
            (CBChainblocksInterface)dlsym(handle, "chainblocksInterface");
#endif
    }

    assert(ifaceproc);
    _core = ifaceproc();
    registerBlocks();
  }
};

class Core {
public:
  static void registerBlock(const char *fullName,
                            CBBlockConstructor constructor) {
    sCore._core.registerBlock(fullName, constructor);
  }

  static void registerObjectType(int32_t vendorId, int32_t typeId,
                                 struct CBObjectInfo info) {
    sCore._core.registerObjectType(vendorId, typeId, info);
  }

  static void registerEnumType(int32_t vendorId, int32_t typeId,
                               struct CBEnumInfo info) {
    sCore._core.registerEnumType(vendorId, typeId, info);
  }

  static void registerRunLoopCallback(const char *eventName,
                                      CBCallback callback) {
    sCore._core.registerRunLoopCallback(eventName, callback);
  }

  static void registerExitCallback(const char *eventName, CBCallback callback) {
    sCore._core.registerExitCallback(eventName, callback);
  }

  static void unregisterRunLoopCallback(const char *eventName) {
    sCore._core.unregisterRunLoopCallback(eventName);
  }

  static void unregisterExitCallback(const char *eventName) {
    sCore._core.unregisterExitCallback(eventName);
  }

  static CBVar *contextVariable(struct CBContext *context, const char *name) {
    return sCore._core.contextVariable(context, name);
  }

  static void throwException(const char *errorText) {
    sCore._core.throwException(errorText);
  }

  static CBVar suspend(struct CBContext *context, double seconds) {
    return sCore._core.suspend(context, seconds);
  }

  static void cloneVar(struct CBVar *dst, const struct CBVar *src) {
    sCore._core.cloneVar(dst, src);
  }

  static void destroyVar(struct CBVar *var) { sCore._core.destroyVar(var); }

  static CBRunChainOutput runSubChain(struct CBChain *chain,
                                      struct CBContext *context,
                                      struct CBVar input) {
    return sCore._core.runSubChain(chain, context, input);
  }

  static CBValidationResult validateChain(struct CBChain *chain,
                                          CBValidationCallback callback,
                                          void *userData,
                                          struct CBTypeInfo inputType) {
    return sCore._core.validateChain(chain, callback, userData, inputType);
  }

  static void activateBlock(struct CBlock *block, struct CBContext *context,
                            struct CBVar *input, struct CBVar *output) {
    sCore._core.activateBlock(block, context, input, output);
  }

  static void log(const char *msg) { sCore._core.log(msg); }

private:
  static inline CoreLoader sCore{};
};

class Variable {
private:
  CBVar _v{};
  CBVar *_cp = nullptr;
  CBContext *_ctx = nullptr;

public:
  Variable() {}

  Variable(CBVar initialValue) {
    // notice, no cloning here!, purely utility
    _v = initialValue;
  }

  Variable(CBVar initialValue, bool clone) {
    if (clone) {
      Core::cloneVar(&_v, &initialValue);
    } else {
      _v = initialValue;
    }
  }

  ~Variable() { Core::destroyVar(&_v); }

  void setParam(const CBVar &value) {
    Core::cloneVar(&_v, &value);
    _cp = nullptr; // reset this!
  }

  CBVar &getParam() { return _v; }

  CBVar &get(CBContext *ctx, bool global = false) {
    if (unlikely(ctx != _ctx)) {
      // reset the ptr if context changed (stop/restart etc)
      _cp = nullptr;
      _ctx = ctx;
    }

    if (_v.valueType == ContextVar) {
      if (unlikely(!_cp)) {
        _cp = Core::contextVariable(ctx, _v.payload.stringValue);
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
}; // namespace chainblocks

#endif
