#ifndef EF9289B8_FECE_4D29_8B3F_43A88E82C83B
#define EF9289B8_FECE_4D29_8B3F_43A88E82C83B

#include "render_graph.hpp"
#include "render_graph_build_data.hpp"
#include "render_step_impl.hpp"
#include "renderer_storage.hpp"
#include <spdlog/spdlog.h>
#include <variant>
#include <unordered_map>
#include <boost/container/flat_map.hpp>

namespace gfx::detail {
namespace graph_build_data {
static inline auto logger = gfx::getLogger();

struct RenderGraphBuilder {
private:
  template <typename T> using VectorType = boost::container::stable_vector<T>;
  VectorType<NodeBuildData> buildingNodes;
  VectorType<FrameBuildData> buildingFrames;
  VectorType<AttachmentRef> buildingOutputs;
  VectorType<AttachmentRef> buildingInputs;
  VectorType<SizeBuildData> buildingSizeDefinitions;

  std::map<std::string, FrameBuildData *> nameLookup;
  std::map<TextureSubResource, FrameBuildData *> handleLookup;
  // std::vector<FormatConstraintBuildData> formatConstraints;

  bool needPrepare = true;

public:
  float2 referenceOutputSize{};
  std::vector<Output> outputs;

  // Adds a node to the render graph
  void addNode(const ViewData &viewData, const PipelineStepPtr &step, size_t queueDataIndex);
  // Allocate outputs for a render graph node
  void allocateOutputs(NodeBuildData &buildData, const RenderStepOutput &output, bool forceOverwrite = false);
  // Allocate outputs for a render graph node
  void allocateInputs(NodeBuildData &buildData, const RenderStepInput &input);

  // Get or allocate a frame based on it's description
  FrameBuildData *assignFrame(const RenderStepOutput::OutputVariant &output, PipelineStepPtr step,
                              TextureOverrideRef::Binding bindingType, size_t bindingIndex, bool forceNew = false);

  // Find the index of a frame
  FrameBuildData *findFrameByName(const std::string &name);

  bool isWrittenTo(const FrameBuildData &frame);
  bool isWrittenTo(const std::string &name);

  RenderGraph build();

private:
  // Validate node connections & setup output
  // after this additional modification can be made such as inserting clear steps/resizing
  void prepare();

  // Assign input to be allowed any size
  // SizeIndex assignOutputRefSize(FrameBuildData &frame, FrameBuildData &referenceFrame, float2 scale);
  // SizeIndex assignInputAnySize(FrameBuildData &frame);
  // SizeIndex assignOutputDynamicSize(FrameBuildData &frame);

  void attachInputs();
  void attachOutputs();
  void finalizeNodeConnections();
  FrameSize resolveFrameSize(FrameBuildData *assignedFrame, const RenderStepOutput &output);
  RenderTargetLayout getLayout(const NodeBuildData &node) const;
  bool isWrittenTo(const FrameBuildData &frame, const decltype(buildingNodes)::const_iterator &it) const;
  void replaceWrittenFrames(const FrameBuildData &frame, FrameBuildData &newFrame);
  void replaceWrittenFramesAfterNode(const FrameBuildData &frame, FrameBuildData &newFrame,
                                     const decltype(buildingNodes)::iterator &it);
};

void RenderGraphBuilder::addNode(const ViewData &viewData, const PipelineStepPtr &step, size_t queueDataIndex) {
  size_t stepIndex = buildingNodes.size();
  auto &buildData = buildingNodes.emplace_back(viewData, stepIndex, step, queueDataIndex);
  std::visit([&](auto &step) { allocateNodeEdges(*this, buildData, step); }, *step.get());
  needPrepare = true;
}

FrameBuildData *RenderGraphBuilder::findFrameByName(const std::string &name) {
  auto it = nameLookup.find(name);
  if (it != nameLookup.end())
    return it->second;
  return nullptr;
}

bool RenderGraphBuilder::isWrittenTo(const FrameBuildData &frame) {
  prepare();
  return isWrittenTo(frame, buildingNodes.end());
}

bool RenderGraphBuilder::isWrittenTo(const std::string &name) {
  prepare();
  auto frame = findFrameByName(name);
  if (!frame)
    return false;
  return isWrittenTo(*frame);
}

FrameBuildData *RenderGraphBuilder::assignFrame(const RenderStepOutput::OutputVariant &output, PipelineStepPtr step,
                                                TextureOverrideRef::Binding bindingType, size_t bindingIndex, bool forceNew) {
  // Try to find existing frame that matches output
  FrameBuildData *frame = //
      forceNew ? nullptr
               : std::visit(
                     [&](auto &&arg) -> FrameBuildData * {
                       using T = std::decay_t<decltype(arg)>;

                       if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
                         auto it0 = handleLookup.find(arg.subResource);
                         if (it0 != handleLookup.end()) {
                           return it0->second;
                         }
                       }

                       auto it1 = nameLookup.find(arg.name);
                       if (it1 != nameLookup.end()) {
                         return it1->second;
                       }

                       return nullptr;
                     },
                     output);

