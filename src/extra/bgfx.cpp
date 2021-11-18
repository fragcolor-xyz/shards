/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "./bgfx.hpp"
#include "./bgfx/enums.hpp"
#include "./bgfx/objects.hpp"
#include "./imgui.hpp"
#include "SDL_events.h"
#include "bgfx/bgfx.h"
#include "gfx/context.hpp"
#include "gfx/imgui.hpp"
#include "SDL.h"
#include <bx/debug.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <stb_image_write.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/html5.h>
#endif

using namespace chainblocks;

namespace BGFX {

struct BaseWindow : public Base {

	static inline Parameters params{
		{"Title", CBCCSTR("The title of the window to create."), {CoreInfo::StringType}},
		{"Width", CBCCSTR("The width of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
		{"Height", CBCCSTR("The height of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
		{"Contents", CBCCSTR("The contents of this window."), {CoreInfo::BlocksOrNone}},
		{"Fullscreen", CBCCSTR("If the window should use fullscreen mode."), {CoreInfo::BoolType}},
		{"Debug",
		 CBCCSTR("If the device backing the window should be created with "
				 "debug layers on."),
		 {CoreInfo::BoolType}},
		{"ClearColor",
		 CBCCSTR("The color to use to clear the backbuffer at the beginning of a "
				 "new frame."),
		 {CoreInfo::ColorType}}};

	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

	static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

	static CBParametersInfo parameters() { return params; }

	std::string _title;
	int _pwidth = 1024;
	int _pheight = 768;
	int _rwidth = 1024;
	int _rheight = 768;
	bool _debug = false;
	bool _fsMode{false};
	SDL_Window *_window = nullptr;
	CBVar *_sdlWinVar = nullptr;
	CBVar *_imguiCtx = nullptr;
	BlocksVar _blocks;
	CBColor _clearColor{0xC0, 0xC0, 0xAA, 0xFF};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_title = value.payload.stringValue;
			break;
		case 1:
			_pwidth = int(value.payload.intValue);
			break;
		case 2:
			_pheight = int(value.payload.intValue);
			break;
		case 3:
			_blocks = value;
			break;
		case 4:
			_fsMode = value.payload.boolValue;
			break;
		case 5:
			_debug = value.payload.boolValue;
			break;
		case 6:
			_clearColor = value.payload.colorValue;
			break;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_title);
		case 1:
			return Var(_pwidth);
		case 2:
			return Var(_pheight);
		case 3:
			return _blocks;
		case 4:
			return Var(_fsMode);
		case 5:
			return Var(_debug);
		case 6:
			return Var(_clearColor);
		default:
			return Var::Empty;
		}
	}
};

struct MainWindow : public BaseWindow {
	gfx::Context _gfxContext{};
	bool _gfxContextInitialized{false};

	struct ProcessClock {
		decltype(std::chrono::high_resolution_clock::now()) Start;
		ProcessClock() { Start = std::chrono::high_resolution_clock::now(); }
	};

	ProcessClock _absTimer;
	ProcessClock _deltaTimer;
	uint32_t _frameCounter{0};

	CBTypeInfo compose(CBInstanceData &data) {
		if (data.onWorkerThread) {
			throw ComposeError(
				"GFX Blocks cannot be used on a worker thread (e.g. "
				"within an Await block)");
		}

		// Make sure MainWindow is UNIQUE
		for (uint32_t i = 0; i < data.shared.len; i++) {
			if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
				throw CBException("GFX.MainWindow must be unique, found another use!");
			}
		}

		// twice to actually own the data and release...
		IterableExposedInfo rshared(data.shared);
		IterableExposedInfo shared(rshared);
		shared.push_back(ExposedInfo::ProtectedVariable("GFX.Context", CBCCSTR("The BGFX Context."), gfx::ContextType));
		data.shared = shared;
		_blocks.compose(data);

		return data.inputType;
	}

	void cleanup() {
		_blocks.cleanup();

		if (_gfxContextInitialized) {
			_gfxContext.cleanup();
		}

		if (_window) {
			SDL_DestroyWindow(_window);
			SDL_Quit();
			_window = nullptr;
		}

		if (_bgfxCtx) {
			if (_bgfxCtx->refcount > 1) {
#ifdef NDEBUG
				CBLOG_ERROR("MainWindow: Found a dangling reference to GFX.Context");
#else
				CBLOG_ERROR("MainWindow: Found {} dangling reference(s) to GFX.Context", _bgfxCtx->refcount - 1);
#endif
			}
			memset(_bgfxCtx, 0x0, sizeof(CBVar));
			_bgfxCtx = nullptr;
		}
	}

	void pollEvents(std::vector<SDL_Event> &events) {
		events.clear();
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			events.push_back(event);
		}
	}

	void warmup(CBContext *context) {
		// do not touch parameter values
		_rwidth = _pwidth;
		_rheight = _pheight;

		auto initErr = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
		if (initErr != 0) {
			CBLOG_ERROR("Failed to initialize SDL {}", SDL_GetError());
			throw ActivationError("Failed to initialize SDL");
		}

		uint32_t flags = SDL_WINDOW_SHOWN;
#ifndef __EMSCRIPTEN__
		flags |= SDL_WINDOW_ALLOW_HIGHDPI;
#endif
		flags |= SDL_WINDOW_RESIZABLE | (_fsMode ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
#ifdef __APPLE__
		flags |= SDL_WINDOW_METAL;
#endif
		// TODO: SDL_WINDOW_BORDERLESS
		_window = SDL_CreateWindow(_title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _fsMode ? 0 : _rwidth, _fsMode ? 0 : _rheight, flags);

		// Ensure clicks will happen even from out of focus!
		SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");

		_gfxContext.clearColor = _clearColor;

		_gfxContext.init(context, _window);
		_gfxContextInitialized = true;

		_bgfxCtx = referenceVariable(context, "GFX.Context");

		_bgfxCtx->payload.objectTypeId = gfx::ContextTypeId;
		_bgfxCtx->payload.objectValue = &_gfxContext;
		_bgfxCtx->valueType = CBType::Object;

		_deltaTimer.Start = std::chrono::high_resolution_clock::now();

		// reset frame counter
		_frameCounter = 0;

		// init blocks after we initialize the system
		_blocks.warmup(context);
	}

	void processWindowEvents(std::vector<SDL_Event> events) {
		for (const SDL_Event &event : events) {
			if (event.type == SDL_QUIT) {
				// stop the current chain on close
				throw ActivationError("Window closed, aborting chain.");
			}
			else if (event.type == SDL_WINDOWEVENT && SDL_GetWindowID(_window) == event.window.windowID) { // support multiple windows closure
				if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
					// stop the current chain on close
					throw ActivationError("Window closed, aborting chain.");
				}
				else if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
					int w, h;
					SDL_GetWindowSize(_window, &w, &h);
					_gfxContext.resizeMainOutput(w, h);
				}
				else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int w, h;
					SDL_GetWindowSize(_window, &w, &h);
#ifdef __APPLE__
					int real_w, real_h;
					SDL_Metal_GetDrawableSize(_window, &real_w, &real_h);
					_gfxContext._windowScalingW = float(real_w) / float(w);
					_gfxContext._windowScalingH = float(real_h) / float(j);
					// finally in this case override our real values
					w = real_w;
					j = real_h;
#endif
					_gfxContext.resizeMainOutput(w, h);
				}
			}
		}

#ifdef __EMSCRIPTEN__
		double canvasWidth, canvasHeight;
		EMSCRIPTEN_RESULT res = emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);
		if (res != EMSCRIPTEN_RESULT_SUCCESS) {
			throw ActivationError("Failed to callemscripten_get_element_css_size");
		}
		resizeMainOutputConditional(int(canvasWidth), int(canvasHeight));
#endif
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		std::vector<SDL_Event> events;
		pollEvents(events);

		processWindowEvents(events);

		const auto tnow = std::chrono::high_resolution_clock::now();
		std::chrono::duration<float> ddt = tnow - _deltaTimer.Start;
		std::chrono::duration<float> adt = tnow - _absTimer.Start;
		_deltaTimer.Start = tnow;

		gfx::FrameRenderer frameRenderer(_gfxContext, gfx::FrameInputs(ddt.count(), adt.count(), _frameCounter++, events));
		frameRenderer.begin(*context);

		gfx::View &mainView = frameRenderer.pushMainView();
		bgfx::setViewName(mainView.id, "MainWindow");
		bgfx::setViewClear(mainView.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, gfx::colorToUInt32(_clearColor), 1.0f, 0);

		gfx::ImguiContext &imguiContext = *_gfxContext.imguiContext.get();
		imguiContext.beginFrame(mainView, frameRenderer.inputs);

		CBVar output{};
		_blocks.activate(context, input, output);

		frameRenderer.popView();

		imguiContext.endFrame();

		frameRenderer.end(*context);

		return input;
	}
};

struct Texture2D : public BaseConsumer {
	Texture *_texture{nullptr};

