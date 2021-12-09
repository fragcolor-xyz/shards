#pragma once
#include "linalg.hpp"
#include <memory>
#include <vector>

namespace gfx {

struct Mesh;
struct Material;
struct Drawable {
	std::shared_ptr<Mesh> mesh;
	std::shared_ptr<Material> material;
	float4x4 transform;

	Drawable(std::shared_ptr<Mesh> mesh, float4x4 transform, std::shared_ptr<Material> material = std::shared_ptr<Material>())
		: mesh(mesh), material(material), transform(transform) {}
};
typedef std::shared_ptr<Drawable> DrawablePtr;

struct DrawQueue {
private:
	std::vector<DrawablePtr> drawables;

public:
	void add(DrawablePtr drawable) { drawables.push_back(drawable); }
	void clear() { drawables.clear(); }
	const std::vector<DrawablePtr> &getDrawables() const { return drawables; }
};

} // namespace gfx
