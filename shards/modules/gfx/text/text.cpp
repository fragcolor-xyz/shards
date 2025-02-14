#include "types.hpp"
#include "../shards_types.hpp"
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/gfx/shards_types.hpp>
#include <shards/linalg_shim.hpp>
#include "mesh_buffer.hpp"

namespace gfx::text {
using namespace shards;

struct TextPlacement {
  static inline std::string_view quad_str = "quad";
  static inline std::string_view uv_str = "uv";
  static inline std::string_view texture_str = "texture";
  static inline std::string_view codepoint_str = "codepoint";
  static inline std::string_view coord_str = "coord";

  static inline shards::Types Types{
      {CoreInfo::Float4Type, CoreInfo::Float4Type, ShardsTypes::Texture, CoreInfo::IntType, CoreInfo::Int2Type}};
  static inline std::array<SHVar, 5> Keys{Var(quad_str), Var(uv_str), Var(texture_str), Var(codepoint_str), Var(coord_str)};
  static inline shards::Type Type = shards::Type::TableOf(Types, Keys);
  static inline shards::Type SeqType = shards::Type::SeqOf(Type);
};
struct TextPlacementRef {
  TextPlacementRef(TableVar &tv)
      : quad(tv.get<Vec4>(TextPlacement::quad_str)), uv(tv.get<Vec4>(TextPlacement::uv_str)),
        texture(tv.get<Var>(TextPlacement::texture_str)), codepoint(tv.get<Var>(TextPlacement::codepoint_str)),
        coord(tv.get<padded::Int2>(TextPlacement::coord_str)) {}

  Vec4 &quad;
  Vec4 &uv;
  Var &texture;
  Var &codepoint;
  padded::Int2 &coord;
};

// Font map creation shard
struct FontMapShard {
  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return SHFontMap::Type; }
  static SHOptionalString help() { return SHCCSTR("Creates a font map from font data"); }

  PARAM_PARAMVAR(_size, "Size", "Font size in pixels", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_size));

  SHFontMap *_fontMap{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    float fontSize = float((Var &)_size.get());
    const uint32_t pageSize = 128;

    // Create font map from input bytes
    _fontMap->fontMap = FontMap::load(input.payload.bytesValue, input.payload.bytesSize, pageSize, fontSize);

    return SHFontMap::ObjectVar.Get(_fontMap);
  }

  void warmup(SHContext *ctx) {
    PARAM_WARMUP(ctx);
    _fontMap = SHFontMap::ObjectVar.New();
  }

  void cleanup(SHContext *ctx) {
    PARAM_CLEANUP(ctx);
    clear();
  }

  void clear() {
    if (_fontMap) {
      SHFontMap::ObjectVar.Release(_fontMap);
      _fontMap = nullptr;
    }
  }
};

// Dynamic text mesh creation shard
struct DynamicMeshShard {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return SHDynamicMesh::Type; }
  static SHOptionalString help() { return SHCCSTR("Creates a dynamic text mesh"); }

  PARAM_IMPL();

  SHDynamicMesh *_dynMesh{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    // _dynMesh->buffer = std::make_shared<gfx::text::MeshBuffer>();

    // Clear the buffer every activation
    _dynMesh->buffer.begin();

    return SHDynamicMesh::ObjectVar.Get(_dynMesh);
  }

  void warmup(SHContext *ctx) {
    PARAM_WARMUP(ctx);
    _dynMesh = SHDynamicMesh::ObjectVar.New();
  }

  void cleanup(SHContext *ctx) {
    PARAM_CLEANUP(ctx);
    if (_dynMesh) {
      SHDynamicMesh::ObjectVar.Release(_dynMesh);
      _dynMesh = nullptr;
    }
  }
};

// Draw text to dynamic mesh shard
struct DynamicDrawTextShard {
  static inline Types InputTypes{{CoreInfo::StringType, TextPlacement::SeqType}};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh"); }

  TextPlacer placer;

