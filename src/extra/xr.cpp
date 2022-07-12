/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "./bgfx.hpp"
#include "shards/shared.hpp"
#include <linalg_shim.hpp>

namespace shards {
namespace XR {
constexpr uint32_t XRContextCC = 'xr  ';
struct RenderXR;
struct Context {
  static inline Type ObjType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = XRContextCC}}}};
  static inline Type VarType = Type::VariableOf(ObjType);

  RenderXR *xr{nullptr};
};

enum class XRHand { Left, Right };

struct GamePadTable : public TableVar {
  static constexpr std::array<SHString, 4> _keys{
      "buttons",
      "sticks",
      "id",
      "connected",
  };
  static inline Types _types{{
      CoreInfo::Float3SeqType,
      CoreInfo::Float2SeqType,
      CoreInfo::StringType,
      CoreInfo::BoolType,
  }};
  static inline Type ValueType = Type::TableOf(_types, _keys);

  GamePadTable()
      : TableVar(),                      //
        buttons(get<SeqVar>("buttons")), //
        sticks(get<SeqVar>("sticks")),   //
        id((*this)["id"]),               //
        connected((*this)["connected"]) {
    connected = Var(false);
  }

  SeqVar &buttons;
  SeqVar &sticks;
  SHVar &id;
  SHVar &connected;
};

struct ControllerTable : public GamePadTable {
  static constexpr uint32_t HandednessCC = 'xrha';
  static inline Type HandEnumType{{SHType::Enum, {.enumeration = {.vendorId = CoreCC, .typeId = HandednessCC}}}};
  static inline EnumInfo<XRHand> HandEnumInfo{"XrHand", CoreCC, HandednessCC};

  static constexpr std::array<SHString, 7> _keys{
      "handedness", "transform", "inverseTransform", "buttons", "sticks", "id", "connected",
  };
  static inline Types _types{{
      HandEnumType,
      CoreInfo::Float4x4Type,
      CoreInfo::Float4x4Type,
      CoreInfo::Float3SeqType,
      CoreInfo::Float2SeqType,
      CoreInfo::StringType,
      CoreInfo::BoolType,
  }};
  static inline Type ValueType = Type::TableOf(_types, _keys);

  ControllerTable()
      : GamePadTable(),                      //
        handedness((*this)["handedness"]),   //
        transform(get<SeqVar>("transform")), //
        inverseTransform(get<SeqVar>("inverseTransform")) {
    handedness = Var::Enum(XRHand::Left, CoreCC, HandednessCC);
  }

  SHVar &handedness;
  SeqVar &transform;
  SeqVar &inverseTransform;
};

struct Consumer {
  static inline ExposedInfo requiredInfo =
      ExposedInfo(ExposedInfo::Variable("XR.Context", SHCCSTR("The XR Context."), Context::ObjType));

  SHExposedTypesInfo requiredVariables() { return SHExposedTypesInfo(requiredInfo); }

  SHTypeInfo compose(SHInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("XR Shards cannot be used on a worker thread (e.g. "
                         "within an Await shard)");
    }
    return data.inputType;
  }
};

struct RenderXR : public BGFX::BaseConsumer {
  /*
  VR/AR/XR renderer, in the case of Web/Javascript it is required to have a
  function window.ShardsWebXROpenDialog = async function(near, far) { ... }
  that shows a dialog the user has to accept and start a session like:

  let glCanvas = document.getElementById('canvas'); // this is SDL canvas
  canvas let gl = glCanvas.getContext('webgl'); // this is our bgfx context
  await gl.makeXRCompatible();
  let session = await navigator.xr.requestSession('immersive-vr');
  let layer = new XRWebGLLayer(session, gl);
  session.updateRenderState({
    baseLayer: layer,
    depthFar: far,
    depthNear: near
  });
  resolve with the new session if this is done, null otherwise.
  Also populate:
  session.shards.warmup and session.shards.cleanup
  check cleanup and warmup for more details under
  */

