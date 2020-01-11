/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

/*
Utility to auto load auto discover blocks from DLLs
All that is needed is to declare a chainblocks::registerBlocks
At runtime just dlopen the dll, that's it!
*/

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

#ifndef DLLBLOCK_NAMESPACE
#error "You must define a unique DLLBLOCK_NAMESPACE"
#endif

namespace chainblocks {
namespace DLLBLOCK_NAMESPACE {
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
    _core.log("loading external blocks...");
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
                                 CBObjectInfo info) {
    sCore._core.registerObjectType(vendorId, typeId, info);
  }

  static void registerEnumType(int32_t vendorId, int32_t typeId,
                               CBEnumInfo info) {
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

  static CBVar *findVariable(CBContext *context, const char *name) {
    return sCore._core.findVariable(context, name);
  }

  static void throwException(const char *errorText) {
    sCore._core.throwException(errorText);
  }

  static CBVar suspend(CBContext *context, double seconds) {
    return sCore._core.suspend(context, seconds);
  }

  static void cloneVar(CBVar &dst, const CBVar &src) {
    sCore._core.cloneVar(&dst, &src);
  }

  static void destroyVar(CBVar &var) { sCore._core.destroyVar(&var); }

  static void freeArray(void *arr) { sCore._core.freeArray(arr); }

  static CBValidationResult validateChain(CBChain *chain,
                                          CBValidationCallback callback,
                                          void *userData, CBInstanceData data) {
    return sCore._core.validateChain(chain, callback, userData, data);
  }

  static CBRunChainOutput runChain(CBChain *chain, CBContext *context,
                                   CBVar input) {
    return sCore._core.runChain(chain, context, input);
  }

  static CBValidationResult validateBlocks(CBlocks blocks,
                                           CBValidationCallback callback,
                                           void *userData,
                                           CBInstanceData data) {
    return sCore._core.validateBlocks(blocks, callback, userData, data);
  }

  static CBVar runBlocks(CBlocks blocks, CBContext *context, CBVar input) {
    return sCore._core.runBlocks(blocks, context, input);
  }

  static void log(const char *msg) { sCore._core.log(msg); }

private:
  static inline CoreLoader sCore{};
};

typedef TParamVar<Core> ParamVar;
typedef TBlocksVar<Core> BlocksVar;
} // namespace DLLBLOCK_NAMESPACE
}; // namespace chainblocks

#endif
