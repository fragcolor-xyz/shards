#ifndef F7DDD110_DF89_4AA4_A005_18C9BBFDFC40
#define F7DDD110_DF89_4AA4_A005_18C9BBFDFC40

#include <shards.h>
#include <gfx/shader/generator.hpp>

namespace gfx::shader {
struct ShaderCompositionContext {
  IGeneratorContext &generatorContext;

  ShaderCompositionContext(IGeneratorContext &generatorContext) : generatorContext(generatorContext) {}

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
} // namespace gfx::shader

#endif /* F7DDD110_DF89_4AA4_A005_18C9BBFDFC40 */