	void cleanup() {
		if (_texture) {
			Texture::Var.Release(_texture);
			_texture = nullptr;
		}
	}

	static CBTypesInfo inputTypes() { return CoreInfo::ImageType; }

	static CBTypesInfo outputTypes() { return Texture::ObjType; }

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		return Texture::ObjType;
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		auto bpp = 1;
		if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_16BITS_INT) == CBIMAGE_FLAGS_16BITS_INT)
			bpp = 2;
		else if ((input.payload.imageValue.flags & CBIMAGE_FLAGS_32BITS_FLOAT) == CBIMAGE_FLAGS_32BITS_FLOAT)
			bpp = 4;

		// Upload a completely new image if sizes changed, also first activation!
		if (!_texture || input.payload.imageValue.width != _texture->width || input.payload.imageValue.height != _texture->height ||
			input.payload.imageValue.channels != _texture->channels || bpp != _texture->bpp) {
			if (_texture) {
				Texture::Var.Release(_texture);
			}
			_texture = Texture::Var.New();

			_texture->width = input.payload.imageValue.width;
			_texture->height = input.payload.imageValue.height;
			_texture->channels = input.payload.imageValue.channels;
			_texture->bpp = bpp;

			BGFX_TEXTURE2D_CREATE(bpp * 8, _texture->channels, _texture);

			if (_texture->handle.idx == bgfx::kInvalidHandle) throw ActivationError("Failed to create texture");
		}

		// we copy because bgfx is multithreaded
		// this just queues this texture basically
		// this copy is internally managed
		auto mem = bgfx::copy(
			input.payload.imageValue.data, uint32_t(_texture->width) * uint32_t(_texture->height) * uint32_t(_texture->channels) * uint32_t(_texture->bpp));

		bgfx::updateTexture2D(_texture->handle, 0, 0, 0, 0, _texture->width, _texture->height, mem);

		return Texture::Var.Get(_texture);
	}
};

template<char SHADER_TYPE>
struct Shader : public BaseConsumer {
	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
	static CBTypesInfo outputTypes() { return ShaderHandle::ObjType; }

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		return ShaderHandle::ObjType;
	}

	static inline Parameters f_v_params{
		{"VertexShader", CBCCSTR("The vertex shader bytecode."), {CoreInfo::BytesType, CoreInfo::BytesVarType}},
		{"PixelShader", CBCCSTR("The pixel shader bytecode."), {CoreInfo::BytesType, CoreInfo::BytesVarType}}};

	static inline Parameters c_params{{"ComputeShader", CBCCSTR("The compute shader bytecode."), {CoreInfo::BytesType, CoreInfo::BytesVarType}}};

	static CBParametersInfo parameters() {
		if constexpr (SHADER_TYPE == 'c') {
			return c_params;
		}
		else {
			return f_v_params;
		}
	}

	ParamVar _vcode{};
	ParamVar _pcode{};
	ParamVar _ccode{};
	ShaderHandle *_output{nullptr};
	std::array<CBExposedTypeInfo, 3> _requiring;

	CBExposedTypesInfo requiredVariables() {
		int idx = 0;
		_requiring[idx] = BaseConsumer::ContextInfo;
		idx++;

		if constexpr (SHADER_TYPE == 'c') {
			if (_ccode.isVariable()) {
				_requiring[idx].name = _ccode.variableName();
				_requiring[idx].help = CBCCSTR("The required compute shader bytecode.");
				_requiring[idx].exposedType = CoreInfo::BytesType;
				idx++;
			}
		}
		else {
			if (_vcode.isVariable()) {
				_requiring[idx].name = _vcode.variableName();
				_requiring[idx].help = CBCCSTR("The required vertex shader bytecode.");
				_requiring[idx].exposedType = CoreInfo::BytesType;
				idx++;
			}
			if (_pcode.isVariable()) {
				_requiring[idx].name = _pcode.variableName();
				_requiring[idx].help = CBCCSTR("The required pixel shader bytecode.");
				_requiring[idx].exposedType = CoreInfo::BytesType;
				idx++;
			}
		}
		return {_requiring.data(), uint32_t(idx), 0};
	}

	void setParam(int index, const CBVar &value) {
		if constexpr (SHADER_TYPE == 'c') {
			switch (index) {
			case 0:
				_ccode = value;
				break;
			default:
				break;
			}
		}
		else {
			switch (index) {
			case 0:
				_vcode = value;
				break;
			case 1:
				_pcode = value;
				break;
			default:
				break;
			}
		}
	}

	CBVar getParam(int index) {
		if constexpr (SHADER_TYPE == 'c') {
			switch (index) {
			case 0:
				return _ccode;
			default:
				return Var::Empty;
			}
		}
		else {
			switch (index) {
			case 0:
				return _vcode;
			case 1:
				return _pcode;
			default:
				return Var::Empty;
			}
		}
	}

	void cleanup() {
		if constexpr (SHADER_TYPE == 'c') {
			_ccode.cleanup();
		}
		else {
			_vcode.cleanup();
			_pcode.cleanup();
		}

		if (_output) {
			ShaderHandle::Var.Release(_output);
			_output = nullptr;
		}
	}

	void warmup(CBContext *context) {
		if constexpr (SHADER_TYPE == 'c') {
			_ccode.warmup(context);
		}
		else {
			_vcode.warmup(context);
			_pcode.warmup(context);
		}
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		if constexpr (SHADER_TYPE == 'c') {
			const auto &code = _ccode.get();
			auto mem = bgfx::copy(code.payload.bytesValue, code.payload.bytesSize);
			auto sh = bgfx::createShader(mem);
			if (sh.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to load compute shader.");
			}
			auto ph = bgfx::createProgram(sh, true);
			if (ph.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to create compute shader program.");
			}

			if (_output) {
				ShaderHandle::Var.Release(_output);
			}
			_output = ShaderHandle::Var.New();
			_output->handle = ph;
		}
		else {
			const auto &vcode = _vcode.get();
			auto vmem = bgfx::copy(vcode.payload.bytesValue, vcode.payload.bytesSize);
			auto hashOut = getShaderOutputHash(vmem);
			auto vsh = bgfx::createShader(vmem);
			if (vsh.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to load vertex shader.");
			}

			const auto &pcode = _pcode.get();
			auto pmem = bgfx::copy(pcode.payload.bytesValue, pcode.payload.bytesSize);
			auto psh = bgfx::createShader(pmem);
			if (psh.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to load pixel shader.");
			}

			auto ph = bgfx::createProgram(vsh, psh, true);
			if (ph.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to create shader program.");
			}

			// // also build picking program
			auto &pickingShaderData = findEmbeddedShader(gfx::Context::PickingShaderData);
			auto ppmem = bgfx::copy(pickingShaderData.data, pickingShaderData.size);

			overrideShaderInputHash(ppmem, hashOut);
			auto ppsh = bgfx::createShader(ppmem);
			if (ppsh.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to load picking pixel shader.");
			}

			auto pph = bgfx::createProgram(vsh, ppsh, true);
			if (pph.idx == bgfx::kInvalidHandle) {
				throw ActivationError("Failed to create picking shader program.");
			}

			// commit our compiled shaders
			if (_output) {
				ShaderHandle::Var.Release(_output);
			}
			_output = ShaderHandle::Var.New();
			_output->handle = ph;
			// _output->pickingHandle = pph;
		}
		return ShaderHandle::Var.Get(_output);
	}
};

struct Model : public BaseConsumer {
	enum class VertexAttribute {
		Position,  //!< a_position
		Normal,	   //!< a_normal
		Tangent3,  //!< a_tangent
		Tangent4,  //!< a_tangent with handedness
		Bitangent, //!< a_bitangent
		Color0,	   //!< a_color0
		Color1,	   //!< a_color1
		Color2,	   //!< a_color2
		Color3,	   //!< a_color3
		Indices,   //!< a_indices
		Weight,	   //!< a_weight
		TexCoord0, //!< a_texcoord0
		TexCoord1, //!< a_texcoord1
		TexCoord2, //!< a_texcoord2
		TexCoord3, //!< a_texcoord3
		TexCoord4, //!< a_texcoord4
		TexCoord5, //!< a_texcoord5
		TexCoord6, //!< a_texcoord6
		TexCoord7, //!< a_texcoord7
		Skip	   // skips a byte
	};

