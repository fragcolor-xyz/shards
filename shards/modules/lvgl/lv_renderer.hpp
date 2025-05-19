#ifndef C9DAC267_75C6_46F2_BAFC_025360B1C637
#define C9DAC267_75C6_46F2_BAFC_025360B1C637

#include "lvgl.h"
#include "lvgl/src/draw/lv_draw_private.h"
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/drawable.hpp>
#include <gfx/sized_item_pool.hpp>
#include <shards/core/pool.hpp>

namespace shards::lvgl::draw {
using namespace gfx;

inline constexpr auto findDrawableInPoolByMeshSize(size_t targetSize) {
  if constexpr (sizeof(size_t) >= 8) {
    if (targetSize > INT64_MAX) {
      throw std::runtime_error("targetSize too large");
    }
  }
  return [targetSize](std::shared_ptr<MeshDrawable> &item) -> int64_t {
    auto &mesh = item->mesh;
    size_t size = mesh->getNumVertices() * mesh->getFormat().getVertexSize();
    if (size < targetSize) {
      return INT64_MAX;
    }
    // Negate so the smallest buffer will be picked first
    return -(int64_t(size) - int64_t(targetSize));
  };
}

struct PoolTraits {
  using T = std::shared_ptr<MeshDrawable>;
  T newItem() {
    auto drawable = std::make_shared<MeshDrawable>();
    drawable->mesh = std::make_shared<Mesh>();
    return drawable;
  }
  void release(T &) {}
  bool canRecycle(T &v) { return v.use_count() == 1; }
  void recycled(T &v) {}
};

struct MeshDrawablePool : public shards::Pool<std::shared_ptr<MeshDrawable>, PoolTraits> {
  std::shared_ptr<MeshDrawable> &allocateBuffer(size_t size) {
    return this->newValue([](auto &buffer) {}, findDrawableInPoolByMeshSize(size));
  }
};

struct DrawUnit;
struct lv_draw_unit_shards_t {
  lv_draw_unit_t base_unit{};
  DrawUnit *drawUnit{};
};

struct DrawUnit {
  lv_draw_unit_shards_t *unit{};
  lv_cache_t textureCache{};

  gfx::DrawQueuePtr queue;
  MeshDrawablePool drawables;

  DrawUnit() {
    unit = (lv_draw_unit_shards_t *)lv_draw_create_unit(sizeof(lv_draw_unit_shards_t));
    unit->drawUnit = this;
    unit->base_unit.name = "SHARDS_GFX";
    unit->base_unit.delete_cb = &s_delete;
    unit->base_unit.dispatch_cb = &s_dispatch;
    unit->base_unit.evaluate_cb = &s_evaluate;
  }
  ~DrawUnit() {
    unit->drawUnit = nullptr;
  }

  int32_t _delete(lv_draw_unit_t *draw_unit) {
    auto unit = (lv_draw_unit_shards_t *)draw_unit;
    unit->drawUnit = nullptr;
    return 0;
  }
  int32_t _dispatch(lv_draw_unit_t *draw_unit, lv_layer_t *layer) { return 0; }
  int32_t _evaluate(lv_draw_unit_t *u, lv_draw_task_t *t) { return 0; }

  static int32_t s_delete(lv_draw_unit_t *draw_unit) {
    return ((lv_draw_unit_shards_t *)draw_unit)->drawUnit->_delete(draw_unit);
  }
  static int32_t s_dispatch(lv_draw_unit_t *draw_unit, lv_layer_t *layer) {
    return ((lv_draw_unit_shards_t *)draw_unit)->drawUnit->_dispatch(draw_unit, layer);
  }
  static int32_t s_evaluate(lv_draw_unit_t *u, lv_draw_task_t *t) {
    return ((lv_draw_unit_shards_t *)u)->drawUnit->_evaluate(u, t);
  }
};
} // namespace shards::lvgl::draw
#endif /* C9DAC267_75C6_46F2_BAFC_025360B1C637 */
