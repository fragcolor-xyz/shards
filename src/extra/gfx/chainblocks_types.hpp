#pragma once
#include <chainblocks.hpp>
#include <foundation.hpp>
#include <gfx/enums.hpp>
#include <memory>

namespace gfx {

struct Drawable;
struct Mesh;
typedef std::shared_ptr<Drawable> DrawablePtr;
typedef std::shared_ptr<Mesh> MeshPtr;

constexpr uint32_t VendorId = 'cgfx';

constexpr uint32_t WindowTypeId = '_wnd';
static inline chainblocks::Type windowType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = WindowTypeId}}}};

static constexpr uint32_t ContextTypeId = '_ctx';
static inline chainblocks::Type ContextType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ContextTypeId}}}};

constexpr uint32_t ImguiContextTypeId = 'imui';
static inline chainblocks::Type ImguiContextType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ImguiContextTypeId}}}};

static constexpr uint32_t DrawableTypeId = 'mesh';
static inline chainblocks::Type DrawableType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = DrawableTypeId}}}};
static chainblocks::ObjectVar<DrawablePtr> DrawableObjectVar{"GFX.Drawable", VendorId, DrawableTypeId};

static constexpr uint32_t MeshTypeId = 'mesh';
static inline chainblocks::Type MeshType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = MeshTypeId}}}};
static chainblocks::ObjectVar<MeshPtr> MeshObjectVar{"GFX.Mesh", VendorId, MeshTypeId};

static constexpr uint32_t WindingOrderTypeId = 'wind';
static inline chainblocks::Type WindingOrderType = chainblocks::Type::Enum(VendorId, WindingOrderTypeId);
static inline chainblocks::EnumInfo<WindingOrder> CullModeEnumInfo{"WindingOrder", VendorId, WindingOrderTypeId};

} // namespace gfx