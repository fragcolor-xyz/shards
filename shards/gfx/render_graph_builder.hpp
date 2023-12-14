#ifndef EF9289B8_FECE_4D29_8B3F_43A88E82C83B
#define EF9289B8_FECE_4D29_8B3F_43A88E82C83B

#include "render_graph.hpp"
#include "render_graph_build_data.hpp"
#include "render_step_impl.hpp"
#include "renderer_storage.hpp"
#include "steps/effect.hpp"
#include <spdlog/spdlog.h>
#include <variant>
#include <unordered_map>
#include <boost/container/flat_map.hpp>
#include <boost/iterator/reverse_iterator.hpp>
// #include <boost/tti/has_function.hpp>
// #include <boost/graph/

template <> struct fmt::formatter<gfx::detail::graph_build_data::FrameSizing> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end)
      throw format_error("invalid format");
    return it;
  }

  template <typename FormatContext>
  auto format(const gfx::detail::graph_build_data::FrameSizing &size, FormatContext &ctx) -> decltype(ctx.out()) {
    using namespace linalg::ostream_overloads;
    std::stringstream ss;
    std::visit(
        [&](auto &arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, gfx::detail::graph_build_data::FrameSize::RelativeToMain>) {
            ss << "Main";
            if (arg.scale && *arg.scale != linalg::aliases::float2(1.0f))
              ss << " * " << *arg.scale;
          } else if constexpr (std::is_same_v<T, gfx::detail::graph_build_data::FrameSize::RelativeToFrame>) {
            ss << "Frame[" << arg.frame->index << "]";
            if (arg.scale && *arg.scale != linalg::aliases::float2(1.0f))
              ss << " * " << *arg.scale;
          } else if constexpr (std::is_same_v<T, gfx::detail::graph_build_data::FrameSize::Manual>) {
            ss << "Manual";
          } else {
            throw std::logic_error("");
          }
        },
        size);
    return format_to(ctx.out(), "{}", ss.str());
  }
};

namespace gfx::detail {
namespace graph_build_data {
static inline auto logger = ::gfx::getLogger();

// Checks if setupNodeFrames is callable with the given step type
template <typename T, typename = void> struct Has_setupNodeFrames : std::false_type {};
template <typename T>
struct Has_setupNodeFrames<T, std::void_t<decltype(setupNodeFrames(std::declval<RenderGraphBuilder &>(),
                                                                   std::declval<NodeBuildData &>(), std::declval<T &>()))>>
    : std::true_type {};

inline FrameBuildData *NodeBuildData::findInputFrame(const std::string &name) const {
  for (auto &input : inputs) {
    if (input->name == name)
      return input;
  }
  return nullptr;
}
inline FrameBuildData *NodeBuildData::findOutputFrame(const std::string &name) const {
  for (auto &output : outputs) {
    if (output->name == name)
      return output;
  }
  return nullptr;
}

struct References {
  std::vector<FrameBuildData *> usedFrames;
  std::unordered_map<FrameBuildData *, FrameIndex> usedFrameLookup;
};

struct RenderGraphBuilder {
private:
  template <typename T> using VectorType = boost::container::stable_vector<T>;
  VectorType<NodeBuildData> buildingNodes;
  VectorType<FrameBuildData> buildingFrames;

  bool needPrepare = true;
  size_t writeCounter{};

public:
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
  void allocateOutputs(NodeBuildData &nodeBuildData, const RenderStepOutput &output, bool discardAttachments = false) {
    nodeBuildData.output.emplace(std::ref(output));
  }

  // Allocate outputs for a render graph node
  void allocateInputs(NodeBuildData &nodeBuildData, const RenderStepInput &input) {
    nodeBuildData.input.emplace(std::ref(input));
  }

