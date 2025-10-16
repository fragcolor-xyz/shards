#ifndef EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD
#define EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD

#include <shards/shards.h>
#include <shards/modules/langffi/line_info.hpp>
#include "foundation.hpp"

extern SHCore sh_current_interface;
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

uint32_t InternalCore::getSourceFileId(SHStringWithLen path) {
  static SHFileRegistryHandle *frh = shlang_fr_static();
  return shlang_fr_get_file_id(frh, path);
}
SHStringWithLen InternalCore::getSourceFileName(uint32_t file_id) {
  static SHFileRegistryHandle *frh = shlang_fr_static();
  return shlang_fr_get_file_name(frh, file_id);
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

  result->getSourceFileId = InternalCore::getSourceFileId;
  result->getSourceFileName = InternalCore::getSourceFileName;
}

bool InternalCore::read(struct SHStringWithLen name, struct SHStringWithLen code, struct SHStringWithLen basePath,
                        const struct SHStringWithLen *includeDirs, uint32_t numIncludeDirs, struct SHLAst *out_ast) {
  return sh_current_interface.read(name, code, basePath, includeDirs, numIncludeDirs, out_ast);
}

bool InternalCore::loadAst(const uint8_t *bytes, uint32_t size, struct SHLAst *out_ast) {
  return sh_current_interface.loadAst(bytes, size, out_ast);
}

void InternalCore::freeError(struct SHLError *error) { sh_current_interface.freeError(error); }

struct SHLEvalEnv *InternalCore::createEvalEnv(struct SHStringWithLen namespace_) {
  return sh_current_interface.createEvalEnv(namespace_);
}

void InternalCore::freeEvalEnv(struct SHLEvalEnv *env) { sh_current_interface.freeEvalEnv(env); }

bool InternalCore::eval(struct SHLEvalEnv *env, const struct SHVar *ast, struct SHLError *error) {
  return sh_current_interface.eval(env, ast, error);
}

bool InternalCore::transformEnv(struct SHLEvalEnv *env, struct SHStringWithLen name, struct SHLWire *out_wire) {
  return sh_current_interface.transformEnv(env, name, out_wire);
}

bool InternalCore::transformEnvs(struct SHLEvalEnv **env, uint32_t len, struct SHStringWithLen name, struct SHLWire *out_wire) {
  return sh_current_interface.transformEnvs(env, len, name, out_wire);
}

bool InternalCore::evalAst(const struct SHVar *ast, struct SHStringWithLen name, struct SHLWire *out_wire) {
  return sh_current_interface.evalAst(ast, name, out_wire);
}

void InternalCore::freeWire(struct SHLWire *wire) { sh_current_interface.freeWire(wire); }

void InternalCore::freeAst(struct SHLAst *ast) { sh_current_interface.freeAst(ast); }

} // namespace shards

#endif /* EB4DB1E9_73CD_4620_931A_E9A2D5A4A6CD */
