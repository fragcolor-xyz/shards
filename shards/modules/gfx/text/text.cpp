#include "core/foundation.hpp"
#include "types.hpp"
#include "../shards_types.hpp"
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/gfx/shards_types.hpp>
#include <shards/linalg_shim.hpp>
#include <gfx/screen_size.hpp>
#include "mesh_buffer.hpp"

namespace gfx::text {
using namespace shards;

struct TextPlacement {
  static inline std::string_view quad_str = "quad";
  static inline std::string_view uv_str = "uv";
  static inline std::string_view texture_str = "texture";
  static inline std::string_view codepoint_str = "codepoint";
  static inline std::string_view coord_str = "coord";

  static inline std::array<SHVar, 5> Keys{Var(quad_str), Var(uv_str), Var(texture_str), Var(codepoint_str), Var(coord_str)};
  static inline shards::Types Types{
      {CoreInfo::Float4Type, CoreInfo::Float4Type, ShardsTypes::Texture, CoreInfo::IntType, CoreInfo::Int2Type}};
  static inline shards::Type Type = shards::Type::TableOf(Types, Keys);
  static inline shards::Type SeqType = shards::Type::SeqOf(Type);
};
struct TextPlacementRef {
  TextPlacementRef(TableVar &tv)
      : quad(tv.get<Vec4>(TextPlacement::quad_str)), uv(tv.get<Vec4>(TextPlacement::uv_str)),
        texture(tv.get<OwnedVar>(TextPlacement::texture_str)), codepoint(tv.get<Var>(TextPlacement::codepoint_str)),
        coord(tv.get<padded::Int2>(TextPlacement::coord_str)) {}

  Vec4 &quad;
  Vec4 &uv;
  OwnedVar &texture;
  Var &codepoint;
  padded::Int2 &coord;
};

// Font map creation shard
struct FontMapShard {
  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return SHFontMap::Type; }
  static SHOptionalString help() { return SHCCSTR("Creates a font map from font data."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The font data as a byte array."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The created font map object."); }

  PARAM_IMPL();

  SHFontMap *_fontMap{};

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    const uint32_t pageSize = 128;

    // Create font map from input bytes
    _fontMap->fontMap = FontMap::load(input.payload.bytesValue, input.payload.bytesSize, pageSize);

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
  static SHOptionalString help() { return SHCCSTR("Creates a dynamic text mesh."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The created dynamic text mesh object."); }

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

struct DynamicDrawTextShardBase {
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh."); }
  static SHOptionalString outputHelp() { return SHCCSTR("No output, modifies the dynamic mesh in place."); }

  TextPlacer placer;

  PARAM_PARAMVAR(_output, "Output", "Dynamic text mesh to draw to", {SHDynamicMesh::VarType});
  PARAM_PARAMVAR(_offset, "Offset", "Text position", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_scale, "Scale", "Text scale", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_color, "Color", "Text color", {CoreInfo::Float4Type, CoreInfo::Float4VarType});
  PARAM_PARAMVAR(_up, "Up", "Up direction", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_right, "Right", "Right direction", {CoreInfo::Float3Type, CoreInfo::Float3VarType});
  PARAM_PARAMVAR(_halign, "HAlign", "Horizontal alignment (0 = left, 0.5 = centered, 1 = right)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_output), PARAM_IMPL_FOR(_offset), PARAM_IMPL_FOR(_scale), PARAM_IMPL_FOR(_color),
             PARAM_IMPL_FOR(_up), PARAM_IMPL_FOR(_right), PARAM_IMPL_FOR(_halign));

  DynamicDrawTextShardBase() {
    _color = toVar(float4(1.0f, 1.0f, 1.0f, 1.0f));
    _offset = toVar(float3(0.0f, 0.0f, 0.0f));
    _scale = Var(1.0f);
    _up = toVar(float3(0.0f, -1.0f, 0.0));
    _right = toVar(float3(1.0f, 0.0f, 0.0f));
    _halign = Var(0.0f);
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
                                          .alignment = float2(_halign.get().payload.floatValue, 0.0f),
                                      });
  }
};

// Draw text to dynamic mesh shard
struct DynamicDrawTextStringShard : public DynamicDrawTextShardBase {
  static inline Types InputTypes{{CoreInfo::StringType}};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The text to draw."); }
  static SHOptionalString outputHelp() { return SHCCSTR("No output, modifies the dynamic mesh in place."); }

