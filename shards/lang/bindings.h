#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <ostream>
#include <new>
#include <shards/core/runtime.hpp>

struct EvalEnv;

struct Sequence;

struct SHLError {
  char *message;
  uintptr_t line;
  uintptr_t column;
};

struct SHLAst {
  Sequence *ast;
  SHLError *error;
};

struct SHLWire {
  SHWireRef *wire;
  SHLError *error;
};

extern "C" {

void shards_init(SHCore *core);

SHLAst shards_read(const char *code);

EvalEnv *shards_create_env();

void shards_free_env(EvalEnv *env);

EvalEnv *shards_create_sub_env(EvalEnv *env);

const SHLError *shards_eval_env(EvalEnv *env, Sequence *ast);

SHLWire shards_eval(Sequence *sequence, const char *name);

SHVar shards_print_ast(Sequence *ast);

void shards_free_sequence(Sequence *sequence);

void shards_free_wire(SHWireRef *wire);

void shards_free_error(SHLError *error);

int32_t shards_process_args(int32_t argc, const char *const *argv);

} // extern "C"