  PARAM_PARAMVAR(_output, "Output", "Dynamic text mesh to draw to", {SHDynamicMesh::VarType});
  PARAM_PARAMVAR(_font, "Font", "Font to use", {CoreInfo::NoneType, SHFontMap::VarType});
  PARAM_PARAMVAR(_offset, "Offset", "Text position", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_scale, "Scale", "Text scale", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Text color", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_PARAMVAR(_up, "Up", "Up direction", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_right, "Right", "Right direction", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_center, "Center", "Center text", {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_output), PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_offset), PARAM_IMPL_FOR(_scale),
             PARAM_IMPL_FOR(_color), PARAM_IMPL_FOR(_up), PARAM_IMPL_FOR(_right), PARAM_IMPL_FOR(_center));

  DynamicDrawTextShard() {
    _color = toVar(float4(1.0f, 1.0f, 1.0f, 1.0f));
    _offset = toVar(float3(0.0f, 0.0f, 0.0f));
    _scale = Var(1.0f);
    _up = toVar(float3(0.0f, -1.0f, 0.0));
    _right = toVar(float3(1.0f, 0.0f, 0.0f));
    _center = Var(false);
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (data.inputType.basicType == SHType::String) {
      if (!_font.isVariable()) {
        throw SHException("Font is required when input is a string");
      }
    } else {
      OVERRIDE_ACTIVATE(data, activatePlacement);
      if (_font.isVariable()) {
        SPDLOG_WARN("Font is not used when input is a text placement table");
      }
    }

    return outputTypes().elements[0];
  }

  SHVar activatePlacement(SHContext *ctx, const SHVar &input) {
    SeqVar &placement = (SeqVar &)input;
    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(_output.get(), SHDynamicMesh::Type);

    placer.clear();
    for (auto &placement : placement) {
      auto placementRef = TextPlacementRef((TableVar &)placement);

      // Rebuild placement from input
      placer.textQuads.emplace_back(TextQuad{placementRef.quad, placementRef.uv,
                                             varAsObjectChecked<TexturePtr>(placementRef.texture, ShardsTypes::Texture),
                                             (uint32_t)placementRef.codepoint.payload.intValue});
    }

    placerToMesh(dynMesh, placer, true);

    return SHVar{};
  }

  void placerToMesh(SHDynamicMesh &dynMesh, const TextPlacer &placer, bool applyScale = false) {
    auto &offset = (Vec3 &)_offset.get();
    auto &up = (Vec3 &)_up.get();
    auto &right = (Vec3 &)_right.get();
    auto &color = (Vec4 &)_color.get();
    float scale = applyScale ? float((Var &)_scale.get()) : 1.0f;

    // Convert to mesh
    dynMesh.buffer.appendText(placer, MeshBuffer::TextParams{
                                          .offset = offset, // Use input position
                                          .right = right,   // Right direction
                                          .up = up,         // Up direction
                                          .color = color,   // White color
                                          .scale = scale,
                                          .center = _center.get().payload.boolValue,
                                      });
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(_output.get(), SHDynamicMesh::Type);
    auto &fontMap = varAsObjectChecked<SHFontMap>(_font.get(), SHFontMap::Type);

    // Scale, applied at placer level to be pixel-correct
    float scale = float((Var &)_scale.get());

    // Create temporary TextPlacer to generate quads
    placer.clear();
    placer.appendString(fontMap.fontMap, std::string_view(input.payload.stringValue), scale);

    // Convert to mesh
    placerToMesh(dynMesh, placer, false);

    return SHVar{};
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }
};

struct DynamicToMeshShard {
  static inline shards::Types OutTableTypes{{gfx::ShardsTypes::Mesh, gfx::ShardsTypes::Texture}};
  static inline std::array<SHVar, 2> OutTableKeys{Var("mesh"), Var("texture")};
  static inline Type OutTableType = Type::TableOf(OutTableTypes, OutTableKeys);
  static inline Type OutSeqType = Type::SeqOf(OutTableType);

  static SHTypesInfo inputTypes() { return SHDynamicMesh::Type; }
  static SHTypesInfo outputTypes() { return OutSeqType; }
  static SHOptionalString help() { return SHCCSTR("Converts a dynamic text mesh into a static mesh"); }

  PARAM_IMPL();

  gfx::MeshPtr *_mesh{};

