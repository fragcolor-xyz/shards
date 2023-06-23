#ifndef SHARDS_LANG_H
#define SHARDS_LANG_H

#include "shards.h"

typedef struct SHSequence SHSequence;

struct SHLWire {
  SHWireRef *wire;
};

struct SHLError {
  char *message;
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

SHLWire shards_eval(SHSequence *sequence, const char *name);

void shards_free_sequence(SHSequence *sequence);

void shards_free_wire(SHWireRef *wire);

void shards_free_error(SHLError *error);

#ifdef __cplusplus
}
#endif

#endif