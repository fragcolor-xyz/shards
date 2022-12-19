#include "context.hpp"

namespace shards {
namespace Gizmos {

using namespace gfx;

struct HighlightShard : public BaseConsumer {
  // TODO: Merge with DrawShard type
  static inline shards::Types SingleDrawableTypes = shards::Types{gfx::Types::Drawable, gfx::Types::TreeDrawable};
  static inline Type DrawableSeqType = Type::SeqOf(SingleDrawableTypes);
  static inline shards::Types DrawableTypes{gfx::Types::Drawable, gfx::Types::TreeDrawable, DrawableSeqType};

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

    SHTypeInfo inputType = shards::Type::Object(input.payload.objectVendorId, input.payload.objectTypeId);
    if (gfx::Types::Drawable == inputType) {
      SHDrawable *dPtr = static_cast<SHDrawable *>(input.payload.objectValue);
      helperContext.wireframeRenderer.overlayWireframe(*helperContext.queue.get(), dPtr->drawable);
    } else if (gfx::Types::TreeDrawable == inputType) {
      SHTreeDrawable *dhPtr = static_cast<SHTreeDrawable *>(input.payload.objectValue);
      helperContext.wireframeRenderer.overlayWireframe(*helperContext.queue.get(), *dhPtr->drawable.get());
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

void registerHighlightShards() { REGISTER_SHARD("Gizmos.Highlight", HighlightShard); }
} // namespace Gizmos
} // namespace shards
