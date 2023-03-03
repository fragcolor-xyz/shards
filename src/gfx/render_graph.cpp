#include "render_graph.hpp"
#include "render_step_impl.hpp"
#include "renderer_storage.hpp"

namespace gfx::detail {

using Data = RenderGraphBuilder::NodeBuildData;

void RenderGraphBuilder::addNode(const ViewData &viewData, const PipelineStepPtr &step, size_t queueDataIndex) {
  auto &buildData = buildingNodes.emplace_back(viewData, step, queueDataIndex);
  std::visit([&](auto &step) { allocateNodeEdges(*this, buildData, step); }, *step.get());
}

bool RenderGraphBuilder::findFrameIndex(const std::string &name, FrameIndex &outFrameIndex) {
  auto it = nameLookup.find(name);
  if (it == nameLookup.end())
    return false;
  outFrameIndex = it->second;
  return true;
}

FrameIndex RenderGraphBuilder::assignFrame(const RenderStepOutput::OutputVariant &output, int2 targetSize) {
  // Try to find existing frame that matches output
  FrameIndex frameIndex = ~0;
  std::visit(
      [&](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;

        if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
          auto it0 = handleLookup.find(arg.subResource);
          if (it0 == handleLookup.end()) {
            frameIndex = frames.size();
            it0 = handleLookup.insert_or_assign(arg.subResource, frameIndex).first;
          } else {
            frameIndex = it0->second;
          }
        }

        auto it1 = nameLookup.find(arg.name);
        if (it1 == nameLookup.end()) {
          if (frameIndex == size_t(~0))
            frameIndex = frames.size();
          it1 = nameLookup.insert_or_assign(arg.name, frameIndex).first;
        } else {
          frameIndex = it1->second;
        }
      },
      output);

  // Create a new frame
  assert(frameIndex != size_t(~0));
  if (frameIndex >= frames.size()) {
    auto &frame = frames.emplace_back();
    frame.size = targetSize;
    std::visit(
        [&](auto &&arg) {
          frame.name = arg.name;
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            assert(arg.subResource);
            frame.textureOverride = arg.subResource;
            frame.format = frame.textureOverride.texture->getFormat().pixelFormat;
          } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
            frame.format = arg.format;
          }
        },
        output);
  }

  return frameIndex;
}

//   void checkNoReadFrom(size_t startNodeIndex, FrameIndex frameIndex) {
//     // Make sure no node after this references this as an input anymore
//     for (size_t i1 = startNodeIndex; i1 < nodes.size(); i1++) {
//       auto &node = nodes[i1];
//       if (std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex) != node.readsFrom.end())
//         throw std::logic_error("Cannot attach graph output");
//     }
//   }

//   // Updates nodes that write to given frame to write to the new frame instead
//   void replaceFrame(FrameIndex index, FrameIndex newIndex) {
//     for (size_t i = 0; i < nodes.size(); i++) {
//       auto &node = nodes[i];
//       for (auto &attachment : node.writesTo) {
//         if (attachment.frameIndex == index)
//           attachment.frameIndex = newIndex;
//       }
//     }

//     if (externallyWrittenTo.contains(index)) {
//       externallyWrittenTo.erase(index);
//       externallyWrittenTo.insert(newIndex);
//     }
//   }

//   void attachOutput(const std::string &name, TexturePtr texture) {
//     WGPUTextureFormat outputTextureFormat = texture->getFormat().pixelFormat;

//     // Go in reverse, and replace the last write to the given name to this texture
//     for (size_t ir = 0; ir < nodes.size(); ir++) {
//       size_t i = nodes.size() - 1 - ir;
//       auto &node = nodes[i];
//       for (auto &attachment : node.writesTo) {
//         auto &frame = frames[attachment.frameIndex];
//         if (frame.name == name) {
//           FrameIndex outputFrameIndex = frames.size();
//           size_t outputIndex = outputs.size();
//           outputs.emplace_back(RenderGraph::Output{
//               .name = name,
//               .frameIndex = outputFrameIndex,
//           });

//           FrameIndex prevFrameIndex = attachment.frameIndex;
//           frames.emplace_back(RenderGraph::Frame{
//               .name = name,
//               .size = frame.size,
//               .format = outputTextureFormat,
//               .outputIndex = outputIndex,
//           });

