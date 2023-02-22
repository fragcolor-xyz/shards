#ifndef BAF4257E_4054_4E92_B538_547BD5F05C0D
#define BAF4257E_4054_4E92_B538_547BD5F05C0D

#include "shards.h"

extern "C" {
SHARDS_API __cdecl void *shLispCreate(const char *path);
SHARDS_API __cdecl void *shLispCreateSub(void *parent);
SHARDS_API __cdecl void *shLispSetPrefix(const char *envNamespace);
SHARDS_API __cdecl void shLispDestroy(void *env);
SHARDS_API __cdecl SHBool shLispEval(void *env, const char *str, SHVar *output = nullptr);
SHARDS_API __cdecl SHBool shLispAddToEnv(void *env, const char *str, SHVar input, bool reference);
}

#endif /* BAF4257E_4054_4E92_B538_547BD5F05C0D */
