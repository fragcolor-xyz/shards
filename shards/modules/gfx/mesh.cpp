#include "gfx.hpp"
#include "buffer_vars.hpp"
#include <gfx/geom.hpp>
#include <gfx/mesh.hpp>
#include <shards/core/params.hpp>

using namespace shards;
namespace gfx {

struct MeshShard {
  static inline Type VertexAttributeSeqType = Type::SeqOf(CoreInfo::StringType);
  static inline shards::Types VerticesSeqTypes{
      {CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::Float4Type, CoreInfo::ColorType}};
  static inline Type VerticesSeq = Type::SeqOf(VerticesSeqTypes);
  static inline shards::Types IndicesSeqTypes{{CoreInfo::IntType}};
  static inline Type IndicesSeq = Type::SeqOf(IndicesSeqTypes);
  static inline shards::Types InputTableTypes{{VerticesSeq, IndicesSeq}};
  static inline Var InputVerticesTableKey = Var("Vertices");
  static inline Var InputIndicesTableKey = Var("Indices");
  static inline std::array<SHVar, 2> InputTableKeys{InputVerticesTableKey, InputIndicesTableKey};
  static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

  static SHTypesInfo inputTypes() { return InputTable; }
  static SHTypesInfo outputTypes() { return ShardsTypes::Mesh; }

  PARAM_VAR(_layoutParam, "Layout", "The names for each vertex attribute.", {VertexAttributeSeqType});
  PARAM_VAR(_windingOrderParam, "WindingOrder", "Front facing winding order for this mesh.",
            {ShardsTypes::WindingOrderEnumInfo::Type});
  PARAM_IMPL(PARAM_IMPL_FOR(_layoutParam), PARAM_IMPL_FOR(_windingOrderParam));

  MeshPtr *_mesh = {};

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_mesh) {
      ShardsTypes::MeshObjectVar.Release(_mesh);
      _mesh = nullptr;
    }
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _mesh = ShardsTypes::MeshObjectVar.New();
    MeshPtr mesh = (*_mesh) = std::make_shared<Mesh>();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    MeshFormat meshFormat;
    meshFormat.primitiveType = PrimitiveType::TriangleList;
    meshFormat.windingOrder = WindingOrder(_windingOrderParam.payload.enumValue);

    const SHTable &inputTable = input.payload.tableValue;
    SHVar *verticesVar = inputTable.api->tableAt(inputTable, InputVerticesTableKey);
    SHVar *indicesVar = inputTable.api->tableAt(inputTable, InputIndicesTableKey);

    SHSeq &layoutSeq = _layoutParam.payload.seqValue;
    if (layoutSeq.len == 0) {
      throw formatException("Layout must be set");
    }

    meshFormat.indexFormat = determineIndexFormat(*indicesVar);
    validateIndexFormat(meshFormat.indexFormat, *indicesVar);

    determineVertexFormat(meshFormat.vertexAttributes, layoutSeq.len, *verticesVar);

    // Fill vertex attribute names from the Layout parameter
    for (size_t i = 0; i < layoutSeq.len; i++) {
      meshFormat.vertexAttributes[i].name = SHSTRVIEW(layoutSeq.elements[i]);
    }

    validateVertexFormat(meshFormat.vertexAttributes, *verticesVar);

    MeshPtr mesh = *_mesh;

    struct Appender : public std::vector<uint8_t> {
      using std::vector<uint8_t>::size;
      using std::vector<uint8_t>::resize;
      using std::vector<uint8_t>::data;
      void operator()(const void *inData, size_t inSize) {
        size_t offset = size();
        resize(size() + inSize);
        memcpy(data() + offset, inData, inSize);
      }
    };

    Appender vertexData;
    packIntoVertexBuffer(vertexData, *verticesVar);

    Appender indexData;
    packIntoIndexBuffer(indexData, meshFormat.indexFormat, *indicesVar);

    mesh->update(meshFormat, std::move(vertexData), std::move(indexData));

    return ShardsTypes::MeshObjectVar.Get(_mesh);
  }
};

template <typename T> MeshPtr createMesh(const std::vector<T> &verts, const std::vector<geom::GeneratorBase::index_t> &indices) {
  MeshPtr mesh = std::make_shared<Mesh>();
  MeshFormat format{
      .vertexAttributes = T::getAttributes(),
  };
  mesh->update(format, verts.data(), verts.size() * sizeof(T), indices.data(),
               indices.size() * sizeof(geom::GeneratorBase::index_t));
  return mesh;
}

struct BuiltinMeshShard {
  enum class Type {
    Cube,
    Sphere,
    Plane,
  };

  DECL_ENUM_INFO(Type, BuiltinMeshType, 'bmid');

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return ShardsTypes::Mesh; }

  PARAM_VAR(_type, "Type", "The type of object to make.", {BuiltinMeshTypeEnumInfo::Type})
  PARAM_IMPL(PARAM_IMPL_FOR(_type));

  MeshPtr *_output{};
  void clearOutput() {
    if (_output) {
      ShardsTypes::MeshObjectVar.Release(_output);
      _output = nullptr;
    }
  }

  BuiltinMeshShard() { _type = Var::Enum(Type::Cube, BuiltinMeshTypeEnumInfo::VendorId, BuiltinMeshTypeEnumInfo::TypeId); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    clearOutput();
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (!_output) {
      _output = ShardsTypes::MeshObjectVar.New();

      switch (Type(_type.payload.enumValue)) {
      case Type::Cube: {
        geom::CubeGenerator gen;
        gen.generate();
        *_output = createMesh(gen.vertices, gen.indices);
      } break;
      case Type::Sphere: {
        geom::SphereGenerator gen;
        gen.generate();
        *_output = createMesh(gen.vertices, gen.indices);
      } break;
      case Type::Plane: {
        geom::PlaneGenerator gen;
        gen.generate();
        *_output = createMesh(gen.vertices, gen.indices);
      } break;
      }
    }

    return ShardsTypes::MeshObjectVar.Get(_output);
  }
};

void registerMeshShards() {
  REGISTER_ENUM(BuiltinMeshShard::BuiltinMeshTypeEnumInfo);

  REGISTER_SHARD("GFX.Mesh", MeshShard);
  REGISTER_SHARD("GFX.BuiltinMesh", BuiltinMeshShard);
}
} // namespace gfx
