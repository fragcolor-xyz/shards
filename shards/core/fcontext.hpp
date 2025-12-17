/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

#ifndef SH_CORE_FCONTEXT_HPP
#define SH_CORE_FCONTEXT_HPP

#include <cstddef>
#include <cstdint>

namespace shards {
namespace fcontext {

// Opaque context handle - points to saved register state on stack
typedef void *fcontext_t;

// Transfer structure returned by jump_fcontext
// Contains the context we came from and optional user data
struct transfer_t {
  fcontext_t fctx; // Context that yielded to us
  void *data;      // User data passed through the switch
};

// Create a new fiber context on the given stack
// sp: Stack pointer (top of allocated stack memory, i.e., base + size)
// size: Size of the allocated stack in bytes
// fn: Entry function to call when context is first resumed
// Returns: New context handle, or nullptr on failure
extern "C" fcontext_t sh_make_fcontext(void *sp, std::size_t size, void (*fn)(transfer_t));

// Switch to target context
// to: Context to switch to
// vp: User data to pass to the target context
// Returns: transfer_t containing the context we came from and data it passed
extern "C" transfer_t sh_jump_fcontext(fcontext_t const to, void *vp);

// Switch to target context and execute function on its stack
// to: Context to switch to
// vp: User data to pass
// fn: Function to execute on target's stack before resuming target
// Returns: transfer_t from the eventual return
extern "C" transfer_t sh_ontop_fcontext(fcontext_t const to, void *vp, transfer_t (*fn)(transfer_t));

} // namespace fcontext
} // namespace shards

#endif // SH_CORE_FCONTEXT_HPP