  static inline Parameters params{
      {"Contents", SHCCSTR("The shards expressing the contents to render."), {CoreInfo::ShardsOrNone}},
      {"Near", SHCCSTR("The distance from the near clipping plane."), {CoreInfo::FloatType}},
      {"Far", SHCCSTR("The distance from the far clipping plane."), {CoreInfo::FloatType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _shards = value;
      break;
    case 1:
      _near = value.payload.floatValue;
      break;
    case 2:
      _far = value.payload.floatValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _shards;
    case 1:
      return Var(_near);
    case 2:
      return Var(_far);
    default:
      throw InvalidParameterIndex();
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  SHTypeInfo compose(SHInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("XR Shards cannot be used on a worker thread (e.g. "
                         "within an Await shard)");
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "XR.Context") == 0) {
        throw SHException("XR.Context must be unique, found another use!");
      }
    }

    // twice to actually own the data and release...
    IterableExposedInfo rshared(data.shared);
    IterableExposedInfo shared(rshared);
    shared.push_back(ExposedInfo::ProtectedVariable("XR.Context", SHCCSTR("The XR Context."), Context::ObjType));
    data.shared = shared;
    _shards.compose(data);

    return data.inputType;
  }

  void warmup(SHContext *context) {
    BGFX::BaseConsumer::_warmup(context);

    _shards.warmup(context);

    _xrContextPVar = referenceVariable(context, "XR.Context");
    _xrContextPVar->valueType = SHType::Object;
    _xrContextPVar->payload.objectVendorId = CoreCC;
    _xrContextPVar->payload.objectTypeId = XRContextCC;
    _xrContextPVar->payload.objectValue = &_xrContext;

#ifdef __EMSCRIPTEN__
    if (!bool(_xrSession)) {
      // session is lazy loaded and unique
      // check globals first
      auto session = emscripten::val::global("ShardsWebXRSession");
      if (session.as<bool>()) {
        _xrSession = session;
      } else {
        // if not avail try to init it
        auto xrSupported = false;
        const auto navigator = emscripten::val::global("navigator");
        if (navigator.as<bool>()) {
          const auto xr = navigator["xr"];
          if (xr.as<bool>()) {
            const auto supported = emscripten_wait<emscripten::val>(
                context, xr.call<emscripten::val>("isSessionSupported", emscripten::val("immersive-vr")));
            xrSupported = supported.as<bool>();
            SHLOG_INFO("WebXR session supported: {}", xrSupported);
          } else {
            SHLOG_INFO("WebXR navigator.xr not available");
          }
        } else {
          SHLOG_INFO("WebXR navigator not available");
        }
        if (xrSupported) {
          const auto dialog = emscripten::val::global("ShardsWebXROpenDialog");
          if (!dialog.as<bool>()) {
            throw ActivationError("Failed to find webxr permissions call "
                                  "(window.ShardsWebXROpenDialog).");
          }
          // if we are the first users of session this will be true
          // and we need to fetch session
          auto session = emscripten_wait<emscripten::val>(context, dialog(_near, _far));
          if (session.as<bool>()) {
            emscripten::val::global("ShardsWebXRSession") = session;
            _xrSession = session;
          }
        }
      }
    }

    if (bool(_xrSession)) {
      auto ctx = reinterpret_cast<BGFX::Context *>(_bgfxCtx->payload.objectValue);

      _sh = (*_xrSession)["shards"];
      if (!_sh->as<bool>()) {
        throw ActivationError("Failed to get internal session.shards object.");
      }

      // first time per shard initialization
      const auto refspacePromise = _xrSession->call<emscripten::val>("requestReferenceSpace", emscripten::val("local"));
      _refSpace = emscripten_wait<emscripten::val>(context, refspacePromise);
      if (!_refSpace->as<bool>()) {
        throw ActivationError("Failed to request reference space.");
      }

      SHLOG_INFO("Entering immersive VR mode");

      // this call should stop the regular run loop
      // and start requesting XR frames via requestAnimationFrame
      _sh->call<void>("warmup");

      // ok this is a bit tricky, we are going to swap run loop
      // the next mesh tick will be inside the VR callback
      // to do so we simply yield here once
      suspend(context, 0);

      SHLOG_INFO("Entered immersive VR mode");

      // we should be resuming inside the VR loop
      _glLayer = (*_xrSession)["renderState"]["baseLayer"].as<emscripten::val>();
      if (!_glLayer->as<bool>()) {
        throw ActivationError("Failed to get render state baseLayer.");
      }

      _views[0] = ctx->nextViewId();
      _views[1] = ctx->nextViewId();

      auto gframebuffer = (*_glLayer)["framebuffer"];
      if (gframebuffer.as<bool>()) {
        const auto width = (*_glLayer)["framebufferWidth"].as<int>();
        const auto height = (*_glLayer)["framebufferHeight"].as<int>();
        // first of all we need to make this WebGLFramebuffer object
        // compatible with emscripten traditional gl emulated uint IDs This is
        // hacky cos requires internal emscripten knowledge and might change
        // any time as its private code
        const auto GL = emscripten::val::module_property("GL");
        if (!GL.as<bool>()) {
          throw ActivationError("Failed to get GL from module.");
        }
        auto framebuffers = GL["framebuffers"];
        if (!framebuffers.as<bool>()) {
          throw ActivationError("Failed to get GL.framebuffers.");
        }
        const auto jnewFbId = GL.call<emscripten::val>("getNewId", framebuffers);
        framebuffers.set(jnewFbId, gframebuffer);
        gframebuffer.set("name", jnewFbId);
        // ok we have a real framebuffer, that is opaque so we need to do some
        // magic to make it usable by bgfx
        // Also here set real size cos internally bgfx won't clear that on
        // internal framebuffer destroy when replaced
        _framebuffer = bgfx::createFrameBuffer(uint16_t(width), uint16_t(height), bgfx::TextureFormat::RGBA8);
        // we need to make sure the above buffer has been created...
        // to do so we draw here a frame (might create artifacts)
        bgfx::frame();
        const auto pframebuffer = uintptr_t(jnewFbId.as<uint32_t>());
        if (bgfx::overrideInternal(_framebuffer, pframebuffer) != pframebuffer) {
          throw ActivationError("Failed to override internal framebuffer.");
        }

        bgfx::setViewFrameBuffer(_views[0], _framebuffer);
        bgfx::setViewFrameBuffer(_views[1], _framebuffer);
      }

      bgfx::setViewClear(_views[0], BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0xC0FFEEFF, 1.0f, 0);
      bgfx::setViewClear(_views[1], BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, 0xC0FFEEFF, 1.0f, 0);

      SHLOG_INFO("Started immersive VR rendering");
    }
#endif
  }

