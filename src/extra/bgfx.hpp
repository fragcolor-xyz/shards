/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2020 Giovanni Petrantoni */

#pragma once

#include "bgfx/bgfx.h"
#include "bgfx/platform.h"
#include "blocks/shared.hpp"

using namespace chainblocks;
namespace BGFX {
enum class Renderer { None, DirectX11, Vulkan, OpenGL, Metal };

#if defined(__linux__) || defined(__EMSCRIPTEN__)
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
  }

private:
  std::deque<ViewInfo> viewsStack;
  bgfx::ViewId nextViewCounter{0};
};

struct Texture {
  static inline Type ObjType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxTextureHandleCC}}}};
  static inline Type SeqType = Type::SeqOf(ObjType);
  static inline Type VarType = Type::VariableOf(ObjType);
  static inline Type VarSeqType = Type::VariableOf(SeqType);

  typedef ObjectVar<Texture> VarInfo;
  static inline VarInfo Var{"BGFX-Texture", CoreCC, BgfxTextureHandleCC};

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

struct ShaderHandle {
  static inline Type ObjType{
      {CBType::Object,
       {.object = {.vendorId = CoreCC, .typeId = BgfxShaderHandleCC}}}};
  static inline Type VarType = Type::VariableOf(ObjType);

  typedef ObjectVar<ShaderHandle> VarInfo;
  static inline VarInfo Var{"BGFX-Shader", CoreCC, BgfxShaderHandleCC};

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

  typedef ObjectVar<ModelHandle> VarInfo;
  static inline VarInfo Var{"BGFX-Model", CoreCC, BgfxModelHandleCC};

  struct StaticModel {
    bgfx::VertexBufferHandle vb;
    bgfx::IndexBufferHandle ib;
  };

  struct DynamicModel {
    bgfx::DynamicVertexBufferHandle vb;
    bgfx::DynamicIndexBufferHandle ib;
  };

  uint64_t topologyStateFlag{0}; // Triangle list

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

  static inline ExposedInfo requiredInfo = ExposedInfo(ExposedInfo::Variable(
      "GFX.Context", CBCCSTR("The BGFX Context."), Context::Info));

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }
    return data.inputType;
  }
};
}; // namespace BGFX
