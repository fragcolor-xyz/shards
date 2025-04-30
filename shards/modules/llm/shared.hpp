#include <shards/core/module.hpp>
#include <shards/core/runtime.hpp>
#include <shards/shards.h>
#include <shards/core/shared.hpp>
#include <shards/utility.hpp>
#include <shards/core/params.hpp>
#include <shards/log/log.hpp>

#include <llama.h>
#include <ggml.h>

namespace shards {
namespace llm {
struct ModelData {
  static inline int32_t ObjectId = 'llam';
  static inline const char VariableName[] = "LLM.Model";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);
  static inline shards::ObjectVar<ModelData> ObjectVar{VariableName, RawType.object.vendorId, RawType.object.typeId};

  static inline std::atomic_uint32_t usageCounter;

  ModelData() {
    SHLOG_DEBUG("ModelData constructor called");

    uint32_t expected = usageCounter.load(std::memory_order_acquire);
    uint32_t desired;
    do {
      desired = expected + 1;
    } while (!usageCounter.compare_exchange_weak(expected, desired, std::memory_order_release));

    if (desired == 1) {
      SHLOG_DEBUG("Initializing llama backend");
      llama_backend_init();
    }
  }

  ~ModelData() {
    SHLOG_DEBUG("ModelData destructor called");

    uint32_t prev = usageCounter.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      SHLOG_DEBUG("Freeing llama backend");
      llama_backend_free();
    }
  }

  std::shared_ptr<llama_model> model;
};
} // namespace llm
} // namespace shards
