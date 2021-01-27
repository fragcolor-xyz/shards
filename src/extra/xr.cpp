/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "blocks/shared.hpp"

namespace chainblocks {
namespace XR {
struct RenderXR : public BGFX::BaseConsumer {
  /*
  VR/AR/XR renderer, in the case of Web/Javascript it is required to have a
  function window.cb_emscripten_wait_webxr_dialog = async function() { ... }
  that shows a dialog the user has to accept and start a session like:

  let glCanvas = document.getElementById('canvas'); // this is SDL canvas
  canvas let gl = glCanvas.getContext('webgl'); // this is our bgfx context
  let session = await navigator.xr.requestSession('immersive-vr');
  let layer = new XRWebGLLayer(session, gl);
  session.updateRenderState({ baseLayer: layer });

  resolve to true if this is done, false otherwise.
  */

  static inline Parameters params{
      {"Contents",
       CBCCSTR("The blocks expressing the contents to render."),
       {CoreInfo::BlocksOrNone}},
      {"Near",
       CBCCSTR("The distance from the near clipping plane."),
       {CoreInfo::FloatType}},
      {"Far",
       CBCCSTR("The distance from the far clipping plane."),
       {CoreInfo::FloatType}}};
  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _blocks = value;
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

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _blocks;
    case 1:
      return Var(_near);
    case 2:
      return Var(_far);
    default:
      throw InvalidParameterIndex();
    }
  }

#ifdef __EMSCRIPTEN__
  static inline std::optional<emscripten::val> _xrSession;

  std::optional<emscripten::val> _glLayer;
  std::optional<emscripten::val> _refSpace;
  std::optional<emscripten::val> _cb;
#endif

  bgfx::FrameBufferHandle _framebuffer = BGFX_INVALID_HANDLE;
  bgfx::ViewId _views[2];
  CBVar *_bgfx_context{nullptr};
  float _near{0.1};
  float _far{1000.0};
  BlocksVar _blocks;

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  CBTypeInfo compose(const CBInstanceData &data) {
    BaseConsumer::compose(data);
    _blocks.compose(data);
    return CoreInfo::AnyType;
  }

  void warmup(CBContext *context) {
    _blocks.warmup(context);

    _bgfx_context = referenceVariable(context, "GFX.Context");
    assert(_bgfx_context->valueType == CBType::Object);

#ifdef __EMSCRIPTEN__
    if (!bool(_xrSession)) {
      // session is lazy loaded and unique
      // check globals first
      auto session = emscripten::val::global("ChainblocksWebXRSession");
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
                context,
                xr.call<emscripten::val>("isSessionSupported",
                                         emscripten::val("immersive-vr")));
            xrSupported = supported.as<bool>();
            LOG(INFO) << "WebXR session supported: " << xrSupported;
          } else {
            LOG(INFO) << "WebXR navigator.xr not available.";
          }
        } else {
          LOG(INFO) << "WebXR navigator not available.";
        }
        if (xrSupported) {
          const auto dialog =
              emscripten::val::global("ChainblocksWebXROpenDialog");
          if (!dialog.as<bool>()) {
            throw ActivationError("Failed to find webxr permissions call "
                                  "(window.ChainblocksWebXROpenDialog).");
          }
          // if we are the first users of session this will be true
          // and we need to fetch session
          auto session =
              emscripten_wait<emscripten::val>(context, dialog(_near, _far));
          if (session.as<bool>()) {
            emscripten::val::global("ChainblocksWebXRSession") = session;
            _xrSession = session;
          }
        }
      }
    }

    if (bool(_xrSession)) {
      auto ctx =
          reinterpret_cast<BGFX::Context *>(_bgfx_context->payload.objectValue);

      _cb = (*_xrSession)["chainblocks"];
      if (!_cb->as<bool>()) {
        throw ActivationError(
            "Failed to get internal session.chainblocks object.");
      }

      // first time per block initialization
      const auto refspacePromise = _xrSession->call<emscripten::val>(
          "requestReferenceSpace", emscripten::val("local"));
      _refSpace = emscripten_wait<emscripten::val>(context, refspacePromise);
      if (!_refSpace->as<bool>()) {
        throw ActivationError("Failed to request reference space.");
      }

      LOG(INFO) << "Entering immersive VR mode.";

      // this call should stop the regular run loop
      // and start requesting XR frames via requestAnimationFrame
      _cb->call<void>("warmup");

      // ok this is a bit tricky, we are going to swap run loop
      // the next node tick will be inside the VR callback
      // to do so we simply yield here once
      suspend(context, 0);

      LOG(INFO) << "Entered immersive VR mode.";

      // we should be resuming inside the VR loop
      _glLayer =
          (*_xrSession)["renderState"]["baseLayer"].as<emscripten::val>();
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
        const auto jnewFbId =
            GL.call<emscripten::val>("getNewId", framebuffers);
        framebuffers.set(jnewFbId, gframebuffer);
        gframebuffer.set("name", jnewFbId);
        // ok we have a real framebuffer, that is opaque so we need to do some
        // magic to make it usable by bgfx
        // Also here set real size cos internally bgfx won't clear that on
        // internal framebuffer destroy when replaced
        _framebuffer = bgfx::createFrameBuffer(
            uint16_t(width), uint16_t(height), bgfx::TextureFormat::RGBA8);
        // we need to make sure the above buffer has been created...
        // to do so we draw here a frame (might create artifacts)
        bgfx::frame();
        const auto pframebuffer = uintptr_t(jnewFbId.as<uint32_t>());
        if (bgfx::overrideInternal(_framebuffer, pframebuffer) !=
            pframebuffer) {
          throw ActivationError("Failed to override internal framebuffer.");
        }

        bgfx::setViewFrameBuffer(_views[0], _framebuffer);
        bgfx::setViewFrameBuffer(_views[1], _framebuffer);
      }

      bgfx::setViewClear(
          _views[0], BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
          0xC0FFEEFF, 1.0f, 0);
      bgfx::setViewClear(
          _views[1], BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
          0xC0FFEEFF, 1.0f, 0);

      LOG(INFO) << "Started immersive VR rendering.";
    }
