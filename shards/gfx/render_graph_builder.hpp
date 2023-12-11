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

inline FrameBuildData *NodeBuildData::findInputFrame(const std::string &name) const {
  for (auto &input : inputs) {
    if (input->name == name)
      return input;
  }
  return nullptr;
}

struct RenderGraphBuilder {
private:
  template <typename T> using VectorType = boost::container::stable_vector<T>;
  VectorType<NodeBuildData> buildingNodes;
  VectorType<FrameBuildData> buildingFrames;
  // VectorType<AttachmentRef> buildingOutputs;
  // VectorType<AttachmentRef> buildingInputs;
  // VectorType<SizeBuildData> buildingSizeDefinitions;

  bool needPrepare = true;
  size_t writeCounter{};

public:
  float2 referenceOutputSize{};
  std::vector<Output> outputs;

  // Adds a node to the render graph
  void addNode(const PipelineStepPtr &step, size_t queueDataIndex) {
    auto &buildData = buildingNodes.emplace_back(step, queueDataIndex);
    buildData.srcIndex = buildingNodes.size() - 1;
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

  FrameBuildData *findNamedFrame(const RenderStepOutput::Named &named, const FrameSizing &size) {
    return findNamedFrame(named, size, [](const FrameBuildData &frame) -> bool { return true; });
  }

  template <typename T>
  std::enable_if_t<std::is_invocable_r_v<bool, T, const FrameBuildData &>, FrameBuildData *>
  findNamedFrame(const RenderStepOutput::Named &named, const FrameSizing &size, T filter) {
    for (auto &frame : buildingFrames) {
      if (frame.textureOverride) // These will always be unique
        continue;
      if (frame.name != named.name)
        continue;
      if (frame.format != named.format)
        continue;
      if (*frame.sizing != size)
        continue;
      if (!filter(frame))
        continue;
      return &frame;
    }
    return nullptr;
  }

  FrameBuildData *findNamedFrame(const RenderStepInput::Named &named) {
    FrameBuildData *candidate{};
    for (auto it = buildingFrames.rbegin(); it != buildingFrames.rend(); ++it) {
      if (it->name != named.name)
        continue;
      // Make sure to always pick the frame that was written to last
      if (!candidate || it->lastWriteIndex > candidate->lastWriteIndex)
        candidate = &*it;
    }
    return candidate;
  }

  FrameSizing resolveSize(NodeBuildData &node, const RenderStepOutput::OutputSizing &size) {
    return std::visit(
        [&](auto &size) -> FrameSizing {
          using T = std::decay_t<decltype(size)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToInputSize>) {
            if (size.name) {
              auto frame = node.findInputFrame(*size.name);
              if (!frame)
                throw std::runtime_error(
                    fmt::format("Error in node {}, named input {} specified in relative output size was not found", node.srcIndex,
                                *size.name));
              return FrameSize::RelativeToFrame{
                  .frame = frame,
                  .scale = size.scale,
              };
            } else {
              if (node.inputs.empty())
                throw std::runtime_error(
                    fmt::format("Error in node {}, relative output size specified, but no inputs", node.srcIndex));
              return FrameSize::RelativeToFrame{
                  .frame = node.inputs[0],
                  .scale = size.scale,
              };
            }
          } else if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToMainSize>) {
            return FrameSize::RelativeToMain{
                .scale = size.scale,
            };
          } else if constexpr (std::is_same_v<T, RenderStepOutput::ManualSize>) {
            return FrameSize::Manual{};
          } else {
            throw std::logic_error("");
          }
        },
        size);
  }

  FrameBuildData *findOrCreateInputFrame(NodeBuildData &node, const RenderStepInput::InputVariant &output) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
            assert(arg.subResource);
            FrameBuildData &frame = buildingFrames.emplace_back();
            frame.name = arg.name;
            frame.format = arg.subResource.texture->getFormat().pixelFormat;
            frame.textureOverride = TextureOverrideRef{
                .step = node.step,
                .bindingType = TextureOverrideRef::Output,
                .bindingIndex = 0,
            };
            frame.sizing = FrameSize::Manual{};
            return &frame;
          } else if constexpr (std::is_same_v<T, RenderStepInput::Named>) {
            FrameBuildData *frame = findNamedFrame(arg);
            if (!frame) {
              throw std::runtime_error(fmt::format("Error in node {}, named input {} was not found", node.srcIndex, arg.name));
            }
            return frame;
          }
        },
        output);
    return frame;
  }

  void fixupWriteToInputFrame(NodeBuildData &node, FrameBuildData *&outputFrame) {
    bool readsFromOutput{};
    for (auto &input : node.inputs) {
      if (input == outputFrame) {
        readsFromOutput = true;
        break;
      }
    }

    if (readsFromOutput) {
      // Find another frame to output to
      RenderStepOutput::Named named(outputFrame->name, outputFrame->format);
      FrameBuildData *replacementFrame =
          findNamedFrame(named, *outputFrame->sizing, [&](const FrameBuildData &frame) { return &frame != outputFrame; });
      if (!replacementFrame) {
        auto &newFrame = buildingFrames.emplace_back(*outputFrame);
        replacementFrame = &newFrame;
      }
      outputFrame = replacementFrame;
    }
  }

  FrameBuildData *findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output,
                                          const RenderStepOutput::OutputSizing &sizing) {
    return findOrCreateOutputFrame(node, output, sizing, [](const FrameBuildData &frame) -> bool { return true; });
  }

  template <typename T>
  std::enable_if_t<std::is_invocable_r_v<bool, T, const FrameBuildData &>, FrameBuildData *>
  findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output,
                          const RenderStepOutput::OutputSizing &sizing, T filter) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            assert(arg.subResource);
            FrameBuildData &frame = buildingFrames.emplace_back();
            frame.name = arg.name;
            frame.format = arg.subResource.texture->getFormat().pixelFormat;
            frame.textureOverride = TextureOverrideRef{
                .step = node.step,
                .bindingType = TextureOverrideRef::Output,
                .bindingIndex = 0,
            };
            frame.sizing = resolveSize(node, sizing);
            return &frame;
          } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
            FrameSizing frameSize = resolveSize(node, sizing);
            FrameBuildData *frame = findNamedFrame(arg, frameSize, filter);
            if (!frame) {
              FrameBuildData &oframe = buildingFrames.emplace_back();
              oframe.name = arg.name;
              oframe.format = arg.format;
              oframe.sizing = frameSize;
              frame = &oframe;
            }
            return frame;
          }
        },
        output);
    return frame;
  }

  void assignUniqueNodeInputs(NodeBuildData &node) {
    if (node.input.has_value()) {
      // size_t inputIndex = 0;
      for (auto &input : node.input->get().attachments) {
        // std::visit(
        //     [&](auto &arg) {
        //       using T = std::decay_t<decltype(arg)>;
        //       if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
        //         assert(arg.subResource);
        //         FrameBuildData &frame = buildingFrames.emplace_back();
        //         frame.name = arg.name;
        //         frame.format = arg.subResource.texture->getFormat().pixelFormat;
        //         frame.textureOverride = TextureOverrideRef{
        //             .step = node.step,
        //             .bindingType = TextureOverrideRef::Output,
        //             .bindingIndex = inputIndex,
        //         };
        //         node.inputs.emplace_back(&frame);
        //       } else if constexpr (std::is_same_v<T, RenderStepInput::Named>) {
        //         auto it = namedFrames.find(arg.name);
        //         if (it == namedFrames.end())
        //           throw std::runtime_error(
        //               fmt::format("Error in node {}, named input {} was not found", node.srcIndex, inputIndex));
        //         FrameBuildData &frame = *it->second;
        //         // if (frame.format != arg.format)
        //         //   throw std::runtime_error(fmt::format("Error in node {}, named input {} format mismatch (expected: {}, was:
        //         //   {})",
        //         //                                        nodeIndex, inputIndex, frame.format, arg.format));
        //         node.inputs.emplace_back(&frame);
        //       } else {
        //         throw std::logic_error("");
        //       }
        //     },
        //     input);
        // ++inputIndex;
        FrameBuildData *frame = findOrCreateInputFrame(node, input);
        node.inputs.emplace_back(frame);
      }
    }
  }

  void assignUniqueNodeOutputs(NodeBuildData &node) {
    auto &outputSizing = node.output->get().outputSizing;
    if (node.output.has_value()) {
      // size_t outputIndex = 0;
      for (auto &output : node.output->get().attachments) {
        // FrameBuildData *frame = std::visit(
        //     [&](auto &arg) -> FrameBuildData * {
        //       using T = std::decay_t<decltype(arg)>;
        //       if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
        //         assert(arg.subResource);
        //         FrameBuildData &frame = buildingFrames.emplace_back();
        //         frame.name = arg.name;
        //         frame.format = arg.subResource.texture->getFormat().pixelFormat;
        //         frame.textureOverride = TextureOverrideRef{
        //             .step = node.step,
        //             .bindingType = TextureOverrideRef::Output,
        //             .bindingIndex = outputIndex,
        //         };
        //         return &frame;
        //       } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
        //         // auto it = namedFrames.find(arg.name);
        //         // if (it != namedFrames.end()) {
        //         //   return it->second;
        //         // } else {
        //         //   auto &frame = buildingFrames.emplace_back();
        //         //   frame.name = arg.name;
        //         //   namedFrames.emplace(arg.name, &frame);
        //         //   return &frame;
        //         // }
        //       }
        //     },
        //     output);
        FrameBuildData *frame = findOrCreateOutputFrame(node, output, outputSizing);
        fixupWriteToInputFrame(node, frame);
        node.outputs.emplace_back(frame);
        frame->lastWriteIndex = writeCounter;

        // FrameBuildData &frame = buildingFrames.emplace_back();
        // std::visit(
        //     [&](auto &&arg) {
        //       frame.name = arg.name;

        //       std::visit(
        //           [&](auto &size) {
        //             using T = std::decay_t<decltype(size)>;
        //             if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToInputSize>) {
        //               if (size.name) {
        //                 auto it = namedFrames.find(*size.name);
        //                 if (it == namedFrames.end())
        //                   throw std::runtime_error(
        //                       fmt::format("Error in node {}, named input {} specified in relative output size was not found",
        //                                   nodeIndex, *size.name));
        //                 frame.sizing = FrameSize::RelativeToFrame{
        //                     .frame = it->second,
        //                     .scale = size.scale,
        //                 };
        //               } else {
        //                 if (node.inputs.empty())
        //                   throw std::runtime_error(
        //                       fmt::format("Error in node {}, relative output size specified, but no inputs", nodeIndex));
        //                 frame.sizing = FrameSize::RelativeToFrame{
        //                     .frame = node.inputs[0],
        //                     .scale = size.scale,
        //                 };
        //               }
        //             } else if constexpr (std::is_same_v<T, RenderStepOutput::RelativeToMainSize>) {
        //               frame.sizing = FrameSize::RelativeToMain{
        //                   .scale = size.scale,
        //               };
        //             } else if constexpr (std::is_same_v<T, RenderStepOutput::ManualSize>) {
        //               frame.sizing = FrameSize::Manual{};
        //             } else {
        //               throw std::logic_error("");
        //             }
        //           },
        //           outputSizing);

        //       using T = std::decay_t<decltype(arg)>;
        //       if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
        //         assert(arg.subResource);
        //         frame.format = arg.subResource.texture->getFormat().pixelFormat;
        //         frame.textureOverride = TextureOverrideRef{
        //             .step = node.step,
        //             .bindingType = TextureOverrideRef::Output,
        //             .bindingIndex = outputIndex,
        //         };
        //       } else if constexpr (std::is_same_v<T, RenderStepOutput::Named>) {
        //         frame.format = arg.format;
        //         namedFrames[frame.name] = &frame;
        //       } else {
        //         throw std::logic_error("");
        //       }
        //     },
        //     output);
        // node.outputs.emplace_back(&frame);
        // ++outputIndex;
      }
    }
  }

  void assignUniqueFrames() {
    writeCounter = 0;
    std::unordered_map<std::string, FrameBuildData *> namedFrames;
    for (auto &node : buildingNodes) {
      assignUniqueNodeInputs(node);
      assignUniqueNodeOutputs(node);
      ++writeCounter;
    }
  }

  template <typename T> static inline size_t indexOf(const boost::container::stable_vector<T> &vec, const T *ptr) {
    size_t idx{};
    for (const auto &it : vec) {
      if (&it == ptr)
        return idx;
      ++idx;
    }
    throw std::logic_error("Not in target collection");
  }

  void dumpDebugInfo() {
    SPDLOG_LOGGER_DEBUG(logger, "- Render graph builder -");
    SPDLOG_LOGGER_DEBUG(logger, "Frames:");

    size_t index{};
    for (auto &frame : buildingFrames) {
      std::string outSuffix;
      if (frame.textureOverride) {
        UniqueId stepId = std::visit([](auto &arg) { return arg.getId(); }, *frame.textureOverride->step.get());
        outSuffix += fmt::format(" bind: {}/{}/{}", magic_enum::enum_name(frame.textureOverride->bindingType),
                                 frame.textureOverride->bindingIndex, stepId.getTag());
      }
      SPDLOG_LOGGER_DEBUG(logger, " - [{}] {} fmt: {}{}", index, frame.name, magic_enum::enum_name(frame.format), outSuffix);
      ++index;
    }

    index = 0;
    SPDLOG_LOGGER_DEBUG(logger, "Nodes:");
    for (auto &node : buildingNodes) {
      std::string writesToStr;
      for (auto &wt : node.outputs)
        writesToStr += fmt::format("{}, ", indexOf(buildingFrames, wt));
      if (!writesToStr.empty())
        writesToStr.resize(writesToStr.size() - 2);

      std::string readsFromStr;
      for (auto &wt : node.inputs)
        readsFromStr += fmt::format("{}, ", indexOf(buildingFrames, wt));
      if (!readsFromStr.empty())
        readsFromStr.resize(readsFromStr.size() - 2);

      std::string logStr;
      if (!writesToStr.empty()) {
        if (!logStr.empty())
          logStr += " ";
        logStr += fmt::format("writes: {}", writesToStr);
      }
      if (!readsFromStr.empty()) {
        if (!logStr.empty())
          logStr += " ";
        logStr += fmt::format("reads: {}", readsFromStr);
      }

      SPDLOG_LOGGER_DEBUG(logger, " - [{}] {}", index, logStr);
      ++index;
    }
  }

  void deduplicateFrames() {}

  RenderGraph build() {
    RenderGraph result;
    assignUniqueFrames();
    dumpDebugInfo();
    return result;
  }
};
} // namespace graph_build_data
using RenderGraphBuilder = graph_build_data::RenderGraphBuilder;

} // namespace gfx::detail

#endif /* EF9289B8_FECE_4D29_8B3F_43A88E82C83B */
