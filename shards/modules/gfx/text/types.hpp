#ifndef SHARDS_GFX_TEXT_TYPES_HPP
#define SHARDS_GFX_TEXT_TYPES_HPP

#include "font.hpp"
#include "mesh_buffer.hpp"
#include <gfx/mesh.hpp>
#include <shards/core/foundation.hpp>
#include <gfx/linalg.hpp>

namespace gfx::text {
using namespace linalg::aliases;

// Font map wrapper object
struct SHFontMap {
  static inline int32_t ObjectId = 'FONT';
  static inline const char VariableName[] = "Text.FontMap";
  static inline ::shards::Type Type = ::shards::Type::Object(shards::CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);

  static inline shards::ObjectVar<SHFontMap, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                        RawType.object.typeId};

  FontMap::Ptr fontMap;
};

// Dynamic text mesh wrapper object
struct SHDynamicMesh {
  static inline int32_t ObjectId = 'DTXT';
  static inline const char VariableName[] = "Text.DynamicMesh";
  static inline ::shards::Type Type = ::shards::Type::Object(shards::CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);

  static inline shards::ObjectVar<SHDynamicMesh, nullptr, nullptr, nullptr, true> ObjectVar{VariableName, RawType.object.vendorId,
                                                                                            RawType.object.typeId};

  gfx::text::MeshBuffer buffer;
};

} // namespace shards::text

#endif