  void cleanup() {
    _shards.cleanup();

#ifdef __EMSCRIPTEN__
    if (_sh) {
      // on the JS side this should also do:
      // 1. call cancelAnimationFrame
      // 2. make sure we don't queue another VR frame
      // 3. do not close the session
      _sh->call<void>("cleanup");
    }

    _sh.reset();
    _glLayer.reset();
    _refSpace.reset();
#endif

    if (_xrContextPVar) {
      releaseVariable(_xrContextPVar);
      _xrContextPVar = nullptr;
    }

    BGFX::BaseConsumer::_cleanup();
  }

  void populateInputsData() {
#ifdef __EMSCRIPTEN__
    const auto populateGamePadData = [](const emscripten::val &source, GamePadTable &data) {
      const auto gamepad = source["gamepad"];
      if (gamepad.as<bool>()) {
        if (data.id.valueType == SHType::None) {
          cloneVar(data.id, Var(gamepad["id"].as<std::string>()));
        }

        data.connected = Var(gamepad["connected"].as<bool>());

        const auto buttons = gamepad["buttons"];
        const auto nbuttons = buttons["length"].as<size_t>();
        data.buttons.resize(nbuttons);
        for (size_t i = 0; i < nbuttons; ++i) {
          const auto button = buttons[i];
          data.buttons[i] = Var(button["pressed"].as<double>(), button["touched"].as<double>(), button["value"].as<double>());
        }

        const auto axes = gamepad["axes"];
        const auto naxes = axes["length"].as<size_t>();
        if ((naxes % 2) != 0) {
          throw ActivationError("Expected a controller with 2 axis sticks.");
        }
        data.sticks.resize(naxes / 2);
        for (size_t i = 0; i < naxes; i += 2) {
          data.sticks[i / 2] = Var(axes[i + 0].as<double>(), axes[i + 1].as<double>());
        }
      }
    };

    const auto inputSources = (*_xrSession)["inputSources"];
    const auto len = inputSources["length"].as<size_t>();
    for (size_t i = 0; i < len; i++) {
      const auto source = inputSources[i];
      const auto gripSpace = source["gripSpace"];
      if (gripSpace.as<bool>()) {
        auto isHand = source["handedness"] != emscripten::val("none");
        if (isHand) {
          auto hand = source["handedness"] == emscripten::val("left") ? XRHand::Left : XRHand::Right;
          const auto pose = _frame->call<emscripten::val>("getPose", gripSpace, *_refSpace);
          if (pose.as<bool>()) {
            const auto transform = pose["transform"];
            {
              const auto mat = transform["matrix"];
              _hands[(int)hand].transform.resize(4);
              for (int j = 0; j < 4; j++) {
                _hands[(int)hand].transform[j] = Var(mat[(j * 4) + 0].as<float>(), mat[(j * 4) + 1].as<float>(),
                                                     mat[(j * 4) + 2].as<float>(), mat[(j * 4) + 3].as<float>());
              }
            }
            {
              const auto mat = transform["inverse"]["matrix"];
              _hands[(int)hand].inverseTransform.resize(4);
              for (int j = 0; j < 4; j++) {
                _hands[(int)hand].inverseTransform[j] = Var(mat[(j * 4) + 0].as<float>(), mat[(j * 4) + 1].as<float>(),
                                                            mat[(j * 4) + 2].as<float>(), mat[(j * 4) + 3].as<float>());
              }
            }
            populateGamePadData(source, _hands[(int)hand]);
          }
        }
      }
    }
#endif
  }