	static bgfx::Attrib::Enum toBgfx(VertexAttribute attribute) {
		switch (attribute) {
		case VertexAttribute::Position:
			return bgfx::Attrib::Position;
		case VertexAttribute::Normal:
			return bgfx::Attrib::Normal;
		case VertexAttribute::Tangent3:
		case VertexAttribute::Tangent4:
			return bgfx::Attrib::Tangent;
		case VertexAttribute::Bitangent:
			return bgfx::Attrib::Bitangent;
		case VertexAttribute::Color0:
			return bgfx::Attrib::Color0;
		case VertexAttribute::Color1:
			return bgfx::Attrib::Color1;
		case VertexAttribute::Color2:
			return bgfx::Attrib::Color2;
		case VertexAttribute::Color3:
			return bgfx::Attrib::Color3;
		case VertexAttribute::Indices:
			return bgfx::Attrib::Indices;
		case VertexAttribute::Weight:
			return bgfx::Attrib::Weight;
		case VertexAttribute::TexCoord0:
			return bgfx::Attrib::TexCoord0;
		case VertexAttribute::TexCoord1:
			return bgfx::Attrib::TexCoord1;
		case VertexAttribute::TexCoord2:
			return bgfx::Attrib::TexCoord2;
		case VertexAttribute::TexCoord3:
			return bgfx::Attrib::TexCoord3;
		case VertexAttribute::TexCoord4:
			return bgfx::Attrib::TexCoord4;
		case VertexAttribute::TexCoord5:
			return bgfx::Attrib::TexCoord5;
		case VertexAttribute::TexCoord6:
			return bgfx::Attrib::TexCoord6;
		case VertexAttribute::TexCoord7:
			return bgfx::Attrib::TexCoord7;
		default:
			throw CBException("Invalid toBgfx case");
		}
	}

	typedef EnumInfo<VertexAttribute> VertexAttributeInfo;
	static inline VertexAttributeInfo sVertexAttributeInfo{"VertexAttribute", CoreCC, 'gfxV'};
	static inline Type VertexAttributeType = Type::Enum(CoreCC, 'gfxV');
	static inline Type VertexAttributeSeqType = Type::SeqOf(VertexAttributeType);

	static inline Types VerticesSeqTypes{{CoreInfo::FloatType, CoreInfo::Float2Type, CoreInfo::Float3Type, CoreInfo::ColorType, CoreInfo::IntType}};
	static inline Type VerticesSeq = Type::SeqOf(VerticesSeqTypes);
	// TODO support other topologies then triangle list
	static inline Types IndicesSeqTypes{{CoreInfo::Int3Type}};
	static inline Type IndicesSeq = Type::SeqOf(IndicesSeqTypes);
	static inline Types InputTableTypes{{VerticesSeq, IndicesSeq}};
	static inline std::array<CBString, 2> InputTableKeys{"Vertices", "Indices"};
	static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

	static CBTypesInfo inputTypes() { return InputTable; }
	static CBTypesInfo outputTypes() { return ModelHandle::ObjType; }

	static inline Parameters params{
		{"Layout", CBCCSTR("The vertex layout of this model."), {VertexAttributeSeqType}},
		{"Dynamic",
		 CBCCSTR("If the model is dynamic and will be optimized to change as "
				 "often as every frame."),
		 {CoreInfo::BoolType}},
		{"CullMode", CBCCSTR("Triangles facing the specified direction are not drawn."), {Enums::CullModeType}}};

	static CBParametersInfo parameters() { return params; }

	std::vector<Var> _layout;
	bgfx::VertexLayout _blayout;
	std::vector<CBType> _expectedTypes;
	size_t _lineElems{0};
	size_t _elemSize{0};
	bool _dynamic{false};
	ModelHandle *_output{nullptr};
	Enums::CullMode _cullMode{Enums::CullMode::Back};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_layout = std::vector<Var>(Var(value));
			break;
		case 1:
			_dynamic = value.payload.boolValue;
			break;
		case 2:
			_cullMode = Enums::CullMode(value.payload.enumValue);
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_layout);
		case 1:
			return Var(_dynamic);
		case 2:
			return Var::Enum(_cullMode, CoreCC, Enums::CullModeCC);
		default:
			return Var::Empty;
		}
	}

	void cleanup() {
		if (_output) {
			ModelHandle::Var.Release(_output);
			_output = nullptr;
		}
	}

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);

		if (_dynamic) {
			OVERRIDE_ACTIVATE(data, activateDynamic);
		}
		else {
			OVERRIDE_ACTIVATE(data, activateStatic);
		}

		_expectedTypes.clear();
		_elemSize = 0;
		bgfx::VertexLayout layout;
		layout.begin();
		for (auto &entry : _layout) {
			auto e = VertexAttribute(entry.payload.enumValue);
			auto elems = 0;
			auto atype = bgfx::AttribType::Float;
			auto normalized = false;
			switch (e) {
			case VertexAttribute::Skip:
				layout.skip(1);
				_expectedTypes.emplace_back(CBType::None);
				_elemSize += 1;
				continue;
			case VertexAttribute::Position:
			case VertexAttribute::Normal:
			case VertexAttribute::Tangent3:
			case VertexAttribute::Bitangent: {
				elems = 3;
				atype = bgfx::AttribType::Float;
				_expectedTypes.emplace_back(CBType::Float3);
				_elemSize += 12;
			} break;
			case VertexAttribute::Tangent4:
			// w includes handedness
			case VertexAttribute::Weight: {
				elems = 4;
				atype = bgfx::AttribType::Float;
				_expectedTypes.emplace_back(CBType::Float4);
				_elemSize += 16;
			} break;
			case VertexAttribute::TexCoord0:
			case VertexAttribute::TexCoord1:
			case VertexAttribute::TexCoord2:
			case VertexAttribute::TexCoord3:
			case VertexAttribute::TexCoord4:
			case VertexAttribute::TexCoord5:
			case VertexAttribute::TexCoord6:
			case VertexAttribute::TexCoord7: {
				elems = 2;
				atype = bgfx::AttribType::Float;
				_expectedTypes.emplace_back(CBType::Float2);
				_elemSize += 8;
			} break;
			case VertexAttribute::Color0:
			case VertexAttribute::Color1:
			case VertexAttribute::Color2:
			case VertexAttribute::Color3: {
				elems = 4;
				normalized = true;
				atype = bgfx::AttribType::Uint8;
				_expectedTypes.emplace_back(CBType::Color);
				_elemSize += 4;
			} break;
			case VertexAttribute::Indices: {
				elems = 4;
				atype = bgfx::AttribType::Uint8;
				_expectedTypes.emplace_back(CBType::Int4);
				_elemSize += 4;
			} break;
			default:
				throw ComposeError("Invalid VertexAttribute");
			}
			layout.add(toBgfx(e), elems, atype, normalized);
		}
		layout.end();

		_blayout = layout;
		_lineElems = _expectedTypes.size();

		return ModelHandle::ObjType;
	}

	CBVar activateStatic(CBContext *context, const CBVar &input) {
		// this is the most efficient way to find items in table
		// without hashing and possible allocations etc
		CBTable table = input.payload.tableValue;
		CBTableIterator it;
		table.api->tableGetIterator(table, &it);
		CBVar vertices{};
		CBVar indices{};
		while (true) {
			CBString k;
			CBVar v;
			if (!table.api->tableNext(table, &it, &k, &v)) break;

			switch (k[0]) {
			case 'V':
				vertices = v;
				break;
			case 'I':
				indices = v;
				break;
			default:
				break;
			}
		}

		assert(vertices.valueType == Seq && indices.valueType == Seq);

		if (!_output) {
			_output = ModelHandle::Var.New();
		}
		else {
			// in the case of static model, we destroy it
			// well, release, some variable might still be using it
			ModelHandle::Var.Release(_output);
			_output = ModelHandle::Var.New();
		}

		ModelHandle::StaticModel model;

		const auto nElems = size_t(vertices.payload.seqValue.len);
		if ((nElems % _lineElems) != 0) {
			throw ActivationError("Invalid amount of vertex buffer elements");
		}
		const auto line = nElems / _lineElems;
		const auto size = line * _elemSize;

		auto buffer = bgfx::alloc(size);
		size_t offset = 0;

		for (size_t i = 0; i < nElems; i += _lineElems) {
			size_t idx = 0;
			for (auto expected : _expectedTypes) {
				const auto &elem = vertices.payload.seqValue.elements[i + idx];
				// allow colors to be also Int
				if (expected == CBType::Color && elem.valueType == CBType::Int) {
					expected = CBType::Int;
				}

				if (elem.valueType != expected) {
					CBLOG_ERROR("Expected vertex element of type: {} got instead: {}", type2Name(expected), type2Name(elem.valueType));
					throw ActivationError("Invalid vertex buffer element type");
				}

				switch (elem.valueType) {
				case CBType::None:
					offset += 1;
					break;
				case CBType::Float2:
					memcpy(buffer->data + offset, &elem.payload.float2Value, sizeof(float) * 2);
					offset += sizeof(float) * 2;
					break;
				case CBType::Float3:
					memcpy(buffer->data + offset, &elem.payload.float3Value, sizeof(float) * 3);
					offset += sizeof(float) * 3;
					break;
				case CBType::Float4:
					memcpy(buffer->data + offset, &elem.payload.float4Value, sizeof(float) * 4);
					offset += sizeof(float) * 4;
					break;
				case CBType::Int4: {
					if (elem.payload.int4Value[0] < 0 || elem.payload.int4Value[0] > 255 || elem.payload.int4Value[1] < 0 || elem.payload.int4Value[1] > 255 ||
						elem.payload.int4Value[2] < 0 || elem.payload.int4Value[2] > 255 || elem.payload.int4Value[3] < 0 || elem.payload.int4Value[3] > 255) {
						throw ActivationError("Int4 value must be between 0 and 255 for a vertex buffer");
					}
					CBColor value;
					value.r = uint8_t(elem.payload.int4Value[0]);
					value.g = uint8_t(elem.payload.int4Value[1]);
					value.b = uint8_t(elem.payload.int4Value[2]);
					value.a = uint8_t(elem.payload.int4Value[3]);
					memcpy(buffer->data + offset, &value.r, 4);
					offset += 4;
				} break;
				case CBType::Color:
					memcpy(buffer->data + offset, &elem.payload.colorValue.r, 4);
					offset += 4;
					break;
				case CBType::Int: {
					uint32_t intColor = uint32_t(elem.payload.intValue);
					memcpy(buffer->data + offset, &intColor, 4);
					offset += 4;
				} break;
				default:
					throw ActivationError("Invalid type for a vertex buffer");
				}
				idx++;
			}
		}

		const auto nindices = size_t(indices.payload.seqValue.len);
		uint16_t flags = BGFX_BUFFER_NONE;
		size_t isize = 0;
		bool compressed = true;
		if (nindices > UINT16_MAX) {
			flags |= BGFX_BUFFER_INDEX32;
			isize = nindices * sizeof(uint32_t) * 3; // int3s
			compressed = false;
		}
		else {
			isize = nindices * sizeof(uint16_t) * 3; // int3s
		}

		auto ibuffer = bgfx::alloc(isize);
		offset = 0;

		auto &selems = indices.payload.seqValue.elements;
		for (size_t i = 0; i < nindices; i++) {
			const static Var min{0, 0, 0};
			const Var max{int(nindices * 3), int(nindices * 3), int(nindices * 3)};
			if (compressed) {
				if (selems[i] < min || selems[i] > max) {
					throw ActivationError("Vertex index out of range");
				}
				const uint16_t t[] = {
					uint16_t(selems[i].payload.int3Value[0]), uint16_t(selems[i].payload.int3Value[1]), uint16_t(selems[i].payload.int3Value[2])};
				memcpy(ibuffer->data + offset, t, sizeof(uint16_t) * 3);
				offset += sizeof(uint16_t) * 3;
			}
			else {
				if (selems[i] < min || selems[i] > max) {
					throw ActivationError("Vertex index out of range");
				}
				memcpy(ibuffer->data + offset, &selems[i].payload.int3Value, sizeof(uint32_t) * 3);
				offset += sizeof(uint32_t) * 3;
			}
		}

		model.vb = bgfx::createVertexBuffer(buffer, _blayout);
		model.ib = bgfx::createIndexBuffer(ibuffer, flags);
		_output->cullMode = _cullMode;

		_output->model = model;

		return ModelHandle::Var.Get(_output);
	}

	CBVar activateDynamic(CBContext *context, const CBVar &input) { return ModelHandle::Var.Get(_output); }

	CBVar activate(CBContext *context, const CBVar &input) { throw ActivationError("Invalid activation function"); }
};