  // Create a new frame
  if (!frame) {
    frame = &buildingFrames.emplace_back();
    std::visit(
        [&](auto &&arg) {
          frame->name = arg.name;
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            assert(arg.subResource);
            frame->format = arg.subResource.texture->getFormat().pixelFormat;
            frame->textureOverride = TextureOverrideRef{
                // .stepIndex = stepIndex,
                .step = step,
                .bindingType = bindingType,
                .bindingIndex = bindingIndex,
            };
            handleLookup.insert_or_assign(arg.subResource, frame);
          } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
            frame->format = arg.format;
          }
          nameLookup.insert_or_assign(frame->name, frame);
        },
        output);
  }

  return frame;
}

void RenderGraphBuilder::replaceWrittenFrames(const FrameBuildData &frame, FrameBuildData &newFrame) {
  replaceWrittenFramesAfterNode(frame, newFrame, buildingNodes.begin());
}

void RenderGraphBuilder::replaceWrittenFramesAfterNode(const FrameBuildData &frame, FrameBuildData &newFrame,
                                                       const decltype(buildingNodes)::iterator &it) {
  for (auto it1 = it; it1 != buildingNodes.end(); ++it1) {
    for (auto &attachment : it1->attachments) {
      if (attachment.frame == &frame) {
        attachment.frame = &newFrame;
      }
    }
  }
}

void RenderGraphBuilder::attachInputs() {
  size_t inputIndex = 0;
  for (auto &node : buildingNodes) {
    for (auto &frame : node.readsFrom) {
      // frame->textureOverride
      // AttachmentRef attachment{.node = &node, .attachment = frame};
      // buildingInputs.push_back(attachment);
      // frame->binding = buildingInputs.size();
    }
  }
}

void RenderGraphBuilder::attachOutputs() {
  buildingOutputs.resize(outputs.size());

  std::list<size_t> outputsToAttach;
  for (size_t i = 0; i < outputs.size(); i++)
    outputsToAttach.push_back(i);

  // Iterate over nodes in reverse, attaching the output to the first matching frame name
  for (auto nodeIt = buildingNodes.rbegin(); nodeIt != buildingNodes.rend(); ++nodeIt) {
    if (outputsToAttach.empty())
      break;

    for (auto outIt = outputsToAttach.begin(); outIt != outputsToAttach.end();) {
      size_t outputIndex = *outIt;
      auto &output = outputs[outputIndex];
      auto &node = *nodeIt;
      for (auto &attachment : node.attachments) {
        auto &frame = *attachment.frame;
        if (frame.name == output.name) {
          auto &newFrame = buildingFrames.emplace_back();
          newFrame.name = frame.name;
          // newFrame.size = frame.size;
          newFrame.format = output.format;
          // newFrame.outputIndex.emplace(outputIndex);
          nameLookup[frame.name] = &newFrame;

          // Make sure no node after this references this as an input anymore
          // TODO: support this case by forcing a copy from intermediate texture to output
          // checkNoReadFrom(i + 1, prevFrameIndex);

          // Replace the output frame
          replaceWrittenFrames(frame, newFrame);

          // Reference frame in output description
          // buildingOutputs[outputIndex].frame = &newFrame;

          // This output is now hooked up, remove it from the queue
          outIt = outputsToAttach.erase(outIt);
          goto nextOutput;
        }
      }
      ++outIt;
    nextOutput:;
    }
  }

  // Warn about unmapped outputs
  for (auto &failed : outputsToAttach) {
    auto &output = outputs[failed];
    SPDLOG_LOGGER_WARN(logger, "Failed to attach render graph output {} ({}), possibly because no frame writes to it.", failed,
                       output.name);
  }
}

