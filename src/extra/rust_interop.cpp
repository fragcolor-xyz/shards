#include "rust_interop.hpp"
#include "gfx/shards_types.hpp"
#include "gfx.hpp"
#include <foundation.hpp>

using shards::Var;
namespace gfx {
SHTypeInfo *getMainWindowGlobalsType() {
  static SHTypeInfo type = gfx::MainWindowGlobals::Type;
  return &type;
}
const char *getMainWindowGlobalsVarName() { return gfx::Base::mainWindowGlobalsVarName; }
SHTypeInfo *getQueueType() {
  static SHTypeInfo type = Types::DrawQueue;
  return &type;
}

template <typename T> T *castChecked(const SHVar &var, const shards::Type &type) {
  SHTypeInfo typeInfo(type);
  if (var.valueType != SHType::Object)
    throw std::logic_error("Invalid type");
  if (var.payload.objectVendorId != typeInfo.object.vendorId)
    throw std::logic_error("Invalid object vendor id");
  if (var.payload.objectTypeId != typeInfo.object.typeId)
    throw std::logic_error("Invalid object type id");
  return reinterpret_cast<T *>(var.payload.objectValue);
}

SHVar MainWindowGlobals_getDefaultQueue(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = castChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return Var::Object(&globals->shDrawQueue, SHTypeInfo(Types::DrawQueue).object.vendorId,
                     SHTypeInfo(Types::DrawQueue).object.typeId);
}
Context *MainWindowGlobals_getContext(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = castChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return globals->context.get();
}
Renderer *MainWindowGlobals_getRenderer(const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = castChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return globals->renderer.get();
}

DrawQueuePtr *getDrawQueueFromVar(const SHVar &var) {
  SHDrawQueue *shDrawQueue = castChecked<SHDrawQueue>(var, Types::DrawQueue);
  return &shDrawQueue->queue;
}

const egui::Input *getEguiWindowInputs(gfx::EguiInputTranslator *translator, const SHVar &mainWindowGlobals) {
  MainWindowGlobals *globals = castChecked<MainWindowGlobals>(mainWindowGlobals, MainWindowGlobals::Type);
  return translator->translateFromInputEvents(globals->events, *globals->window.get(), globals->time, globals->deltaTime);
}
} // namespace gfx