struct CameraBase : public BaseConsumer {
	// TODO must expose view, proj and viewproj in a stack fashion probably
	static inline Types InputTableTypes{{CoreInfo::Float3Type, CoreInfo::Float3Type}};
	static inline std::array<CBString, 2> InputTableKeys{"Position", "Target"};
	static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);
	static inline Types InputTypes{{CoreInfo::NoneType, InputTable, CoreInfo::Float4x4Type}};

	static CBTypesInfo inputTypes() { return InputTypes; }
	static CBTypesInfo outputTypes() { return InputTypes; }

	int _width = 0;
	int _height = 0;
	int _offsetX = 0;
	int _offsetY = 0;

	static inline Parameters params{
		{"OffsetX", CBCCSTR("The horizontal offset of the viewport."), {CoreInfo::IntType}},
		{"OffsetY", CBCCSTR("The vertical offset of the viewport."), {CoreInfo::IntType}},
		{"Width",
		 CBCCSTR("The width of the viewport, if 0 it will use the full current "
				 "view width."),
		 {CoreInfo::IntType}},
		{"Height",
		 CBCCSTR("The height of the viewport, if 0 it will use the full current "
				 "view height."),
		 {CoreInfo::IntType}}};

	static CBParametersInfo parameters() { return params; }

	void cleanup() { BaseConsumer::_cleanup(); }

	void warmup(CBContext *context) { BaseConsumer::_warmup(context); }

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		return data.inputType;
	}

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_offsetX = int(value.payload.intValue);
			break;
		case 1:
			_offsetY = int(value.payload.intValue);
			break;
		case 2:
			_width = int(value.payload.intValue);
			break;
		case 3:
			_height = int(value.payload.intValue);
			break;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_offsetX);
		case 1:
			return Var(_offsetY);
		case 2:
			return Var(_width);
		case 3:
			return Var(_height);
		default:
			throw InvalidParameterIndex();
		}
	}
};

struct Camera : public CameraBase {
	float _near = 0.1;
	float _far = 1000.0;
	float _fov = 60.0;

	static inline Parameters params{
		{{"Near", CBCCSTR("The distance from the near clipping plane."), {CoreInfo::FloatType}},
		 {"Far", CBCCSTR("The distance from the far clipping plane."), {CoreInfo::FloatType}},
		 {"FieldOfView", CBCCSTR("The field of view of the camera."), {CoreInfo::FloatType}}},
		CameraBase::params};

	static CBParametersInfo parameters() { return params; }

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_near = float(value.payload.floatValue);
			break;
		case 1:
			_far = float(value.payload.floatValue);
			break;
		case 2:
			_fov = float(value.payload.floatValue);
			break;
		default:
			CameraBase::setParam(index - 3, value);
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_near);
		case 1:
			return Var(_far);
		case 2:
			return Var(_fov);
		default:
			return CameraBase::getParam(index - 3);
		}
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frame = getFrameRenderer();

		auto &currentView = frame.getCurrentView();

		aligned_array<float, 16> view;
		bool hasView;
		switch (input.valueType) {
		case CBType::Table: {
			// this is the most efficient way to find items in table
			// without hashing and possible allocations etc
			CBTable table = input.payload.tableValue;
			CBTableIterator it;
			table.api->tableGetIterator(table, &it);
			CBVar position{};
			CBVar target{};
			while (true) {
				CBString k;
				CBVar v;
				if (!table.api->tableNext(table, &it, &k, &v)) break;

				switch (k[0]) {
				case 'P':
					position = v;
					break;
				case 'T':
					target = v;
					break;
				default:
					break;
				}
			}

			assert(position.valueType == CBType::Float3 && target.valueType == CBType::Float3);

			bx::Vec3 *bp = reinterpret_cast<bx::Vec3 *>(&position.payload.float3Value);
			bx::Vec3 *bt = reinterpret_cast<bx::Vec3 *>(&target.payload.float3Value);
			bx::mtxLookAt(view.data(), *bp, *bt, {0.0, 1.0, 0.0}, bx::Handness::Right);

			hasView = true;
		} break;

		case CBType::Seq: {
			auto *mat = reinterpret_cast<float *>(view.data());
			auto &vmat = input;

			if (vmat.payload.seqValue.len != 4) {
				throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
			}

			memcpy(&mat[0], &vmat.payload.seqValue.elements[0].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[4], &vmat.payload.seqValue.elements[1].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[8], &vmat.payload.seqValue.elements[2].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[12], &vmat.payload.seqValue.elements[3].payload.float4Value, sizeof(float) * 4);

			hasView = true;
		} break;

		default:
			hasView = false;
			break;
		}

		int width = _width != 0 ? _width : currentView.viewport.width;
		int height = _height != 0 ? _height : currentView.viewport.height;

		aligned_array<float, 16> proj;
		bx::mtxProj(proj.data(), _fov, float(width) / float(height), _near, _far, bgfx::getCaps()->homogeneousDepth, bx::Handness::Right);

		if constexpr (gfx::CurrentRenderer == gfx::Renderer::OpenGL) {
			// workaround for flipped Y render to textures
			if (currentView.id > 0) {
				proj[5] = -proj[5];
				proj[8] = -proj[8];
				proj[9] = -proj[9];
			}
		}

		bgfx::setViewTransform(currentView.id, hasView ? view.data() : nullptr, proj.data());
		bgfx::setViewRect(currentView.id, uint16_t(_offsetX), uint16_t(_offsetY), uint16_t(width), uint16_t(height));
		if (currentView.id == 0) {
			// also set picking view
			bgfx::setViewTransform(gfx::PickingViewId, hasView ? view.data() : nullptr, proj.data());
			bgfx::setViewRect(gfx::PickingViewId, uint16_t(_offsetX), uint16_t(_offsetY), uint16_t(width), uint16_t(height));
		}

		// set viewport params
		currentView.viewport.x = _offsetX;
		currentView.viewport.y = _offsetY;
		currentView.viewport.width = width;
		currentView.viewport.height = height;

		// populate view matrices
		if (hasView) {
			currentView.view = Mat4::FromArray(view);
		}
		else {
			currentView.view = linalg::identity;
		}
		currentView.proj = Mat4::FromArray(proj);
		currentView.invalidate();

		return input;
	}
};

