#include "foundation.hpp"

static struct Globals {
  Globals() {
    LOG(TRACE) << "EM MAIN FIBER INIT";
    emscripten_fiber_init_from_current_context(&main_coro, asyncify_main_stack,
                                               CBCoro::as_stack_size);

#ifndef NDEBUG
    CBCoro c1;
    c1.init(
      [](CBChain *chain, CBFlow *flow, CBCoro *coro) {
        LOG(TRACE) << "Inside coro";
        coro->yield();
        LOG(TRACE) << "Inside coro again";
        coro->yield();
      },
      nullptr, nullptr);
    c1.resume();
    c1.resume();
#endif
  }

  emscripten_fiber_t main_coro{};
  uint8_t asyncify_main_stack[CBCoro::as_stack_size];
} Globals;

[[noreturn]] static void action(void *p) {
  LOG(TRACE) << "EM FIBER ACTION RUN";
  auto coro = reinterpret_cast<CBCoro *>(p);
  coro->func(coro->chain, coro->flow, coro);
  throw;
}

CBCoro::CBCoro() {}

void CBCoro::init(
    const std::function<void(CBChain *, CBFlow *, CBCoro *)> &func,
    CBChain *chain, CBFlow *flow) {
  LOG(TRACE) << "EM FIBER INIT";
  this->chain = chain;
  this->flow = flow;
  this->func = func;
  emscripten_fiber_init(&em_fiber, action, this, c_stack, stack_size,
                        asyncify_stack, as_stack_size);
}

void CBCoro::resume() {
  LOG(TRACE) << "EM FIBER SWAP RESUME";
  emscripten_fiber_swap(&Globals.main_coro, &em_fiber);
}

void CBCoro::yield() {
  LOG(TRACE) << "EM FIBER SWAP YIELD";
  emscripten_fiber_swap(&em_fiber, &Globals.main_coro);
}
