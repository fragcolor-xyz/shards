#include "mesh_tree_drawable.hpp"
#include "transform_updater.hpp"
#include <stdexcept>
#include <boost/container/small_vector.hpp>

namespace gfx {

DrawablePtr MeshTreeDrawable::clone() const {
  MeshTreeDrawable::Ptr result = std::make_shared<MeshTreeDrawable>();
  result->label = label;
  result->trs = trs;
  for (auto &drawable : drawables) {
    result->drawables.push_back(std::static_pointer_cast<MeshDrawable>(drawable->clone()));
  }
  for (auto &child : children) {
    result->addChild(std::static_pointer_cast<MeshTreeDrawable>(child->clone()));
  }
  result->id = getNextDrawableId();
  return result;
}

DrawableProcessorConstructor MeshTreeDrawable::getProcessor() const { throw std::logic_error("not implemented"); }

static void updateSkin(MeshDrawable &drawable, const std::shared_ptr<Skin> &_skin) {
  auto &skin = *_skin.get();
  skin.jointMatrices.resize(skin.joints.size());
  for (size_t i = 0; i < skin.joints.size(); ++i) {
    if (auto joint = skin.joints[i].lock()) {
      skin.jointMatrices[i] =
      // skin.jointMatrices[i] = linalg::mul(joint->resolvedTransform, linalg::mul(skin.inverseBindMatrices[i],
      // linalg::inverse(drawable.transform))); 
      linalg::mul(linalg::mul(linalg::inverse(drawable.transform), joint->resolvedTransform), skin.inverseBindMatrices[i]);
        // linalg::mul(linalg::mul(skin.inverseBindMatrices[i], joint->resolvedTransform), linalg::inverse(drawable.transform));

    }
  }
}

bool MeshTreeDrawable::expand(shards::pmr::vector<const IDrawable *> &outDrawables) const {
  boost::container::small_vector<MeshDrawable *, 16> skinsToUpdate;

  TransformUpdaterCollector collector;
  collector.collector = [&](const DrawablePtr &drawable) {
    outDrawables.push_back(drawable.get());
    if (MeshDrawable *md = dynamic_cast<MeshDrawable *>(drawable.get())) {
      if (md->skin) {
        skinsToUpdate.push_back(md);
      }
    }
  };
  collector.update(const_cast<MeshTreeDrawable &>(*this));

  for (auto &skin : skinsToUpdate) {
    updateSkin(*skin, skin->skin);
  }
  return true;
}

} // namespace gfx
