#ifndef BAF4257E_4054_4E92_B538_547BD5F05C0D
#define BAF4257E_4054_4E92_B538_547BD5F05C0D

#include <shards/shards.h>

#ifdef shards_EXPORTS
#define SHLISP_API SHARDS_EXPORT
#else
#define SHLISP_API SHARDS_IMPORT
#endif

extern "C" {
SHLISP_API __cdecl void *shLispCreate(const char *path);
SHLISP_API __cdecl void *shLispCreateSub(void *parent);
SHLISP_API __cdecl void shLispSetPrefix(const char *envNamespace);
SHLISP_API __cdecl void shLispDestroy(void *env);
SHLISP_API __cdecl SHBool shLispEval(void *env, const char *str, SHVar *output = nullptr);
SHLISP_API __cdecl SHBool shLispAddToEnv(void *env, const char *str, SHVar input, bool reference);
}

#endif /* BAF4257E_4054_4E92_B538_547BD5F05C0D */
