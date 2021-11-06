/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef CB_BGFX_HPP
#define CB_BGFX_HPP

#include "SDL.h"
#include "bgfx/bgfx.h"
#include "bgfx/embedded_shader.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"
#include "linalg_shim.hpp"

#include "fs_picking.bin.h"

using namespace chainblocks;
namespace BGFX {
constexpr uint16_t PickingBufferSize = 128;

enum class Renderer { None, DirectX11, Vulkan, OpenGL, Metal };

#if defined(BGFX_CONFIG_RENDERER_VULKAN)
constexpr Renderer CurrentRenderer = Renderer::Vulkan;
#elif defined(__linux__) || defined(__EMSCRIPTEN__) ||                         \
    defined(BGFX_CONFIG_RENDERER_OPENGL)
constexpr Renderer CurrentRenderer = Renderer::OpenGL;
#elif defined(_WIN32)
constexpr Renderer CurrentRenderer = Renderer::DirectX11;
#elif defined(__APPLE__)
constexpr Renderer CurrentRenderer = Renderer::Metal;
#endif

constexpr uint32_t BgfxTextureHandleCC = 'gfxT';
constexpr uint32_t BgfxShaderHandleCC = 'gfxS';
constexpr uint32_t BgfxModelHandleCC = 'gfxM';
constexpr uint32_t BgfxContextCC = 'gfx ';
constexpr uint32_t BgfxNativeWindowCC = 'gfxW';

// BGFX_CONFIG_MAX_VIEWS is 256
constexpr bgfx::ViewId MaxViews = 256;
constexpr bgfx::ViewId GuiViewId = MaxViews - 1;
constexpr bgfx::ViewId BlittingViewId = GuiViewId - 1;
constexpr bgfx::ViewId PickingViewId = BlittingViewId - 1;

// FROM BGFX, MIGHT BREAK IF BGFX CHANGES
constexpr bool isShaderVerLess(uint32_t _magic, uint8_t _version) {
  return (_magic & BX_MAKEFOURCC(0, 0, 0, 0xff)) <
         BX_MAKEFOURCC(0, 0, 0, _version);
}
// ~ FROM BGFX, MIGHT BREAK IF BGFX CHANGES

constexpr uint32_t getShaderOutputHash(const bgfx::Memory *mem) {
  uint32_t magic = *(uint32_t *)mem->data;
  uint32_t hashIn = *(uint32_t *)(mem->data + 4);
  uint32_t hashOut = 0x0;
  if (isShaderVerLess(magic, 6)) {
    hashOut = hashIn;
  } else {
    hashOut = *(uint32_t *)(mem->data + 8);
  }
  return hashOut;
}

inline void overrideShaderInputHash(const bgfx::Memory *mem, uint32_t hash) {
  // hack/fix hash in or creation of program will fail!
  // otherwise bgfx would fail inside createShader
  *(uint32_t *)(mem->data + 4) = hash;
}

inline const bgfx::EmbeddedShader::Data &
findEmbeddedShader(const bgfx::EmbeddedShader &shader) {
  bgfx::RendererType::Enum type = bgfx::getRendererType();
  for (uint32_t i = 0; i < bgfx::RendererType::Count; ++i) {
    if (shader.data[i].type == type) {
      return shader.data[i];
    }
  }
  throw std::runtime_error("Could not find embedded shader");
}

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

struct NativeWindow {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxNativeWindowCC}}}};
};

struct alignas(16) ViewInfo {
  bgfx::ViewId id{0};

  int width{0};
  int height{0};

  bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;

  struct {
    int x{0};
    int y{0};
    int width{64};
    int height{64};
  } viewport;

  Mat4 view;
  Mat4 proj;

  const Mat4 &invView() const {
    if (unlikely(_invView.x._private[0] == 0)) {
      _invView = linalg::inverse(view);
      _invView.x._private[0] = 1;
    }
    return _invView;
  }

  const Mat4 &invProj() const {
    if (unlikely(_invProj.x._private[0] == 0)) {
      _invProj = linalg::inverse(proj);
      _invProj.x._private[0] = 1;
    }
    return _invProj;
  }

  const Mat4 &viewProj() const {
    if (unlikely(_viewProj.x._private[0] == 0)) {
      _viewProj = linalg::mul(view, proj);
      _viewProj.x._private[0] = 1;
    }
    return _viewProj;
  }

  const Mat4 &invViewProj() const {
    if (unlikely(_invViewProj.x._private[0] == 0)) {
      _invViewProj = linalg::inverse(viewProj());
      _invViewProj.x._private[0] = 1;
    }
    return _invViewProj;
  }

  void invalidate() {
    _invView.x._private[0] = 0;
    _invProj.x._private[0] = 0;
    _viewProj.x._private[0] = 0;
    _invViewProj.x._private[0] = 0;
  }

