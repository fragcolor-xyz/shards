#ifndef B755A658_60B4_4264_915A_4F4571F9CBD0
#define B755A658_60B4_4264_915A_4F4571F9CBD0

#include "drawable.hpp"
#include "pipeline_builder.hpp"
#include "renderer_types.hpp"
#include <taskflow/taskflow.hpp>
#include <memory_resource>
#include <memory>

namespace gfx::detail {

struct AsyncGraphicsContext;

namespace pmr {
using namespace std::pmr;
}
// Any value returned by a processor
// retrieve the value from another step in the same processor by calling get<ExpectedType>()
// the type should be the same as the value that was set
struct ProcessorDynamicValue {
private:
  using Deleter = void (*)(void *);
  void *ptr{};
  Deleter deleter{};

public:
  ProcessorDynamicValue() = default;
  template <typename T> ProcessorDynamicValue(T *ptr) : ptr(ptr), deleter([](void *v) { std::destroy_at((T *)v); }) {}
  ProcessorDynamicValue(void *ptr, Deleter &&deleter) : ptr(ptr), deleter(deleter) {}
  ProcessorDynamicValue(ProcessorDynamicValue &&other) : ptr(other.ptr), deleter(other.deleter) {
    other.ptr = nullptr;
    other.deleter = nullptr;
  }
  ProcessorDynamicValue &operator=(ProcessorDynamicValue &&other) {
    ptr = other.ptr;
    deleter = other.deleter;
    other.ptr = nullptr;
    other.deleter = nullptr;
    return *this;
  }
  ProcessorDynamicValue(const ProcessorDynamicValue &) = default;
  ProcessorDynamicValue &operator=(const ProcessorDynamicValue &) = default;

  operator bool() const { return ptr; }

  void destroy() {
    if (ptr) {
      deleter(ptr);
      ptr = nullptr;
      deleter = nullptr;
    }
  }

  template <typename T> T &get() {
    assert(ptr);
    return *static_cast<T *>(ptr);
  }

  template <typename T> const T &get() const {
    assert(ptr);
    return *static_cast<const T *>(ptr);
  }

  template <typename T> static inline ProcessorDynamicValue make(T *ptr) { return ProcessorDynamicValue(ptr); }
};

// Context data when evaluating rendering commands
struct DrawablePrepareContext {
  using Allocator = pmr::polymorphic_allocator<>;

  Context &context;
  const CachedPipeline &cachedPipeline;

  // The flow that this request runs in
  tf::Subflow &subflow;

  // View data
  const ViewData &viewData;

  // The drawables
  const std::vector<const IDrawable *> &drawables;

  // Sorting mode for drawables, set on the pipeline step
  SortMode sortMode;
};

struct DrawableEncodeContext {
  WGPURenderPassEncoder encoder;
  const CachedPipeline &cachedPipeline;
  const ViewData &viewData;

  // Data that was returned by prepare
  ProcessorDynamicValue preparedData;
};

struct IDrawableProcessor : public IPipelineModifier {
  virtual ~IDrawableProcessor() = default;

  // Called at the start of a frame, before rendering
  virtual void reset(size_t frameCounter) = 0;

  // Hook for modifying the built pipeline for the referenceDrawable.
  // Whenver a group of objects is grouped under new pipeline, this function is called once before building.
  virtual void buildPipeline(PipelineBuilder &builder) override = 0;

  // Request to prepare draw data (buffers/bindings/etc.)
  // the returned value is passed to encode
  virtual ProcessorDynamicValue prepare(DrawablePrepareContext &context) = 0;

  // Called to encode the render commands
  virtual void encode(DrawableEncodeContext &context) = 0;
};

} // namespace gfx::detail

#endif /* B755A658_60B4_4264_915A_4F4571F9CBD0 */
