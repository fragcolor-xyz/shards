
#include "fwd.hpp"
#include "gfx/pipeline_step.hpp"
#include <functional>

namespace gfx::detail {
struct CachedDrawable;
struct CachedView;
} // namespace gfx::detail

namespace gfx {
struct IParameterCollector;
struct IFeatureGenerateContext;
typedef std::function<void(IFeatureGenerateContext &)> FeatureGenerator;

struct FeatureCallbackContext {
  Context &context;
  const ViewPtr view;
  const IDrawable *drawable = nullptr;
  const detail::CachedDrawable *cachedDrawable = nullptr;
  const detail::CachedView *cachedView = nullptr;
};

// TODO: merge into generic generators
typedef std::function<bool(const FeatureCallbackContext &)> FeatureFilterCallback;
typedef std::function<void(const FeatureCallbackContext &, IParameterCollector &)> FeatureParameterGenerator;
} // namespace gfx