//           // Make sure no node after this references this as an input anymore
//           // TODO: support this case by forcing a copy from intermediate texture to output
//           checkNoReadFrom(i + 1, prevFrameIndex);

//           replaceFrame(prevFrameIndex, outputFrameIndex);

//           return;
//         }
//       }
//     }
//   }

// Call before allocating a node's outputs
void RenderGraphBuilder::allocateInputs(NodeBuildData &buildData, const std::vector<std::string> &inputs) {
  // nodes.resize(std::max(nodes.size(), nodeIndex + 1));
  // RenderGraphNode &node = nodes[nodeIndex];

  for (auto &inputName : inputs) {
    FrameIndex frameIndex{};
    if (findFrameIndex(inputName, frameIndex)) {
      buildData.readsFrom.push_back(frameIndex);
    }
  }
}

//   bool isWrittenTo(FrameIndex frameIndex, size_t beforeNodeIndex) {
//     if (externallyWrittenTo.contains(frameIndex))
//       return true;
//     for (size_t i = 0; i < beforeNodeIndex; i++) {
//       for (auto &attachment : nodes[i].writesTo) {
//         if (attachment.frameIndex == frameIndex)
//           return true;
//       }
//     }
//     return false;
//   }

//   void finalizeNodeConnections() {
//     static auto logger = gfx::getLogger();

//     for (size_t nodeIndex = 0; nodeIndex < nodeBuildData.size(); nodeIndex++) {
//       RenderGraphNode &node = nodes[nodeIndex];
//       NodeBuildData &nodeBuildData = this->nodeBuildData[nodeIndex];

//       for (auto &attachment : nodeBuildData.attachments) {
//         FrameIndex frameIndex = attachment.frameIndex;

//         WGPUTextureFormat targetFormat{};
//         std::optional<ClearValues> clearValues{};
//         std::visit(
//             [&](auto &&arg) {
//               using T = std::decay_t<decltype(arg)>;
//               clearValues = arg.clearValues;
//               if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
//                 targetFormat = arg.format;
//               } else if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
//                 targetFormat = arg.subResource.texture->getFormat().pixelFormat;
//               } else {
//                 throw std::logic_error("");
//               }
//             },
//             attachment.output);

//         bool sizeMismatch = frames[frameIndex].size != attachment.targetSize;
//         bool formatMismatch = frames[frameIndex].format != targetFormat;

//         bool loadRequired = !clearValues.has_value();
//         if (nodeBuildData.forceOverwrite) {
//           loadRequired = false;
//         }

//         // Check if the same frame is being read from
//         auto it = std::find(node.readsFrom.begin(), node.readsFrom.end(), frameIndex);
//         if (sizeMismatch || formatMismatch || it != node.readsFrom.end()) {
//           // TODO: Implement copy into loaded attachment from previous step
//           if (loadRequired) {
//             std::string err = "Copy required, not implemented";
//             if (sizeMismatch)
//               err += fmt::format(" (size {}=>{})", frames[frameIndex].size, attachment.targetSize);
//             if (formatMismatch)
//               err += fmt::format(" (fmt {}=>{})", magic_enum::enum_name(frames[frameIndex].format),
//                                  magic_enum::enum_name(targetFormat));
//             throw std::logic_error(err);
//           }

//           // Just duplicate the frame for now
//           size_t newFrameIndex = frames.size();
//           auto &frame = frames.emplace_back(RenderGraph::Frame{
//               .name = frames[frameIndex].name,
//               .size = attachment.targetSize,
//               .format = targetFormat,
//           });
//           nameLookup[frame.name] = newFrameIndex;
//           frameIndex = newFrameIndex;
//         }

//         // Check if written, force clear with default values if not
//         bool needsClearValue = false;
//         if (nodeBuildData.forceOverwrite)
//           needsClearValue = true;
//         else if (loadRequired && !isWrittenTo(frameIndex, nodeIndex)) {
//           SPDLOG_LOGGER_DEBUG(
//               logger, "Forcing clear with default values for frame {} accessed by node {}, since it's not written to before",
//               frameIndex, nodeIndex);
//           needsClearValue = true;
//         }

