#include "drawable.hpp"
#include "unique_id.hpp"

namespace gfx {

UniqueId getNextDrawableId() {
  static UniqueIdGenerator gen(UniqueIdTag::Drawable);
  return gen.getNext();
}

UniqueId getNextDrawQueueId() {
  static UniqueIdGenerator gen(UniqueIdTag::DrawQueue);
  return gen.getNext();
}

} // namespace gfx