  TextPlacer placer;

  PARAM_PARAMVAR(_font, "Font", "Font to use", {SHFontMap::VarType});
  PARAM_PARAMVAR(_fontSize, "FontSize", "The font size to use", {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_PARAMVAR(_valign, "VAlign", "Vertical alignment of baseline (0 = bottom, 1 = top, -0.5 = centered on baseline)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL_DERIVED(DynamicDrawTextShardBase, PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_fontSize), PARAM_IMPL_FOR(_valign));

  DynamicDrawTextStringShard() { _valign = Var(-0.5f); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }

  SHVar activate(SHContext *ctx, const SHVar &input) {

    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(_output.get(), SHDynamicMesh::Type);
    auto &fontMap = varAsObjectChecked<SHFontMap>(_font.get(), SHFontMap::Type);

    // Scale, applied at placer level to be pixel-correct
    float scale = float((Var &)_scale.get());

    // Create temporary TextPlacer to generate quads
    auto &fontSize = fontMap.fontMap->getFontSize(_fontSize.get().payload.intValue);
    placer.clear();
    placer.verticalAlignOrigin(fontSize, float((Var &)_valign.get()));
    placer.appendString(fontSize, std::string_view(input.payload.stringValue), scale);

    // Convert to mesh
    placerToMesh(dynMesh, placer, false);

    return SHVar{};
  }
};

struct DynamicDrawTextStringWorldSpaceShard : public DynamicDrawTextShardBase {
  static inline Types InputTypes{{CoreInfo::StringType}};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The text to draw."); }
  static SHOptionalString outputHelp() { return SHCCSTR("No output, modifies the dynamic mesh in place."); }

  TextPlacer placer;

  PARAM_PARAMVAR(_font, "Font", "Font to use", {SHFontMap::VarType});
  PARAM_PARAMVAR(_worldSize, "WorldSize", "Desired world space height of the font",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_view, "View", "View matrix", {Type::VariableOf(gfx::ShardsTypes::View)});
  PARAM_PARAMVAR(_viewSize, "ViewSize", "View size", {CoreInfo::Int2Type, CoreInfo::Int2VarType});
  PARAM_PARAMVAR(_valign, "VAlign", "Vertical alignment of baseline (0 = bottom, 1 = top, -0.5 = centered on baseline)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL_DERIVED(DynamicDrawTextShardBase, PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_worldSize), PARAM_IMPL_FOR(_view),
                     PARAM_IMPL_FOR(_viewSize), PARAM_IMPL_FOR(_valign));

  DynamicDrawTextStringWorldSpaceShard() { _valign = Var(-0.5f); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }

  SHVar activate(SHContext *ctx, const SHVar &input) {

    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(_output.get(), SHDynamicMesh::Type);
    auto &fontMap = varAsObjectChecked<SHFontMap>(_font.get(), SHFontMap::Type);
    auto &view = varAsObjectChecked<SHView>(_view.get(), gfx::ShardsTypes::View);

    // Scale, applied at placer level to be pixel-correct
    float scale = float((Var &)_scale.get());

    float worldSize = float((Var &)_worldSize.get());
    auto &up = (Vec3 &)_up.get();

    // Compute scale based on character position
    auto &offset = (Vec3 &)_offset.get();
    auto &viewSize = (padded::Int2 &)_viewSize.get();

    gfx::ScreenSizeHelper helper{*view.view.get(), float2(*viewSize), scale};
    // float autoScale = helper.getConstantScreenSize(offset, 1.0f);

    float pixelSpan = helper.getLineSegmentPixelSpan(offset, *up * worldSize);

    // Quantize font size to configurable number of levels with more detail at lower resolutions
    constexpr int maxSize = 1024;
    constexpr int numLevels = 128;
    constexpr float invNumLevels = 1.0f / numLevels;
    static float logRange = std::log2(float(maxSize));
    float logScale = std::log2(pixelSpan) / logRange;                                                  // Map range to 0-1
    float quantized = std::floor(logScale * numLevels) * invNumLevels;                                 // Quantize to levels
    int32_t fontSize = std::clamp(int32_t(std::pow(2.0f, quantized * logRange)), 4, maxSize); // Map back to range

    // Scale to adjust the font to fit in the desired world size, and combined with user size
    float adjustedScale = 1.0f / float(fontSize) * worldSize * scale;

    // Create temporary TextPlacer to generate quads
    auto &fontSizeObj = fontMap.fontMap->getFontSize(fontSize);
    placer.clear();
    placer.verticalAlignOrigin(fontSizeObj, float((Var &)_valign.get()));
    placer.appendString(fontSizeObj, std::string_view(input.payload.stringValue), adjustedScale);

    // Convert to mesh
    placerToMesh(dynMesh, placer, false);

    return SHVar{};
  }
};

// Draw text to dynamic mesh shard
struct DynamicDrawTextPlacementShard : public DynamicDrawTextShardBase {
  static inline Types InputTypes{{TextPlacement::SeqType}};

  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh."); }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of text placements."); }
  static SHOptionalString outputHelp() { return SHCCSTR("No output, modifies the dynamic mesh in place."); }

  PARAM_IMPL_DERIVED(DynamicDrawTextShardBase);

  DynamicDrawTextPlacementShard() {}

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
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
                                          .alignment = float2(_halign.get().payload.floatValue, 0.0f),
                                      });
  }

  SHVar activate(SHContext *ctx, const SHVar &input) {
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

  PARAM_PARAMVAR(
      _windingOrderParam, "WindingOrder",
      "Determines which side of the triangle is considered the front face. Typically use CW for UI space (y=down), CCW for "
      "world space (y=up)",
      {ShardsTypes::WindingOrderEnumInfo::Type, Type::VariableOf(ShardsTypes::WindingOrderEnumInfo::Type)});
  PARAM_IMPL(PARAM_IMPL_FOR(_windingOrderParam));

  SeqVar _resultSeq;
  std::vector<MeshTexturePair> _meshTexturePairs;

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  DynamicToMeshShard() {
    _windingOrderParam =
        Var::Enum(WindingOrder::CW, ShardsTypes::WindingOrderEnumInfo::VendorId, ShardsTypes::WindingOrderEnumInfo::TypeId);
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  static inline std::string_view mesh_str = "mesh";
  static inline std::string_view texture_str = "texture";

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(input, SHDynamicMesh::Type);

    auto windingOrder = (WindingOrder)_windingOrderParam.get().payload.intValue;

    // Get drawable with mesh from buffer
    _meshTexturePairs.clear();
    dynMesh.buffer.finalizeMeshes(_meshTexturePairs, windingOrder);

    // Convert to SHVar sequence of tables
    _resultSeq.clear();
    for (const auto &pair : _meshTexturePairs) {
      auto &table = _resultSeq.emplace_back_table();
      auto [mesh, meshVar] = gfx::ShardsTypes::MeshObjectVar.NewOwnedVar();
      auto [texture, textureVar] = gfx::ShardsTypes::TextureObjectVar.NewOwnedVar();
      mesh = pair.mesh;
      texture = pair.texture;
      table.get<OwnedVar>(mesh_str) = std::move(meshVar);
      table.get<OwnedVar>(texture_str) = std::move(textureVar);
    }

    return _resultSeq;
  }
};

