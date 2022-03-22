#pragma once

#include "../params.hpp"
#include "fwd.hpp"
#include <cassert>
#include <map>
#include <vector>

namespace gfx {
namespace shader {
struct UniformLayout {
  size_t offset = {};
  size_t size = {};
  ShaderParamType type = ShaderParamType(~0);
};

struct UniformBufferLayout {
  std::vector<UniformLayout> items;
  std::vector<String> fieldNames;
  size_t size = ~0;
  size_t maxAlignment = 0;

  UniformBufferLayout() = default;
  UniformBufferLayout(const UniformBufferLayout &other) = default;
  UniformBufferLayout(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(UniformBufferLayout &&other) = default;
  UniformBufferLayout &operator=(const UniformBufferLayout &other) = default;
};

struct UniformBufferLayoutBuilder {
private:
  std::map<String, size_t> mapping;
  UniformBufferLayout bufferLayout;
  size_t offset = 0;

public:
  UniformLayout generateNext(ShaderParamType paramType) const {
    UniformLayout result;
    result.offset = offset;
    result.size = getParamTypeSize(paramType);
    result.type = paramType;
    return result;
  }

  const UniformLayout &push(const String &name, UniformLayout &&layout) {
    const UniformLayout &result = pushInternal(name, std::move(layout));
    offset = std::max(offset, layout.offset + layout.size);
    size_t fieldAlignment = getParamTypeWGSLAlignment(layout.type);
    bufferLayout.maxAlignment = std::max(bufferLayout.maxAlignment, fieldAlignment);
    return result;
  }

  const UniformLayout &push(const String &name, ShaderParamType paramType) { return push(name, generateNext(paramType)); }

  UniformBufferLayout &&finalize() {
    bufferLayout.size = offset;
    return std::move(bufferLayout);
  }

private:
  UniformLayout &pushInternal(const String &name, UniformLayout &&layout) {
    auto itExisting = mapping.find(name);
    if (itExisting != mapping.end()) {

      // assert(itExisti);
    }
    size_t index = mapping.size();
    bufferLayout.fieldNames.push_back(name);
    bufferLayout.items.emplace_back(std::move(layout));
    mapping.insert_or_assign(name, index);
    return bufferLayout.items.back();
  }
};
} // namespace shader
} // namespace gfx
