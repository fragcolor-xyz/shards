/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#import <Foundation/Foundation.h>
#include <string.h>

// Runs `thunk(ctx)` inside an Objective-C @try/@catch. Returns NULL on success,
// or a malloc'd UTF-8 description of the caught NSException (the caller must
// free() it).
//
// Swift cannot catch Objective-C exceptions. Without this shim, an NSException
// raised inside a Swift shard's lifecycle — e.g. NSColor/UIColor component
// access on a non-RGB color, or other UIKit/AppKit misuse — unwinds straight
// through Swift frames (which aren't set up for foreign-exception unwinding) and
// hard-crashes the process; the shards runtime only bridges C++ exceptions.
// Routing the Swift→C shard bridge (activate/warmup/cleanup/compose) through here
// turns those otherwise-fatal exceptions into recoverable shard errors that get
// reported back to the wire/model instead.
//
// Plain C function (defined in a .m so it has C linkage); the Swift side binds it
// by symbol via @_silgen_name, so no bridging header is required.
const char *SHRunCatchingNSException(void (*thunk)(void *), void *ctx) {
  @try {
    thunk(ctx);
    return NULL;
  } @catch (NSException *exception) {
    NSString *desc = exception.reason ?: exception.name ?: @"Objective-C exception";
    const char *utf8 = desc.UTF8String;
    return strdup(utf8 ? utf8 : "Objective-C exception");
  }
}
