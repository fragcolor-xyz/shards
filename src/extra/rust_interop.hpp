#ifndef E325D8E3_F64E_413D_965B_DF275CF18AC4
#define E325D8E3_F64E_413D_965B_DF275CF18AC4

#include "shards.h"

namespace gfx {
// gfx::MainWindowGlobals::Type
SHTypeInfo *getMainWindowGlobalsType();
const char* getMainWindowGlobalsVarName();
SHTypeInfo *getQueueType();
SHVar getDefaultQueue(const SHVar* mainWindowGlobals);
} // namespace gfx

#endif /* E325D8E3_F64E_413D_965B_DF275CF18AC4 */