  SHVar activate(SHContext *context, const SHVar &input) {
#ifdef __EMSCRIPTEN__
    gfx::FrameRenderer &frameRenderer = getFrameRenderer();
    if (likely(bool(_sh))) {
      _frame = (*_sh)["frame"];
      DEFER(_frame.reset()); // let's not hold this reference
      if (unlikely(!_frame->as<bool>())) {
        throw ActivationError("WebXR frame data not found.");
      }
      const auto pose = _frame->call<emscripten::val>("getViewerPose", *_refSpace);
      // pose might not be available
      if (likely(pose.as<bool>())) {
        const auto views = pose["views"];
        if (unlikely(!views.as<bool>())) {
          throw ActivationError("WebXR pose had no views.");
        }

        const auto len = views["length"].as<size_t>();
        if (unlikely(len != 2)) {
          throw ActivationError("WebXR views length was not 2.");
        }

        for (size_t i = 0; i < 2; i++) {
          const auto view = views[i];
          const auto viewport = _glLayer->call<emscripten::val>("getViewport", view);
          const auto vX = viewport["x"].as<int>();
          const auto vY = viewport["y"].as<int>();
          const auto vWidth = viewport["width"].as<int>();
          const auto vHeight = viewport["height"].as<int>();

          // push _viewId
          auto &currentView = frameRenderer.pushView({_views[i], vWidth, vHeight});
          DEFER({ frameRenderer.popView(); });

          // Touch _viewId
          bgfx::touch(_views[i]);

          // Set viewport
          bgfx::setViewRect(_views[i], uint16_t(vX), uint16_t(vY), uint16_t(vWidth), uint16_t(vHeight));
          bgfx::setViewScissor(_views[i], uint16_t(vX), uint16_t(vY), uint16_t(vWidth), uint16_t(vHeight));

          aligned_array<float, 16> viewMat;
          {
            const auto jview = view["transform"]["inverse"]["matrix"];
            for (int j = 0; j < 16; j++) {
              viewMat[j] = jview[j].as<float>();
            }
          }
          aligned_array<float, 16> projMat;
          {
            const auto jproj = view["projectionMatrix"];
            for (int j = 0; j < 16; j++) {
              projMat[j] = jproj[j].as<float>();
            }
          }
          bgfx::setViewTransform(_views[i], viewMat.data(), projMat.data());

          // set viewport params
          currentView.viewport.x = vX;
          currentView.viewport.y = vY;
          currentView.viewport.width = vWidth;
          currentView.viewport.height = vHeight;

          // set view transforms
          currentView.view = Mat4::FromArray(viewMat);
          currentView.proj = Mat4::FromArray(projMat);
          currentView.invalidate();

          populateInputsData();

          // activate the shards and render
          SHVar output{};
          _shards.activate(context, input, output);
        }
      } else {
        // LOG_EVERY_N(200, INFO) << "XR pose not available.";
      }
    } else {
      // LOG_EVERY_N(200, INFO) << "Skipping XR rendering, not initialized.";
    }
#endif

    return Var::Empty;
  }

