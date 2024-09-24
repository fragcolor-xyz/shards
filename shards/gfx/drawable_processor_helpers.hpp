#ifndef DB3B2F03_117E_49DC_93AA_28A94118C09D
#define DB3B2F03_117E_49DC_93AA_28A94118C09D

#include <shards/core/pmr/vector.hpp>
#include "renderer_types.hpp"
#include "drawable_processor.hpp"
#include "sampler_cache.hpp"
#include "texture.hpp"
#include "view.hpp"
#include "gfx_wgpu.hpp"
#include "buffer.hpp"
#include "../core/platform.hpp"
#include "../core/assert.hpp"
#include "unique_id.hpp"
#include <variant>

namespace gfx::detail {

namespace bind {
// One single data structure
struct One {};
// A dynamically sized array of the data structure, which is indexed based on the draw instance index
struct PerInstance {};
// A dynamically sized array with a custom length
struct Sized {
  size_t length;
  Sized(size_t length) : length(length) {}
};
}; // namespace bind
using BindDimension = std::variant<bind::One, bind::PerInstance, bind::Sized>;

struct PreparedBuffer {
  WGPUBuffer buffer;
  size_t length;
  size_t stride;
  BindDimension bindDimension;
  // Index of original binding
  size_t binding;
  size_t bindGroup;
};

// Defines some helper functions for implementing drawable processors
struct BindGroupBuilder {
  using allocator_type = shards::pmr::PolymorphicAllocator<>;

  shards::pmr::vector<WGPUBindGroupEntry> entries;

  BindGroupBuilder(allocator_type allocator) : entries(allocator) {}

  void addBinding(const detail::BufferBinding &binding, const PreparedBuffer &pb, size_t numInstances) {
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, bind::One>) {
            addBinding(binding, pb.buffer);
          } else if constexpr (std::is_same_v<T, bind::Sized>) {
            addBinding(binding, pb.buffer, arg.length);
          } else if constexpr (std::is_same_v<T, bind::PerInstance>) {
            // Per-instance buffers are mapped entirely, dynamic offset is applied during encoding
            addBinding(binding, pb.buffer, numInstances);
          }
        },
        pb.bindDimension);
  }

  void addBinding(const detail::BufferBinding &binding, WGPUBuffer buffer, size_t numArrayElements = 1, size_t arrayIndex = 0) {
    auto &entry = entries.emplace_back();
    entry.binding = binding.index;
    entry.size = binding.layout.getArrayStride() * numArrayElements;
    entry.offset = binding.layout.getArrayStride() * arrayIndex;
    entry.buffer = buffer;
  }

  void addTextureBinding(const gfx::shader::TextureBinding &binding, WGPUTextureView texture, WGPUSampler sampler) {
    auto &entry = entries.emplace_back();
    entry.binding = binding.binding;
    entry.textureView = texture;

    auto &samplerEntry = entries.emplace_back();
    samplerEntry.binding = binding.defaultSamplerBinding;
    samplerEntry.sampler = sampler;
  }

  WGPUBindGroup finalize(WGPUDevice device, WGPUBindGroupLayout layout, const char *label = nullptr) {
    WGPUBindGroupDescriptor desc{
        .label = label,
        .layout = layout,
        .entryCount = uint32_t(entries.size()),
        .entries = entries.data(),
    };
    WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(device, &desc);
    shassert(bindGroup);
    return bindGroup;
  }
};

// Defines a range of drawable
struct GroupRange {
  typedef std::vector<UniqueId> ContainerType;

  size_t startIndex;
  size_t numInstances;

  GroupRange(size_t startIndex, size_t numInstances) : startIndex(startIndex), numInstances(numInstances) {}

  // Get span over the source data
  template <typename T, typename TAlloc> boost::span<T> sourceSpan(std::vector<T, TAlloc> &data) {
    return {data.data() + startIndex, numInstances};
  }
  template <typename T, typename TAlloc> boost::span<const T> sourceSpan(const std::vector<T, TAlloc> &data) const {
    return {data.data() + startIndex, numInstances};
  }
};

struct DividedGroupRange {
  size_t startIndex;
  size_t numInstances;
  // Index of first metadata entry
  size_t metaDataOffset;
  // Number of metadata entry divisions in this group
  size_t metaNumDivisions;

  // Get span over the range metadata
  template <typename T, typename TAlloc> boost::span<T> metaSpan(std::vector<T, TAlloc> &data) const {
    return {data.data() + metaDataOffset, metaNumDivisions};
  }

  // Get span over the source data
  template <typename T, typename TAlloc> boost::span<T> sourceSpan(std::vector<T, TAlloc> &data) const {
    return {data.data() + startIndex, numInstances};
  }
};

// Creates draw groups ranges based on DrawGroupKey
// contiguous elements with matching keys will be grouped into the same draw group
template <typename TKeygen, typename TAlloc>
inline void groupDrawables(size_t numDrawables, std::vector<detail::GroupRange, TAlloc> &outRanges, TKeygen &&keyGenerator) {
  if (numDrawables > 0) {
    size_t groupStart = 0;
    auto currentKey = keyGenerator(0);

    auto finishGroup = [&](size_t currentIndex) {
      size_t groupLength = currentIndex - groupStart;
      if (groupLength > 0) {
        outRanges.emplace_back(groupStart, groupLength);
      }
    };
    for (size_t i = 1; i < numDrawables; i++) {
      auto key = keyGenerator(i);
      if (key != currentKey) {
        finishGroup(i);
        groupStart = i;
        currentKey = key;
      }
    }
    finishGroup(numDrawables);
  }
}

