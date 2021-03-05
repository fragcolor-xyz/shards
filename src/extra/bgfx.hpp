/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#ifndef CB_BGFX_HPP
#define CB_BGFX_HPP

#include "SDL.h"
#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"
#include "linalg_shim.hpp"

using namespace chainblocks;
namespace BGFX {
enum class Renderer { None, DirectX11, Vulkan, OpenGL, Metal };

#if defined(__linux__) || defined(__EMSCRIPTEN__) ||                           \
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

struct ViewInfo {
  bgfx::ViewId id{0};

  int width{0};
  int height{0};

  bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE;

  float minDepth;
  float maxDepth;

  Mat4 view;
  Mat4 invView;
  Mat4 proj;
  Mat4 invProj;
  Mat4 viewProj;
  Mat4 invViewProj;
};

struct Context {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxContextCC}}}};

  // Useful to compare with with plugins, they might mismatch!
  const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;

  ViewInfo &currentView() { return viewsStack.back(); };

  ViewInfo &pushView(ViewInfo view) { return viewsStack.emplace_back(view); }

  void popView() {
    assert(viewsStack.size() > 0);
    viewsStack.pop_back();
  }

  size_t viewIndex() const { return viewsStack.size(); }

  bgfx::ViewId nextViewId() {
    assert(nextViewCounter < UINT16_MAX);
    return nextViewCounter++;
  }

  void reset() {
    viewsStack.clear();
    nextViewCounter = 0;
    for (auto &sampler : samplers) {
      bgfx::destroy(sampler);
    }
    samplers.clear();
    lightCount = 0;
  }

  const bgfx::UniformHandle &getSampler(size_t index) {
    const auto nsamplers = samplers.size();
    if (index >= nsamplers) {
      std::string name("DrawSampler_");
      name.append(std::to_string(index));
      return samplers.emplace_back(
          bgfx::createUniform(name.c_str(), bgfx::UniformType::Sampler));
    } else {
      return samplers[index];
    }
  }

  // for now this is very simple, we just compute how many max light sources we
  // have to render. In the future we will do it in a smarter way
  uint32_t getMaxLights() const { return lightCount; }
  void addLight() { lightCount++; }

  // TODO thread_local? anyway sort multiple threads
  // this is written during sleeps between node ticks
  static inline std::vector<SDL_Event> sdlEvents;

private:
  std::deque<ViewInfo> viewsStack;
  bgfx::ViewId nextViewCounter{0};
  std::vector<bgfx::UniformHandle> samplers;
  uint32_t lightCount{0};
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
#define BGFX_TEXTURE2D_CREATE(_bits, _components, _texture, _srgb)             \
  if (_bits == 8) {                                                            \
    switch (_components) {                                                     \
    case 1:                                                                    \
      _texture->handle = bgfx::createTexture2D(                                \
          _texture->width, _texture->height, false, 1,                         \
          bgfx::TextureFormat::R8, _srgb ? BGFX_TEXTURE_SRGB : 0);             \
      break;                                                                   \
    case 2:                                                                    \
      _texture->handle = bgfx::createTexture2D(                                \
          _texture->width, _texture->height, false, 1,                         \
          bgfx::TextureFormat::RG8, _srgb ? BGFX_TEXTURE_SRGB : 0);            \
      break;                                                                   \
    case 3:                                                                    \
      _texture->handle = bgfx::createTexture2D(                                \
          _texture->width, _texture->height, false, 1,                         \
          bgfx::TextureFormat::RGB8, _srgb ? BGFX_TEXTURE_SRGB : 0);           \
      break;                                                                   \
    case 4:                                                                    \
      _texture->handle = bgfx::createTexture2D(                                \
          _texture->width, _texture->height, false, 1,                         \
          bgfx::TextureFormat::RGBA8, _srgb ? BGFX_TEXTURE_SRGB : 0);          \
      break;                                                                   \
    default:                                                                   \
      LOG(FATAL) << "invalid state";                                           \
      break;                                                                   \
    }                                                                          \
  } else if (_bits == 16) {                                                    \
    switch (_components) {                                                     \
    case 1:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::R16U);                    \
      break;                                                                   \
    case 2:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RG16U);                   \
      break;                                                                   \
    case 3:                                                                    \
      throw ActivationError("Format not supported, it seems bgfx has no "      \
                            "RGB16, try using RGBA16 instead (FillAlpha).");   \
      break;                                                                   \
    case 4:                                                                    \
      _texture->handle =                                                       \
          bgfx::createTexture2D(_texture->width, _texture->height, false, 1,   \
                                bgfx::TextureFormat::RGBA16U);                 \
      break;                                                                   \
    default:                                                                   \
      LOG(FATAL) << "invalid state";                                           \
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
      LOG(FATAL) << "invalid state";                                           \
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

  ~ShaderHandle() {
    if (handle.idx != bgfx::kInvalidHandle) {
      bgfx::destroy(handle);
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
  CBVar *_bgfxCtx;
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
};
extern std::unique_ptr<IShaderCompiler> makeShaderCompiler();
} // namespace chainblocks

#endif
