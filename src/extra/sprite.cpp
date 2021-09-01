#include "./sprite.hpp"

namespace chainblocks {
namespace Sprite {


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

      if (api->tableContains(table, "uvs")) {
        auto uvs = api->tableAt(table, "uvs")->payload.seqValue;
        for (uint32_t j = 0; j < uvs.len && j < 4; ++j) {
          region.uvs[j] = (float)uvs.elements[j].payload.floatValue;
        }
      } else {
        // calculate UVs
        double x = bounds.elements[0].payload.intValue;
        double y = bounds.elements[1].payload.intValue;
        double u = x / (double)this->width;
        double v = y / (double)this->height;
        bool rotate = api->tableAt(table, "rotate")->payload.boolValue;
        double u2, v2;
        if (rotate) {
          u2 = (x + region.height) / (double)this->width;
          v2 = (y + region.width) / (double)this->height;
        } else {
          u2 = (x + region.width) / (double)this->width;
          v2 = (y + region.height) / (double)this->height;
        }

        region.uvs[0] = (float)u;
        region.uvs[1] = (float)u2;
        region.uvs[2] = (float)v;
        region.uvs[3] = (float)v2;
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

struct Draw {
  static CBTypesInfo inputTypes() { return CoreInfo::Float4x4Type; }

  static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Type; }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _sheet = value;
      break;
    case 1:
      _texture = value;
      break;
    case 2:
      _index = value;
      break;
    case 3:
      _shader = value;
      break;
    case 4:
      _blend = value;
      break;

    default:
      throw CBException("Parameter out of range.");
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _sheet;
    case 1:
      return _texture;
    case 2:
      return _index;
    case 3:
      return _shader;
    case 4:
      return _blend;
    }
    throw CBException("Parameter out of range.");
  }

  void cleanup() {
    _sheet.cleanup();
    _texture.cleanup();
    _index.cleanup();
    _shader.cleanup();

    if (_bgfxContext) {
      releaseVariable(_bgfxContext);
      _bgfxContext = nullptr;
    }
  }

  void warmup(CBContext *context) {
    _sheet.warmup(context);
    _texture.warmup(context);
    _index.warmup(context);
    _shader.warmup(context);

    _bgfxContext = referenceVariable(context, "GFX.Context");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.seqValue.len != 4) {
      throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
    }

    auto *sheet = reinterpret_cast<Sheet *>(_sheet.get().payload.objectValue);

    auto *ctx =
        reinterpret_cast<BGFX::Context *>(_bgfxContext->payload.objectValue);
    const auto &currentView = ctx->currentView();

    // step: transform
    bgfx::Transform t;
    // using allocTransform to avoid an extra copy
    auto idx = bgfx::allocTransform(&t, 1);
    memcpy(&t.data[0], &input.payload.seqValue.elements[0].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[4], &input.payload.seqValue.elements[1].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[8], &input.payload.seqValue.elements[2].payload.float4Value,
           sizeof(float) * 4);
    memcpy(&t.data[12], &input.payload.seqValue.elements[3].payload.float4Value,
           sizeof(float) * 4);
    bgfx::setTransform(idx, 1);

    // step: create quad
    auto index = _index.get().payload.intValue;
    const auto &region = sheet->regions[index % sheet->regions.size()];
    bgfx::TransientVertexBuffer vb;
    // FIXME: bound/size doesn't work if the sprite is rotated
    createQuad(&vb, region.width / (float)currentView.width,
               region.height / (float)currentView.height);
    // step: apply texture coordinates
    auto *vertex = (PosTexCoord0Vertex *)vb.data;
    const auto &uvs = region.uvs;
    float minu = uvs[0];
    float maxu = uvs[1];
    float minv = uvs[2];
    float maxv = uvs[3];

    vertex[0].m_u = minu;
    vertex[0].m_v = minv;

    vertex[1].m_u = maxu;
    vertex[1].m_v = minv;

    vertex[2].m_u = maxu;
    vertex[2].m_v = maxv;

    vertex[3].m_u = minu;
    vertex[3].m_v = minv;

    vertex[4].m_u = maxu;
    vertex[4].m_v = maxv;

    vertex[5].m_u = minu;
    vertex[5].m_v = maxv;

    bgfx::setVertexBuffer(0, &vb);

    // step: render
    auto shader = reinterpret_cast<BGFX::ShaderHandle *>(
        _shader.get().payload.objectValue);
    assert(shader);

    auto state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    // blend state
    const auto getBlends = [&](CBVar &blend) {
      uint64_t s = 0;
      uint64_t d = 0;
      uint64_t o = 0;
      ForEach(blend.payload.tableValue, [&](auto key, auto &val) {
        if (key[0] == 'S')
          s = BGFX::Enums::BlendToBgfx(val.payload.enumValue);
        else if (key[0] == 'D')
          d = BGFX::Enums::BlendToBgfx(val.payload.enumValue);
        else if (key[0] == 'O')
          o = BGFX::Enums::BlendOpToBgfx(val.payload.enumValue);
      });
      return std::make_tuple(s, d, o);
    };

