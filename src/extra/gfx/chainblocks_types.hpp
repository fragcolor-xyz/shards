#pragma once
#include <chainblocks.hpp>

// Type Ids
namespace gfx {
constexpr uint32_t VendorId = 'cbgx';

constexpr uint32_t ImguiContextTypeId = 'imui';
static inline chainblocks::Type ImguiContextType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ImguiContextTypeId}}}};

static constexpr uint32_t ContextTypeId = 'ctx ';
static inline chainblocks::Type ContextType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ContextTypeId}}}};

static constexpr uint32_t ModelTypeId = 'mdl ';
static inline chainblocks::Type ModelType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = ModelTypeId}}}};

constexpr uint32_t NativeWindowTypeId = 'nwnd';
static inline chainblocks::Type NativeWindowType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = NativeWindowTypeId}}}};

constexpr uint32_t WindowTypeId = 'wnd ';
static inline chainblocks::Type windowType{{CBType::Object, {.object = {.vendorId = VendorId, .typeId = WindowTypeId}}}};
} // namespace gfx