#endif
  }

  void cleanup() {
    _blocks.cleanup();

#ifdef __EMSCRIPTEN__
    if (_cb) {
      // on the JS side this should also do:
      // 1. call cancelAnimationFrame
      // 2. make sure we don't queue another VR frame
      // 3. do not close the session
      _cb->call<void>("cleanup");
    }

    _cb.reset();
    _glLayer.reset();
    _refSpace.reset();

    if (_bgfx_context) {
      releaseVariable(_bgfx_context);
      _bgfx_context = nullptr;
    }
#endif
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto ctx =
        reinterpret_cast<BGFX::Context *>(_bgfx_context->payload.objectValue);
#ifdef __EMSCRIPTEN__
    if (likely(bool(_cb))) {
      const auto frame = (*_cb)["frame"];
      if (unlikely(!frame.as<bool>())) {
        throw ActivationError("WebXR frame data not found.");
      }
      const auto pose =
          frame.call<emscripten::val>("getViewerPose", *_refSpace);
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
          const auto view = views[i].as<emscripten::val>();
          const auto viewport =
              _glLayer->call<emscripten::val>("getViewport", view);
          const auto vX = viewport["x"].as<int>();
          const auto vY = viewport["y"].as<int>();
          const auto vWidth = viewport["width"].as<int>();
          const auto vHeight = viewport["height"].as<int>();

          // push _viewId
          ctx->pushView({_views[i], vWidth, vHeight});
          DEFER({ ctx->popView(); });

          // Touch _viewId
          bgfx::touch(_views[i]);

          // Set viewport
          bgfx::setViewRect(_views[i], uint16_t(vX), uint16_t(vY),
                            uint16_t(vWidth), uint16_t(vHeight));
          bgfx::setViewScissor(_views[i], uint16_t(vX), uint16_t(vY),
                               uint16_t(vWidth), uint16_t(vHeight));

          float viewMat[16];
          {
            const auto jview = view["transform"]["inverse"]["matrix"];
            for (int j = 0; j < 16; j++) {
              viewMat[j] = jview[j].as<float>();
            }
          }
          float projMat[16];
          {
            const auto jproj = view["projectionMatrix"];
            for (int j = 0; j < 16; j++) {
              projMat[j] = jproj[j].as<float>();
            }
          }
          bgfx::setViewTransform(_views[i], viewMat, projMat);

          // activate the blocks and render
          CBVar output{};
          _blocks.activate(context, input, output);
        }
      } else {
        LOG_EVERY_N(200, INFO) << "XR pose not available.";
      }
    } else {
      LOG_EVERY_N(200, INFO) << "Skipping XR rendering, not initialized.";
    }
#endif

    return Var::Empty;
  }
};

void registerBlocks() { REGISTER_CBLOCK("XR.Render", RenderXR); }
} // namespace XR
} // namespace chainblocks