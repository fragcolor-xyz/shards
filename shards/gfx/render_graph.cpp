#include "render_graph.hpp"
#include "render_step_impl.hpp"
#include "renderer_storage.hpp"
#include "spdlog/spdlog.h"
#include <unordered_map>

namespace gfx::detail {

using Data = RenderGraphBuilder::NodeBuildData;
static auto logger = gfx::getLogger();

void RenderGraphBuilder::addNode(const ViewData &viewData, const PipelineStepPtr &step, size_t queueDataIndex) {
  size_t stepIndex = buildingNodes.size();
  auto &buildData = buildingNodes.emplace_back(viewData, stepIndex, step, queueDataIndex);
  std::visit([&](auto &step) { allocateNodeEdges(*this, buildData, step); }, *step.get());
  needPrepare = true;
}

RenderGraphBuilder::FrameBuildData *RenderGraphBuilder::findFrameByName(const std::string &name) {
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

RenderGraphBuilder::FrameBuildData *RenderGraphBuilder::assignFrame(const RenderStepOutput::OutputVariant &output,
                                                                    PipelineStepPtr step, TextureOverrideRef::Binding bindingType,
                                                                    size_t bindingIndex) {
  // Try to find existing frame that matches output
  FrameBuildData *frame = std::visit(
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
                step = 
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

void RenderGraphBuilder::finalizeNodeConnections() {
  for (auto it = buildingNodes.begin(); it != buildingNodes.end(); ++it) {
    NodeBuildData &nodeBuildData = *it;

    // Reset
    nodeBuildData.sizeConstraints.clear();

    // Assign all input sizes (if not done already), pick the first one as reference size
    std::optional<size_t> mainInputSizeId;
    for (auto &inputFrame : nodeBuildData.readsFrom) {
      assignInputAnySize(*inputFrame);
      if (!mainInputSizeId.has_value()) {
        mainInputSizeId = inputFrame->sizeId.value();
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
      }
    }

    // Check written attachments
    std::optional<size_t> outputSizeId;
    // for (auto &attachment : nodeBuildData.attachments) {
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
      if (!frame->sizeId) {
        if (frame->sizeScale) {
          frame->sizeId = assignOutputRefSize(*frame, *nodeBuildData.readsFrom[0], frame->sizeScale.value());
        } else {
          // Should have been assigned during allocateOutputs
          assert(frame->sizeId);
        }
      }

      // Add size dependency
      if (attachmentIdx > 0) {
        auto &prevAttachment = nodeBuildData.attachments[attachmentIdx - 1];
        assert(prevAttachment.frame->sizeId);
        size_t prevSizeId = prevAttachment.frame->sizeId.value();
        auto &prevBsd = buildingSizeDefinitions[prevSizeId];
        auto &bsd = buildingSizeDefinitions[*frame->sizeId];
        if (bsd.isDynamic || prevBsd.isDynamic) {
          nodeBuildData.sizeConstraints.emplace_back(*frame->sizeId, prevSizeId);
        }
      }

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
        newFrame.sizeId = *mainInputSizeId;
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

size_t RenderGraphBuilder::assignOutputRefSize(FrameBuildData &frame, FrameBuildData &referenceFrame, float2 sizeScale) {
  assert(referenceFrame.sizeId);
  if (!frame.sizeId) {
    auto sbd =
        SizeBuildData{.originalFrame = &referenceFrame, .originalSizeId = referenceFrame.sizeId.value(), .sizeScale = sizeScale};
    auto it = std::find(buildingSizeDefinitions.begin(), buildingSizeDefinitions.end(), sbd);
    if (it != buildingSizeDefinitions.end()) {
      frame.sizeId = it - buildingSizeDefinitions.begin();
    } else {
      frame.sizeId = buildingSizeDefinitions.size();
      buildingSizeDefinitions.push_back(sbd);
    }
  }
  return *frame.sizeId;
}

size_t RenderGraphBuilder::assignInputAnySize(FrameBuildData &frame) { return assignOutputFixedSize(frame); }

size_t RenderGraphBuilder::assignOutputFixedSize(FrameBuildData &frame) {
  if (!frame.sizeId) {
    frame.sizeId = buildingSizeDefinitions.size();
    buildingSizeDefinitions.push_back(SizeBuildData{.originalFrame = &frame, .isDynamic = true});
  }
  return *frame.sizeId;
}

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

  // Add size constraints
  auto &sizeConstraints = outputGraph.sizeConstraints;
  std::unordered_map<size_t, size_t> frameToSizeGroupMap;
  std::unordered_map<size_t, size_t> sizeRemapping;
  for (auto &frame : usedFrames) {
    size_t frameIndex = usedFrameLookup[frame];
    auto it = sizeRemapping.find(frame->sizeId.value());
    if (it == sizeRemapping.end()) {
      auto &newSize = sizeConstraints.emplace_back();

      newSize.frames.push_back(frameIndex);
      auto &sizeDef = buildingSizeDefinitions[frame->sizeId.value()];
      if (sizeDef.originalFrame) {
        size_t parentFrameIndex = usedFrameLookup[sizeDef.originalFrame];
        auto originalSize = frameToSizeGroupMap[parentFrameIndex];
        auto &parent = newSize.parent.emplace();
        parent.sizeScale = sizeDef.sizeScale.value();
        parent.parentSize = originalSize;
      }

      it = sizeRemapping.emplace(frame->sizeId.value(), sizeConstraints.size() - 1).first;
    }
    frameToSizeGroupMap[frameIndex] = it->second;
  }

  for (auto &node : buildingNodes) {
    for (auto &sc : node.sizeConstraints) {
      auto idxA = usedFrameLookup[&buildingFrames[sc.a]];
      auto idxB = usedFrameLookup[&buildingFrames[sc.b]];
      auto itA = frameToSizeGroupMap.find(idxA);
      auto itB = frameToSizeGroupMap.find(idxB);
      if (itA != frameToSizeGroupMap.end() && itB != frameToSizeGroupMap.end()) {
        // Merge groups
        size_t sizeGroupId = std::min(itA->second, itB->second);
        frameToSizeGroupMap[idxA] = sizeGroupId;
        frameToSizeGroupMap[idxB] = sizeGroupId;
      } else {
        if (itB != frameToSizeGroupMap.end()) {
          std::swap(itA, itB);
        }
        if (itA == frameToSizeGroupMap.end()) {
          itA = frameToSizeGroupMap.emplace(idxA, frameToSizeGroupMap.size()).first;
        }
        frameToSizeGroupMap[idxB] = itA->second;
      }
    }
  }

  for (auto &[frameIndex, sizeGroupIndex] : frameToSizeGroupMap) {
    sizeConstraints.resize(std::max(sizeConstraints.size(), sizeGroupIndex + 1));
    sizeConstraints[sizeGroupIndex].frames.push_back(frameIndex);
  }

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
            auto frame = assignFrame(
                RenderStepOutput::Texture{
                    .name = arg.name,
                    .subResource = arg.subResource,
                },
                buildData.step, TextureOverrideRef::Binding::Input, inputIndex);
            assignInputAnySize(*frame);
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

    FrameBuildData *outputFrame =
        assignFrame(attachment, nodeBuildData.stepIndex, TextureOverrideRef::Binding::Output, outputIndex);

    // Assign this frame a fixed size if it doesn't have a relative scale
    // Relative scale is assigned later during node connection validation
    if (!output.sizeScale) {
      assignOutputFixedSize(*outputFrame);
    } else {
      outputFrame->sizeScale = output.sizeScale;
    }

    nodeBuildData.attachments.push_back(RenderGraphBuilder::Attachment{attachment, outputFrame});
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

RenderGraphEvaluator::RenderGraphEvaluator(allocator_type allocator, Renderer &renderer, RendererStorage &storage)
    : allocator(allocator), writtenTextures(allocator), frameTextures(allocator), renderer(renderer), storage(storage) {}

void RenderGraphEvaluator::getFrameTextures(shards::pmr::vector<ResolvedFrameTexture> &outFrameTextures,
                                            const std::span<TextureSubResource> &outputs, const RenderGraph &graph,
                                            Context &context) {
  outFrameTextures.clear();
  outFrameTextures.reserve(graph.frames.size());

  // Entire texture view
  auto resolve = [&](TexturePtr texture) {
    return ResolvedFrameTexture(texture, storage.getTextureView(texture->createContextDataConditional(context), 0, 0));
  };

  // Subresource texture view
  auto resolveSubResourceView = [&](const TextureSubResource &resource) {
    return ResolvedFrameTexture(resource.texture, storage.getTextureView(resource.texture->createContextDataConditional(context),
                                                                         resource.faceIndex, resource.mipIndex));
  };

  for (auto &frame : graph.frames) {
    std::visit(
        [](auto &binding) {
          using T = std::decay_t<decltype(binding)>;
          if constexpr (std::is_same_v<T, RenderGraph::OutputIndex>) {
          } else if constexpr (std::is_same_v<T, RenderGraph::TextureOverrideRef>) {
            // auto& step = this-> binding.stepIndex
          }
        },
        frame.binding);
    // if (frame.outputIndex.has_value()) {
    //   size_t outputIndex = frame.outputIndex.value();
    //   if (outputIndex >= outputs.size())
    //     throw std::logic_error("Missing output");

    //   outFrameTextures.push_back(resolveSubResourceView(outputs[outputIndex]));
    // } else if (frame.textureOverride) {
    //   // Update the texture size/format
    //   auto desc = frame.textureOverride.texture->getDesc();
    //   desc.resolution = frame.size;
    //   desc.format.pixelFormat = frame.format;
    //   frame.textureOverride.texture->init(desc);
    //   outFrameTextures.push_back(resolveSubResourceView(frame.textureOverride));
    // } else {
    //   TexturePtr texture = storage.renderTextureCache.allocate(RenderTargetFormat{.format = frame.format, .size = frame.size},
    //                                                            storage.frameCounter);
    //   outFrameTextures.push_back(resolve(texture));
    // }
  }
}

WGPURenderPassDescriptor RenderGraphEvaluator::createRenderPassDescriptor(const RenderGraph &graph, Context &context,
                                                                          const RenderGraphNode &node) {
  // NOTE: Allocated in frame memory
  WGPURenderPassDepthStencilAttachment *depthStencilAttachmentPtr{};
  auto &colorAttachments = *allocator.new_object<shards::pmr::vector<WGPURenderPassColorAttachment>>();

  size_t depthIndex = node.renderTargetLayout.depthTargetIndex.value_or(~0);
  for (size_t i = 0; i < node.writesTo.size(); i++) {
    const RenderGraphNode::Attachment &nodeAttachment = node.writesTo[i];
    ResolvedFrameTexture &resolvedFrameTexture = frameTextures[nodeAttachment.frameIndex];

    if (i == depthIndex) {
      auto &attachment = *(depthStencilAttachmentPtr = allocator.new_object<WGPURenderPassDepthStencilAttachment>());
      attachment = WGPURenderPassDepthStencilAttachment{
          .view = resolvedFrameTexture.view,
      };

      if (nodeAttachment.clearValues) {
        auto &clearDepth = nodeAttachment.clearValues.value().depth;
        attachment.depthLoadOp = WGPULoadOp_Clear;
        attachment.depthClearValue = clearDepth;
      } else {
        attachment.depthLoadOp = WGPULoadOp_Load;
      }
      attachment.depthStoreOp = WGPUStoreOp_Store;

      // TODO: Stencil
      uint32_t clearStencil = 0;
      attachment.stencilClearValue = clearStencil;
      attachment.stencilLoadOp = WGPULoadOp_Undefined;
      attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    } else {
      auto &attachment = colorAttachments.emplace_back(WGPURenderPassColorAttachment{
          .view = resolvedFrameTexture.view,
      });

      if (nodeAttachment.clearValues) {
        auto &clearColor = nodeAttachment.clearValues.value().color;
        attachment.loadOp = WGPULoadOp_Clear;
        attachment.clearValue = WGPUColor{clearColor.x, clearColor.y, clearColor.z, clearColor.w};
      } else {
        attachment.loadOp = WGPULoadOp_Load;
      }
      attachment.storeOp = WGPUStoreOp_Store;
    }

    writtenTextures.insert(resolvedFrameTexture.texture.get());
  }

  return WGPURenderPassDescriptor{
      .colorAttachmentCount = uint32_t(colorAttachments.size()),
      .colorAttachments = colorAttachments.data(),
      .depthStencilAttachment = depthStencilAttachmentPtr,
  };
}

void RenderGraphEvaluator::evaluate(const RenderGraph &graph, IRenderGraphEvaluationData &evaluationData,
                                    std::span<TextureSubResource> outputs) {
  // Evaluate all render steps in the order they are defined
  // potential optimizations:
  // - evaluate steps without dependencies in parralel
  // - group multiple steps into single render passes

  auto &context = renderer.getContext();

  // for (const auto &node : graph.nodes) {
  //   if (node.updateOutputs)
  //     node.updateOutputs(const_cast<RenderGraphNode&>(node));
  // }

  // Allocate frame textures
  getFrameTextures(frameTextures, outputs, graph, context);

  WGPUCommandEncoderDescriptor desc = {};
  WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(context.wgpuDevice, &desc);

  static std::shared_ptr<debug::Debugger> NoDebugger;
  auto &debugger = ignoreInDebugger ? NoDebugger : storage.debugger;

  size_t nodeIdx = 0;
  for (const auto &node : graph.nodes) {
    if (debugger)
      debugger->frameQueueRenderGraphNodeBegin(nodeIdx);

    const ViewData &viewData = evaluationData.getViewData(node.queueDataIndex);

    WGPURenderPassDescriptor renderPassDesc = createRenderPassDescriptor(graph, context, node);
    if (node.setupPass)
      node.setupPass(renderPassDesc);
    WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDesc);
    RenderGraphEncodeContext ctx{
        .encoder = renderPassEncoder,
        .viewData = viewData,
        .evaluator = *this,
        .graph = graph,
        .node = node,
        .ignoreInDebugger = ignoreInDebugger,
    };
    if (node.encode)
      node.encode(ctx);
    wgpuRenderPassEncoderEnd(renderPassEncoder);

    if (debugger)
      debugger->frameQueueRenderGraphNodeEnd(*this);
    ++nodeIdx;
  }

  WGPUCommandBufferDescriptor cmdBufDesc{};
  WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

  context.submit(cmdBuf);

  // TODO: move this earlier into the process once rendering is multi-threaded
  for (auto &queue : graph.autoClearQueues) {
    queue->clear();
  }
}

} // namespace gfx::detail