  std::vector<MeshPtr *> _meshes;
  std::vector<TexturePtr *> _textures;
  SeqVar _resultSeq;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_mesh) {
      gfx::ShardsTypes::MeshObjectVar.Release(_mesh);
      _mesh = nullptr;
    }
    clearObjects();
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _mesh = gfx::ShardsTypes::MeshObjectVar.New();
    *_mesh = std::make_shared<gfx::Mesh>();
  }

  void clearObjects() {
    for (auto &mesh : _meshes) {
      gfx::ShardsTypes::MeshObjectVar.Release(mesh);
    }
    for (auto &texture : _textures) {
      gfx::ShardsTypes::TextureObjectVar.Release(texture);
    }
    _meshes.clear();
    _textures.clear();
  }
  SHVar activate(SHContext *context, const SHVar &input) {
    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(input, SHDynamicMesh::Type);

    // Get drawable with mesh from buffer
    auto meshTexturePairs = dynMesh.buffer.finalizeMeshes();

    clearObjects();

    // Convert to SHVar sequence of tables
    _resultSeq.clear();
    for (const auto &pair : meshTexturePairs) {
      auto &table = _resultSeq.emplace_back_table();
      auto &mesh = _meshes.emplace_back(gfx::ShardsTypes::MeshObjectVar.New());
      auto &texture = _textures.emplace_back(gfx::ShardsTypes::TextureObjectVar.New());
      *mesh = pair.mesh;
      *texture = pair.texture;
      table.insert("mesh", gfx::ShardsTypes::MeshObjectVar.Get(mesh));
      table.insert("texture", gfx::ShardsTypes::TextureObjectVar.Get(texture));
    }

    return _resultSeq;
  }
};

// Text placement shard
struct TextPlacementShard {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TextPlacement::SeqType; }
  static SHOptionalString help() { return SHCCSTR("Places text and returns the placement structure"); }

  TextPlacer _placer;
  SeqVar _resultSeq;
  std::vector<TexturePtr *> _textures;

  PARAM_PARAMVAR(_font, "Font", "Font to use", {SHFontMap::VarType});
  PARAM_PARAMVAR(_scale, "Scale", "Text scale", {CoreInfo::FloatType});
  PARAM_PARAMVAR(_valign, "VAlign", "Vertical alignment of baseline (0 = bottom, 1 = top)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_scale), PARAM_IMPL_FOR(_valign));

  TextPlacementShard() {
    _scale = Var(1.0f);
    _valign = Var(1.0f);
  }

  void clearTextures() {
    for (auto &texture : _textures) {
      gfx::ShardsTypes::TextureObjectVar.Release(texture);
    }
    _textures.clear();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    auto &fontMap = varAsObjectChecked<SHFontMap>(_font.get(), SHFontMap::Type);
    float scale = float((Var &)_scale.get());

    // Clear previous placements and textures
    _placer.clear();
    clearTextures();

    // Append the input string to the placer
    _placer.verticalAlignOrigin(fontMap.fontMap, float((Var &)_valign.get()));
    _placer.appendString(fontMap.fontMap, std::string_view(input.payload.stringValue), scale);

    // Create a sequence to return the placement
    _resultSeq.resize(_placer.textQuads.size());
    for (size_t idx = 0; idx < _placer.textQuads.size(); ++idx) {
      auto &quad = _placer.textQuads[idx];
      TextPlacementRef placement{_resultSeq.get<TableVar>(idx)};
      placement.quad = quad.quad;
      placement.uv = quad.uv;

      // Cache the texture
      auto &texture = _textures.emplace_back(gfx::ShardsTypes::TextureObjectVar.New());
      *texture = quad.texture;
      placement.texture = gfx::ShardsTypes::TextureObjectVar.Get(texture);
      placement.codepoint = Var(int64_t(quad.codepoint));
      placement.coord = linalg::vec<int64_t, 2>(quad.coord);
    }

    return _resultSeq;
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) {
    PARAM_CLEANUP(ctx);
    clearTextures();
  }
};

void registerTextShards() {
  REGISTER_SHARD("GFX.FontMap", FontMapShard);
  REGISTER_SHARD("GFX.DynMesh", DynamicMeshShard);
  REGISTER_SHARD("GFX.DynDrawText", DynamicDrawTextShard);
  REGISTER_SHARD("GFX.DynToMesh", DynamicToMeshShard);
  REGISTER_SHARD("GFX.TextPlacement", TextPlacementShard);
}

} // namespace gfx::text

SHARDS_REGISTER_FN(text) { gfx::text::registerTextShards(); }