  const ControllerTable &getHandData(XRHand hand) const { return _hands[(int)hand]; }

private:
#ifdef __EMSCRIPTEN__
  static inline std::optional<emscripten::val> _xrSession;

  std::optional<emscripten::val> _glLayer;
  std::optional<emscripten::val> _refSpace;
  std::optional<emscripten::val> _sh;
  std::optional<emscripten::val> _frame;
#endif

  bgfx::FrameBufferHandle _framebuffer = BGFX_INVALID_HANDLE;
  bgfx::ViewId _views[2];
  float _near{0.1};
  float _far{1000.0};
  ShardsVar _shards;
  Context _xrContext{this};
  SHVar *_xrContextPVar{nullptr};
  std::array<ControllerTable, 2> _hands;
};

struct Controller : public Consumer {
  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return ControllerTable::ValueType; }

  static inline Parameters params{
      {"Hand", SHCCSTR("Which hand we want to track."), {ControllerTable::HandEnumType}},
      {"Inverse", SHCCSTR("If the output should be the inverse transformation matrix."), {CoreInfo::BoolType}}};
  static SHParametersInfo parameters() { return params; }

  SHVar *_xrContext{nullptr};
  XRHand _hand{XRHand::Left};
  bool _inverse{false};

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _hand = XRHand(value.payload.enumValue);
      break;
    case 1:
      _inverse = value.payload.boolValue;
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_hand, CoreCC, 'xrha');
    case 1:
      return Var(_inverse);
    default:
      throw InvalidParameterIndex();
    }
  }

  SHTypeInfo compose(SHInstanceData &data) {
    Consumer::compose(data);
    return ControllerTable::ValueType;
  }

  void warmup(SHContext *context) {
    _xrContext = referenceVariable(context, "XR.Context");
    assert(_xrContext->valueType == SHType::Object);
  }

  void cleanup() {
    if (_xrContext) {
      releaseVariable(_xrContext);
      _xrContext = nullptr;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto xrCtx = reinterpret_cast<Context *>(_xrContext->payload.objectValue);
    auto &handData = xrCtx->xr->getHandData(_hand);
    return handData;
  }
};

void registerShards() {
  REGISTER_SHARD("XR.Render", RenderXR);
  REGISTER_SHARD("XR.Controller", Controller);
}
} // namespace XR
} // namespace shards