FrameSize RenderGraphBuilder::resolveFrameSize(FrameBuildData *assignedFrame, const RenderStepOutput &output) {
  std::visit(
      [&](auto &arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToInputSize>) {
        } else {
          assert(assignedFrame->sizing);
          return *assignedFrame->sizing;
        }
      },
      output.outputSizing);
}

void RenderGraphBuilder::finalizeNodeConnections() {
  size_t nodeIndex{};
  for (auto it = buildingNodes.begin(); it != buildingNodes.end(); ++it, ++nodeIndex) {
    NodeBuildData &nodeBuildData = *it;

    // Reset
    nodeBuildData.sizeConstraints.clear();

    // Assign all input sizes (if not done already), pick the first one as reference size
    // std::optional<SizeIndex> mainInputSizeId;
    // boost::container::flat_map<std::string, FrameBuildData *> inputFrameMap;
    // for (auto &inputFrame : nodeBuildData.readsFrom) {
    //   inputFrameMap[inputFrame->name] = inputFrame;

    // assignInputAnySize(*inputFrame);
    // if (!mainInputSizeId.has_value()) {
    // mainInputSizeId = inputFrame->sizeId.value();
    //   if (!inputFrame->sizeId) {
    //     inputFrame->sizeId = sizeDefinitions.size();
    //     sizeDefinitions.push_back(SizeBuildData{.originalFrame = inputFrame});
    //   }
    //   mainInputSizeId = inputFrame->sizeId;
    // } else {
    //   if (!inputFrame->sizeId) {
    //     inputFrame->sizeId = mainInputSizeId;
    //   } else {
    // SizeConstraintBuildData c{*inputSizeId, *inputFrame->sizeId};
    // if (c.a == c.b)
    //   continue;
    // if (std::find(sizeConstraints.begin(), sizeConstraints.end(), c) == sizeConstraints.end()) {
    //   sizeConstraints.push_back(c);
    // }
    // }
    // }
    // }

    // Connect relative output sizes
    for (size_t attachmentIdx = 0; attachmentIdx < nodeBuildData.attachments.size(); attachmentIdx++) {
      auto &attachment = nodeBuildData.attachments[attachmentIdx];
      auto frame = attachment.frame;

      // Solve relative size
      if (!frame->sizing) {
        assert(frame->deferredFrameSize && "Invalid frame, unknown size");
        std::optional<RenderStepOutput::RelativeToInputSize> size;
        frame->deferredFrameSize.swap(size);

        FrameBuildData *inputFrame{};
        if (size->name) {
          std::string frameKey = std::move(size->name.value());
          inputFrame = findFrameByName(size->name.value());
          if (!inputFrame) {
            throw std::runtime_error(fmt::format("Invalid relative size of output {} to node {}, the input \"{}\" does not exist",
                                                 attachmentIdx, nodeIndex, *size->name));
          }
        } else {
          if (nodeBuildData.readsFrom.size() == 0) {
            throw std::runtime_error(
                fmt::format("Invalid relative size of output {} to node {}, no input to reference", attachmentIdx, nodeIndex));
          }
          inputFrame = nodeBuildData.readsFrom[0];
        }

        // This should've been assigned
        if (!inputFrame->sizing) {
          throw std::runtime_error(fmt::format("Invalid input \"{}\" to node {}", inputFrame->name, nodeIndex));
        }

        // Update relative frame size
        frame->sizing = FrameSize::RelativeToFrame{.frame = inputFrame, .scale = size->scale};
      }
    }

    // Check written attachments
    std::optional<SizeIndex> outputSizeId;
    for (size_t attachmentIdx = 0; attachmentIdx < nodeBuildData.attachments.size(); attachmentIdx++) {
      auto &attachment = nodeBuildData.attachments[attachmentIdx];
      auto frame = attachment.frame;

      WGPUTextureFormat targetFormat{};
      std::optional<ClearValues> clearValues{};
      std::visit(
          [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            clearValues = arg.clearValues;
            if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
              targetFormat = arg.format;
            } else if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
              targetFormat = arg.subResource.texture->getFormat().pixelFormat;
            } else {
              throw std::logic_error("");
            }
          },
          attachment.output);

      // Assign output size
      // if (!frame->sizeId) {
      //   if (frame->sizing) {
      //     // frame->sizeId = assignOutputRefSize(*frame, *nodeBuildData.readsFrom[0], frame->sizeScale.value());
      //   } else {
      //     // Should have been assigned during allocateOutputs
      //     assert(frame->sizeId);
      //   }
      // }

      // Add size dependency
      // if (attachmentIdx > 0) {
      //   auto &prevAttachment = nodeBuildData.attachments[attachmentIdx - 1];
      //   assert(prevAttachment.frame->sizeId);
      //   size_t prevSizeId = prevAttachment.frame->sizeId.value();
      //   auto &prevBsd = buildingSizeDefinitions[prevSizeId];
      //   auto &bsd = buildingSizeDefinitions[*frame->sizeId];
      //   if (bsd.isDynamic || prevBsd.isDynamic) {
      //     nodeBuildData.sizeConstraints.emplace_back(*frame->sizeId, prevSizeId);
      //   }
      // }

      // if (mainInputSizeId.has_value()) {
      // }
      // attachment.targetSize
      // if (nodeBuildData.outputSize) {
      //   if (attachment.targetSize != *nodeBuildData.outputSize)
      //     throw std::runtime_error(fmt::format("Output size of attachment {} ({}) does not match previous attachments of size
      //     {}",
      //                                          frame->name, frame->size, *nodeBuildData.outputSize));
      // } else {
      //   nodeBuildData.outputSize = attachment.targetSize;
      // }

      // bool sizeMismatch = frame->size != attachment.targetSize;
      bool formatMismatch = frame->format != targetFormat;

      bool loadRequired = !clearValues.has_value();
      if (nodeBuildData.forceOverwrite) {
        loadRequired = false;
      }

      // Check if the same frame is being read from
      auto readFromIt = std::find(nodeBuildData.readsFrom.begin(), nodeBuildData.readsFrom.end(), frame);
      if (/* sizeMismatch || */ formatMismatch || readFromIt != nodeBuildData.readsFrom.end()) {
        // TODO: Implement copy into loaded attachment from previous step
        if (loadRequired) {
          std::string err = "Copy required, not implemented";
          // if (sizeMismatch)
          //   err += fmt::format(" (size {}=>{})", frame->size, attachment.targetSize);
          if (formatMismatch)
            err += fmt::format(" (fmt {}=>{})", magic_enum::enum_name(frame->format), magic_enum::enum_name(targetFormat));
          throw std::logic_error(err);
        }

        // Just duplicate the frame for now
        FrameBuildData &newFrame = buildingFrames.emplace_back();
        newFrame.name = frame->name;
        // newFrame.sizeId = *mainInputSizeId;
        // newFrame.size = attachment.targetSize;
        newFrame.format = targetFormat;
        nameLookup[frame->name] = &newFrame;
        attachment.frame = &newFrame;
        replaceWrittenFramesAfterNode(*frame, newFrame, it);
        frame = &newFrame;
      }

      // Check if written, force clear with default values if not
      bool needsClearValue = false;
      if (nodeBuildData.forceOverwrite)
        needsClearValue = true;
      else if (loadRequired && !isWrittenTo(*frame, it)) {
        SPDLOG_LOGGER_DEBUG(
            logger, "Forcing clear with default values for frame {} ({}) accessed by node {}, since it's not written to before",
            (void *)frame, frame->name, std::distance(buildingNodes.begin(), it));
        needsClearValue = true;
      }

      // Set default clear value based on format
      if (needsClearValue) {
        auto &formatDesc = getTextureFormatDescription(targetFormat);
        if (hasAnyTextureFormatUsage(formatDesc.usage, TextureFormatUsage::Depth | TextureFormatUsage::Stencil)) {
          clearValues = ClearValues::getDefaultDepthStencil();
        } else {
          clearValues = ClearValues::getDefaultColor();
        }
      }

      if (clearValues.has_value()) {
        attachment.clearValues = clearValues.value();
      }
    }
  }
}

void RenderGraphBuilder::prepare() {
  if (!needPrepare)
    return;

  // Fixup all read/write connections
  finalizeNodeConnections();

  // Attach outputs to nodes
  attachOutputs();

  needPrepare = false;
}

// SizeIndex RenderGraphBuilder::assignOutputRefSize(FrameBuildData &frame, FrameBuildData &referenceFrame, float2 sizeScale) {
//   assert(referenceFrame.sizeId);
//   if (!frame.sizeId) {
//     auto sbd =
//         SizeBuildData{.originalFrame = &referenceFrame, .originalSizeId = referenceFrame.sizeId.value(), .sizeScale =
//         sizeScale};
//     auto it = std::find(buildingSizeDefinitions.begin(), buildingSizeDefinitions.end(), sbd);
//     if (it != buildingSizeDefinitions.end()) {
//       frame.sizeId = it - buildingSizeDefinitions.begin();
//     } else {
//       frame.sizeId = buildingSizeDefinitions.size();
//       buildingSizeDefinitions.push_back(sbd);
//     }
//   }
//   return *frame.sizeId;
// }

// SizeIndex RenderGraphBuilder::assignInputAnySize(FrameBuildData &frame) { return assignOutputDynamicSize(frame); }

// SizeIndex RenderGraphBuilder::assignOutputDynamicSize(FrameBuildData &frame) {
//   if (!frame.sizeId) {
//     frame.sizeId = buildingSizeDefinitions.size();
//     buildingSizeDefinitions.push_back(SizeBuildData{.originalFrame = &frame, .isDynamic = true});
//   }
//   return *frame.sizeId;
// }

