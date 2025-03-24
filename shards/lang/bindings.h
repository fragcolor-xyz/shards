#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include <shards/core/runtime.hpp>
struct Program;
struct Sequence;

extern "C" {

void shards_init(SHCore *core);

bool shards_read(SHStringWithLen name,
                 SHStringWithLen code,
                 SHStringWithLen base_path,
                 const SHStringWithLen *include_dirs,
                 uint32_t num_include_dirs,
                 SHLAst *out_ast);

bool shards_load_ast(const uint8_t *bytes, uint32_t size, SHLAst *out_ast);

SHVar shards_save_ast(Program *ast);

SHLEvalEnv *shards_create_env(SHStringWithLen namespace_);

void shards_forbid_shard(SHLEvalEnv *env, SHStringWithLen name);

void shards_free_env(SHLEvalEnv *env);

SHLEvalEnv *shards_create_sub_env(SHLEvalEnv *env, SHStringWithLen namespace_);

bool shards_eval_env(SHLEvalEnv *env, const SHVar *ast, SHLError *out_error);

/// It will consume the env
bool shards_transform_env(SHLEvalEnv *env, SHStringWithLen name, SHLWire *out_wire);

bool shards_transform_envs(SHLEvalEnv **env,
                           uintptr_t len,
                           SHStringWithLen name,
                           SHLWire *out_wire);

bool shards_eval_ast(const SHVar *ast, SHStringWithLen name, SHLWire *out_wire);

SHVar shards_print_ast(const SHVar *ast);

SHVar shards_clone_ast(const SHVar *ast);

/// To be used before compose or schedule basically to report errors
void shards_propagate_error(const SHVar *ast,
                            uint64_t wire_id,
                            uint64_t shard_id,
                            uint32_t line,
                            uint32_t column,
                            const SHVar *error);

void shards_free_wire(SHLWire *wire);

void shards_free_error(SHLError *error);

void shardsRegister_langffi_langffi(SHCore *core);

/// Please note it will consume `from` but not `to`
bool shards_merge_envs(SHLEvalEnv *from, SHLEvalEnv *to, SHLError *out_error);

extern void shards_flush_logs();

void setup_panic_hook();

int32_t shards_process_args(int32_t argc, const char *const *argv, bool no_cancellation);

} // extern "C"