// Takes an existing grouping and subdivide it based on a finer grouping
// resulting in a new flat set
// outSubdivisions will contain the index of the parent group for each new group
template <typename TKeygen, typename TAlloc>
inline void subdivideGroupedDrawables(const std::vector<detail::GroupRange, TAlloc> &inRanges,
                                      std::vector<detail::GroupRange, TAlloc> &outRanges, TKeygen &&keyGenerator) {
  for (size_t g = 0; g < inRanges.size(); g++) {
    auto &baseRange = inRanges[g];
    size_t groupStart = baseRange.startIndex;
    auto currentKey = keyGenerator(groupStart);

    auto finishGroup = [&](size_t currentIndex) {
      size_t groupLength = currentIndex - groupStart;
      if (groupLength > 0) {
        outRanges.emplace_back(groupStart, groupLength);
      }
    };

    for (size_t i = 1; i < baseRange.numInstances; i++) {
      auto key = keyGenerator(baseRange.startIndex + i);
      if (key != currentKey) {
        finishGroup(baseRange.startIndex + i);
        groupStart = baseRange.startIndex + i;
        currentKey = key;
      }
    }
    finishGroup(baseRange.startIndex + baseRange.numInstances);
  }
}

// Takes an outer range and an inner range and annotate the outer range with the information
// needed to iterate over it's members
template <typename TAlloc, typename TAlloc1>
inline void groupRangeToDividedGroupRange(const std::vector<GroupRange, TAlloc> &outerRanges,
                                          const std::vector<GroupRange, TAlloc> &innerRanges,
                                          std::vector<DividedGroupRange, TAlloc1> &outRanges) {
  shassert(innerRanges.size() >= outerRanges.size());
  size_t outerIdx = 0;
  size_t startInnerIdx = 0;
  size_t instanceOffset = 0;
  auto finishGroup = [&](size_t endInnerIdx) {
    size_t numDivs = endInnerIdx - startInnerIdx;
    if (numDivs > 0) {
      DividedGroupRange &outRange = outRanges.emplace_back();
      outRange.startIndex = outerRanges[outerIdx].startIndex;
      outRange.numInstances = outerRanges[outerIdx].numInstances;
      outRange.metaDataOffset = startInnerIdx;
      outRange.metaNumDivisions = numDivs;
      outerIdx++;
    }
  };

  for (size_t innerIdx = 0; innerIdx < innerRanges.size(); innerIdx++) {
    const GroupRange &outerRange = outerRanges[outerIdx];
    const GroupRange &innerRange = innerRanges[innerIdx];
    instanceOffset += innerRange.numInstances;

    if (instanceOffset >= outerRange.startIndex + outerRange.numInstances) {
      finishGroup(innerIdx + 1);
      startInnerIdx = innerIdx + 1;
    }
  }
  finishGroup(innerRanges.size());
}

inline void packDrawData(uint8_t *outData, size_t outSize, const StructLayout &layout, const ParameterStorage &parameterStorage) {
  size_t layoutIndex = 0;
  for (auto &fieldName : layout.fieldNames) {
#if SH_ANDROID
    auto drawDataIt = parameterStorage.basic.find(fieldName);
#else
    auto drawDataIt = parameterStorage.basic.find(fieldName);
#endif
    if (drawDataIt != parameterStorage.basic.end()) {
      const StructLayoutItem &itemLayout = layout.items[layoutIndex];
      NumType drawDataFieldType = getNumParameterType(drawDataIt->second);
      if (itemLayout.type != drawDataFieldType) {
        SPDLOG_LOGGER_WARN(getLogger(), "Shader field \"{}\" type mismatch layout:{} parameterStorage:{}", fieldName,
                           itemLayout.type, drawDataFieldType);
      } else {
        packNumParameter(outData + itemLayout.offset, outSize - itemLayout.offset, drawDataIt->second);
      }
    }
    layoutIndex++;
  }
}

inline void setViewParameters(ParameterStorage &outDrawData, const ViewData &viewData, const Rect &viewport) {
  static FastString fs_view = "view";
  static FastString fs_invView = "invView";
  static FastString fs_proj = "proj";
  static FastString fs_invProj = "invProj";
  static FastString fs_viewport = "viewport";
  outDrawData.setParam(fs_view, viewData.view->view);
  outDrawData.setParam(fs_invView, viewData.cachedView.invViewTransform);
  outDrawData.setParam(fs_proj, viewData.cachedView.projectionTransform);
  outDrawData.setParam(fs_invProj, viewData.cachedView.invProjectionTransform);
  outDrawData.setParam(fs_viewport, float4(float(viewport.x), float(viewport.y), float(viewport.width), float(viewport.height)));
}

struct TextureData {
  const TextureContextData *data{};
  WGPUSampler sampler{};
  UniqueId id{};

  operator bool() const { return data != nullptr; }
};

struct BufferData {
  const BufferContextData *data{};
  UniqueId id{};

  operator bool() const { return data != nullptr; }
};

template <typename TTextureBindings> auto mapTextureBinding(const TTextureBindings &textureBindings, FastString name) -> int32_t {
  auto it = std::find_if(textureBindings.begin(), textureBindings.end(), [&](auto it) { return it.name == name; });
  if (it != textureBindings.end())
    return int32_t(it - textureBindings.begin());
  return -1;
};

} // namespace gfx::detail

#endif /* DB3B2F03_117E_49DC_93AA_28A94118C09D */
