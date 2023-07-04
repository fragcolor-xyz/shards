#ifndef SHARDS_LANG_H
#define SHARDS_LANG_H

#include "shards.h"

typedef struct SHSequence SHSequence;
typedef struct SHEnvironment SHEnvironment;

struct SHLError {
  char *message;
  size_t line;
  size_t column;
};

struct SHLWire {
  SHWireRef *wire;
  SHLError *error;
};

struct SHLAst {
  SHSequence *ast;
  SHLError *error;
};

#ifdef __cplusplus
extern "C" {
#endif

void shards_init(void *core);

SHLAst shards_read(const char *code);

SHEnvironment *shards_create_env(void);

void shards_free_env(SHEnvironment *env);

SHEnvironment *shards_create_sub_env(SHEnvironment *env);

SHLError *shards_eval_env(SHEnvironment *env, SHSequence *sequence);

SHLWire shards_eval(SHSequence *sequence, const char *name);

void shards_free_sequence(SHSequence *sequence);

void shards_free_wire(SHWireRef *wire);

void shards_free_error(SHLError *error);

int shards_process_args(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif