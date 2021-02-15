/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#ifndef CB_BGFX_HPP
#define CB_BGFX_HPP

#include "SDL.h"
#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"

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
};

struct Context {
  static inline Type Info{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxContextCC}}}};

  // Useful to compare with with plugins, they might mismatch!
  const static inline uint32_t BgfxABIVersion = BGFX_API_VERSION;

  ViewInfo &currentView() { return viewsStack.back(); };

  void pushView(ViewInfo view) { viewsStack.emplace_back(view); }

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
  enum class CullMode { None, Front, Back };
  static constexpr uint32_t CullModeCC = 'gfxC';
  static inline EnumInfo<CullMode> CullModeEnumInfo{"CullMode", CoreCC,
                                                    CullModeCC};
  static inline Type CullModeType = Type::Enum(CoreCC, CullModeCC);

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
  CullMode cullMode{CullMode::Back};

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
                        std::string_view defines   //
                        ) = 0;
};
extern std::unique_ptr<IShaderCompiler> makeShaderCompiler();
} // namespace chainblocks

#endif
