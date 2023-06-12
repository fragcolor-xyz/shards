#ifndef B37A77BC_0C30_4248_9B98_162909DB42DB
#define B37A77BC_0C30_4248_9B98_162909DB42DB

#include <shards/modules/gfx/gfx.hpp>
#include <shards/modules/gfx/window.hpp>
#include <shards/core/params.hpp>
#include <shards/core/brancher.hpp>
#include <shards/core/object_var_util.hpp>
#include <shards/iterator.hpp>
#include <shards/modules/core/time.hpp>
#include <shards/input/window_input.hpp>
#include <shards/input/debug.hpp>
#include <shards/input/state.hpp>
#include <shards/input/detached.hpp>
#include <shards/core/module.hpp>

namespace shards::input {

struct ShardsInput {
  
  struct Frame {
    std::vector<input::ConsumableEvent> events;
    bool canReceiveInput{};
    InputRegion region;

    void clear() {
      canReceiveInput = false;
      events.clear();
    }
  };
  using EventBuffer = input::EventBuffer<Frame>;

};

}

#endif /* B37A77BC_0C30_4248_9B98_162909DB42DB */