RenderGraph RenderGraphBuilder::build() {
  prepare();

  RenderGraph outputGraph;

  // Find used frames
  std::vector<FrameBuildData *> usedFrames;
  std::unordered_map<FrameBuildData *, FrameIndex> usedFrameLookup;
  auto addUsedFrame = [&](FrameBuildData *frame) {
    if (!usedFrameLookup.contains(frame)) {
      usedFrameLookup[frame] = usedFrames.size();
      usedFrames.emplace_back(frame);
    }
  };
  for (auto &buildingNode : buildingNodes) {
    for (auto &frame : buildingNode.readsFrom) {
      addUsedFrame(frame);
    }
    for (auto &attachment : buildingNode.attachments) {
      addUsedFrame(attachment.frame);
    }
  }

  // Add frames to graph
  for (auto &usedFrame : usedFrames) {
    auto &frame = outputGraph.frames.emplace_back();
    frame.format = usedFrame->format;
    frame.name = usedFrame->name;
    // frame.size = usedFrame->size;
    // frame.outputIndex = usedFrame->outputIndex;
    // frame.textureOverride = usedFrame->textureOverride;
  }

  size_t globalReferenceSize = ~0;
  std::unordered_map<SizeIndex, SizeIndex> sizeRemapping;

  // Simplifies a given size index to the lowest size index that is an alias for the same size
  auto simplifySize = [&](SizeIndex sizeId) {
    SizeIndex remapped = sizeId;
    SizeIndex remappedMin = sizeId;
    do {
      auto it = sizeRemapping.find(sizeId);
      if (it == sizeRemapping.end())
        break;
      remapped = it->second;
      remappedMin = std::min(remapped, remappedMin);
    } while (remapped != sizeId); // Break on recursion
    return remappedMin;
  };

  // Remap sizes
  struct Constraint {
    SizeIndex parent;
    std::optional<float2> scale;

    Constraint(SizeIndex parent) : parent(parent) {}
    Constraint(SizeIndex parent, float2 scale) : parent(parent), scale(scale) {}
  };

  std::unordered_map<SizeIndex, boost::container::small_vector<Constraint, 8>> sizeConstraints;
  std::vector<boost::container::small_vector<FrameBuildData *, 8>> sizeConstraints2;

  std::vector<SizeIndex> frameSizes;
  std::map<FrameSizing, SizeIndex> frameSizeMap;

  for (auto &node : buildingNodes) {

    for (auto &in : node.readsFrom) {
      assert(in->sizing);
      // frameSizeMap.
    }

    auto assignFrameSize = [&](FrameBuildData *frame) {
      assert(frame->sizing);
      return std::visit(
          [&](auto &arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, FrameSize::Manual>) {
              SizeIndex newSizeIndex = frameSizes.size();
              frameSizes.emplace_back();
              return newSizeIndex;
            } else if constexpr (std::is_same_v<T, FrameSize::RelativeToMain>) {
              auto it = frameSizeMap.find(arg);
              if (it != frameSizeMap.end()) {
                return it->second;
              } else {
                SizeIndex newSizeIndex = frameSizes.size();
                frameSizes.emplace_back();
                frameSizeMap.insert_or_assign(arg, newSizeIndex);
                return newSizeIndex;
              }
            } else if constexpr (std::is_same_v<T, FrameSize::RelativeToFrame>) {
              // Create new relative size
              auto it2 = frameSizeMap.find(arg);
              if (it2 != frameSizeMap.end()) {
                return it2->second;
              } else {
                assert(arg.frame->sizing);
                auto it = frameSizeMap.find(*arg.frame->sizing);
                if (it == frameSizeMap.end()) {
                  throw std::logic_error("Dependent frame does not have a valid size");
                } else {
                  if (!arg.scale) {
                    return it->second;
                  } else {
                    SizeIndex newSizeIndex = frameSizes.size();
                    frameSizes.emplace_back();
                    frameSizeMap.insert_or_assign(arg, newSizeIndex);
                    return newSizeIndex;
                  }
                }
              }
            } else {
              throw std::logic_error("");
            }
          },
          *frame->sizing);
    };

    if (!node.attachments.empty()) {
      auto firstOutputFrame = node.attachments[0].frame;
      SizeIndex firstSize = assignFrameSize(firstOutputFrame);
      if (node.attachments.size() > 1) {
        auto &constraints = sizeConstraints2.emplace_back();
        constraints.push_back(firstOutputFrame);

        for (size_t i = 1; i < node.attachments.size(); i++) {
          auto &output = node.attachments[i];
          SizeIndex size = assignFrameSize(firstOutputFrame);
          // if (size != firstSize) {
          constraints.push_back(output.frame);
          // }
        }
      }
    }
  }

  // std::optional<size_t> refSize;
  // // Try to derive size from dynamic output
  // for (auto &attachment : node.attachments) {
  //   auto &frame = *attachment.frame;
  //   if (frame.sizeId) {
  //     refSize = frame.sizeId;
  //   }
  // }
  // // Try to derive size from inputs
  // if (!refSize) {
  //   for (auto &input : node.readsFrom) {
  //     refSize = *input->sizeId;
  //   }
  // }
  // // Fallback to using the global reference size
  // if (!refSize)
  //   refSize = globalReferenceSize;

  // for (auto &output : node.attachments) {
  //   auto &frame = output.frame;
  //   assert(frame->sizeId);
  //   if (frame->sizeScale) {
  //     sizeConstraints[*frame->sizeId].emplace_back(*refSize, *frame->sizeScale);
  //   } else {
  //     sizeConstraints[*frame->sizeId].emplace_back(*refSize);
  //   }
  // }

  // for (auto &[size, constraints] : sizeConstraints) {
  //   SPDLOG_LOGGER_DEBUG(getLogger(), "Size {} has {} constraints: ", size, constraints.size());
  //   for (auto &c : constraints) {
  //     if (c.scale) {
  //       SPDLOG_LOGGER_DEBUG(getLogger(), "  - parent: {} x {}", c.parent, c.scale.value());
  //     } else {
  //       SPDLOG_LOGGER_DEBUG(getLogger(), "  - parent: {}", c.parent);
  //     }
  //   }
  // }

  // Add size constraints
  // auto &sizeConstraints = outputGraph.sizeConstraints;
  // std::unordered_map<size_t, size_t> frameToSizeGroupMap;
  // for (auto &frame : usedFrames) {
  //   size_t frameIndex = usedFrameLookup[frame];
  //   auto it = sizeRemapping.find(frame->sizeId.value());
  //   if (it == sizeRemapping.end()) {
  //     auto &newSize = sizeConstraints.emplace_back();

  //     newSize.frames.push_back(frameIndex);
  //     auto &sizeDef = buildingSizeDefinitions[frame->sizeId.value()];
  //     if (sizeDef.originalFrame) {
  //       size_t parentFrameIndex = usedFrameLookup[sizeDef.originalFrame];
  //       auto originalSize = frameToSizeGroupMap[parentFrameIndex];
  //       auto &parent = newSize.parent.emplace();
  //       parent.sizeScale = sizeDef.sizeScale.value();
  //       parent.parentSize = originalSize;
  //     }

  //     it = sizeRemapping.emplace(frame->sizeId.value(), sizeConstraints.size() - 1).first;
  //   }
  //   frameToSizeGroupMap[frameIndex] = it->second;
  // }

  // for (auto &node : buildingNodes) {
  //   for (auto &sc : node.sizeConstraints) {
  //     auto idxA = usedFrameLookup[&buildingFrames[sc.a]];
  //     auto idxB = usedFrameLookup[&buildingFrames[sc.b]];
  //     auto itA = frameToSizeGroupMap.find(idxA);
  //     auto itB = frameToSizeGroupMap.find(idxB);
  //     if (itA != frameToSizeGroupMap.end() && itB != frameToSizeGroupMap.end()) {
  //       // Merge groups
  //       size_t sizeGroupId = std::min(itA->second, itB->second);
  //       frameToSizeGroupMap[idxA] = sizeGroupId;
  //       frameToSizeGroupMap[idxB] = sizeGroupId;
  //     } else {
  //       if (itB != frameToSizeGroupMap.end()) {
  //         std::swap(itA, itB);
  //       }
  //       if (itA == frameToSizeGroupMap.end()) {
  //         itA = frameToSizeGroupMap.emplace(idxA, frameToSizeGroupMap.size()).first;
  //       }
  //       frameToSizeGroupMap[idxB] = itA->second;
  //     }
  //   }
  // }

  // for (auto &[frameIndex, sizeGroupIndex] : frameToSizeGroupMap) {
  //   sizeConstraints.resize(std::max(sizeConstraints.size(), sizeGroupIndex + 1));
  //   sizeConstraints[sizeGroupIndex].frames.push_back(frameIndex);
  // }

  // Check for any frame that doesn't have a size constraint

  // for (auto [frame, idx] : usedFrameLookup) {
  //   auto it = sizeGroupMap.find(*frame->sizeId);
  //   if (it == sizeGroupMap.end()) {
  //     // size_t sizeGroupId = outputGraph.sizeGroups.size();
  //     sizeGroupMap[frame->sizeId] = sizeGroupId;
  //     auto &sizeGroup = outputGraph.sizeGroups.emplace_back();
  //     sizeGroup.frames.push_back(idx);
  //   } else {
  //     auto &sizeGroup = outputGraph.sizeGroups[it->second];
  //     sizeGroup.frames.push_back(idx);
  //   }
  // }

  // Add outputs to graph
  assert(buildingOutputs.size() == outputs.size());
  for (size_t i = 0; i < buildingOutputs.size(); i++) {
    auto &output = outputGraph.outputs.emplace_back();
    output.frameIndex = usedFrameLookup[buildingOutputs[i].attachment];
    output.name = outputs[i].name;
  }

  // Add nodes to graph
  for (auto &buildingNode : buildingNodes) {
    auto &outputNode = outputGraph.nodes.emplace_back();
    outputNode.renderTargetLayout = getLayout(buildingNode);

    outputNode.queueDataIndex = buildingNode.queueDataIndex;

    // if (!buildingNode.outputSize) {
    //   throw std::runtime_error("Node does not have any outputs");
    // }
    // outputNode.outputSize = buildingNode.outputSize.value();

    for (auto &attachment : buildingNode.attachments) {
      auto &frame = attachment.frame;
      auto frameIndex = usedFrameLookup[frame];

      auto &writeTo = outputNode.writesTo.emplace_back(frameIndex);
      writeTo.clearValues = attachment.clearValues;
    }

    for (auto &frame : buildingNode.readsFrom) {
      auto frameIndex = usedFrameLookup[frame];
      outputNode.readsFrom.emplace_back(frameIndex);
    }

    // Setup step rendering callbacks
    std::visit([&](auto &step) { setupRenderGraphNode(outputNode, buildingNode, step); }, *buildingNode.step.get());

    // This value should be set by the setup callback above
    // so run it after
    for (auto &autoClearQueues : buildingNode.autoClearQueues) {
      outputGraph.autoClearQueues.push_back(autoClearQueues);
    }
  }

  return outputGraph;
}

bool RenderGraphBuilder::isWrittenTo(const FrameBuildData &targetFrame,
                                     const decltype(buildingNodes)::const_iterator &itEnd) const {
  for (auto it1 = buildingNodes.cbegin(); it1 != itEnd; ++it1) {
    for (auto &attachment : it1->attachments) {
      if (attachment.frame == &targetFrame)
        return true;
    }
  }
  return false;
}

//   void checkNoReadFrom(size_t startNodeIndex, FrameIndex frameIndex) {
//     // Make sure no node after this references this as an input anymore
//     for (size_t i1 = startNodeIndex; i1 < nodes.size(); i1++) {
//       auto &node = nodes[i1];
//       if (std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex) != node.readsFrom.end())
//         throw std::logic_error("Cannot attach graph output");
//     }
//   }

// Call before allocating a node's outputs
void RenderGraphBuilder::allocateInputs(NodeBuildData &buildData, const RenderStepInput &input_) {
  size_t inputIndex{};
  for (auto &input : input_.attachments) {
    std::visit(
        [&](auto &&arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepInput::Named>) {
            if (FrameBuildData *frame = findFrameByName(arg.name)) {
              buildData.readsFrom.push_back(frame);
            }
          } else if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
            // Allocate a new frame based on this input and assign it
            auto frame = assignFrame(RenderStepOutput::Texture(arg.name, arg.subResource), buildData.step,
                                     TextureOverrideRef::Binding::Input, inputIndex);
            frame->sizing = FrameSize::Manual{};
            buildData.readsFrom.push_back(frame);
          } else {
            throw std::logic_error("Unknown variant");
          }
        },
        input);
    ++inputIndex;
  }
}

// When setting forceOverwrite, ignores clear value, marks this attachment as being completely overwritten
// e.g. fullscreen effect passes
void RenderGraphBuilder::allocateOutputs(NodeBuildData &nodeBuildData, const RenderStepOutput &output, bool forceOverwrite) {
  nodeBuildData.forceOverwrite = forceOverwrite;
  nodeBuildData.outputSizing = output.outputSizing;

  size_t outputIndex{};
  for (auto &attachment : output.attachments) {
    // int2 targetSize{};
    // std::visit(
    //     [&](auto &&arg) -> int2 {
    //       using T = std::decay_t<decltype(arg)>;
    //       if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {

    //       } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
    //       } else {
    //         throw std::logic_error("Invalid variant");
    //       }
    //     },
    //     attachment);
    // if (targetSize.x <= 0 || targetSize.y <= 0)
    //   throw std::logic_error("Invalid output size");

    FrameBuildData *outputFrame = assignFrame(attachment, nodeBuildData.step, TextureOverrideRef::Binding::Output, outputIndex);
    // if(outputFrame

    // Assign this frame a fixed size if it doesn't have a relative scale
    // Relative scale is assigned later during node connection validation
    FrameSizing expectedSizing;
    std::visit(
        [&](auto &arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToMainSize>) {
            expectedSizing = FrameSize::RelativeToMain{.scale = arg.scale};
          } else if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToInputSize>) {
            // Defer until node connection
            // outputFrame->deferredFrameSize.emplace(arg);
          } else if constexpr (std::is_same_v<T, RenderStepOutput::ManualSize>) {
            expectedSizing = FrameSize::Manual{};
          }

          if (!outputFrame->sizing) {
            outputFrame->sizing = expectedSizing;
          } else if (expectedSizing != outputFrame->sizing.value()) {
            SPDLOG_LOGGER_DEBUG(getLogger(), "Frame size missmatch, creating new frame");
            outputFrame = assignFrame(attachment, nodeBuildData.step, TextureOverrideRef::Binding::Output, outputIndex, true);
          }
        },
        output.outputSizing);
    // if (!output.outputSizing) {
    //   assignOutputDynamicSize(*outputFrame);
    // } else {
    //   outputFrame->sizeScale = output.sizeScale;
    // }

    nodeBuildData.attachments.push_back(Attachment{attachment, outputFrame});
    ++outputIndex;
  }
}

RenderTargetLayout RenderGraphBuilder::getLayout(const NodeBuildData &node) const {
  RenderTargetLayout layout{};

  for (auto &attachment : node.attachments) {
    auto &frame = *attachment.frame;

    std::string name = frame.name;
    if (name == "depth") {
      layout.depthTargetIndex = layout.targets.size();
    }

    layout.targets.emplace_back(RenderTargetLayout::Target{
        .name = std::move(name),
        .format = frame.format,
    });
  }

  return layout;
}
} // namespace graph_build_data
using RenderGraphBuilder = graph_build_data::RenderGraphBuilder;

} // namespace gfx::detail

#endif /* EF9289B8_FECE_4D29_8B3F_43A88E82C83B */
