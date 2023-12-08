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
  void addNode(const PipelineStepPtr &step, size_t queueDataIndex) {
    auto &buildData = buildingNodes.emplace_back(step, queueDataIndex);
    std::visit([&](auto &step) { allocateNodeEdges(*this, buildData, step); }, *step.get());
    needPrepare = true;
  }

  // Allocate outputs for a render graph node

  // When setting forceOverwrite, ignores clear value, marks this attachment as being completely overwritten
  // e.g. fullscreen effect passes
  void allocateOutputs(NodeBuildData &nodeBuildData, const RenderStepOutput &output, bool forceOverwrite = false) {
    nodeBuildData.output.emplace(std::ref(output));
  }
  // Allocate outputs for a render graph node
  void allocateInputs(NodeBuildData &nodeBuildData, const RenderStepInput &input) {
    nodeBuildData.input.emplace(std::ref(input));
  }

  using NamedFrameLookup = std::unordered_map<std::string, FrameBuildData *>;
  void assignUniqueNodeInputs(NodeBuildData &node, size_t nodeIndex, NamedFrameLookup &namedFrames) {
    if (node.input.has_value()) {
      size_t inputIndex = 0;
      for (auto &input : node.input->get().attachments) {
        std::visit(
            [&](auto &arg) {
              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
                assert(arg.subResource);
                FrameBuildData &frame = buildingFrames.emplace_back();
                frame.name = arg.name;
                frame.format = arg.subResource.texture->getFormat().pixelFormat;
                frame.textureOverride = TextureOverrideRef{
                    .step = node.step,
                    .bindingType = TextureOverrideRef::Output,
                    .bindingIndex = inputIndex,
                };
                node.inputs.emplace_back(&frame);
              } else if constexpr (std::is_same_v<T, RenderStepInput::Named>) {
                auto it = namedFrames.find(arg.name);
                if (it == namedFrames.end())
                  throw std::runtime_error(fmt::format("Error in node {}, named input {} was not found", nodeIndex, inputIndex));
                FrameBuildData &frame = *it->second;
                // if (frame.format != arg.format)
                //   throw std::runtime_error(fmt::format("Error in node {}, named input {} format mismatch (expected: {}, was: {})",
                //                                        nodeIndex, inputIndex, frame.format, arg.format));
                node.inputs.emplace_back(&frame);
              } else {
                throw std::logic_error("");
              }
            },
            input);
        ++inputIndex;
      }
    }
  }

  void assignUniqueNodeOutputs(NodeBuildData &node, size_t nodeIndex, NamedFrameLookup &namedFrames) {
    auto &outputSizing = node.output->get().outputSizing;
    if (node.output.has_value()) {
      size_t outputIndex = 0;
      for (auto &output : node.output->get().attachments) {
        FrameBuildData &frame = buildingFrames.emplace_back();
        std::visit(
            [&](auto &&arg) {
              frame.name = arg.name;

              std::visit(
                  [&](auto &size) {
                    using T = std::decay_t<decltype(size)>;
                    if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToInputSize>) {
                      if (size.name) {
                        auto it = namedFrames.find(*size.name);
                        if (it == namedFrames.end())
                          throw std::runtime_error(
                              fmt::format("Error in node {}, named input {} specified in relative output size was not found",
                                          nodeIndex, *size.name));
                        frame.sizing = FrameSize::RelativeToFrame{
                            .frame = it->second,
                            .scale = size.scale,
                        };
                      } else {
                        if (node.inputs.empty())
                          throw std::runtime_error(
                              fmt::format("Error in node {}, relative output size specified, but no inputs", nodeIndex));
                        frame.sizing = FrameSize::RelativeToFrame{
                            .frame = node.inputs[0],
                            .scale = size.scale,
                        };
                      }
                    } else if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToMainSize>) {
                      frame.sizing = FrameSize::RelativeToMain{
                          .scale = size.scale,
                      };
                    } else if constexpr (std::is_same_v<T, RenderStepOutput::ManualSize>) {
                      frame.sizing = FrameSize::Manual{};
                    } else {
                      throw std::logic_error("");
                    }
                  },
                  outputSizing);

              using T = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
                assert(arg.subResource);
                frame.format = arg.subResource.texture->getFormat().pixelFormat;
                frame.textureOverride = TextureOverrideRef{
                    .step = node.step,
                    .bindingType = TextureOverrideRef::Output,
                    .bindingIndex = outputIndex,
                };
              } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
                frame.format = arg.format;
                namedFrames[frame.name] = &frame;
              } else {
                throw std::logic_error("");
              }
            },
            output);
        node.outputs.emplace_back(&frame);
        ++outputIndex;
      }
    }
  }

  void assignUniqueFrames() {
    std::unordered_map<std::string, FrameBuildData *> namedFrames;
    size_t nodeIndex = 0;
    for (auto &node : buildingNodes) {
      assignUniqueNodeInputs(node, nodeIndex, namedFrames);
      assignUniqueNodeOutputs(node, nodeIndex, namedFrames);
      ++nodeIndex;
    }
  }

  RenderGraph build() {
    RenderGraph result;
    assignUniqueFrames();
    return result;
  }

private:
  FrameBuildData *findFrameByName(const std::string &name) {
    auto it = nameLookup.find(name);
    if (it != nameLookup.end())
      return it->second;
    return nullptr;
  }
};
} // namespace graph_build_data
using RenderGraphBuilder = graph_build_data::RenderGraphBuilder;

} // namespace gfx::detail

#endif /* EF9289B8_FECE_4D29_8B3F_43A88E82C83B */