    switch (_blend.valueType) {
    case Table: {
      const auto [s, d, o] = getBlends(_blend);
      state |= BGFX_STATE_BLEND_FUNC(s, d) | BGFX_STATE_BLEND_EQUATION(o);
    } break;

    case Seq: {
      assert(_blend.payload.seqValue.len == 2);
      const auto [sc, dc, oc] = getBlends(_blend.payload.seqValue.elements[0]);
      const auto [sa, da, oa] = getBlends(_blend.payload.seqValue.elements[1]);
      state |= BGFX_STATE_BLEND_FUNC_SEPARATE(sc, dc, sa, da) |
               BGFX_STATE_BLEND_EQUATION_SEPARATE(oc, oa);
    } break;

    default: {
      if (sheet->premultiply) {
        state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                       BGFX_STATE_BLEND_INV_SRC_ALPHA) |
                 BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD);
      } else {
        state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                       BGFX_STATE_BLEND_INV_SRC_ALPHA) |
                 BGFX_STATE_BLEND_EQUATION(BGFX_STATE_BLEND_EQUATION_ADD);
      }
    } break;
    }

    bgfx::setState(state);

    auto *texture =
        reinterpret_cast<BGFX::Texture *>(_texture.get().payload.objectValue);
    bgfx::setTexture(0, ctx->getSampler(0), texture->handle);

    bgfx::submit(currentView.id, shader->handle);

    // pass through
    return input;
  }

  // note: duplicated from GFX.Draw in bgfx.cpp
  static inline bool LayoutSetup{false};

  // note: duplicated from GFX.Draw in bgfx.cpp
  void setup() {
    if (!LayoutSetup) {
      PosTexCoord0Vertex::init();
      LayoutSetup = true;
    }
  }

  // note: duplicated from GFX.Draw in bgfx.cpp
  struct PosTexCoord0Vertex {
    float m_x;
    float m_y;
    float m_z;
    float m_u;
    float m_v;

    static void init() {
      ms_layout.begin()
          .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
          .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
          .end();
    }

    static inline bgfx::VertexLayout ms_layout;
  };

  // note: modified from GFX.Draw in bgfx.cpp to be a real quad
  void createQuad(bgfx::TransientVertexBuffer *vb, float width = 1.0f,
                  float height = 1.0f) {
    if (6 ==
        bgfx::getAvailTransientVertexBuffer(6, PosTexCoord0Vertex::ms_layout)) {
      bgfx::allocTransientVertexBuffer(vb, 6, PosTexCoord0Vertex::ms_layout);
      PosTexCoord0Vertex *vertex = (PosTexCoord0Vertex *)vb->data;

      const float zz = 0.0f;

      // note: anchored at the center
      const float minx = -width / 2;
      const float maxx = width / 2;
      const float miny = -height / 2;
      const float maxy = height / 2;

      const float minu = 0.0f;
      const float maxu = 1.0f;

      float minv = 0.0f;
      float maxv = 1.0f;

      // first triangle
      vertex[0].m_x = minx;
      vertex[0].m_y = miny;
      vertex[0].m_z = zz;
      vertex[0].m_u = minu;
      vertex[0].m_v = minv;

      vertex[1].m_x = maxx;
      vertex[1].m_y = miny;
      vertex[1].m_z = zz;
      vertex[1].m_u = maxu;
      vertex[1].m_v = minv;

      vertex[2].m_x = maxx;
      vertex[2].m_y = maxy;
      vertex[2].m_z = zz;
      vertex[2].m_u = maxu;
      vertex[2].m_v = maxv;

      // second triangle
      vertex[3].m_x = minx;
      vertex[3].m_y = miny;
      vertex[3].m_z = zz;
      vertex[3].m_u = minu;
      vertex[3].m_v = minv;

      vertex[4].m_x = maxx;
      vertex[4].m_y = maxy;
      vertex[4].m_z = zz;
      vertex[4].m_u = maxu;
      vertex[4].m_v = maxv;

      vertex[5].m_x = minx;
      vertex[5].m_y = maxy;
      vertex[5].m_z = zz;
      vertex[5].m_u = minu;
      vertex[5].m_v = maxv;
    }
  }

private:
  static inline Parameters _params = {
      {"Sheet", CBCCSTR("The sprite sheet."), {Sheet::ObjType, Sheet::VarType}},
      {"Texture",
       CBCCSTR("The texture to render."),
       {BGFX::Texture::ObjType, BGFX::Texture::VarType}},
      {"Index",
       CBCCSTR("The index of the sprite in the sprite sheet."),
       {CoreInfo::IntType, CoreInfo::IntVarType}},
      // FIXME should the shader be contextual, hardcoded, have a default value?
      {"Shader",
       CBCCSTR("The shader program to use for this draw."),
       {BGFX::ShaderHandle::ObjType, BGFX::ShaderHandle::VarType}},
      {"Blend",
       CBCCSTR(
           "The blend state table to describe and enable blending. If it's a "
           "single table the state will be assigned to both RGB and Alpha, if "
           "2 tables are specified, the first will be RGB, the second Alpha."),
       {CoreInfo::NoneType, BGFX::Enums::BlendTable,
        BGFX::Enums::BlendTableSeq}}};

  // params
  ParamVar _sheet{};
  ParamVar _texture{};
  ParamVar _index{};
  ParamVar _shader{};
  OwnedVar _blend{};

  // misc
  CBVar *_bgfxContext{nullptr};
};

void registerBlocks() {
  REGISTER_CBLOCK("Sprite.Draw", Draw);
  REGISTER_CBLOCK("Sprite.Sheet", Sheet);
}

}; // namespace Sprite
}; // namespace chainblocks
