#ifndef F07FE4E2_30DA_4A94_B9E7_56D5B42CD733
#define F07FE4E2_30DA_4A94_B9E7_56D5B42CD733

#include <shards/common_types.hpp>
#include <shards/utility.hpp>
#include <shards/core/foundation.hpp>

namespace shards::Animations {

constexpr uint32_t VendorId = shards::CoreCC;

enum class Interpolation { Linear, Step, Cubic };

namespace detail {
using namespace shards;
// NOTE: This needs to be a struct ensure correct initialization order under clang
struct Container {
  DECL_ENUM_INFO(Interpolation, Interpolation, 'i11n');

  static inline Types PathComponentTypes{CoreInfo::StringType};
  static inline Type Path = Type::SeqOf(PathComponentTypes);

  static inline shards::Types ValueTableTypes{Path, CoreInfo::AnyType};
  static inline std::array<SHVar, 2> ValueTableKeys{Var("Path"), Var("Value")};
  static inline Type ValueTable = Type::TableOf(ValueTableTypes, ValueTableKeys);
  static inline Type AnimationValues = Type::SeqOf(ValueTable);

  static inline shards::Types KeyframeTableTypes{CoreInfo::FloatType, CoreInfo::AnyType, CoreInfo::AnyType};
  static inline std::array<SHVar, 3> KeyframeTableKeys{Var("Time"), Var("Value"), Var()};
  static inline Type KeyframeTable = Type::TableOf(KeyframeTableTypes, KeyframeTableKeys);

  static inline shards::Types TrackTableTypes{Path, Type::SeqOf(KeyframeTable)};
  static inline std::array<SHVar, 2> TrackTableKeys{Var("Path"), Var("Frames")};
  static inline Type TrackTable = Type::TableOf(TrackTableTypes, TrackTableKeys);

  static inline Type Animation = Type::SeqOf(TrackTable);
  static inline Types AnimationOrAnimationVar{Type::VariableOf(Animation), Animation};
};
} // namespace detail

using ShardsTypes = detail::Container;

}; // namespace shards::Animations

#endif /* F07FE4E2_30DA_4A94_B9E7_56D5B42CD733 */
