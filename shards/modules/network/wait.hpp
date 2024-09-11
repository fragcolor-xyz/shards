#ifndef A2649780_0878_4E30_ABB2_7016E495BD04
#define A2649780_0878_4E30_ABB2_7016E495BD04

#include <shards/core/foundation.hpp>
#include <shards/core/object_type.hpp>
#include <condition_variable>
#include <mutex>

namespace shards {
namespace Network {

struct WaitSignal {
  static inline int32_t ObjectId = 'Ncnd';
  static inline const char VariableName[] = "Network.WaitSignal";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);
  static inline shards::ObjectVar<WaitSignal, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                         RawType.object.typeId};

  std::string _debugName = "hello";
  std::condition_variable _cond;
  std::mutex _mtx;
  std::string _guards = "guard";

  ~WaitSignal() {
    SPDLOG_INFO(">??");
  }
};
} // namespace Network
} // namespace shards

#endif /* A2649780_0878_4E30_ABB2_7016E495BD04 */
