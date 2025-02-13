#include "types.hpp"
#include "../shards_types.hpp"
#include <shards/core/params.hpp>
#include <shards/core/module.hpp>
#include <shards/modules/gfx/shards_types.hpp>
#include <shards/linalg_shim.hpp>
#include "mesh_buffer.hpp"

namespace gfx::text {
using namespace shards;

// Font map creation shard
struct FontMapShard {
  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHTypesInfo outputTypes() { return SHFontMap::Type; }
  static SHOptionalString help() { return SHCCSTR("Creates a font map from font data"); }

  PARAM_PARAMVAR(_size, "Size", "Font size in pixels", {CoreInfo::FloatType});
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
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString help() { return SHCCSTR("Draws text to a dynamic mesh"); }

  PARAM_PARAMVAR(_output, "Output", "Dynamic text mesh to draw to", {SHDynamicMesh::VarType});
  PARAM_PARAMVAR(_font, "Font", "Font to use", {SHFontMap::VarType});
  PARAM_PARAMVAR(_position, "Position", "Text position", {CoreInfo::Float3Type});
  PARAM_PARAMVAR(_scale, "Scale", "Text scale", {CoreInfo::FloatType});
  PARAM_IMPL(PARAM_IMPL_FOR(_output), PARAM_IMPL_FOR(_font), PARAM_IMPL_FOR(_position), PARAM_IMPL_FOR(_scale));

  SHVar activate(SHContext *ctx, const SHVar &input) {
    auto &dynMesh = varAsObjectChecked<SHDynamicMesh>(_output.get(), SHDynamicMesh::Type);
    auto &fontMap = varAsObjectChecked<SHFontMap>(_font.get(), SHFontMap::Type);

    float scale = float((Var &)_scale.get());
    float3 position = toFloat3(_position.get());

    // Use MeshBuffer directly
    dynMesh.buffer.begin();

    // Create temporary TextPlacer to generate quads
    TextPlacer placer;
    placer.appendString(fontMap.fontMap, std::string_view(input.payload.stringValue), scale);

    // Convert to mesh
    dynMesh.buffer.appendText(placer,
                              position,          // Use input position
                              float3(1, 0, 0),   // Right direction
                              float3(0, 1, 0),   // Up direction
                              float4(1, 1, 1, 1) // White color
    );

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

void registerTextShards() {
  REGISTER_SHARD("GFX.FontMap", FontMapShard);
  REGISTER_SHARD("GFX.DynMesh", DynamicMeshShard);
  REGISTER_SHARD("GFX.DynDrawText", DynamicDrawTextShard);
  REGISTER_SHARD("GFX.DynToMesh", DynamicToMeshShard);
}

} // namespace gfx::text

SHARDS_REGISTER_FN(text) { gfx::text::registerTextShards(); }
