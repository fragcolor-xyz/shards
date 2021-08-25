#include "bgfx.hpp"
#include "blocks/shared.hpp"

namespace chainblocks {
namespace Sprite {

constexpr uint32_t SheetCC = 'shee';

struct Sheet {
  static inline Type ObjType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = SheetCC}}}};
  static inline Type VarType = Type::VariableOf(ObjType);

  static inline ObjectVar<Sheet> Var{"Sprite-Sheet", CoreCC, SheetCC};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }

  static CBTypesInfo outputTypes() { return Sheet::ObjType; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _atlas = value;
      break;

    default:
      throw CBException("Parameter out of range.");
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _atlas;
    }
    throw CBException("Parameter out of range.");
  }

  void cleanup() { _atlas.cleanup(); }

  void warmup(CBContext *context) { _atlas.warmup(context); }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto table = _atlas.get().payload.tableValue;
    auto api = table.api;

    auto size = api->tableAt(table, "size")->payload.seqValue;
    this->width = size.elements[0].payload.intValue;
    this->height = size.elements[1].payload.intValue;

    this->premultiply = api->tableAt(table, "pma")->payload.boolValue;

    auto regions = api->tableAt(table, "regions")->payload.seqValue;
    this->regions.reserve(regions.len);
    for (uint32_t i = 0; i < regions.len; ++i) {
      auto table = regions.elements[i].payload.tableValue;
      auto api = table.api;

      auto region = Region{};

      auto bounds = api->tableAt(table, "bounds")->payload.seqValue;
      region.width = bounds.elements[2].payload.intValue;
      region.height = bounds.elements[3].payload.intValue;

      auto uvs = api->tableAt(table, "uvs")->payload.seqValue;
      for (uint32_t j = 0; j < uvs.len && j < 4; ++j) {
        region.uvs[j] = (float)uvs.elements[j].payload.floatValue;
      }

      this->regions.push_back(region);
    }

    return Var::Object(this, CoreCC, SheetCC);
  }

private:
  static inline Parameters _params = {
      {"Atlas",
       CBCCSTR("The atlas definition."),
       {CoreInfo::AnyTableType, CoreInfo::AnyVarTableType}}};

  // params
  ParamVar _atlas{};

public:
  struct Region {
    uint32_t width;
    uint32_t height;
    float uvs[4];
  };

  uint32_t width;
  uint32_t height;
  bool premultiply;
  std::vector<Region> regions;
};

void registerBlocks() {
  REGISTER_CBLOCK("Sprite.Sheet", Sheet);
}

}; // namespace Sprite
}; // namespace chainblocks
