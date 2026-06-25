// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2024 Fragcolor Pte. Ltd.
//
// On-device link stubs for the esp32-p4 bring-up:
//   * the shlang SOURCE PARSER FFI (normally the Rust langffi crate) — not needed
//     on device, where we load pre-serialized wires rather than parse .shs text;
//   * activateShardInline — the desktop inline-shard fast path lives in
//     modules/core; here we just call the shard's own activate (semantically
//     identical, only loses the inlining optimization);
//   * registerModuleShards — the core module's shard registry; real shards are the
//     next increment, this no-op lets registerShards() complete and the image link.
#include <shards/core/runtime.hpp>
#include <shards/core/foundation.hpp>
#include <shards/modules/langffi/line_info.hpp>
#include <ctime>
#include <unistd.h>

// esp32p4 newlib doesn't provide nanosleep; back it with usleep (us granularity is
// plenty for the runtime's sleep helper).
extern "C" int nanosleep(const struct timespec *req, struct timespec *rem) {
  (void)rem;
  if (req)
    usleep((useconds_t)(req->tv_sec * 1000000ULL + (req->tv_nsec + 999) / 1000));
  return 0;
}

namespace shards {
const SHVar *activateShardInline(Shard *blk, SHContext *context, const SHVar &input) noexcept {
  return blk->activate(blk, context, &input);
}
} // namespace shards

// Hand-written registry (replaces the build-generated one) for the Rust-FREE subset
// of core shards. Excludes rust/serialization/memoize/parallel/wires (Rust-backed or
// thread-pool dependent). Each fn is the SHARDS_REGISTER_FN(core,<id>) entry point.
extern "C" {
void shardsRegister_core_core(SHCore *core);
void shardsRegister_core_casting(SHCore *core);
void shardsRegister_core_flow(SHCore *core);
void shardsRegister_core_linalg(SHCore *core);
void shardsRegister_core_math(SHCore *core);
void shardsRegister_core_seqs(SHCore *core);
void shardsRegister_core_strings(SHCore *core);
void shardsRegister_core_logging(SHCore *core);
void shardsRegister_core_time(SHCore *core);
void shardsRegister_core_exposed(SHCore *core);
void shardsRegister_core_trait(SHCore *core);
}

namespace shards {
void registerModuleShards(SHCore *core) {
  shardsRegister_core_core(core);
  shardsRegister_core_casting(core);
  shardsRegister_core_flow(core);
  shardsRegister_core_linalg(core);
  shardsRegister_core_math(core);
  shardsRegister_core_seqs(core);
  shardsRegister_core_strings(core);
  shardsRegister_core_logging(core);
  shardsRegister_core_time(core);
  shardsRegister_core_exposed(core);
  shardsRegister_core_trait(core);
}
} // namespace shards

// shlang parser FFI (extern "C"): all fail/no-op — no on-device parser.
extern "C" {
bool shards_read(SHStringWithLen, SHStringWithLen, SHStringWithLen, const SHStringWithLen *, uint32_t, SHLAst *) { return false; }
bool shards_load_ast(const uint8_t *, uint32_t, SHLAst *) { return false; }
void shards_free_error(SHLError *) {}
SHLEvalEnv *shards_create_env(SHStringWithLen) { return nullptr; }
void shards_free_env(SHLEvalEnv *) {}
bool shards_eval_env(SHLEvalEnv *, const SHVar *, SHLError *) { return false; }
bool shards_set_defines(SHLEvalEnv *, const SHVar *, SHLError *) { return false; }
bool shards_transform_env(SHLEvalEnv *, SHStringWithLen, SHLWire *) { return false; }
bool shards_transform_envs(SHLEvalEnv **, size_t, SHStringWithLen, SHLWire *) { return false; }
bool shards_eval_ast(const SHVar *, SHStringWithLen, SHLWire *) { return false; }
void shards_free_wire(SHLWire *) {}
void shards_free_ast(SHLAst *) {}
}

// langffi file registry (declared in line_info.hpp, C++ linkage).
SHFileRegistryHandle *shlang_fr_static() { return nullptr; }
uint32_t shlang_fr_get_file_id(SHFileRegistryHandle *, SHStringWithLen) { return 0; }
SHStringWithLen shlang_fr_get_file_name(SHFileRegistryHandle *, uint32_t) { return SHStringWithLen{nullptr, 0}; }