//         // Set default clear value based on format
//         if (needsClearValue) {
//           auto &formatDesc = getTextureFormatDescription(targetFormat);
//           if (hasAnyTextureFormatUsage(formatDesc.usage, TextureFormatUsage::Depth | TextureFormatUsage::Stencil)) {
//             clearValues = ClearValues::getDefaultDepthStencil();
//           } else {
//             clearValues = ClearValues::getDefaultColor();
//           }
//         }

//         if (clearValues.has_value())
//           node.writesTo.emplace_back(frameIndex, clearValues.value());
//         else {
//           node.writesTo.emplace_back(frameIndex);
//         }
//       }
//     }
//   }

//   // When setting forceOverwrite, ignores clear value, marks this attachment as being completely overwritten
//   // e.g. fullscreen effect passes
void RenderGraphBuilder::allocateOutputs(NodeBuildData &nodeBuildData, const RenderStepOutput &output, bool forceOverwrite) {
  nodeBuildData.forceOverwrite = forceOverwrite;

  for (auto &attachment : output.attachments) {
    int2 targetSize = int2(linalg::floor(referenceOutputSize * output.sizeScale.value_or(float2(1.0f))));

    FrameIndex frameIndex = assignFrame(attachment, targetSize);

    nodeBuildData.attachments.push_back(Attachment{attachment, frameIndex, targetSize});
  }
}

//   RenderTargetLayout getLayout(size_t nodeIndex) const {
//     if (nodeIndex >= nodes.size())
//       throw std::out_of_range("nodeIndex");

//     RenderTargetLayout layout{};

//     const auto &node = nodes[nodeIndex];
//     for (auto &nodeIndex : node.writesTo) {
//       auto &frame = frames[nodeIndex];

//       std::string name = frame.name;
//       if (name == "depth") {
//         layout.depthTargetIndex = layout.targets.size();
//       }

//       layout.targets.emplace_back(RenderTargetLayout::Target{
//           .name = std::move(name),
//           .format = frame.format,
//       });
//     }

//     return layout;
//   }

//   RenderGraph finalize() {
//     updateNodeLayouts();
//     validateNodes();

//     RenderGraph graph{
//         .nodes = std::move(nodes),
//         .frames = std::move(frames),
//         .outputs = std::move(outputs),
//     };
//     return graph;
//   }

// private:
//   void updateNodeLayouts() {
//     size_t nodeIndex{};
//     for (auto &node : nodes) {
//       node.renderTargetLayout = getLayout(nodeIndex);
//       nodeIndex++;
//     }
//   }

//   void validateNodes() {
//     static auto logger = gfx::getLogger();

//     for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++) {
//       auto &node = nodes[nodeIndex];
//       if (node.writesTo.size() == 0) {
//         throw std::runtime_error(fmt::format("Node {} does not write to any outputs", nodeIndex));
//       }
//     }
//   }

RenderGraphEvaluator::RenderGraphEvaluator(allocator_type allocator, Renderer &renderer, RendererStorage &storage)
    : allocator(allocator), writtenTextures(allocator), frameTextures(allocator), renderer(renderer), storage(storage) {}

void RenderGraphEvaluator::getFrameTextures(shards::pmr::vector<ResolvedFrameTexture> &outFrameTextures,
                                            const std::span<TextureSubResource> &outputs, const RenderGraph &graph,
                                            Context &context, size_t frameCounter) {
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
    if (frame.outputIndex.has_value()) {
      size_t outputIndex = frame.outputIndex.value();
      if (outputIndex >= outputs.size())
        throw std::logic_error("Missing output");

      outFrameTextures.push_back(resolveSubResourceView(outputs[outputIndex]));
    } else if (frame.textureOverride) {
      outFrameTextures.push_back(resolveSubResourceView(frame.textureOverride));
    } else {
      TexturePtr texture =
          storage.renderTextureCache.allocate(RenderTargetFormat{.format = frame.format, .size = frame.size}, frameCounter);
      outFrameTextures.push_back(resolve(texture));
    }
  }
}

} // namespace gfx::detail