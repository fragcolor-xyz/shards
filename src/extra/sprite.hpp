#ifndef CB_SPRITE_HPP
#define CB_SPRITE_HPP

#include "./bgfx.hpp"
#include "blocks/shared.hpp"

namespace chainblocks {
namespace Sprite {

constexpr uint32_t SheetCC = 'shee';

struct Enums {
  enum class HAlign { Left = 0, Center, Right };
  
  static constexpr uint32_t HAlignCC = 'hali';
  static inline EnumInfo<HAlign> HAlignEnumInfo{"HAlign", CoreCC, HAlignCC};
  static inline Type HAlignType = Type::Enum(CoreCC, HAlignCC);

  enum class VAlign { Top = 0, Center, Bottom };

  static constexpr uint32_t VAlignCC = 'vali';
  static inline EnumInfo<VAlign> VAlignEnumInfo{"VAlign", CoreCC, VAlignCC};
  static inline Type VAlignType = Type::Enum(CoreCC, VAlignCC);
};

} // namespace Sprite
} // namespace chainblocks

#endif