struct CameraOrtho : public CameraBase {
	float _near = 0.0;
	float _far = 100.0;
	float _left = 0.0;
	float _right = 1.0;
	float _bottom = 1.0;
	float _top = 0.0;

	static inline Parameters params{
		{{"Left", CBCCSTR("The left of the projection."), {CoreInfo::FloatType}},
		 {"Right", CBCCSTR("The right of the projection."), {CoreInfo::FloatType}},
		 {"Bottom", CBCCSTR("The bottom of the projection."), {CoreInfo::FloatType}},
		 {"Top", CBCCSTR("The top of the projection."), {CoreInfo::FloatType}},
		 {"Near", CBCCSTR("The distance from the near clipping plane."), {CoreInfo::FloatType}},
		 {"Far", CBCCSTR("The distance from the far clipping plane."), {CoreInfo::FloatType}}},
		CameraBase::params};

	static CBParametersInfo parameters() { return params; }

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_left = float(value.payload.floatValue);
			break;
		case 1:
			_right = float(value.payload.floatValue);
			break;
		case 2:
			_bottom = float(value.payload.floatValue);
			break;
		case 3:
			_top = float(value.payload.floatValue);
			break;
		case 4:
			_near = float(value.payload.floatValue);
			break;
		case 5:
			_far = float(value.payload.floatValue);
			break;
		default:
			CameraBase::setParam(index - 6, value);
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_left);
		case 1:
			return Var(_right);
		case 2:
			return Var(_bottom);
		case 3:
			return Var(_top);
		case 4:
			return Var(_near);
		case 5:
			return Var(_far);
		default:
			return CameraBase::getParam(index - 6);
		}
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		auto &currentView = frameRenderer.getCurrentView();

		aligned_array<float, 16> view;
		if (input.valueType == CBType::Table) {
			// this is the most efficient way to find items in table
			// without hashing and possible allocations etc
			CBTable table = input.payload.tableValue;
			CBTableIterator it;
			table.api->tableGetIterator(table, &it);
			CBVar position{};
			CBVar target{};
			while (true) {
				CBString k;
				CBVar v;
				if (!table.api->tableNext(table, &it, &k, &v)) break;

				switch (k[0]) {
				case 'P':
					position = v;
					break;
				case 'T':
					target = v;
					break;
				default:
					break;
				}
			}

			assert(position.valueType == CBType::Float3 && target.valueType == CBType::Float3);

			bx::Vec3 *bp = reinterpret_cast<bx::Vec3 *>(&position.payload.float3Value);
			bx::Vec3 *bt = reinterpret_cast<bx::Vec3 *>(&target.payload.float3Value);
			bx::mtxLookAt(view.data(), *bp, *bt);
		}

		int width = _width != 0 ? _width : currentView.viewport.width;
		int height = _height != 0 ? _height : currentView.viewport.height;

		aligned_array<float, 16> proj;
		bx::mtxOrtho(proj.data(), _left, _right, _bottom, _top, _near, _far, 0.0, bgfx::getCaps()->homogeneousDepth, bx::Handness::Right);

		if constexpr (gfx::CurrentRenderer == gfx::Renderer::OpenGL) {
			// workaround for flipped Y render to textures
			if (currentView.id > 0) {
				proj[5] = -proj[5];
				proj[8] = -proj[8];
				proj[9] = -proj[9];
			}
		}

		bgfx::setViewTransform(currentView.id, input.valueType == CBType::Table ? view.data() : nullptr, proj.data());
		bgfx::setViewRect(currentView.id, uint16_t(_offsetX), uint16_t(_offsetY), uint16_t(width), uint16_t(height));
		if (currentView.id == 0) {
			// also set picking view
			bgfx::setViewTransform(gfx::PickingViewId, input.valueType == CBType::Table ? view.data() : nullptr, proj.data());
			bgfx::setViewRect(gfx::PickingViewId, uint16_t(_offsetX), uint16_t(_offsetY), uint16_t(width), uint16_t(height));
		}

		// set viewport params
		currentView.viewport.x = _offsetX;
		currentView.viewport.y = _offsetY;
		currentView.viewport.width = width;
		currentView.viewport.height = height;

		// populate view matrices
		if (input.valueType != CBType::Table) {
			currentView.view = linalg::identity;
		}
		else {
			currentView.view = Mat4::FromArray(view);
		}
		currentView.proj = Mat4::FromArray(proj);
		currentView.invalidate();

		return input;
	}
};

struct Draw : public BaseConsumer {
	// a matrix (in the form of 4 float4s)
	// or multiple matrices (will draw multiple times, instanced TODO)
	static CBTypesInfo inputTypes() { return CoreInfo::Float4x4Types; }
	static CBTypesInfo outputTypes() { return CoreInfo::Float4x4Types; }

	// keep in mind that bgfx does its own sorting, so we don't need to make this
	// block way too smart
	static inline Parameters params{
		{"Shader", CBCCSTR("The shader program to use for this draw."), {ShaderHandle::ObjType, ShaderHandle::VarType}},
		{"Textures",
		 CBCCSTR("A texture or the textures to pass to the shaders."),
		 {Texture::ObjType, Texture::VarType, Texture::SeqType, Texture::VarSeqType, CoreInfo::NoneType}},
		{"Model",
		 CBCCSTR("The model to draw. If no model is specified a full screen quad "
				 "will be used."),
		 {ModelHandle::ObjType, ModelHandle::VarType, CoreInfo::NoneType}},
		{"Blend",
		 CBCCSTR("The blend state table to describe and enable blending. If it's a "
				 "single table the state will be assigned to both RGB and Alpha, if "
				 "2 tables are specified, the first will be RGB, the second Alpha."),
		 {CoreInfo::NoneType, Enums::BlendTable, Enums::BlendTableSeq}}};

	static CBParametersInfo parameters() { return params; }

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_shader = value;
			break;
		case 1:
			_textures = value;
			break;
		case 2:
			_model = value;
			break;
		case 3:
			_blend = value;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _shader;
		case 1:
			return _textures;
		case 2:
			return _model;
		case 3:
			return _blend;
		default:
			return Var::Empty;
		}
	}

	ParamVar _shader{};
	ParamVar _textures{};
	ParamVar _model{};
	OwnedVar _blend{};
	std::array<CBExposedTypeInfo, 5> _requiring;

	CBExposedTypesInfo requiredVariables() {
		int idx = 0;
		_requiring[idx] = BaseConsumer::ContextInfo;
		idx++;

		if (_shader.isVariable()) {
			_requiring[idx].name = _shader.variableName();
			_requiring[idx].help = CBCCSTR("The required shader.");
			_requiring[idx].exposedType = ShaderHandle::ObjType;
			idx++;
		}
		if (_textures.isVariable()) {
			_requiring[idx].name = _textures.variableName();
			_requiring[idx].help = CBCCSTR("The required texture.");
			_requiring[idx].exposedType = Texture::ObjType;
			idx++;
			_requiring[idx].name = _textures.variableName();
			_requiring[idx].help = CBCCSTR("The required textures.");
			_requiring[idx].exposedType = Texture::SeqType;
			idx++;
		}
		if (_model.isVariable()) {
			_requiring[idx].name = _model.variableName();
			_requiring[idx].help = CBCCSTR("The required model.");
			_requiring[idx].exposedType = ModelHandle::ObjType;
			idx++;
		}
		return {_requiring.data(), uint32_t(idx), 0};
	}

	void warmup(CBContext *context) {
		BaseConsumer::_warmup(context);

		_shader.warmup(context);
		_textures.warmup(context);
		_model.warmup(context);
	}

	void cleanup() {
		_shader.cleanup();
		_textures.cleanup();
		_model.cleanup();

		BaseConsumer::_cleanup();
	}

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);

		if (data.inputType.seqTypes.elements[0].basicType == CBType::Seq) {
			// Instanced rendering
			OVERRIDE_ACTIVATE(data, activate);
		}
		else {
			// Single rendering
			OVERRIDE_ACTIVATE(data, activateSingle);
		}
		return data.inputType;
	}

	struct PosTexCoord0Vertex {
		float m_x;
		float m_y;
		float m_z;
		float m_u;
		float m_v;

		static void init() {
			ms_layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float).end();
		}

		static inline bgfx::VertexLayout ms_layout;
	};

	static inline bool LayoutSetup{false};

	void setup() {
		if (!LayoutSetup) {
			PosTexCoord0Vertex::init();
			LayoutSetup = true;
		}
	}

	void screenSpaceQuad(float width = 1.0f, float height = 1.0f) {
		if (3 == bgfx::getAvailTransientVertexBuffer(3, PosTexCoord0Vertex::ms_layout)) {
			bgfx::TransientVertexBuffer vb;
			bgfx::allocTransientVertexBuffer(&vb, 3, PosTexCoord0Vertex::ms_layout);
			PosTexCoord0Vertex *vertex = (PosTexCoord0Vertex *)vb.data;

			const float zz = 0.0f;

			const float minx = -width;
			const float maxx = width;
			const float miny = 0.0f;
			const float maxy = height * 2.0f;

			const float minu = -1.0f;
			const float maxu = 1.0f;

			float minv = 0.0f;
			float maxv = 2.0f;

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

			bgfx::setVertexBuffer(0, &vb);
		}
	}

	struct Drawable : public gfx::IDrawable {
		Drawable(CBChain *drawableChain) : _chain(drawableChain) {}
		virtual ~Drawable() {}
		CBChain *getChain() override { return _chain; }

	private:
		CBChain *_chain;
	};

	// use deque for stable memory location
	std::deque<Drawable> _frameDrawables;

	void render(gfx::FrameRenderer &frameRenderer, uint32_t id) {
		auto shader = reinterpret_cast<ShaderHandle *>(_shader.get().payload.objectValue);
		assert(shader);
		auto model = reinterpret_cast<ModelHandle *>(_model.get().payload.objectValue);

		const auto &currentView = frameRenderer.getCurrentView();

		uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;

		const auto getBlends = [&](CBVar &blend) {
			uint64_t s = 0;
			uint64_t d = 0;
			uint64_t o = 0;
			ForEach(blend.payload.tableValue, [&](auto key, auto &val) {
				if (key[0] == 'S')
					s = Enums::BlendToBgfx(val.payload.enumValue);
				else if (key[0] == 'D')
					d = Enums::BlendToBgfx(val.payload.enumValue);
				else if (key[0] == 'O')
					o = Enums::BlendOpToBgfx(val.payload.enumValue);
			});
			return std::make_tuple(s, d, o);
		};

		if (_blend.valueType == Table) {
			const auto [s, d, o] = getBlends(_blend);
			state |= BGFX_STATE_BLEND_FUNC(s, d) | BGFX_STATE_BLEND_EQUATION(o);
		}
		else if (_blend.valueType == Seq) {
			assert(_blend.payload.seqValue.len == 2);
			const auto [sc, dc, oc] = getBlends(_blend.payload.seqValue.elements[0]);
			const auto [sa, da, oa] = getBlends(_blend.payload.seqValue.elements[1]);
			state |= BGFX_STATE_BLEND_FUNC_SEPARATE(sc, dc, sa, da) | BGFX_STATE_BLEND_EQUATION_SEPARATE(oc, oa);
		}

		if (model) {
			std::visit(
				[](auto &m) {
					// Set vertex and index buffer.
					bgfx::setVertexBuffer(0, m.vb);
					bgfx::setIndexBuffer(m.ib);
				},
				model->model);
			state |= BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS;

			switch (model->cullMode) {
			case Enums::CullMode::Front: {
				if constexpr (gfx::CurrentRenderer == gfx::Renderer::OpenGL) {
					// workaround for flipped Y render to textures
					if (currentView.id > 0) {
						state |= BGFX_STATE_CULL_CW;
					}
					else {
						state |= BGFX_STATE_CULL_CCW;
					}
				}
				else {
					state |= BGFX_STATE_CULL_CCW;
				}
			} break;
			case Enums::CullMode::Back: {
				if constexpr (gfx::CurrentRenderer == gfx::Renderer::OpenGL) {
					// workaround for flipped Y render to textures
					if (currentView.id > 0) {
						state |= BGFX_STATE_CULL_CCW;
					}
					else {
						state |= BGFX_STATE_CULL_CW;
					}
				}
				else {
					state |= BGFX_STATE_CULL_CW;
				}
			} break;
			default:
				break;
			}
		}
		else {
			screenSpaceQuad();
		}

		// set state, (it's auto reset after submit)
		bgfx::setState(state);

		gfx::Context &gfxContext = getGfxContext();

		auto vtextures = _textures.get();
		if (vtextures.valueType == CBType::Object) {
			auto texture = reinterpret_cast<Texture *>(vtextures.payload.objectValue);
			bgfx::setTexture(0, gfxContext.getSampler(0), texture->handle);
		}
		else if (vtextures.valueType == CBType::Seq) {
			auto textures = vtextures.payload.seqValue;
			for (uint32_t i = 0; i < textures.len; i++) {
				auto texture = reinterpret_cast<Texture *>(textures.elements[i].payload.objectValue);
				bgfx::setTexture(uint8_t(i), gfxContext.getSampler(i), texture->handle);
			}
		}

		bool picking = currentView.id == 0 && frameRenderer.isPicking() && model;

		// Submit primitive for rendering to the current view.
		bgfx::submit(currentView.id, shader->handle, 0, picking ? 0 : BGFX_DISCARD_ALL);

		if (picking) {
			float fid[4];
			fid[0] = ((id >> 0) & 0xff) / 255.0f;
			fid[1] = ((id >> 8) & 0xff) / 255.0f;
			fid[2] = ((id >> 16) & 0xff) / 255.0f;
			fid[3] = ((id >> 24) & 0xff) / 255.0f;
			bgfx::setUniform(gfxContext.getPickingUniform(), fid, 1);
			bgfx::submit(gfx::PickingViewId, shader->pickingHandle, 0, BGFX_DISCARD_ALL);
		}
	}

	CBVar activateSingle(CBContext *context, const CBVar &input) {
		if (input.payload.seqValue.len != 4) {
			throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
		}

		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		_frameDrawables.clear();

		bgfx::Transform t;
		// using allocTransform to avoid an extra copy
		auto idx = bgfx::allocTransform(&t, 1);
		memcpy(&t.data[0], &input.payload.seqValue.elements[0].payload.float4Value, sizeof(float) * 4);
		memcpy(&t.data[4], &input.payload.seqValue.elements[1].payload.float4Value, sizeof(float) * 4);
		memcpy(&t.data[8], &input.payload.seqValue.elements[2].payload.float4Value, sizeof(float) * 4);
		memcpy(&t.data[12], &input.payload.seqValue.elements[3].payload.float4Value, sizeof(float) * 4);
		bgfx::setTransform(idx, 1);

		auto id = frameRenderer.addFrameDrawable(&_frameDrawables.emplace_back(context->currentChain()));

		render(frameRenderer, id);

		return input;
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		const auto instances = input.payload.seqValue.len;
		constexpr uint16_t stride = 64; // matrix 4x4
		if (bgfx::getAvailInstanceDataBuffer(instances, stride) != instances) {
			throw ActivationError("Instance buffer overflow");
		}

		bgfx::InstanceDataBuffer idb;
		bgfx::allocInstanceDataBuffer(&idb, instances, stride);
		uint8_t *data = idb.data;

		auto currentChain = context->currentChain();

		for (uint32_t i = 0; i < instances; i++) {
			float *mat = reinterpret_cast<float *>(data);
			CBVar &vmat = input.payload.seqValue.elements[i];

			if (vmat.payload.seqValue.len != 4) {
				throw ActivationError("Invalid Matrix4x4 input, should Float4 x 4.");
			}

			memcpy(&mat[0], &vmat.payload.seqValue.elements[0].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[4], &vmat.payload.seqValue.elements[1].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[8], &vmat.payload.seqValue.elements[2].payload.float4Value, sizeof(float) * 4);
			memcpy(&mat[12], &vmat.payload.seqValue.elements[3].payload.float4Value, sizeof(float) * 4);

			data += stride;
		}

		bgfx::setInstanceDataBuffer(&idb);

		auto id = frameRenderer.addFrameDrawable(&_frameDrawables.emplace_back(currentChain));

		render(frameRenderer, id);

		return input;
	}
};

struct RenderTarget : public BaseConsumer {
	static inline Parameters params{
		{"Width", CBCCSTR("The width of the texture to render."), {CoreInfo::IntType}},
		{"Height", CBCCSTR("The height of the texture to render."), {CoreInfo::IntType}},
		{"Contents", CBCCSTR("The blocks expressing the contents to render."), {CoreInfo::BlocksOrNone}},
		{"GUI",
		 CBCCSTR("If this render target should be able to render GUI blocks "
				 "within. If false any GUI block inside will be rendered on the "
				 "top level surface."),
		 {CoreInfo::BoolType}},
		{"ClearColor",
		 CBCCSTR("The color to use to clear the backbuffer at the beginning of a "
				 "new frame."),
		 {CoreInfo::ColorType}}};
	static CBParametersInfo parameters() { return params; }

	static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }

	BlocksVar _blocks;
	int _width{256};
	int _height{256};
	bool _gui{false};
	bgfx::FrameBufferHandle _framebuffer = BGFX_INVALID_HANDLE;
	CBColor _clearColor{0x30, 0x30, 0x30, 0xFF};

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_width = int(value.payload.intValue);
			break;
		case 1:
			_height = int(value.payload.intValue);
			break;
		case 2:
			_blocks = value;
			break;
		case 3:
			_gui = value.payload.boolValue;
			break;
		case 4:
			_clearColor = value.payload.colorValue;
			break;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_width);
		case 1:
			return Var(_height);
		case 2:
			return _blocks;
		case 3:
			return Var(_gui);
		case 4:
			return Var(_clearColor);
		default:
			throw InvalidParameterIndex();
		}
	}
};

struct RenderTexture : public RenderTarget {
	// to make it simple our render textures are always 16bit linear
	// TODO we share same size/formats depth buffers, expose only color part

	static CBTypesInfo outputTypes() { return Texture::ObjType; }

	Texture *_texture{nullptr}; // the color part we expose
	Texture _depth{};

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		_blocks.compose(data);
		return Texture::ObjType;
	}

	void warmup(CBContext *context) {
		BaseConsumer::_warmup(context);

		_texture = Texture::Var.New();
		_texture->handle = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT);
		_texture->width = _width;
		_texture->height = _height;
		_texture->channels = 4;
		_texture->bpp = 2;

		_depth.handle = bgfx::createTexture2D(_width, _height, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT | BGFX_TEXTURE_RT_WRITE_ONLY);

		bgfx::TextureHandle textures[] = {_texture->handle, _depth.handle};
		_framebuffer = bgfx::createFrameBuffer(2, textures, false);

		_blocks.warmup(context);
	}

	void cleanup() {
		_blocks.cleanup();

		if (_framebuffer.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(_framebuffer);
			_framebuffer.idx = bgfx::kInvalidHandle;
		}

		if (_depth.handle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(_depth.handle);
			_depth.handle.idx = bgfx::kInvalidHandle;
		}

		if (_texture) {
			Texture::Var.Release(_texture);
			_texture = nullptr;
		}

		BaseConsumer::_cleanup();
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		bgfx::ViewId viewId = getFrameRenderer().nextViewId();

		bgfx::setViewRect(viewId, 0, 0, _width, _height);
		bgfx::setViewClear(viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL, Var(_clearColor).colorToInt(), 1.0f, 0);

		frameRenderer.pushView(gfx::View(viewId, gfx::Rect(_width, _height), _framebuffer));
		DEFER({ frameRenderer.popView(); });

		bgfx::setViewFrameBuffer(viewId, _framebuffer);

		CBVar output{};
		_blocks.activate(context, input, output);

		return Texture::Var.Get(_texture);
	}
};

struct SetUniform : public BaseConsumer {
	static inline Types InputTypes{
		{CoreInfo::Float4Type, CoreInfo::Float4x4Type, CoreInfo::Float3x3Type, CoreInfo::Float4SeqType, CoreInfo::Float4x4SeqType, CoreInfo::Float3x3SeqType}};

	static CBTypesInfo inputTypes() { return InputTypes; }
	static CBTypesInfo outputTypes() { return InputTypes; }

	std::string _name;
	bgfx::UniformType::Enum _type;
	bool _isArray{false};
	int _elems{1};
	CBTypeInfo _expectedType;
	bgfx::UniformHandle _handle = BGFX_INVALID_HANDLE;
	std::vector<float> _scratch;

	static inline Parameters Params{
		{"Name",
		 CBCCSTR("The name of the uniform shader variable. Uniforms are so named "
				 "because they do not change from one shader invocation to the "
				 "next within a particular rendering call thus their value is "
				 "uniform among all invocations. Uniform names are unique."),
		 {CoreInfo::StringType}},
		{"MaxSize",
		 CBCCSTR("If the input contains multiple values, the maximum expected "
				 "size of the input."),
		 {CoreInfo::IntType}}};

	static CBParametersInfo parameters() { return Params; }

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_name = value.payload.stringValue;
			break;
		case 1:
			_elems = value.payload.intValue;
			break;
		default:
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return Var(_name);
		case 1:
			return Var(_elems);
		default:
			throw InvalidParameterIndex();
		}
	}

	bgfx::UniformType::Enum findSingleType(const CBTypeInfo &t) {
		if (t.basicType == CBType::Float4) {
			return bgfx::UniformType::Vec4;
		}
		else if (t.basicType == CBType::Seq) {
			if (t.fixedSize == 4 && t.seqTypes.elements[0].basicType == CBType::Float4) {
				return bgfx::UniformType::Mat4;
			}
			else if (t.fixedSize == 3 && t.seqTypes.elements[0].basicType == CBType::Float3) {
				return bgfx::UniformType::Mat3;
			}
		}
		throw ComposeError("Invalid input type for GFX.SetUniform");
	}

	CBTypeInfo compose(const CBInstanceData &data) {
		_expectedType = data.inputType;
		if (_elems == 1) {
			_type = findSingleType(data.inputType);
			_isArray = false;
		}
		else {
			if (data.inputType.basicType != CBType::Seq || data.inputType.seqTypes.len == 0) throw ComposeError("Invalid input type for GFX.SetUniform");
			_type = findSingleType(data.inputType.seqTypes.elements[0]);
			_isArray = true;
		}
		return data.inputType;
	}

	void warmup(CBContext *context) { _handle = bgfx::createUniform(_name.c_str(), _type, !_isArray ? 1 : _elems); }

	void cleanup() {
		if (_handle.idx != bgfx::kInvalidHandle) {
			bgfx::destroy(_handle);
			_handle.idx = bgfx::kInvalidHandle;
		}
	}

	void fillElement(const CBVar &elem, int &offset) {
		if (elem.valueType == CBType::Float4) {
			memcpy(_scratch.data() + offset, &elem.payload.float4Value, sizeof(float) * 4);
			offset += 4;
		}
		else {
			// Seq
			if (_type == bgfx::UniformType::Mat3) {
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[0], sizeof(float) * 3);
				offset += 3;
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[1], sizeof(float) * 3);
				offset += 3;
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[2], sizeof(float) * 3);
				offset += 3;
			}
			else {
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[0], sizeof(float) * 4);
				offset += 4;
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[1], sizeof(float) * 4);
				offset += 4;
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[2], sizeof(float) * 4);
				offset += 4;
				memcpy(_scratch.data() + offset, &elem.payload.seqValue.elements[3], sizeof(float) * 4);
				offset += 4;
			}
		}
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		_scratch.clear();
		uint16_t elems = 0;
		if (unlikely(_isArray)) {
			const int len = int(input.payload.seqValue.len);
			if (len > _elems) {
				throw ActivationError("Input array size exceeded maximum uniform array size.");
			}
			int offset = 0;
			switch (_type) {
			case bgfx::UniformType::Vec4:
				_scratch.resize(4 * len);
				break;
			case bgfx::UniformType::Mat3:
				_scratch.resize(3 * 3 * len);
				break;
			case bgfx::UniformType::Mat4:
				_scratch.resize(4 * 4 * len);
				break;
			default:
				throw InvalidParameterIndex();
			}
			for (auto &elem : input) {
				fillElement(elem, offset);
				elems++;
			}
		}
		else {
			int offset = 0;
			switch (_type) {
			case bgfx::UniformType::Vec4:
				_scratch.resize(4);
				break;
			case bgfx::UniformType::Mat3:
				_scratch.resize(3 * 3);
				break;
			case bgfx::UniformType::Mat4:
				_scratch.resize(4 * 4);
				break;
			default:
				throw InvalidParameterIndex();
			}
			fillElement(input, offset);
			elems++;
		}

		bgfx::setUniform(_handle, _scratch.data(), elems);

		return input;
	}
};

struct Screenshot : public BaseConsumer {
	static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
	static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

	static inline Parameters params{
		{"Overwrite",
		 CBCCSTR("If each activation should overwrite the previous screenshot at "
				 "the given path if existing. If false the path will be suffixed "
				 "in order to avoid overwriting."),
		 {CoreInfo::BoolType}}};

	static CBParametersInfo parameters() { return params; }

	void setParam(int index, const CBVar &value) { _overwrite = value.payload.boolValue; }

	CBVar getParam(int index) { return Var(_overwrite); }

	void cleanup() { BaseConsumer::_cleanup(); }

	void warmup(CBContext *context) { BaseConsumer::_warmup(context); }

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		return CoreInfo::StringType;
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		const auto &currentView = frameRenderer.getCurrentView();
		if (_overwrite) {
			bgfx::requestScreenShot(currentView.fb, input.payload.stringValue);
		}
		else {
			std::filesystem::path base(input.payload.stringValue);
			const auto baseName = base.stem().string();
			const auto ext = base.extension().string();
			uint32_t suffix = 0;
			std::filesystem::path path = base;
			while (std::filesystem::exists(path)) {
				const auto ssuffix = std::to_string(suffix++);
				const auto newName = baseName + ssuffix + ext;
				path = base.root_path() / newName;
			}
			const auto s = path.string();
			bgfx::requestScreenShot(currentView.fb, s.c_str());
		}
		return input;
	}

	bool _overwrite{true};
};

struct Unproject : public BaseConsumer {
	static CBTypesInfo inputTypes() { return CoreInfo::Float2Type; }
	static CBTypesInfo outputTypes() { return CoreInfo::Float3Type; }

	float _z{0.0};