  void setupClears(NodeBuildData &nodeBuildData) {
    if (!nodeBuildData.output)
      throw std::logic_error("Output was not setup during allocateNodeEdges");
    auto &attachments = nodeBuildData.output->get().attachments;
    for (size_t i = 0; i < attachments.size(); i++) {
      std::visit(
          [&](auto &output) {
            if (output.clearValues) {
              auto &clear = nodeBuildData.requiredClears.emplace_back();
              clear.clearValues = *output.clearValues;
              clear.frame = nodeBuildData.outputs[i];
            }
          },
          attachments[i]);
    }
  }

  using NamedFrameLookup = std::unordered_map<std::string, FrameBuildData *>;

  FrameBuildData *findNamedFrame(const RenderStepOutput::Named &named, const FrameSizing &size) {
    return findNamedFrame(named, size, [](const FrameBuildData &frame) -> bool { return true; });
  }

  FrameSizing simplifySize(const FrameSizing &size) {
    return std::visit(
        [&](auto &arg) -> FrameSizing {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, FrameSize::RelativeToFrame>) {
            if (!arg.scale || *arg.scale == float2(1.0f))
              return *arg.frame->sizing;
          }
          return arg;
        },
        size);
  }

  bool compareSize(const FrameSizing &a, const FrameSizing &b) { return simplifySize(a) == simplifySize(b); }

  template <typename T>
  std::enable_if_t<std::is_invocable_r_v<bool, T, const FrameBuildData &>, FrameBuildData *>
  findNamedFrame(const RenderStepOutput::Named &named, const FrameSizing &size, T filter) {
    FrameBuildData *candidate{};
    for (auto &frame : buildingFrames) {
      if (frame.textureOverride) // These will always be unique
        continue;
      if (frame.name != named.name)
        continue;
      if (frame.format != named.format)
        continue;
      if (!compareSize(*frame.sizing, size))
        continue;
      if (!filter(frame))
        continue;
      // Make sure to always pick the frame that was written to last
      if (!candidate || frame.lastWriteIndex > candidate->lastWriteIndex)
        candidate = &frame;
    }
    return candidate;
  }

  template <typename T>
  std::enable_if_t<std::is_invocable_r_v<bool, T, const FrameBuildData &>, FrameBuildData *>
  findAnyNamedOutputFrame(const std::string &name, T filter) {
    FrameBuildData *candidate{};
    for (auto &frame : buildingFrames) {
      if (frame.name != name)
        continue;
      if (!filter(frame))
        continue;
      // Make sure to always pick the frame that was written to last
      if (!candidate || frame.lastWriteIndex > candidate->lastWriteIndex)
        candidate = &frame;
    }
    return candidate;
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

  FrameBuildData &newFrame() {
    auto &result = buildingFrames.emplace_back();
    result.index = buildingFrames.size() - 1;
    return result;
  }

  FrameBuildData *findOrCreateInputFrame(NodeBuildData &node, const RenderStepInput::InputVariant &output) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
            assert(arg.subResource);
            FrameBuildData &frame = newFrame();
            frame.name = arg.name;
            frame.format = arg.subResource.texture->getFormat().pixelFormat;
            frame.textureOverride = TextureOverrideRef{
                .step = node.step,
                .bindingType = TextureOverrideRef::Input,
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

  void fixupWriteToInputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output, FrameBuildData *&outputFrame) {
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
        newFrame.index = buildingFrames.size() - 1;
        replacementFrame = &newFrame;
      }

      bool needPrevious = std::visit(
          [&](auto &arg) {
            if (!arg.clearValues) {
              return true;
            }
            return false;
          },
          output);
      if (needPrevious) {
        node.requiredCopies.emplace_back(NodeBuildData::CopyOperation::Before, outputFrame, replacementFrame);
      }

      outputFrame = replacementFrame;
    }
  }

