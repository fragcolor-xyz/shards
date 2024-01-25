#ifndef EF9289B8_FECE_4D29_8B3F_43A88E82C83B
#define EF9289B8_FECE_4D29_8B3F_43A88E82C83B

#include "boost/container/small_vector.hpp"
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
#include <boost/tti/has_member_data.hpp>

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

BOOST_TTI_TRAIT_HAS_MEMBER_DATA(has_render_step_input, input);
BOOST_TTI_TRAIT_HAS_MEMBER_DATA(has_render_step_output, output);

static inline auto logger = ::gfx::getLogger();

// Checks if setupNodeFrames is callable with the given step type
template <typename T, typename = void> struct Has_setupNodeFrames : std::false_type {};
template <typename T>
struct Has_setupNodeFrames<T, std::void_t<decltype(setupNodeFrames(std::declval<RenderGraphBuilder &>(),
                                                                   std::declval<NodeBuildData &>(), std::declval<T &>()))>>
    : std::true_type {};

inline FrameBuildData *NodeBuildData::findInputFrame(FastString name) const {
  for (auto &input : inputs) {
    if (input->name == name)
      return input;
  }
  return nullptr;
}
inline FrameBuildData *NodeBuildData::findOutputFrame(FastString name) const {
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
  VectorType<NodeBuildData> generatedNodes;
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

  using NamedFrameLookup = std::unordered_map<FastString, FrameBuildData *>;

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
    size_t lwi{};
    for (auto &frame : buildingFrames) {
      if (std::get_if<TextureOverrideRef>(&frame.binding)) // These will always be unique
        continue;
      if (frame.name != named.name)
        continue;
      lwi = std::max(lwi, frame.lastWriteIndex);
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

    // Validate that this is the latest frame
    if (candidate && candidate->lastWriteIndex != lwi) {
      // The frame that matches the conditions is not the one most recently written to
      // return false so we can generate a transition
      candidate = nullptr;
    }

    return candidate;
  }

  template <typename T>
  std::enable_if_t<std::is_invocable_r_v<bool, T, const FrameBuildData &>, FrameBuildData *>
  findAnyNamedOutputFrame(FastString name, T filter) {
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

  FrameBuildData *findOrCreateInputFrame(NodeBuildData &node, const RenderStepInput::InputVariant &output, size_t index) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepInput::Texture>) {
            shassert(arg.subResource);
            FrameBuildData &frame = newFrame();
            frame.name = arg.name;
            frame.format = arg.subResource.texture->getFormat().pixelFormat;
            frame.binding = TextureOverrideRef{
                .step = node.step,
                .bindingType = TextureOverrideRef::Input,
                .bindingIndex = index,
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

  TextureSubResource resolveSubResource(const RenderGraph::TextureOverrideRef &ref) {
    return std::visit(
        [&](auto &step) -> TextureSubResource {
          using T1 = std::decay_t<decltype(step)>;
          if constexpr (has_render_step_input<T1, RenderStepInput>::type::value) {
            RenderStepInput &input = step.input;
            if (ref.bindingType == RenderGraph::TextureOverrideRef::Input) {
              auto tex = std::get_if<RenderStepInput::Texture>(&input.attachments[ref.bindingIndex]);
              if (!tex)
                throw std::logic_error("Expected a texture binding");
              return tex->subResource;
            }
          }
          if constexpr (has_render_step_output<T1, std::optional<RenderStepOutput>>::type::value) {
            RenderStepOutput &output = *step.output;
            if (ref.bindingType == RenderGraph::TextureOverrideRef::Output) {
              auto tex = std::get_if<RenderStepOutput::Texture>(&output.attachments[ref.bindingIndex]);
              if (!tex)
                throw std::logic_error("Expected a texture binding");
              return tex->subResource;
            }
          }
          throw std::logic_error("Invalid bindings");
        },
        *ref.step.get());
  }

  bool isSameBinding(RenderGraph::FrameBinding a, RenderGraph::FrameBinding b) {
    auto outA = std::get_if<RenderGraph::OutputIndex>(&a);
    auto outB = std::get_if<RenderGraph::OutputIndex>(&b);
    if (outA && outB) {
      return *outA == *outB;
    }

    // TODO: This can be compared here but whenever the textures change it might not be valid anymore
    //       hashing should prevent this although currently not handled
    auto texA = std::get_if<RenderGraph::TextureOverrideRef>(&a);
    auto texB = std::get_if<RenderGraph::TextureOverrideRef>(&b);
    if (texA && texB) {
      return resolveSubResource(*texA).texture == resolveSubResource(*texB).texture;
    }
    return false;
  }

  void fixupWriteToInputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output, FrameBuildData *&outputFrame) {
    FrameBuildData **inputFrameConflict{};
    for (auto &input : node.inputs) {
      if (input == outputFrame || isSameBinding(input->binding, outputFrame->binding)) {
        inputFrameConflict = &input;
        break;
      }
    }

    if (inputFrameConflict) {
      // Find another frame to output to
      RenderStepOutput::Named named(outputFrame->name, outputFrame->format);
      FrameBuildData *replacementFrame =
          findNamedFrame(named, *outputFrame->sizing, [&](const FrameBuildData &frame) { return &frame != outputFrame; });
      if (!replacementFrame) {
        auto &newFrame = buildingFrames.emplace_back(*outputFrame);
        newFrame.binding = std::monostate{};
        newFrame.index = buildingFrames.size() - 1;
        replacementFrame = &newFrame;
      }

      node.requiredCopies.emplace_back(NodeBuildData::CopyOperation::Before, *inputFrameConflict, replacementFrame);
      *inputFrameConflict = replacementFrame;
    }
  }

  FrameBuildData *findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output, size_t index,
                                          const RenderStepOutput::OutputSizing &sizing) {
    return findOrCreateOutputFrame(node, output, index, sizing, [](const FrameBuildData &frame) -> bool { return true; });
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
          if (transitionSrcFrame->name == "depth" && transitionSrcFrame->sizing != output->sizing) {
            // Ignore transitions on depth buffers with different size
            SPDLOG_LOGGER_DEBUG(logger, "Ignoring transition on depth buffer frame {} => {}", transitionSrcFrame->index,
                                output->index);
          } else {
            SPDLOG_LOGGER_DEBUG(logger, "Transitioning frame {} => {}", transitionSrcFrame->index, output->index);
            node.requiredCopies.emplace_back(NodeBuildData::CopyOperation::Before, transitionSrcFrame, output);
          }
        }
      }
    }
  }

  template <typename TL>
  std::enable_if_t<std::is_invocable_r_v<bool, TL, const FrameBuildData &>, FrameBuildData *>
  findOrCreateOutputFrame(NodeBuildData &node, const RenderStepOutput::OutputVariant &output, size_t index,
                          const RenderStepOutput::OutputSizing &sizing, TL filter) {
    FrameBuildData *frame = std::visit(
        [&](auto &arg) -> FrameBuildData * {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, RenderStepOutput::Texture>) {
            shassert(arg.subResource);
            FrameBuildData &frame = newFrame();
            frame.name = arg.name;
            frame.format = arg.subResource.texture->getFormat().pixelFormat;
            frame.binding = TextureOverrideRef{
                .step = node.step,
                .bindingType = TextureOverrideRef::Output,
                .bindingIndex = index,
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
      auto &inputs = node.input->get().attachments;
      for (size_t i = 0; i < inputs.size(); i++) {
        auto &input = inputs[i];
        FrameBuildData *frame = findOrCreateInputFrame(node, input, i);
        node.inputs.emplace_back(frame);
      }
    }
  }

  void assignUniqueNodeOutputs(NodeBuildData &node) {
    auto &outputSizing = node.output->get().outputSizing;
    if (node.output.has_value()) {
      auto &outputs = node.output->get().attachments;
      for (size_t i = 0; i < outputs.size(); i++) {
        auto &output = outputs[i];
        FrameBuildData *frame = findOrCreateOutputFrame(node, output, i, outputSizing);
        fixupWriteToInputFrame(node, output, frame);
        node.outputs.emplace_back(frame);
        frame->lastWriteIndex = writeCounter;
      }
    }
  }

  void assignUniqueFrames() {
    writeCounter = 0;
    std::unordered_map<FastString, FrameBuildData *> namedFrames;
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
      outputFrame.binding = RenderGraph::OutputIndex(i);

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
    auto checkFrame = [&](FrameBuildData *frame) -> bool {
      auto outIndex = std::get_if<RenderGraph::OutputIndex>(&frame->binding);
      return (outIndex && *outIndex == outputIndex);
    };
    for (auto &node : buildingNodes) {
      for (auto &out : node.outputs)
        if (checkFrame(out))
          return true;
      for (auto &cpy : node.requiredCopies)
        if (checkFrame(cpy.dst))
          return true;
      for (auto &clr : node.requiredClears)
        if (checkFrame(clr.frame))
          return true;
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

  void dumpDebugInfo(References &references, spdlog::level::level_enum logLevel) {
    SPDLOG_LOGGER_CALL(logger, logLevel, "-- Render graph builder --");
    SPDLOG_LOGGER_CALL(logger, logLevel, "Frames:");

    size_t index{};
    for (auto &frame : buildingFrames) {
      bool unused = references.usedFrameLookup.find(&frame) == references.usedFrameLookup.end();

      std::string outSuffix;
      if (auto textureOverride = std::get_if<TextureOverrideRef>(&frame.binding)) {
        outSuffix += fmt::format(" binding: {}/stepId:{}/idx:{}", magic_enum::enum_name(textureOverride->bindingType),
                                 getStepId(textureOverride->step), textureOverride->bindingIndex);
      }
      if (auto outputIndex = std::get_if<RenderGraph::OutputIndex>(&frame.binding)) {
        outSuffix += fmt::format(" out: {}", *outputIndex);
      }
      SPDLOG_LOGGER_CALL(logger, logLevel, " - [{}{}] {} fmt: {} size: {}{}", index, unused ? ":UNUSED" : "", frame.name,
                          magic_enum::enum_name(frame.format), *frame.sizing, outSuffix);
      ++index;
    }

    index = 0;
    SPDLOG_LOGGER_CALL(logger, logLevel, "Output Graph:");
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
      auto append = [&](auto sv) {
        if (!logStr.empty() && logStr.back() != '[')
          logStr += " ";
        logStr += sv;
      };

      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before) {
          append(fmt::format("copyBefore: {} -> {}", indexOf(buildingFrames, copy.src), indexOf(buildingFrames, copy.dst)));
        }
      }

      for (auto &clear : node.requiredClears)
        append(fmt::format("{}: {}", clear.discard ? "discard" : "clear", indexOf(buildingFrames, clear.frame)));

      append("[");
      if (!readsFromStr.empty())
        append(fmt::format("reads: {}", readsFromStr));
      if (!writesToStr.empty())
        append(fmt::format("writes: {}", writesToStr));
      logStr += "]";

      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::After) {
          append(fmt::format("copyAfter: {} -> {}", indexOf(buildingFrames, copy.src), indexOf(buildingFrames, copy.dst)));
        }
      }

      SPDLOG_LOGGER_CALL(logger, logLevel, " - {} (step:{}/{})", logStr, getPipelineStepName(node.step), getStepId(node.step));

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

  bool validateUninitializedFrames() {
    bool errors{};
    std::set<FrameBuildData *> writtenFrames;
    for (auto &node : buildingNodes) {
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before)
          writtenFrames.emplace(copy.dst);
      }
      for (auto &clear : node.requiredClears) {
        writtenFrames.emplace(clear.frame);
      }
      for (auto &input : node.inputs) {
        if (input->binding.index() == 0 && !writtenFrames.contains(input)) {
          SPDLOG_LOGGER_ERROR(logger, "Frame {} is read from but never written to", input->index);
          errors = true;
        }
      }
      for (auto &output : node.outputs) {
        if (!writtenFrames.contains(output)) {
          SPDLOG_LOGGER_DEBUG(logger, "Potentially uninitialized frame {}", output->index);
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

  NodeBuildData *generateCopyNode(NodeBuildData::CopyOperation &copy) {
    auto copyStep = steps::Copy::create(RenderStepInput::make(copy.src->name),
                                        RenderStepOutput::make(RenderStepOutput::Named(copy.dst->name, copy.dst->format)));
    std::get<RenderFullscreenStep>(*copyStep.get()).name =
        fmt::format("Copy {} {}=>{}", copy.src->name, copy.src->format, copy.dst->format);
    NodeBuildData &copyNode = generatedNodes.emplace_back(copyStep, size_t(~0));
    copyNode.inputs.emplace_back(copy.src);
    copyNode.outputs.emplace_back(copy.dst);
    copyNode.step = copyStep;
    return &copyNode;
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

      if (frame->binding.index() == 0) {
        if (std::get_if<FrameSize::Manual>(&*frame->sizing)) {
          SPDLOG_LOGGER_WARN(logger, "Frame {}/{} can not be manually sized, sizing to output", frame->index, frame->name);
          frame->sizing = FrameSize::RelativeToMain{};
        }
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

      outFrame.binding = frame->binding;
    }

    result.sizes.resize(sizeLookup.size());
    for (auto &it : sizeLookup) {
      auto &outSize = result.sizes[it.second];
      outSize = std::visit(
          [&](auto &arg) -> RenderGraph::Size {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, FrameSize::RelativeToFrame>) {
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
  }

  void assignNodes(RenderGraph &result, References &references) {
    // Expand node list and genrate dynamic nodes (copies)
    std::list<NodeBuildData *> nodeQueue;
    for (auto &node : buildingNodes) {
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::Before) {
          nodeQueue.emplace_back(generateCopyNode(copy));
        }
      }
      nodeQueue.emplace_back(&node);
      for (auto &copy : node.requiredCopies) {
        if (copy.order == NodeBuildData::CopyOperation::After) {
          nodeQueue.emplace_back(generateCopyNode(copy));
        }
      }
    }

    for (auto it = nodeQueue.begin(); it != nodeQueue.end(); ++it) {
      auto &node = **it;

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

      // Set clear operations
      for (auto &clear : node.requiredClears) {
        size_t outputIndex = outputLookup[clear.frame];
        if (clear.discard) {
          outNode.outputs[outputIndex].loadOp = Discard{};
        } else {
          outNode.outputs[outputIndex].loadOp = clear.clearValues;
        }
      }

      // Create the render target layout
      for (auto &output : node.outputs) {
        auto &target = outNode.renderTargetLayout.targets.emplace_back();
        target.name = output->name;
        target.format = output->format;
        if (hasAnyTextureFormatUsage(getTextureFormatDescription(output->format).usage, TextureFormatUsage::Depth)) {
          outNode.renderTargetLayout.depthTargetIndex = outNode.renderTargetLayout.targets.size() - 1;
        }
      }

      std::visit([&](auto &step) { setupRenderGraphNode(outNode, node, step); }, *node.step.get());

      // This value should be set by the setup callback above
      // so run it after
      for (auto &autoClearQueues : node.autoClearQueues) {
        result.autoClearQueues.push_back(autoClearQueues);
      }
    }
  }

  bool validateConnections() {
    boost::container::small_vector<std::string, 8> errors;

    for (auto &node : buildingNodes) {
      for (auto &out : node.outputs) {
        auto inIt = std::find(node.inputs.begin(), node.inputs.end(), out);
        if (inIt != node.inputs.end()) {
          errors.emplace_back(fmt::format("Node {} has an output {} that is also an input", node.srcIndex, out->index));
        }
      }

      if (node.outputs.empty()) {
        errors.emplace_back(fmt::format("Node {} has no outputs, it must have at least one", node.srcIndex));
      }
    }

    if (errors.size() > 0) {
      References references;
      SPDLOG_LOGGER_ERROR(logger, "Render graph has invalid connections:");
      collectReferences(references);
      dumpDebugInfo(references, spdlog::level::err);
      for (auto &error : errors) {
        SPDLOG_LOGGER_ERROR(logger, error);
      }
      return false;
    }

    return true;
  }

  bool prepare() {
    buildingFrames.clear();
    generatedNodes.clear();
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

    if (!validateConnections())
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
      dumpDebugInfo(references, spdlog::level::debug);
    }

    return result;
  }
};
} // namespace graph_build_data
} // namespace gfx::detail

#endif /* EF9289B8_FECE_4D29_8B3F_43A88E82C83B */
