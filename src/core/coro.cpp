#include "foundation.hpp"

thread_local emscripten_fiber_t *em_local_coro{nullptr};
thread_local emscripten_fiber_t em_main_coro{};
thread_local uint8_t em_asyncify_main_stack[CBCoro::as_stack_size];

[[noreturn]] static void action(void *p) {
  LOG(TRACE) << "EM FIBER ACTION RUN";
  auto coro = reinterpret_cast<CBCoro *>(p);
  coro->func();
  // If entry_func returns, the entire program will end, as if main had
  // returned.
  abort();
}

void CBCoro::init(const std::function<void()> &func) {
  LOG(TRACE) << "EM FIBER INIT";
  this->func = func;
  c_stack = new (std::align_val_t{16}) uint8_t[CB_STACK_SIZE];
  emscripten_fiber_init(&em_fiber, action, this, c_stack, stack_size,
                        asyncify_stack, as_stack_size);
}

NO_INLINE void CBCoro::resume() {
  LOG(TRACE) << "EM FIBER SWAP RESUME " << (void *)(&em_fiber);
  // ensure local thread is setup
  if (!em_main_coro.stack_ptr) {
    LOG(DEBUG) << "CBCoro - initialization of new thread";
    emscripten_fiber_init_from_current_context(
        &em_main_coro, em_asyncify_main_stack, CBCoro::as_stack_size);
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

NO_INLINE void CBCoro::yield() {
  LOG(TRACE) << "EM FIBER SWAP YIELD " << (void *)(&em_fiber);
  // always yields to main
  assert(em_parent_fiber);
  em_local_coro = em_parent_fiber;
  emscripten_fiber_swap(&em_fiber, em_parent_fiber);
}