  mutable Mat4 _invView{};
  mutable Mat4 _invProj{};
  mutable Mat4 _viewProj{};
  mutable Mat4 _invViewProj{};
};

struct IDrawable {
  virtual CBChain *getChain() = 0;
};

struct Context {
  static inline bgfx::EmbeddedShader PickingShaderData =
      BGFX_EMBEDDED_SHADER(fs_picking);

  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxContextCC}}}};

  // Useful to compare with with plugins, they might mismatch!
  const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;

  void reset() {
    _viewsStack.clear();
    _nextViewCounter = 0;

    for (auto &sampler : _samplers) {
      bgfx::destroy(sampler);
    }
    _samplers.clear();

    // _pickingRT is destroyed when FB is destroyed!
    if (_pickingUniform.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_pickingUniform);
      _pickingUniform = BGFX_INVALID_HANDLE;
    }
    if (_pickingTexture.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(_pickingTexture);
      _pickingTexture = BGFX_INVALID_HANDLE;
    }

    _lightCount = 0;
    _currentFrame = 0;
  }

  ViewInfo &currentView() { return _viewsStack.back(); };

  ViewInfo &pushView(ViewInfo view) { return _viewsStack.emplace_back(view); }

  void popView() {
    assert(_viewsStack.size() > 0);
    _viewsStack.pop_back();
  }

  size_t viewIndex() const { return _viewsStack.size(); }

  bgfx::ViewId nextViewId() {
    assert(_nextViewCounter < UINT16_MAX);
    return _nextViewCounter++;
  }

  void newFrame() {
    _frameDrawablesCount = 0;
    _frameDrawables.clear();
  }

  void setBgfxFrame(uint32_t frame) { _currentFrame = frame; }

  uint32_t getBgfxFrame() const { return _currentFrame; }

  uint32_t addFrameDrawable(IDrawable *drawable) {
    // 0 idx = empty
    uint32_t id = ++_frameDrawablesCount;
    _frameDrawables[id] = drawable;
    return id;
  }

  IDrawable *getFrameDrawable(uint32_t id) {
    if (id == 0) {
      return nullptr;
    }
    return _frameDrawables[id];
  }

  const bgfx::UniformHandle &getSampler(size_t index) {
    const auto nSamplers = _samplers.size();
    if (index >= nSamplers) {
      std::string name("DrawSampler_");
      name.append(std::to_string(index));
      return _samplers.emplace_back(
          bgfx::createUniform(name.c_str(), bgfx::UniformType::Sampler));
    } else {
      return _samplers[index];
    }
  }

  // for now this is very simple, we just compute how many max light sources we
  // have to render. In the future we will do it in a smarter way
  constexpr uint32_t getMaxLights() const { return _lightCount; }
  void addLight() { _lightCount++; }

  constexpr bool isPicking() const { return _picking; }

  void setPicking(bool picking) { _picking = picking; }

  bgfx::TextureHandle &pickingTexture() { return _pickingTexture; }

  bgfx::TextureHandle &pickingRenderTarget() { return _pickingRT; }

  const bgfx::UniformHandle getPickingUniform() {
    if (_pickingUniform.idx == bgfx::kInvalidHandle) {
      _pickingUniform =
          bgfx::createUniform("u_picking_id", bgfx::UniformType::Vec4);
    }
    return _pickingUniform;
  }

  // TODO thread_local? anyway sort multiple threads
  // this is written during sleeps between node ticks
  static inline std::vector<SDL_Event> sdlEvents;

private:
  std::deque<ViewInfo> _viewsStack;
  bgfx::ViewId _nextViewCounter{0};

  std::deque<bgfx::UniformHandle> _samplers;

  uint32_t _lightCount{0};

  uint32_t _frameDrawablesCount{0};
  std::unordered_map<uint32_t, IDrawable *> _frameDrawables;

  bool _picking{false};
  bgfx::UniformHandle _pickingUniform = BGFX_INVALID_HANDLE;
  bgfx::TextureHandle _pickingTexture = BGFX_INVALID_HANDLE;
  bgfx::TextureHandle _pickingRT = BGFX_INVALID_HANDLE;

  uint32_t _currentFrame{0};
};

struct Texture {
  static inline Type ObjType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxTextureHandleCC}}}};
  static inline Type SeqType = Type::SeqOf(ObjType);
  static inline Type VarType = Type::VariableOf(ObjType);
  static inline Type VarSeqType = Type::VariableOf(SeqType);

  static inline ObjectVar<Texture> Var{"BGFX-Texture", CoreCC,
                                       BgfxTextureHandleCC};

  bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
  uint16_t width = 0;
  uint16_t height = 0;
  uint8_t channels = 0;
  int bpp = 1;

  ~Texture() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
    }
  }
};

