#ifndef F07FE4E2_30DA_4A94_B9E7_56D5B42CD733
#define F07FE4E2_30DA_4A94_B9E7_56D5B42CD733

#include <common_types.hpp>
#include <foundation.hpp>
#include <utility.hpp>

namespace shards::Animations {

constexpr uint32_t VendorId = shards::CoreCC;

enum class Interpolation { Step, Linear };

// This represents opaque animation data that does not have/require a representation inside shards
struct IAnimationData {
  virtual ~IAnimationData() = default;
};

namespace detail {
#define OBJECT(_id, _displayName, _definedAs, _type)                                                                            \
  static constexpr uint32_t SH_CONCAT(_definedAs, TypeId) = uint32_t(_id);                                                      \
  static inline Type _definedAs{{SHType::Object, {.object = {.vendorId = VendorId, .typeId = SH_CONCAT(_definedAs, TypeId)}}}}; \
  static inline ObjectVar<_type> SH_CONCAT(_definedAs, ObjectVar){_displayName, VendorId, SH_CONCAT(_definedAs, TypeId)};

using namespace shards;
// NOTE: This needs to be a struct ensure correct initialization order under clang
struct Container {
  OBJECT('anmf', "IAnimationData", AnimationData, IAnimationData);

  DECL_ENUM_INFO(Interpolation, Interpolation, 'i11n');

  static inline Types PathComponentTypes{CoreInfo::StringType};
  static inline Type Path = Type::SeqOf(PathComponentTypes);

  static inline shards::Types ValueTableTypes{Path, CoreInfo::AnyType};
  static inline std::array<SHString, 2> ValueTableKeys{"Path", "Value"};
  static inline Type ValueTable = Type::TableOf(ValueTableTypes, ValueTableKeys);
  static inline Type AnimationValues = Type::SeqOf(ValueTable);

  static inline shards::Types KeyframeTableTypes{CoreInfo::FloatType, CoreInfo::AnyType};
  static inline std::array<SHString, 2> KeyframeTableKeys{"Time", "Value"};
  static inline Type KeyframeTable = Type::TableOf(KeyframeTableTypes, KeyframeTableKeys);

  static inline shards::Types TrackTableTypes{Path, Type::SeqOf(KeyframeTable)};
  static inline std::array<SHString, 2> TrackTableKeys{"Path", "Frames"};
  static inline Type TrackTable = Type::TableOf(TrackTableTypes, TrackTableKeys);

  static inline Type Animation = Type::SeqOf(TrackTable);
  static inline Types AnimationOrAnimationVar{Type::VariableOf(Animation), Animation};
};
#undef OBJECT
} // namespace detail

using Types = detail::Container;

}; // namespace shards::Animations

#endif /* F07FE4E2_30DA_4A94_B9E7_56D5B42CD733 */
