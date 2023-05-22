/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2020 Fragcolor Pte. Ltd. */

#include "foundation.hpp"

thread_local emscripten_fiber_t *em_local_coro{nullptr};
thread_local emscripten_fiber_t em_main_coro{};
thread_local uint8_t em_asyncify_main_stack[SHCoro::as_stack_size];

[[noreturn]] static void action(void *p) {
  SHLOG_TRACE("EM FIBER ACTION RUN");
  auto coro = reinterpret_cast<SHCoro *>(p);
  coro->func();
  // If entry_func returns, the entire program will end, as if main had
  // returned.
  abort();
}

void SHCoro::init(const std::function<void()> &func) {
  SHLOG_TRACE("EM FIBER INIT");
  this->func = func;
  c_stack = new (std::align_val_t{16}) uint8_t[stack_size];
  emscripten_fiber_init(&em_fiber, action, this, c_stack, stack_size, asyncify_stack, as_stack_size);
}

NO_INLINE void SHCoro::resume() {
  SHLOG_TRACE("EM FIBER SWAP RESUME {}", reinterpret_cast<uintptr_t>(&em_fiber));
  // ensure local thread is setup
  if (!em_main_coro.stack_ptr) {
    SHLOG_DEBUG("SHCoro - initialization of new thread");
    emscripten_fiber_init_from_current_context(&em_main_coro, em_asyncify_main_stack, SHCoro::as_stack_size);
  }
  // ensure we have a local coro
  if (!em_local_coro) {
    em_local_coro = &em_main_coro;
  }
  // from current to new
  em_parent_fiber = em_local_coro;
  em_local_coro = &em_fiber;
  emscripten_fiber_swap(em_parent_fiber, &em_fiber);
}

NO_INLINE void SHCoro::yield() {
  SHLOG_TRACE("EM FIBER SWAP YIELD {}", reinterpret_cast<uintptr_t>(&em_fiber));
  // always yields to main
  assert(em_parent_fiber);
  em_local_coro = em_parent_fiber;
  emscripten_fiber_swap(&em_fiber, em_parent_fiber);
}
