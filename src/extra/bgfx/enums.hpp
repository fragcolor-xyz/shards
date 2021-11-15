#pragma once

#include "bgfx/bgfx.h"
#include "blocks/shared.hpp"
using namespace chainblocks;

namespace BGFX {

struct Enums {
  enum class CullMode { None, Front, Back };
  static constexpr uint32_t CullModeCC = 'gfxC';
  static inline EnumInfo<CullMode> CullModeEnumInfo{"CullMode", CoreCC,
                                                    CullModeCC};
  static inline Type CullModeType = Type::Enum(CoreCC, CullModeCC);

  enum class Blend {
    Zero,
    One,
    SrcColor,
    InvSrcColor,
    SrcAlpha,
    InvSrcAlpha,
    DstAlpha,
    InvDstAlpha,
    DstColor,
    InvDstColor,
    SrcAlphaSat,
    Factor,
    InvFactor
  };
  static constexpr uint32_t BlendCC = 'gfxB';
  static inline EnumInfo<Blend> BlendEnumInfo{"Blend", CoreCC, BlendCC};
  static inline Type BlendType = Type::Enum(CoreCC, BlendCC);

  static constexpr uint64_t BlendToBgfx(CBEnum eval) {
    switch (Blend(eval)) {
    case Blend::Zero:
      return BGFX_STATE_BLEND_ZERO;
    case Blend::One:
      return BGFX_STATE_BLEND_ONE;
    case Blend::SrcColor:
      return BGFX_STATE_BLEND_SRC_COLOR;
    case Blend::InvSrcColor:
      return BGFX_STATE_BLEND_INV_SRC_COLOR;
    case Blend::SrcAlpha:
      return BGFX_STATE_BLEND_SRC_ALPHA;
    case Blend::InvSrcAlpha:
      return BGFX_STATE_BLEND_INV_SRC_ALPHA;
    case Blend::DstAlpha:
      return BGFX_STATE_BLEND_DST_ALPHA;
    case Blend::InvDstAlpha:
      return BGFX_STATE_BLEND_INV_DST_ALPHA;
    case Blend::DstColor:
      return BGFX_STATE_BLEND_DST_COLOR;
    case Blend::InvDstColor:
      return BGFX_STATE_BLEND_INV_DST_COLOR;
    case Blend::SrcAlphaSat:
      return BGFX_STATE_BLEND_SRC_ALPHA_SAT;
    case Blend::Factor:
      return BGFX_STATE_BLEND_FACTOR;
    case Blend::InvFactor:
      return BGFX_STATE_BLEND_INV_FACTOR;
    }
    return 0;
  }

  enum class BlendOp { Add, Sub, RevSub, Min, Max };
  static constexpr uint32_t BlendOpCC = 'gfxO';
  static inline EnumInfo<BlendOp> BlendOpEnumInfo{"BlendOp", CoreCC, BlendOpCC};
  static inline Type BlendOpType = Type::Enum(CoreCC, BlendOpCC);

  static constexpr uint64_t BlendOpToBgfx(CBEnum eval) {
    switch (BlendOp(eval)) {
    case BlendOp::Add:
      return BGFX_STATE_BLEND_EQUATION_ADD;
    case BlendOp::Sub:
      return BGFX_STATE_BLEND_EQUATION_SUB;
    case BlendOp::RevSub:
      return BGFX_STATE_BLEND_EQUATION_REVSUB;
    case BlendOp::Min:
      return BGFX_STATE_BLEND_EQUATION_MIN;
    case BlendOp::Max:
      return BGFX_STATE_BLEND_EQUATION_MAX;
    }
    return 0;
  }

  static inline std::array<CBString, 3> _blendKeys{"Src", "Dst", "Op"};
  static inline Types _blendTypes{{BlendType, BlendType, BlendOpType}};
  static inline Type BlendTable = Type::TableOf(_blendTypes, _blendKeys);
  static inline Type BlendTableSeq = Type::SeqOf(BlendTable, 2);
};
} // namespace BGFX
