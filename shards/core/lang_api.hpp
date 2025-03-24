#ifndef EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD
#define EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD

#include <shards/shards.h>

namespace shards {
extern "C" {
bool shards_read(SHStringWithLen name, SHStringWithLen code, SHStringWithLen base_path, const SHStringWithLen *include_dirs,
                 uint32_t num_include_dirs, SHLAst *out_ast);
bool shards_load_ast(const uint8_t *bytes, uint32_t size, SHLAst *out_ast);
void shards_free_error(SHLError *error);
SHLEvalEnv *shards_create_env(SHStringWithLen namespace_);
void shards_free_env(SHLEvalEnv *env);
bool shards_eval_env(SHLEvalEnv *env, const SHVar *ast, SHLError *error);
bool shards_transform_env(SHLEvalEnv *env, SHStringWithLen name, SHLWire *out_wire);
bool shards_transform_envs(SHLEvalEnv **env, size_t len, SHStringWithLen name, SHLWire *out_wire);
bool shards_eval_ast(const SHVar *ast, SHStringWithLen name, SHLWire *out_wire);
void shards_free_wire(SHLWire *wire);
void shards_free_ast(SHLAst *ast);
}
inline void setupCoreLang(SHCore *result) {
  result->read = [](SHStringWithLen name, SHStringWithLen code, SHStringWithLen basePath, const SHStringWithLen *includeDirs,
                    uint32_t numIncludeDirs,
                    SHLAst *out_ast) { return shards_read(name, code, basePath, includeDirs, numIncludeDirs, out_ast); };

  result->loadAst = [](const uint8_t *bytes, uint32_t size, SHLAst *out_ast) { return shards_load_ast(bytes, size, out_ast); };

  result->freeError = [](SHLError *error) { shards_free_error(error); };

  result->createEvalEnv = [](SHStringWithLen namespace_) { return shards_create_env(namespace_); };

  result->freeEvalEnv = [](SHLEvalEnv *env) { shards_free_env(env); };

  result->eval = [](SHLEvalEnv *env, const SHVar *ast, SHLError *error) -> bool { return shards_eval_env(env, ast, error); };

  result->transformEnv = [](SHLEvalEnv *env, SHStringWithLen name, SHLWire *out_wire) {
    return shards_transform_env(env, name, out_wire);
  };

  result->transformEnvs = [](SHLEvalEnv **env, uint32_t len, SHStringWithLen name, SHLWire *out_wire) {
    return shards_transform_envs(env, len, name, out_wire);
  };

  result->evalAst = [](const SHVar *ast, SHStringWithLen name, SHLWire *out_wire) {
    return shards_eval_ast(ast, name, out_wire);
  };

  result->freeWire = [](SHLWire *wire) { shards_free_wire(wire); };

  result->freeAst = [](SHLAst *ast) { shards_free_ast(ast); };
}
} // namespace shards

#endif /* EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD */
