#include "gfx.hpp"
#include "gfx/buffer_vars.hpp"
#include <gfx/context.hpp>
#include <gfx/loop.hpp>
#include <gfx/math.hpp>
#include <gfx/mesh.hpp>
#include <gfx/renderer.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <linalg_shim.hpp>

using namespace chainblocks;

namespace gfx {
using chainblocks::Mat4;

MainWindowGlobals &Base::getMainWindowGlobals() {
  MainWindowGlobals *globalsPtr = reinterpret_cast<MainWindowGlobals *>(_mainWindowGlobalsVar->payload.objectValue);
  if (!globalsPtr) {
    throw chainblocks::ActivationError("Graphics context not set");
  }
  return *globalsPtr;
}
Context &Base::getContext() { return *getMainWindowGlobals().context.get(); }
Window &Base::getWindow() { return *getMainWindowGlobals().window.get(); }

struct MainWindow : public Base {
  static inline Parameters params{
      {"Title", CBCCSTR("The title of the window to create."), {CoreInfo::StringType}},
      {"Width", CBCCSTR("The width of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Height", CBCCSTR("The height of the window to create. In pixels and DPI aware."), {CoreInfo::IntType}},
      {"Contents", CBCCSTR("The contents of this window."), {CoreInfo::BlocksOrNone}},
      {"Debug",
       CBCCSTR("If the device backing the window should be created with "
               "debug layers on."),
       {CoreInfo::BoolType}},
  };

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() { return params; }

  std::string _title;
  int _pwidth = 1280;
  int _pheight = 720;
  bool _debug = false;
  Window _window;
  BlocksVar _blocks;

  ViewPtr mainView;
  std::shared_ptr<MainWindowGlobals> globals;
  ::gfx::Loop loop;

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
      _debug = value.payload.boolValue;
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
      return Var(_debug);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError("GFX Blocks cannot be used on a worker thread (e.g. "
                         "within an Await block)");
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    // Make sure MainWindow is UNIQUE
    for (uint32_t i = 0; i < data.shared.len; i++) {
      if (strcmp(data.shared.elements[i].name, "GFX.Context") == 0) {
        throw CBException("GFX.MainWindow must be unique, found another use!");
      }
    }

    globals = std::make_shared<MainWindowGlobals>();

    // twice to actually own the data and release...
    IterableExposedInfo rshared(data.shared);
    IterableExposedInfo shared(rshared);
    shared.push_back(
        ExposedInfo::ProtectedVariable(Base::mainWindowGlobalsVarName, CBCCSTR("The graphics context"), MainWindowGlobals::Type));
    data.shared = shared;

    _blocks.compose(data);

    return CoreInfo::NoneType;
  }

  void warmup(CBContext *context) {
    globals = std::make_shared<MainWindowGlobals>();

    WindowCreationOptions windowOptions = {};
    windowOptions.width = _pwidth;
    windowOptions.height = _pheight;
    windowOptions.title = _title;
    globals->window = std::make_shared<Window>();
    globals->window->init(windowOptions);

    ContextCreationOptions contextOptions = {};
    contextOptions.debug = _debug;
    globals->context = std::make_shared<Context>();
    globals->context->init(*globals->window.get(), contextOptions);

    globals->renderer = std::make_shared<Renderer>(*globals->context.get());

    _mainWindowGlobalsVar = referenceVariable(context, Base::mainWindowGlobalsVarName);
    _mainWindowGlobalsVar->payload.objectTypeId = MainWindowGlobals::TypeId;
    _mainWindowGlobalsVar->payload.objectValue = globals.get();
    _mainWindowGlobalsVar->valueType = CBType::Object;

    // placeholder main view
    mainView = std::make_shared<View>();
    mainView->view = linalg::lookat_matrix(float3(0, 0.0f, 12.0f), float3(0, 0, 0), float3(0, 1, 0));
    mainView->proj = ViewPerspectiveProjection{
        degToRad(45.0f),
        FovDirection::Horizontal,
    };

    _blocks.warmup(context);
  }

  void cleanup() {
    globals.reset();
    _blocks.cleanup();

    if (_mainWindowGlobalsVar) {
      if (_mainWindowGlobalsVar->refcount > 1) {
#ifdef NDEBUG
        CBLOG_ERROR("MainWindow: Found a dangling reference to GFX.Context");
#else
        CBLOG_ERROR("MainWindow: Found {} dangling reference(s) to GFX.Context", _mainWindowGlobalsVar->refcount - 1);
#endif
      }
      memset(_mainWindowGlobalsVar, 0x0, sizeof(CBVar));
      _mainWindowGlobalsVar = nullptr;
    }
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) {
    auto &renderer = globals->renderer;
    auto &context = globals->context;
    auto &window = globals->window;
    auto &events = globals->events;

    window->pollEvents(events);
    for (auto &event : events) {
      if (event.type == SDL_QUIT) {
        throw ActivationError("Window closed, aborting chain.");
      } else if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
          throw ActivationError("Window closed, aborting chain.");
        } else if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
          gfx::int2 newSize = window->getDrawableSize();
          context->resizeMainOutputConditional(newSize);
        }
      }
    }

    float deltaTime = 0.0;
    auto &drawQueue = globals->drawQueue;
    if (loop.beginFrame(0.0f, deltaTime)) {
      context->beginFrame();
      renderer->swapBuffers();
      drawQueue.clear();

      CBVar _blocksOutput{};
      _blocks.activate(cbContext, input, _blocksOutput);

      renderer->render(drawQueue, mainView);
      context->endFrame();
    }

    return CBVar{};
  }
};

struct DrawableBlock {
  static inline Type MeshVarType = Type::VariableOf(MeshType);
  static inline Type TransformVarType = Type::VariableOf(CoreInfo::Float4x4Type);

  static inline Parameters params{
      {"Mesh", CBCCSTR("The mesh to draw"), {MeshVarType}},
      {"Transform", CBCCSTR("The transform to use"), {TransformVarType}},
  };

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return DrawableType; }
  static CBParametersInfo parameters() { return params; }

  ParamVar _meshVar{};
  ParamVar _transformVar{};

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _meshVar = value;
      break;
    case 1:
      _transformVar = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _meshVar;
    case 1:
      return _transformVar;
    default:
      return Var::Empty;
    }
  }

  void warmup(CBContext *context) {
    _meshVar.warmup(context);
    _transformVar.warmup(context);
  }

  void cleanup() {
    _meshVar.cleanup();
    _transformVar.cleanup();
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) {
    MeshPtr *meshPtr = (MeshPtr *)_meshVar.get().payload.objectValue;
    if (!meshPtr)
      throw formatException("Mesh must be set");

    CBDrawable *cbDrawable = DrawableObjectVar.New();
    cbDrawable->drawable = std::make_shared<Drawable>(*meshPtr, chainblocks::Mat4(_transformVar.get()));
    if (_transformVar.isVariable()) {
      cbDrawable->transformVar = (CBVar &)_transformVar;
      cbDrawable->transformVar.warmup(cbContext);
    }

    return DrawableObjectVar.Get(cbDrawable);
  }
};

struct Draw : public BaseConsumer {
  static inline Type DrawableSeqType = Type::SeqOf(DrawableType);
  static inline Types DrawableTypes{DrawableType, DrawableSeqType};

  static CBTypesInfo inputTypes() { return DrawableTypes; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() { return CBParametersInfo(); }

  void warmup(CBContext *cbContext) { baseConsumerWarmup(cbContext); }
  void cleanup() { baseConsumerCleanup(); }
  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.basicType == CBType::Seq) {
      OVERRIDE_ACTIVATE(data, activateSeq);
    } else {
      OVERRIDE_ACTIVATE(data, activateSingle);
    }
    return CoreInfo::NoneType;
  }

  void addDrawableToQueue(DrawablePtr drawable) {
    auto &dq = getMainWindowGlobals().drawQueue;
    dq.add(drawable);
  }

  void updateCBDrawable(CBDrawable *cbDrawable) {
    // Update transform if it's referencing a context variable
    if (cbDrawable->transformVar.isVariable()) {
      cbDrawable->drawable->transform = chainblocks::Mat4(cbDrawable->transformVar.get());
    }
  }

  CBVar activateSeq(CBContext *cbContext, const CBVar &input) {
    spdlog::debug("activate GFX.Draw [Seq]");

    auto &seq = input.payload.seqValue;
    for (size_t i = 0; i < seq.len; i++) {
      CBDrawable *cbDrawable = (CBDrawable *)seq.elements[i].payload.objectValue;
      assert(cbDrawable);
      updateCBDrawable(cbDrawable);
      addDrawableToQueue(cbDrawable->drawable);
    }

    return CBVar{};
  }

  CBVar activateSingle(CBContext *cbContext, const CBVar &input) {
    spdlog::debug("activate GFX.Draw [Single]");

    CBDrawable *cbDrawable = (CBDrawable *)input.payload.objectValue;
    updateCBDrawable(cbDrawable);
    addDrawableToQueue(cbDrawable->drawable);

    return CBVar{};
  }

  CBVar activate(CBContext *cbContext, const CBVar &input) { throw ActivationError("GFX.Draw: Unsupported input type"); }
};

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
    case 2:
      return Var::Enum(_windingOrder, CoreCC, WindingOrderTypeId);
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

void registerBlocks() {
  REGISTER_CBLOCK("GFX.MainWindow", MainWindow);
  REGISTER_CBLOCK("GFX.Mesh", MeshBlock);
  REGISTER_CBLOCK("GFX.Drawable", DrawableBlock);
  REGISTER_CBLOCK("GFX.Draw", Draw);
}
} // namespace gfx