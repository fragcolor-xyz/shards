#include "../gfx.hpp"
#include "buffer_vars.hpp"
#include <gfx/mesh.hpp>

using namespace chainblocks;
namespace gfx {

struct MeshBlock {
  static inline Type VertexAttributeSeqType = Type::SeqOf(CoreInfo::StringType);
  static inline Types VerticesSeqTypes{
      {CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::ColorType, CoreInfo::Int4Type}};
  static inline Type VerticesSeq = Type::SeqOf(VerticesSeqTypes);
  static inline Types IndicesSeqTypes{{CoreInfo::IntType}};
  static inline Type IndicesSeq = Type::SeqOf(IndicesSeqTypes);
  static inline Types InputTableTypes{{VerticesSeq, IndicesSeq}};
  static inline const char *InputVerticesTableKey = "Vertices";
  static inline const char *InputIndicesTableKey = "Indices";
  static inline std::array<CBString, 2> InputTableKeys{InputVerticesTableKey, InputIndicesTableKey};
  static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

  static CBTypesInfo inputTypes() { return InputTable; }
  static CBTypesInfo outputTypes() { return MeshType; }

  static inline Parameters params{{"Layout", CBCCSTR("The names for each vertex attribute."), {VertexAttributeSeqType}},
                                  {"WindingOrder", CBCCSTR("Front facing winding order for this mesh."), {WindingOrderType}}};

  static CBParametersInfo parameters() { return params; }

  std::vector<Var> _layout;
  WindingOrder _windingOrder{WindingOrder::CCW};

  MeshPtr *_mesh = {};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _layout = std::vector<Var>(Var(value));
      break;
    case 1:
      _windingOrder = WindingOrder(value.payload.enumValue);
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_layout);
    case 1:
      return Var::Enum(_windingOrder, VendorId, WindingOrderTypeId);
    default:
      return Var::Empty;
    }
  }

  void cleanup() {
    if (_mesh) {
      MeshObjectVar.Release(_mesh);
      _mesh = nullptr;
    }
  }

  void warmup(CBContext *context) {
    _mesh = MeshObjectVar.New();
    MeshPtr mesh = (*_mesh) = std::make_shared<Mesh>();
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    MeshFormat meshFormat;
    meshFormat.primitiveType = PrimitiveType::TriangleList;
    meshFormat.windingOrder = _windingOrder;

    const CBTable &inputTable = input.payload.tableValue;
    CBVar *verticesVar = inputTable.api->tableAt(inputTable, InputVerticesTableKey);
    CBVar *indicesVar = inputTable.api->tableAt(inputTable, InputIndicesTableKey);

    if (_layout.size() == 0) {
      throw formatException("Layout must be set");
    }

    meshFormat.indexFormat = determineIndexFormat(*indicesVar);
    validateIndexFormat(meshFormat.indexFormat, *indicesVar);

    determineVertexFormat(meshFormat.vertexAttributes, _layout.size(), *verticesVar);

    // Fill vertex attribute names from the Layout parameter
    for (size_t i = 0; i < _layout.size(); i++) {
      meshFormat.vertexAttributes[i].name = _layout[i].payload.stringValue;
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

    return MeshObjectVar.Get(_mesh);
  }
};

void registerMeshBlocks() { REGISTER_CBLOCK("GFX.Mesh", MeshBlock); }
} // namespace gfx
