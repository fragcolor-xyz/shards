#ifndef F7DDD110_DF89_4AA4_A005_18C9BBFDFC40
#define F7DDD110_DF89_4AA4_A005_18C9BBFDFC40

#include <optional>
#include <shards/shards.h>
#include <shards/ops.hpp>
#include <gfx/shader/generator.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/hash.inl>
#include "../shards_utils.hpp"

namespace gfx::shader {
using VariableMap = std::unordered_map<std::string, shards::OwnedVar>;
struct ShaderCompositionContext {
  IGeneratorContext &generatorContext;
  const VariableMap &composeWith;

  ShaderCompositionContext(IGeneratorContext &generatorContext, const VariableMap &composeWith)
      : generatorContext(generatorContext), composeWith(composeWith) {}

  std::optional<SHVar> getComposeTimeConstant(const std::string &key) {
    auto it = composeWith.find(key);
    if (it == composeWith.end())
      return std::nullopt;
    return it->second;
  }

  static ShaderCompositionContext &get();
  template <typename T> static auto withContext(ShaderCompositionContext &ctx, T &&cb) -> decltype(cb()) {
    using R = decltype(cb());

    setContext(&ctx);
    if constexpr (std::is_void_v<R>) {
      cb();
      setContext(nullptr);
    } else {
      auto result = cb();
      setContext(nullptr);
      return result;
    }
  }

private:
  static void setContext(ShaderCompositionContext *context);
};

void applyShaderEntryPoint(SHContext *context, shader::EntryPoint &entryPoint, const SHVar &input,
                           const VariableMap &composeWithVariables = VariableMap());

template <typename T>
void applyComposeWithHashed(SHContext *context, const SHVar &input, SHVar &hash, gfx::shader::VariableMap &composedWith,
                            T apply) {
  checkType(input.valueType, SHType::Table, "ComposeWith table");

  static thread_local shards::HashState<XXH128_hash_t> shHashState;
  XXH3_state_s hashState;
  XXH3_INITSTATE(&hashState);
  XXH3_128bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
  for (auto &[k, v] : input.payload.tableValue) {
    if (v.valueType == SHType::ContextVar) {
      shards::ParamVar pv(v);
      pv.warmup(context);
      shHashState.updateHash(pv.get(), &hashState);
    } else {
      uint8_t constData = 0xff;
      XXH3_128bits_update(&hashState, &constData, 1);
    }
  }
  auto digest = XXH3_128bits_digest(&hashState);
  SHVar newHash = shards::Var(int64_t(digest.low64), int64_t(digest.high64));

  if (newHash != hash) {
    hash = newHash;
    composedWith.clear();
    for (auto &[k, v] : input.payload.tableValue) {
      if (k.valueType != SHType::String)
        throw formatException("ComposeWith key must be a string");
      std::string keyStr(SHSTRVIEW(k));
      shards::ParamVar pv(v);
      pv.warmup(context);
      auto &var = composedWith.emplace(std::move(keyStr), pv.get()).first->second;
      if (var.valueType == SHType::None) {
        throw formatException("Required variable {} not found", k);
      }
    }

    apply();
  }
}
} // namespace gfx::shader

#endif /* F7DDD110_DF89_4AA4_A005_18C9BBFDFC40 */
