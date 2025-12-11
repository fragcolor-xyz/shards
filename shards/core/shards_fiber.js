/**
 * JSPI-based fiber management library for Shards WebAssembly
 *
 * This library provides fiber/coroutine support using JavaScript Promise Integration (JSPI)
 * instead of Asyncify. JSPI handles the native Wasm stack automatically, but we must manually
 * save/restore the linear memory stack (spill stack) where C++ variables live.
 *
 * Key insight: Each fiber needs its own saved stack buffer to avoid the reentrancy problem
 * where concurrent suspensions could corrupt each other's stack data.
 */

var LibraryShardsJspi = {
  // Fiber manager singleton - tracks all fiber contexts
  $fiberManager: {
    contextStack: [],      // Stack of active fiber contexts (for nested fiber support)
    fiberMap: new Map(),   // fiberId -> FiberContext
    nextId: 1,
    initialized: false,
  },
  $fiberManager__deps: [],

  /**
   * Create a new fiber context. Returns the fiber ID.
   * Called from C++ Fiber::init()
   */
  shardsFiberCreate__deps: ['$fiberManager'],
  shardsFiberCreate: function() {
    const id = fiberManager.nextId++;
    fiberManager.fiberMap.set(id, {
      id: id,
      stackTop: 0,              // Stack pointer when fiber entered
      savedStack: null,         // Saved linear memory stack (Uint8Array)
      savedStackPointer: 0,     // Stack pointer at suspension
      parentId: null,           // Parent fiber ID for nested contexts
      resolve: null,            // Promise resolver for resumption
      state: 'created'          // created | running | suspended | completed
    });
    return id;
  },

  /**
   * Called when a fiber starts execution.
   * Tracks the stack top and manages the context stack for nesting.
   */
  shardsFiberEnter__deps: ['$fiberManager'],
  shardsFiberEnter: function(fiberId) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) {
      err('shardsFiberEnter: Unknown fiber ' + fiberId);
      return;
    }

    // Track parent for nested fiber support
    const parentCtx = fiberManager.contextStack.length > 0
      ? fiberManager.contextStack[fiberManager.contextStack.length - 1]
      : null;
    ctx.parentId = parentCtx ? parentCtx.id : null;

    // Record stack top at entry
    ctx.stackTop = stackSave();

    // Push to context stack
    fiberManager.contextStack.push(ctx);
    ctx.state = 'running';
  },

  /**
   * Suspend the current fiber. This is an async function that:
   * 1. Saves the linear memory stack to a buffer
   * 2. Awaits a Promise (blocking until shardsFiberResume is called)
   * 3. Restores the linear memory stack on resume
   *
   * The __async: true tells Emscripten to wrap this with WebAssembly.Suspending
   */
  shardsFiberSuspend__deps: ['$fiberManager'],
  shardsFiberSuspend__async: true,
  shardsFiberSuspend: async function(fiberId) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) {
      err('shardsFiberSuspend: Unknown fiber ' + fiberId);
      return;
    }

    // Save the linear memory stack
    ctx.savedStackPointer = stackSave();
    const stackSize = ctx.stackTop - ctx.savedStackPointer;

    if (stackSize > 0) {
      // Copy stack region to saved buffer
      // Note: HEAPU8.slice creates a copy, which is what we want
      ctx.savedStack = HEAPU8.slice(ctx.savedStackPointer, ctx.stackTop);
    } else {
      ctx.savedStack = null;
    }

    ctx.state = 'suspended';

    // Wait for resume - this suspends the Wasm execution
    await new Promise(function(resolve) {
      ctx.resolve = resolve;
    });

    // On resume: restore the linear memory stack
    if (ctx.savedStack && ctx.savedStack.length > 0) {
      HEAPU8.set(ctx.savedStack, ctx.savedStackPointer);
    }
    stackRestore(ctx.savedStackPointer);
    ctx.state = 'running';
  },

  /**
   * Resume a suspended fiber by resolving its Promise.
   * Called from C++ Fiber::resume()
   */
  shardsFiberResume__deps: ['$fiberManager'],
  shardsFiberResume: function(fiberId) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) {
      err('shardsFiberResume: Unknown fiber ' + fiberId);
      return;
    }

    if (ctx.state === 'suspended' && ctx.resolve) {
      const resolve = ctx.resolve;
      ctx.resolve = null;
      resolve();
    }
    // If state is 'created', fiber hasn't started yet - no-op
  },

  /**
   * Called when a fiber completes execution.
   * Removes it from the context stack.
   */
  shardsFiberExit__deps: ['$fiberManager'],
  shardsFiberExit: function(fiberId) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) {
      err('shardsFiberExit: Unknown fiber ' + fiberId);
      return;
    }

    // Remove from context stack
    const idx = fiberManager.contextStack.indexOf(ctx);
    if (idx >= 0) {
      fiberManager.contextStack.splice(idx, 1);
    }

    ctx.state = 'completed';
    ctx.savedStack = null;  // Free memory
  },

  /**
   * Destroy a fiber context. Called from C++ Fiber destructor.
   */
  shardsFiberDestroy__deps: ['$fiberManager'],
  shardsFiberDestroy: function(fiberId) {
    fiberManager.fiberMap.delete(fiberId);
  },

  /**
   * Check if a fiber has completed.
   * Returns 1 if completed (or doesn't exist), 0 otherwise.
   */
  shardsFiberIsCompleted__deps: ['$fiberManager'],
  shardsFiberIsCompleted: function(fiberId) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) return 1;  // Doesn't exist = completed
    return ctx.state === 'completed' ? 1 : 0;
  },

  /**
   * Start a fiber's entry function. This wraps the C function with WebAssembly.promising
   * and invokes it. The fiber will run until it suspends or completes.
   *
   * @param fiberId - The fiber ID
   * @param entryFuncPtr - Pointer to the C entry function (in Wasm table)
   * @param arg - Argument to pass to the entry function (typically Fiber* pointer)
   */
  shardsFiberStartEntry__deps: ['$fiberManager', '$wasmTable'],
  shardsFiberStartEntry: function(fiberId, entryFuncPtr, arg) {
    const ctx = fiberManager.fiberMap.get(fiberId);
    if (!ctx) {
      err('shardsFiberStartEntry: Unknown fiber ' + fiberId);
      return;
    }

    // Get the entry function from the Wasm table
    var entryFunc = wasmTable.get(entryFuncPtr);
    if (!entryFunc) {
      err('shardsFiberStartEntry: Invalid function pointer ' + entryFuncPtr);
      return;
    }

    // Wrap with WebAssembly.promising to enable suspension
    var promisingEntry = WebAssembly.promising(entryFunc);

    // Start the fiber - this returns a Promise
    ctx.entryPromise = promisingEntry(arg);

    // Handle completion/error
    ctx.entryPromise.then(function() {
      // Note: fiber should have called shardsFiberExit before returning
      if (ctx.state !== 'completed') {
        ctx.state = 'completed';
      }
    }).catch(function(e) {
      err('JSPI: Fiber ' + fiberId + ' entry function threw: ' + e);
      ctx.state = 'completed';
    });
  },

  /**
   * Get the current fiber ID (topmost on context stack).
   * Returns -1 if no fiber is active.
   */
  shardsFiberGetCurrent__deps: ['$fiberManager'],
  shardsFiberGetCurrent: function() {
    if (fiberManager.contextStack.length === 0) {
      return -1;
    }
    return fiberManager.contextStack[fiberManager.contextStack.length - 1].id;
  },
};

addToLibrary(LibraryShardsJspi);