  FrameBuildData *findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output,
                                          const RenderStepOutput::OutputSizing &sizing) {
    return findOrCreateOutputFrame(node, output, sizing, [](const FrameBuildData &frame) -> bool { return true; });
  }

  void fixupOutputFrameTransitions(NodeBuildData &node) {
    for (size_t i = 0; i < node.outputs.size(); i++) {
      auto &output = node.outputs[i];
      if (output->creator == &node) {
        bool hasClear = std::find_if(node.requiredClears.begin(), node.requiredClears.end(),
                                     [&](auto &clear) { return clear.frame == output; }) != node.requiredClears.end();

        // Frame is cleared, ignore since no transition is needed
        if (hasClear)
          continue;

        // Find an existing output frame that we need to copy from
        FrameBuildData *transitionSrcFrame =
            findAnyNamedOutputFrame(output->name, [&](const FrameBuildData &frame) { return &frame != output; });

        // Found a frame with the same name but potentially different format/size
        if (transitionSrcFrame) {
          SPDLOG_LOGGER_DEBUG(logger, "Transitioning frame {} => {}", transitionSrcFrame->index, output->index);
          node.requiredCopies.emplace_back(NodeBuildData::CopyOperation::Before, transitionSrcFrame, output);
        }
      }
    }
  }

  template <typename TL>
  std::enable_if_t<std::is_invocable_r_v<bool, TL, const FrameBuildData &>, FrameBuildData *>
  findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output,
                          const RenderStepOutput::OutputSizing &sizing, TL filter) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            assert(arg.subResource);
            FrameBuildData &frame = newFrame();
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
              FrameBuildData &oframe = newFrame();
              oframe.creator = &node;
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
      for (auto &input : node.input->get().attachments) {
        FrameBuildData *frame = findOrCreateInputFrame(node, input);
        node.inputs.emplace_back(frame);
      }
    }
  }

  void assignUniqueNodeOutputs(NodeBuildData &node) {
    auto &outputSizing = node.output->get().outputSizing;
    if (node.output.has_value()) {
      for (auto &output : node.output->get().attachments) {
        FrameBuildData *frame = findOrCreateOutputFrame(node, output, outputSizing);
        fixupWriteToInputFrame(node, output, frame);
        node.outputs.emplace_back(frame);
        frame->lastWriteIndex = writeCounter;
      }
    }
  }

  void assignUniqueFrames() {
    writeCounter = 0;
    std::unordered_map<std::string, FrameBuildData *> namedFrames;
    for (auto &node : buildingNodes) {
      node.inputs.clear();
      node.outputs.clear();

      assignUniqueNodeInputs(node);
      assignUniqueNodeOutputs(node);
      std::visit(
          [&](auto &arg) {
            using NodeType = std::decay_t<decltype(arg)>;
            if constexpr (Has_setupNodeFrames<NodeType>::value) {
              // If this is implemented for the step type, it can be used to customize the node after frames are assigned
              // This should normally call setupClears at some point or manually assign the required clears
              setupNodeFrames(*this, node, arg);
            } else {
              // Manually call setup code normally handled by setupNodeFrames
              setupClears(node);
            }
          },
          *node.step.get());
      fixupOutputFrameTransitions(node);
      ++writeCounter;
    }
  }

  using RNodeIt = decltype(buildingNodes)::reverse_iterator;

  // Traces back a frame to the first node that writes or clears it
  RNodeIt traceFirstWrite(RNodeIt lastNodeUsage, FrameBuildData *frame) {
    RNodeIt earliestWriteOrClear = lastNodeUsage;
    for (auto it = lastNodeUsage; it != buildingNodes.rend(); ++it) {
      auto &clears = it->requiredClears;
      if (std::find_if(clears.begin(), clears.end(), [&](auto &clear) { return clear.frame == frame; }) != clears.end()) {
        // This is a clear, so stop here
        earliestWriteOrClear = it;
        break;
      }
      auto &outputs = it->outputs;
      if (std::find(outputs.begin(), outputs.end(), frame) != outputs.end()) {
        // In case this frame is never cleared, just record the earliest write
        earliestWriteOrClear = it;
      }
    }
    return earliestWriteOrClear;
  }

  bool hasAnyReads(RNodeIt begin, RNodeIt end, FrameBuildData *frame_) const {
    for (auto it = begin; it != end; ++it) {
      for (auto &frame : it->inputs) {
        if (frame == frame_) {
          return true;
        }
      }
    }
    return false;
  }

  void replaceOutputFrames(RNodeIt begin, RNodeIt end, FrameBuildData *srcFrame, FrameBuildData *dstFrame) {
    for (auto it = begin; it != end; ++it) {
      for (auto &frame : it->outputs) {
        if (frame == srcFrame) {
          SPDLOG_LOGGER_DEBUG(logger, "Replacing output frame {} => {} in node {}", srcFrame->index, dstFrame->index,
                              it->srcIndex);
          frame = dstFrame;
        }
      }
      for (auto &copy : it->requiredCopies) {
        if (copy.dst == srcFrame) {
          SPDLOG_LOGGER_DEBUG(logger, "Replacing copy dst frame {} => {} in node {}", srcFrame->index, dstFrame->index,
                              it->srcIndex);
          copy.dst = dstFrame;
        }
      }
      for (auto &clear : it->requiredClears) {
        if (clear.frame == srcFrame) {
          SPDLOG_LOGGER_DEBUG(logger, "Replacing clear frame {} => {} in node {}", srcFrame->index, dstFrame->index,
                              it->srcIndex);
          clear.frame = dstFrame;
        }
      }
    }
  }

  void attachOutputs() {
    std::list<size_t> outputsToAttach;
    for (size_t i = 0; i < outputs.size(); i++) {
      FrameBuildData &outputFrame = newFrame();
      outputFrame.format = outputs[i].format;
      outputFrame.name = outputs[i].name;
      outputFrame.sizing = FrameSize::RelativeToMain{};
      outputFrame.outputIndex = i;

      // Iterate over nodes in reverse, attaching the output to the first matching frame name
      bool outputAttached = false;
      for (auto nodeIt = buildingNodes.rbegin(); nodeIt != buildingNodes.rend(); ++nodeIt) {
        FrameBuildData *frame = nodeIt->findOutputFrame(outputs[i].name);
        if (frame) {
          // Trace back to the node that clears or first writes this frame
          auto origin = traceFirstWrite(nodeIt, frame);

          // Any reads from this frame means we can't replace it as we might not be able to bind outputs
          bool canReplaceOutputFrame = !hasAnyReads(buildingNodes.rbegin(), origin + 1, frame);
          if (canReplaceOutputFrame) {
            // We can replace the usages with the output frame directly
            replaceOutputFrames(nodeIt, origin + 1, frame, &outputFrame);
          } else {
            // Copy this frame into the output
            SPDLOG_LOGGER_DEBUG(logger, "Can not replace output frame {} => {} because it is read from, copying instead",
                                frame->index, outputFrame.index);
            nodeIt->requiredCopies.emplace_back(NodeBuildData::CopyOperation::After, frame, &outputFrame);
          }

          outputAttached = true;
          break;
        }
      }

      if (!outputAttached) {
        SPDLOG_LOGGER_WARN(logger, "Failed to attach render graph output {} ({}), possibly because no frame writes to it.", i,
                           outputs[i].name);
      }
    }
  }

  bool isOutputWrittenTo(size_t outputIndex) const {
    for (auto &node : buildingNodes) {
      for (auto &out : node.outputs) {
        if (out->outputIndex && *out->outputIndex == outputIndex) {
          return true;
        }
      }
    }
    return false;
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

  size_t usageCount(const FrameBuildData &frame) {
    size_t count{};
    for (auto &node : buildingNodes) {
      for (auto &input : node.inputs) {
        if (input == &frame)
          ++count;
      }
      for (auto &output : node.outputs) {
        if (output == &frame)
          ++count;
      }
    }
    return count;
  }

  static inline size_t getStepId(PipelineStepPtr step) {
    return std::visit([](auto &arg) { return arg.getId().getIdPart(); }, *step.get());
  }

  void dumpDebugInfo(References &references) {
    SPDLOG_LOGGER_DEBUG(logger, "-- Render graph builder --");
    SPDLOG_LOGGER_DEBUG(logger, "Frames:");

    size_t index{};
    for (auto &frame : buildingFrames) {
      bool unused = references.usedFrameLookup.find(&frame) == references.usedFrameLookup.end();

      std::string outSuffix;
      if (frame.textureOverride) {
        outSuffix += fmt::format(" binding: {}/stepId:{}/idx:{}", magic_enum::enum_name(frame.textureOverride->bindingType),
                                 getStepId(frame.textureOverride->step), frame.textureOverride->bindingIndex);
      }
      if (frame.outputIndex) {
        outSuffix += fmt::format(" out: {}", *frame.outputIndex);
      }
      SPDLOG_LOGGER_DEBUG(logger, " - [{}{}] {} fmt: {} size: {}{}", index, unused ? ":UNUSED" : "", frame.name,
                          magic_enum::enum_name(frame.format), *frame.sizing, outSuffix);
      ++index;
    }

    index = 0;
    SPDLOG_LOGGER_DEBUG(logger, "Output Graph:");
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
      if (!readsFromStr.empty()) {
        if (!logStr.empty())
          logStr += " ";
        logStr += fmt::format("reads: {}", readsFromStr);
      }
      if (!writesToStr.empty()) {
        if (!logStr.empty())
          logStr += " ";
        logStr += fmt::format("writes: {}", writesToStr);
      }

      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before) {
          SPDLOG_LOGGER_DEBUG(logger, " - copy before: {} -> {}", indexOf(buildingFrames, copy.src),
                              indexOf(buildingFrames, copy.dst));
        }
      }

      for (auto &clear : node.requiredClears) {
        SPDLOG_LOGGER_DEBUG(logger, " - {} {}", clear.discard ? "discard" : "clear", indexOf(buildingFrames, clear.frame));
      }

      SPDLOG_LOGGER_DEBUG(logger, " - [stepId:{}] {}", getStepId(node.step), logStr);

      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::After) {
          SPDLOG_LOGGER_DEBUG(logger, " - copy after: {} -> {}", indexOf(buildingFrames, copy.src),
                              indexOf(buildingFrames, copy.dst));
        }
      }
      ++index;
    }
  }

  void collectReferences(References &outReferences) {
    auto addUsedFrame = [&](FrameBuildData *frame) {
      if (!outReferences.usedFrameLookup.contains(frame)) {
        outReferences.usedFrameLookup[frame] = outReferences.usedFrames.size();
        outReferences.usedFrames.emplace_back(frame);
      }
    };
    for (auto &buildingNode : buildingNodes) {
      for (auto &frame : buildingNode.inputs) {
        addUsedFrame(frame);
      }
      for (auto &frame : buildingNode.outputs) {
        addUsedFrame(frame);
      }
      for (auto &copy : buildingNode.requiredCopies) {
        addUsedFrame(copy.src);
        addUsedFrame(copy.dst);
      }
      for (auto &clear : buildingNode.requiredClears) {
        addUsedFrame(clear.frame);
      }
    }
  }

  void insertForceClear(NodeBuildData &node, FrameBuildData &frame) {
    SPDLOG_LOGGER_WARN(logger, "Frame {} is not cleared, forcing clear", frame.index);

    ClearValues clearValues;
    auto &formatDesc = getTextureFormatDescription(frame.format);
    if (hasAnyTextureFormatUsage(formatDesc.usage, TextureFormatUsage::Depth | TextureFormatUsage::Stencil)) {
      clearValues = ClearValues::getDefaultDepthStencil();
    } else {
      clearValues = ClearValues::getDefaultColor();
    }
    node.requiredClears.emplace_back(NodeBuildData::ClearOperation{
        .frame = &frame,
        .clearValues = clearValues,
    });
  }

  bool validateUninitializedFrames() {
    bool errors{};
    std::set<FrameBuildData *> writtenFrames;
    for (auto &node : buildingNodes) {
      for (auto &input : node.inputs) {
        assert(!input->outputIndex);
        if (!input->textureOverride && !writtenFrames.contains(input)) {
          SPDLOG_LOGGER_ERROR(logger, "Frame {} is read from but never written to", input->index);
          errors = true;
        }
      }
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before)
          writtenFrames.emplace(copy.dst);
      }
      for (auto &clear : node.requiredClears) {
        writtenFrames.emplace(clear.frame);
      }
      for (auto &output : node.outputs) {
        if (!writtenFrames.contains(output)) {
          // insertForceClear(node, *output);
          SPDLOG_LOGGER_WARN(logger, "Potentially uninitialized frame {}", output->index);
        }
        writtenFrames.emplace(output);
      }
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::After)
          writtenFrames.emplace(copy.dst);
      }
    }
    return !errors;
  }

  void generateCopyNode(RenderGraphNode &node, NodeBuildData::CopyOperation &copy) {
    NodeBuildData tmpCopyData(nullptr, 0);
    tmpCopyData.inputs.emplace_back(copy.src);
    tmpCopyData.outputs.emplace_back(copy.dst);
    auto copyStep = steps::Copy::create(RenderStepInput::make("copySrc"),
                                        RenderStepOutput::make(RenderStepOutput::Named("copyDst", copy.dst->format)));
    tmpCopyData.step = copyStep;
    node.originalStep = copyStep;
    std::visit([&](auto &arg) { setupRenderGraphNode(node, tmpCopyData, arg); }, *copyStep.get());
  }

  void assignFrames(RenderGraph &result, References &references) {
    std::map<FrameSizing, size_t> sizeLookup;
    std::map<size_t, size_t> sizeIndexToFrameIndex;
    std::map<FrameBuildData *, size_t> frameToSizeIndex;
    result.frames.resize(references.usedFrames.size());
    for (size_t frameIndex = 0; frameIndex < references.usedFrames.size(); frameIndex++) {
      auto &frame = references.usedFrames[frameIndex];
      auto &outFrame = result.frames[references.usedFrameLookup[frame]];
      outFrame.format = frame->format;
      outFrame.name = frame->name;

      if (!frame->textureOverride && !frame->outputIndex) {
        if (std::get_if<FrameSize::Manual>(&*frame->sizing)) {
          SPDLOG_LOGGER_WARN(logger, "Frame {}/{} can not be manually sized, sizing to output", frame->index, frame->name);
        }
        frame->sizing = FrameSize::RelativeToMain{};
      }

      auto it = sizeLookup.find(*frame->sizing);
      if (it != sizeLookup.end()) {
        outFrame.size = it->second;
      } else {
        it = sizeLookup.insert_or_assign(*frame->sizing, sizeLookup.size()).first;
        outFrame.size = it->second;
      }
      sizeIndexToFrameIndex[it->second] = frameIndex;
      frameToSizeIndex[frame] = it->second;

      if (frame->outputIndex) {
        outFrame.binding = RenderGraph::OutputIndex(*frame->outputIndex);
      } else if (frame->textureOverride) {
        outFrame.binding = *frame->textureOverride;
      }
    }

    // graph::Graph sizeGraph;

    result.sizes.resize(sizeLookup.size());
    for (auto &it : sizeLookup) {
      auto &outSize = result.sizes[it.second];
      // auto &sizeGraphNode = sizeGraph.nodes.emplace_back();
      outSize = std::visit(
          [&](auto &arg) -> RenderGraph::Size {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, FrameSize::RelativeToFrame>) {
              // sizeGraphNode.dependencies.emplace_back(frameToSizeIndex[arg.frame]);
              return RenderGraphFrameSize::RelativeToFrame{
                  .frame = references.usedFrameLookup[arg.frame],
                  .scale = arg.scale,
              };
            } else if constexpr (std::is_same_v<T, FrameSize::RelativeToMain>) {
              return RenderGraphFrameSize::RelativeToMain{
                  .scale = arg.scale,
              };
            } else if constexpr (std::is_same_v<T, FrameSize::Manual>) {
              return RenderGraphFrameSize::Manual{
                  .frame = sizeIndexToFrameIndex[it.second],
              };
            } else {
              throw std::logic_error("");
            }
          },
          it.first);
    }

    // TODO: sizes should already be sorted because of node dependency ordering
    // Topologically sort sizes by dependencies
    // so they can be computed linearly
    // std::vector<size_t> sizesSorted;
    // graph::topologicalSort(sizeGraph, sizesSorted);
    // std::vector<size_t> sizesReverseMap;
    // sizesReverseMap.resize(sizesSorted.size());
    // for (size_t i = 0; i < sizesSorted.size(); i++) {
    //   sizesReverseMap[sizesSorted[i]] = i;
    // }

    // // Remap sizes
    // for (auto &frame : result.frames) {
    //   frame.size = sizesReverseMap[frame.size];
    // }
    // std::vector<RenderGraph::Size> unsortedSizes;
    // std::swap(result.sizes, unsortedSizes);
    // result.sizes.resize(sizeLookup.size());
    // for (auto &it : sizeLookup) {
    //   result.sizes[it.second] = it->first;
    // }
    // for (size_t i = 0; i < sizeLookup.size(); i++) {
    //   result.sizes.emplace_back(sizeLookup[i]);
    // }
  }

  void assignNodes(RenderGraph &result, References &references) {
    for (auto &node : buildingNodes) {
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before) {
          auto &outNode = result.nodes.emplace_back();
          generateCopyNode(outNode, copy);
        }
      }

      auto &outNode = result.nodes.emplace_back();
      for (auto &input : node.inputs) {
        outNode.inputs.emplace_back(references.usedFrameLookup[input]);
      }
      std::map<FrameBuildData *, size_t> outputLookup;
      for (auto &output : node.outputs) {
        outputLookup.emplace(output, outNode.outputs.size());
        outNode.outputs.emplace_back(references.usedFrameLookup[output]);
      }
      outNode.queueDataIndex = node.queueDataIndex;
      outNode.originalStep = node.step;

      for (auto &clear : node.requiredClears) {
        size_t outputIndex = outputLookup[clear.frame];
        if (clear.discard) {
          outNode.outputs[outputIndex].loadOp = Discard{};
        } else {
          outNode.outputs[outputIndex].loadOp = clear.clearValues;
        }
      }

      for (auto &input : node.outputs) {
        auto &target = outNode.renderTargetLayout.targets.emplace_back();
        target.name = input->name;
        target.format = input->format;
        if (hasAnyTextureFormatUsage(getTextureFormatDescription(input->format).usage, TextureFormatUsage::Depth)) {
          outNode.renderTargetLayout.depthTargetIndex = outNode.renderTargetLayout.targets.size() - 1;
        }
      }

      std::visit([&](auto &step) { setupRenderGraphNode(outNode, node, step); }, *node.step.get());

      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::After) {
          auto &outNode = result.nodes.emplace_back();
          generateCopyNode(outNode, copy);
        }
      }

      // This value should be set by the setup callback above
      // so run it after
      for (auto &autoClearQueues : node.autoClearQueues) {
        result.autoClearQueues.push_back(autoClearQueues);
      }
    }
  }

  bool prepare() {
    buildingFrames.clear();
    for (auto &node : buildingNodes) {
      node.requiredCopies.clear();
      node.requiredClears.clear();
      node.inputs.clear();
      node.outputs.clear();
    }

    assignUniqueFrames();
    attachOutputs();

    if (!validateUninitializedFrames())
      return false;
    return true;
  }

  std::optional<RenderGraph> build() {
    RenderGraph result;

    if (!prepare())
      return std::nullopt;

    References references;
    collectReferences(references);

    assignFrames(result, references);
    assignNodes(result, references);

    if (logger->level() <= spdlog::level::debug) {
      dumpDebugInfo(references);
    }

    return result;
  }
};
} // namespace graph_build_data
} // namespace gfx::detail

#endif /* EF9289B8_FECE_4D29_8B3F_43A88E82C83B */