// Text placement shard
struct TextPlacementShard {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return TextPlacement::SeqType; }
  static SHOptionalString help() { return SHCCSTR("Places text and returns the placement structure."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The text to place."); }
  static SHOptionalString outputHelp() { return SHCCSTR("A sequence of text placement structures."); }

  TextPlacer _placer;
  SeqVar _resultSeq;
  // std::vector<TexturePtr *> _textures;

  PARAM_PARAMVAR(_font, "Font", "Font to use", {SHFontMap::VarType});
  PARAM_PARAMVAR(_fontSize, "FontSize", "The font size to use", {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_PARAMVAR(_scale, "Scale", "Text scale", {CoreInfo::FloatType});
  PARAM_PARAMVAR(_valign, "VAlign", "Vertical alignment of baseline (0 = bottom, 1 = top, -0.5 = centered on baseline)",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_fontSize), PARAM_IMPL_FOR(_scale), PARAM_IMPL_FOR(_valign));

  TextPlacementShard() {
    _scale = Var(1.0f);
    _valign = Var(1.0f);
    _fontSize = Var(12);
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

    // Append the input string to the placer
    auto &fontSize = fontMap.fontMap->getFontSize(_fontSize.get().payload.intValue);
    _placer.verticalAlignOrigin(fontSize, float((Var &)_valign.get()));
    _placer.appendString(fontSize, std::string_view(input.payload.stringValue), scale);

    // Create a sequence to return the placement
    _resultSeq.resize(_placer.textQuads.size());
    for (size_t idx = 0; idx < _placer.textQuads.size(); ++idx) {
      auto &quad = _placer.textQuads[idx];
      TextPlacementRef placement{_resultSeq.get<TableVar>(idx)};
      placement.quad = quad.quad;
      placement.uv = quad.uv;

      auto [tex, texVar] = gfx::ShardsTypes::TextureObjectVar.NewOwnedVar();
      tex = quad.texture;
      placement.texture = std::move(texVar);
      placement.codepoint = Var(int64_t(quad.codepoint));
      placement.coord = linalg::vec<int64_t, 2>(quad.coord);
    }

    return _resultSeq;
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }
};
struct FontSpaceSizeShard {
  static SHTypesInfo inputTypes() { return SHFontMap::Type; }
  static SHTypesInfo outputTypes() { return CoreInfo::Int2Type; }
  static SHOptionalString help() { return SHCCSTR("Retrieves the monospace character spacing size from a FontMap."); }
  static SHOptionalString inputHelp() { return SHCCSTR("The FontMap object."); }
  static SHOptionalString outputHelp() { return SHCCSTR("The monospace character size as a float2."); }

  PARAM_PARAMVAR(_fontSize, "Font Size", "The font size to use", {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_fontSize))

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *ctx) { PARAM_WARMUP(ctx); }
  void cleanup(SHContext *ctx) { PARAM_CLEANUP(ctx); }

  SHVar activate(SHContext *ctx, const SHVar &input) {
    auto &fontMap = varAsObjectChecked<SHFontMap>(input, SHFontMap::Type);
    auto &fontSize = fontMap.fontMap->getFontSize(_fontSize.get().payload.intValue);
    return toVar(fontSize.spaceSize);
  }
};

void registerTextShards() {
  REGISTER_SHARD("GFX.FontMap", FontMapShard);
  REGISTER_SHARD("GFX.DynMesh", DynamicMeshShard);
  REGISTER_SHARD("GFX.DynDrawText", DynamicDrawTextStringShard);
  REGISTER_SHARD("GFX.DynDrawTextWorldSpace", DynamicDrawTextStringWorldSpaceShard);
  REGISTER_SHARD("GFX.DynDrawTextPlacement", DynamicDrawTextPlacementShard);
  REGISTER_SHARD("GFX.DynToMesh", DynamicToMeshShard);
  REGISTER_SHARD("GFX.TextPlacement", TextPlacementShard);
  REGISTER_SHARD("GFX.FontSpaceSize", FontSpaceSizeShard);
}

} // namespace gfx::text

SHARDS_REGISTER_FN(text) { gfx::text::registerTextShards(); }
