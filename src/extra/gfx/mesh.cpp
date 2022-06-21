#include "../gfx.hpp"
#include "buffer_vars.hpp"
#include <gfx/geom.hpp>
#include <gfx/mesh.hpp>
#include <params.hpp>

using namespace shards;
namespace gfx {

struct MeshShard {
  static inline Type VertexAttributeSeqType = Type::SeqOf(CoreInfo::StringType);
  static inline shards::Types VerticesSeqTypes{
      {CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::Float4Type, CoreInfo::ColorType}};
  static inline Type VerticesSeq = Type::SeqOf(VerticesSeqTypes);
  static inline shards::Types IndicesSeqTypes{{CoreInfo::IntType}};
  static inline Type IndicesSeq = Type::SeqOf(IndicesSeqTypes);
  static inline shards::Types InputTableTypes{{VerticesSeq, IndicesSeq}};
  static inline const char *InputVerticesTableKey = "Vertices";
  static inline const char *InputIndicesTableKey = "Indices";
  static inline std::array<SHString, 2> InputTableKeys{InputVerticesTableKey, InputIndicesTableKey};
  static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

  static SHTypesInfo inputTypes() { return InputTable; }
  static SHTypesInfo outputTypes() { return Types::Mesh; }

  PARAM_VAR(_layoutParam, "Layout", "The names for each vertex attribute.", {VertexAttributeSeqType});
  PARAM_VAR(_windingOrderParam, "WindingOrder", "Front facing winding order for this mesh.", {Types::WindingOrder});
  PARAM_IMPL(MeshShard, PARAM_IMPL_FOR(_layoutParam), PARAM_IMPL_FOR(_windingOrderParam));

  MeshPtr *_mesh = {};

  void cleanup() {
    PARAM_CLEANUP();
    if (_mesh) {
      Types::MeshObjectVar.Release(_mesh);
      _mesh = nullptr;
    }
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _mesh = Types::MeshObjectVar.New();
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
      meshFormat.vertexAttributes[i].name = layoutSeq.elements[i].payload.stringValue;
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

    return Types::MeshObjectVar.Get(_mesh);
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

  static constexpr uint32_t TypeTypeId = 'bmid';
  static inline shards::Type TypeType = shards::Type::Enum(VendorId, TypeTypeId);
  static inline EnumInfo<Type> TypeEnumInfo{"BuiltinMeshType", VendorId, TypeTypeId};

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return Types::Mesh; }

  PARAM_VAR(_type, "Type", "The type of object to make.", {TypeType})
  PARAM_IMPL(BuiltinMeshShard, PARAM_IMPL_FOR(_type));

  BuiltinMeshShard() {
    _type = Var::Enum(Type::Cube, SHTypeInfo(TypeType).enumeration.vendorId, SHTypeInfo(TypeType).enumeration.typeId);
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup() { PARAM_CLEANUP(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    MeshPtr *meshVar = Types::MeshObjectVar.New();

    switch (Type(_type.payload.enumValue)) {
    case Type::Cube: {
      geom::CubeGenerator gen;
      gen.generate();
      *meshVar = createMesh(gen.vertices, gen.indices);
    } break;
    case Type::Sphere: {
      geom::SphereGenerator gen;
      gen.generate();
      *meshVar = createMesh(gen.vertices, gen.indices);
    } break;
    case Type::Plane: {
      geom::PlaneGenerator gen;
      gen.generate();
      *meshVar = createMesh(gen.vertices, gen.indices);
    } break;
    }

    return Types::MeshObjectVar.Get(meshVar);
  }
};

void registerMeshShards() {
  REGISTER_SHARD("GFX.Mesh", MeshShard);
  REGISTER_SHARD("GFX.BuiltinMesh", BuiltinMeshShard);
}
} // namespace gfx
