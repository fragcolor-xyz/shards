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
SHVar getDefaultQueue(const SHVar *mainWindowGlobals) {

  assert(mainWindowGlobals->payload.objectValue);
  MainWindowGlobals *globals = reinterpret_cast<MainWindowGlobals *>(mainWindowGlobals->payload.objectValue);
  return Var::Object(&globals->shDrawQueue, SHTypeInfo(Types::DrawQueue).object.vendorId,
                     SHTypeInfo(Types::DrawQueue).object.typeId);
}
} // namespace gfx