	static inline Parameters params{
		{"Z",
		 CBCCSTR("The value of Z depth to use, generally 0.0 for  the near "
				 "plane, 1.0 for the far plane."),
		 {CoreInfo::FloatType}}};

	static CBParametersInfo parameters() { return params; }

	void setParam(int index, const CBVar &value) { _z = float(value.payload.floatValue); }

	CBVar getParam(int index) { return Var(_z); }

	void cleanup() { BaseConsumer::_cleanup(); }

	void warmup(CBContext *context) { BaseConsumer::_warmup(context); }

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		return CoreInfo::Float3Type;
	}

	CBOptionalString help() { return CBCCSTR("This block unprojects screen coordinates into world coordinates."); }

	CBVar activate(CBContext *context, const CBVar &input) {
		using namespace linalg;
		using namespace linalg::aliases;

		gfx::FrameRenderer &frameRenderer = getFrameRenderer();

		const auto &currentView = frameRenderer.getCurrentView();

		const auto sx = float(input.payload.float2Value[0]) * float(currentView.viewport.width);
		const auto sy = float(input.payload.float2Value[1]) * float(currentView.viewport.height);
		const auto vx = float(currentView.viewport.x);
		const auto vy = float(currentView.viewport.y);
		const auto vw = float(currentView.viewport.width);
		const auto vh = float(currentView.viewport.height);
		const auto x = (((sx - vx) / vw) * 2.0f) - 1.0f;
		const auto y = 1.0f - (((sy - vy) / vh) * 2.0f);

		const auto m = mul(currentView.invView(), currentView.invProj());
		const auto v = mul(m, float4(x, y, _z, 1.0f));
		const Vec3 res = v / v.w;
		return res;
	}
};

struct CompileShader {
	enum class ShaderType { Vertex, Pixel, Compute };

	std::unique_ptr<IShaderCompiler> _compiler = makeShaderCompiler();
	ShaderType _type{ShaderType::Vertex};

	static inline Types InputTableTypes{{CoreInfo::StringType, CoreInfo::StringType, CoreInfo::StringSeqType}};
	static inline std::array<CBString, 3> InputTableKeys{"varyings", "code", "defines"};
	static inline Type InputTable = Type::TableOf(InputTableTypes, InputTableKeys);

	static constexpr uint32_t ShaderTypeCC = 'gfxS';
	static inline EnumInfo<ShaderType> ShaderTypeEnumInfo{"ShaderType", CoreCC, ShaderTypeCC};
	static inline Type ShaderTypeType = Type::Enum(CoreCC, ShaderTypeCC);

	CBTypesInfo inputTypes() { return InputTable; }
	CBTypesInfo outputTypes() { return CoreInfo::BytesType; }

	static inline Parameters Params{{"Type", CBCCSTR("The type of shader to compile."), {ShaderTypeType}}};

	static CBParametersInfo parameters() { return Params; }

	void setParam(int index, CBVar value) { _type = ShaderType(value.payload.enumValue); }

	CBVar getParam(int index) { return Var::Enum(_type, CoreCC, ShaderTypeCC); }

	CBVar activate(CBContext *context, const CBVar &input) {
		auto &tab = input.payload.tableValue;
		auto vvaryings = tab.api->tableAt(tab, "varyings");
		auto varyings = CBSTRVIEW((*vvaryings));
		auto vcode = tab.api->tableAt(tab, "code");
		auto code = CBSTRVIEW((*vcode));
		auto vdefines = tab.api->tableAt(tab, "defines");
		std::string defines;
		if (vdefines->valueType != CBType::None) {
			for (auto &define : *vdefines) {
				auto d = CBSTRVIEW(define);
				defines += std::string(d) + ";";
			}
		}

		return _compiler->compile(varyings, code, _type == ShaderType::Vertex ? "v" : _type == ShaderType::Pixel ? "f" : "c", defines, context);
	}
};

struct Picking : public BaseConsumer {
	BlocksVar _blocks;
	ParamVar _enabled;
	std::array<CBExposedTypeInfo, 2> _required;
	CBComposeResult _composeResults;
	std::shared_ptr<CBChain> _chain;

	static CBTypesInfo inputTypes() { return CoreInfo::Float2Type; }
	// it's either Chain or None but so we use any to allow this duality
	static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

	static inline Parameters Params{
		{"Blocks",
		 CBCCSTR("Any Draw inside this flow will render to the picking buffer as "
				 "well (main view only)."),
		 {CoreInfo::BlocksOrNone}},
		{"Enabled", CBCCSTR("Whether to enable picking."), {CoreInfo::BoolType, CoreInfo::BoolVarType}}};

	static CBParametersInfo parameters() { return Params; }

	void setParam(int index, const CBVar &value) {
		switch (index) {
		case 0:
			_blocks = value;
			break;
		case 1:
			_enabled = value;
			break;
		}
	}

	CBVar getParam(int index) {
		switch (index) {
		case 0:
			return _blocks;
		case 1:
			return _enabled;
		}
		return CBVar();
	}

	CBExposedTypesInfo requiredVariables() {
		if (_enabled.isVariable()) {
			_required[0] = BaseConsumer::requiredVariables().elements[0];
			_required[1] = CBExposedTypeInfo{_enabled.variableName(), CBCCSTR("The exposed boolean variable to turn picking on and off."), CoreInfo::BoolType};
			return {&_required[0], 2, 0};
		}
		else {
			return BaseConsumer::requiredVariables();
		}
	}

	CBTypeInfo compose(const CBInstanceData &data) {
		BaseConsumer::compose(data);
		_composeResults = _blocks.compose(data);
		return CoreInfo::AnyType;
	}

	CBExposedTypesInfo exposedVariables() { return _composeResults.exposedInfo; }

	void warmup(CBContext *context) {
		BaseConsumer::_warmup(context);
		_blocks.warmup(context);
		_enabled.warmup(context);

		_blitData.resize(gfx::PickingBufferSize * gfx::PickingBufferSize * 4);
	}

	void cleanup() {
		BaseConsumer::_cleanup();

		_chain = nullptr;

		_blocks.cleanup();
		_enabled.cleanup();
	}

	CBVar activate(CBContext *context, const CBVar &input) {
		gfx::FrameRenderer &frameRenderer = getFrameRenderer();
		gfx::Context &gfxContext = getGfxContext();

		const auto &view = frameRenderer.getCurrentView();
		if (view.id != 0) {
			throw ActivationError("Picking can only be used in the main view.");
		}

		_chain = nullptr;

		auto result = Var::Empty;

		auto enabled = _enabled.get();
		frameRenderer.setPicking(enabled.payload.boolValue);
		DEFER(frameRenderer.setPicking(false));

		CBVar output{};
		_blocks.activate(context, input, output);

		if (enabled.payload.boolValue) {
			auto &blitTex = gfxContext.pickingTexture();
			auto &pickRt = gfxContext.pickingRenderTarget();
			bgfx::blit(gfx::BlittingViewId, blitTex, 0, 0, pickRt);
			bgfx::readTexture(blitTex, _blitData.data());
		}
		else {
			memset(_blitData.data(), 0, _blitData.size());
		}

		auto x = int(input.payload.float2Value[0] * float(gfx::PickingBufferSize));
		auto y = int(input.payload.float2Value[1] * float(gfx::PickingBufferSize));
		uint32_t id = _blitData[y * int(gfx::PickingBufferSize) + x];

		const auto picked = frameRenderer.getFrameDrawable(id);

		if (picked) {
			assert(picked->getChain());
			_chain = picked->getChain()->shared_from_this();
			result = Var(CBChain::weakRef(_chain));
		}

		return result;
	}

private:
	std::vector<uint32_t> _blitData;
};

void registerBGFXBlocks() {
	REGISTER_CBLOCK("GFX.MainWindow", MainWindow);
	REGISTER_CBLOCK("GFX.Texture2D", Texture2D);
	using GraphicsShader = Shader<'g'>;
	REGISTER_CBLOCK("GFX.Shader", GraphicsShader);
	using ComputeShader = Shader<'c'>;
	REGISTER_CBLOCK("GFX.ComputeShader", ComputeShader);
	REGISTER_CBLOCK("GFX.Model", Model);
	REGISTER_CBLOCK("GFX.Camera", Camera);
	REGISTER_CBLOCK("GFX.CameraOrtho", CameraOrtho);
	REGISTER_CBLOCK("GFX.Draw", Draw);
	REGISTER_CBLOCK("GFX.RenderTexture", RenderTexture);
	REGISTER_CBLOCK("GFX.SetUniform", SetUniform);
	REGISTER_CBLOCK("GFX.Screenshot", Screenshot);
	REGISTER_CBLOCK("GFX.Unproject", Unproject);
	REGISTER_CBLOCK("GFX.CompileShader", CompileShader);
	REGISTER_CBLOCK("GFX.Picking", Picking);
}
}; // namespace BGFX

#ifdef CHAINBLOCKS_BUILD_TESTS
#include "bgfx_tests.cpp"
#endif