// utility macro to load textures of different sizes
#define BGFX_TEXTURE2D_CREATE(_bits, _components, _texture)                    \
  if (_bits == 8) {                                                            \
    switch (_components) {                                                     \
    case 1:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::R8);                      \
      break;                                                                   \
    case 2:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RG8);                     \
      break;                                                                   \
    case 3:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RGB8);                    \
      break;                                                                   \
    case 4:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RGBA8);                   \
      break;                                                                   \
    default:                                                                   \
      CBLOG_FATAL("invalid state");                                            \
      break;                                                                   \
    }                                                                          \
  } else if (_bits == 16) {                                                    \
    switch (_components) {                                                     \
    case 1:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::R16);                     \
      break;                                                                   \
    case 2:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RG16);                    \
      break;                                                                   \
    case 3:                                                                    \
      throw ActivationError("Format not supported, it seems bgfx has no "      \
                            "RGB16, try using RGBA16 instead (FillAlpha).");   \
      break;                                                                   \
    case 4:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RGBA16);                  \
      break;                                                                   \
    default:                                                                   \
      CBLOG_FATAL("invalid state");                                            \
      break;                                                                   \
    }                                                                          \
  } else if (_bits == 32) {                                                    \
    switch (_components) {                                                     \
    case 1:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::R32F);                    \
      break;                                                                   \
    case 2:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RG32F);                   \
      break;                                                                   \
    case 3:                                                                    \
      throw ActivationError(                                                   \
          "Format not supported, it seems bgfx has no RGB32F, try using "      \
          "RGBA32F instead (FillAlpha).");                                     \
      break;                                                                   \
    case 4:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RGBA32F);                 \
      break;                                                                   \
    default:                                                                   \
      CBLOG_FATAL("invalid state");                                            \
      break;                                                                   \
    }                                                                          \
  }

struct ShaderHandle {
  static inline Type ObjType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxShaderHandleCC}}}};
  static inline Type VarType = Type::VariableOf(ObjType);

  static inline ObjectVar<ShaderHandle> Var{"BGFX-Shader", CoreCC,
                                            BgfxShaderHandleCC};

  bgfx::ProgramHandle handle = BGFX_INVALID_HANDLE;
  bgfx::ProgramHandle pickingHandle = BGFX_INVALID_HANDLE;

  ~ShaderHandle() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
    }
    if (pickingHandle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(pickingHandle);
    }
  }
};

struct ModelHandle {
  static inline Type ObjType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxModelHandleCC}}}};
  static inline Type VarType = Type::VariableOf(ObjType);

  static inline ObjectVar<ModelHandle> Var{"BGFX-Model", CoreCC,
                                           BgfxModelHandleCC};

  struct StaticModel {
    bgfx::VertexBufferHandle vb;
    bgfx::IndexBufferHandle ib;
  };

  struct DynamicModel {
    bgfx::DynamicVertexBufferHandle vb;
    bgfx::DynamicIndexBufferHandle ib;
  };

  uint64_t topologyStateFlag{0}; // Triangle list
  Enums::CullMode cullMode{Enums::CullMode::Back};

  std::variant<StaticModel, DynamicModel> model{
      StaticModel{BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE}};

  void reset() {
    std::visit(
        [](auto &m) {
          if (m.vb.idx != bgfx::kInvalidHandle) {
            bgfx::destroy(m.vb);
          }
          if (m.ib.idx != bgfx::kInvalidHandle) {
            bgfx::destroy(m.ib);
          }
        },
        model);
  }

  ~ModelHandle() { reset(); }
};

constexpr uint32_t windowCC = 'hwnd';

struct Base {
  CBVar *_bgfxCtx{nullptr};
};

struct BaseConsumer : public Base {
  static inline Type windowType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = windowCC}}}};

  static inline CBExposedTypeInfo ContextInfo = ExposedInfo::Variable(
      "GFX.Context", CBCCSTR("The BGFX Context."), Context::Info);
  static inline ExposedInfo requiredInfo = ExposedInfo(ContextInfo);

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  // Required before _bgfxCtx can be used
  void _warmup(CBContext *context) {
    _bgfxCtx = referenceVariable(context, "GFX.Context");
    assert(_bgfxCtx->valueType == CBType::Object);
  }

  // Required during cleanup if _warmup() was called
  void _cleanup() {
    if (_bgfxCtx) {
      releaseVariable(_bgfxCtx);
      _bgfxCtx = nullptr;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }
    return CoreInfo::NoneType; // on purpose to trigger assertion during
                               // validation
  }
};
}; // namespace BGFX

namespace chainblocks {
struct IShaderCompiler {
  virtual ~IShaderCompiler() {}

  virtual CBVar compile(std::string_view varyings, //
                        std::string_view code,     //
                        std::string_view type,     //
                        std::string_view defines,  //
                        CBContext *context         //
                        ) = 0;

  virtual void warmup(CBContext *context) = 0;
};
extern std::unique_ptr<IShaderCompiler> makeShaderCompiler();
} // namespace chainblocks

#endif
