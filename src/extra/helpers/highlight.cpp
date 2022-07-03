#include "helpers.hpp"

namespace shards {
namespace Helpers {

using namespace gfx;

struct HighlightShard : public BaseConsumer {
  // TODO: Merge with DrawShard type
  static inline shards::Types SingleDrawableTypes = shards::Types{gfx::Types::Drawable, gfx::Types::DrawableHierarchy};
  static inline Type DrawableSeqType = Type::SeqOf(SingleDrawableTypes);
  static inline shards::Types DrawableTypes{gfx::Types::Drawable, gfx::Types::DrawableHierarchy, DrawableSeqType};

  static SHTypesInfo inputTypes() { return DrawableTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Queues a draw operation to highlight a drawable"); }

  SHTypeInfo compose(const SHInstanceData &data) {
    if (data.inputType.basicType == SHType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return CoreInfo::NoneType;
  }

  SHVar activateSingle(SHContext *shContext, const SHVar &input) {
    Context &helperContext = getContext();

    SHTypeInfo inputType{.basicType = SHType::Object,
                         .object = {.vendorId = input.payload.objectVendorId, .typeId = input.payload.objectTypeId}};
    if (gfx::Types::Drawable == inputType) {
      SHDrawable *dPtr = static_cast<SHDrawable *>(input.payload.objectValue);
      helperContext.wireframeRenderer.overlayWireframe(*helperContext.queue.get(), dPtr->drawable);
    } else if (gfx::Types::DrawableHierarchy == inputType) {
      SHDrawableHierarchy *dhPtr = static_cast<SHDrawableHierarchy *>(input.payload.objectValue);
      helperContext.wireframeRenderer.overlayWireframe(*helperContext.queue.get(), dhPtr->drawableHierarchy);
    }

    return SHVar{};
  }

  SHVar activateSeq(SHContext *shContext, const SHVar &input) {
    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      (void)activateSingle(shContext, seq.elements[i]);
    }

    return SHVar{};
  }

  SHVar activate(SHContext *shContext, const SHVar &input) { throw ActivationError("Unsupported input type"); }
  void warmup(SHContext *shContext) { baseConsumerWarmup(shContext); }
  void cleanup() { baseConsumerCleanup(); }
};

void registerHighlightShards() { REGISTER_SHARD("Helpers.Highlight", HighlightShard); }
} // namespace Helpers
} // namespace shards