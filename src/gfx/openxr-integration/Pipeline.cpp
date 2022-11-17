#include "Pipeline.h"

#include "Util.h"

#include <array>
#include <sstream>

Pipeline::Pipeline(VkDevice device,
                   VkPipelineLayout pipelineLayout,
                   VkRenderPass renderPass,
                   const std::string& vertexFilename,
                   const std::string& fragmentFilename,
                   const std::vector<VkVertexInputBindingDescription>& vertexInputBindingDescriptions,
                   const std::vector<VkVertexInputAttributeDescription>& vertexInputAttributeDescriptions)
: device(device)
{
  VkShaderModule vertexShaderModule;
  if (!util::loadShaderFromFile(device, vertexFilename, vertexShaderModule))
  {
    std::stringstream s;
    s << "Vertex shader \"" << vertexFilename << "\"";
    util::error(Error::FileMissing, s.str().c_str());
    valid = false;
    return;
  }

  VkShaderModule fragmentShaderModule;
  if (!util::loadShaderFromFile(device, fragmentFilename, fragmentShaderModule))
  {
    std::stringstream s;
    s << "Fragment shader \"" << fragmentFilename << "\"";
    util::error(Error::FileMissing, s.str().c_str());
    valid = false;
    return;
  }

  VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfoVertex{
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
  };
  pipelineShaderStageCreateInfoVertex.module = vertexShaderModule;
  pipelineShaderStageCreateInfoVertex.stage = VK_SHADER_STAGE_VERTEX_BIT;
  pipelineShaderStageCreateInfoVertex.pName = "main";

  VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfoFragment{
    VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO
  };
  pipelineShaderStageCreateInfoFragment.module = fragmentShaderModule;
  pipelineShaderStageCreateInfoFragment.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  pipelineShaderStageCreateInfoFragment.pName = "main";

  const std::array shaderStages = { pipelineShaderStageCreateInfoVertex, pipelineShaderStageCreateInfoFragment };

  VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
  };

  pipelineVertexInputStateCreateInfo.vertexBindingDescriptionCount =
    static_cast<uint32_t>(vertexInputBindingDescriptions.size());
  pipelineVertexInputStateCreateInfo.pVertexBindingDescriptions = vertexInputBindingDescriptions.data();
  pipelineVertexInputStateCreateInfo.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
  pipelineVertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();

  VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
  };
  pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
  };
  pipelineViewportStateCreateInfo.viewportCount = 1u;
  pipelineViewportStateCreateInfo.scissorCount = 1u;

  VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
  };
  pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
  pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
  pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;

  VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
  };
  pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
  };
  VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
  pipelineColorBlendAttachmentState.colorWriteMask =
    VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;
  pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
  pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
  pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
  pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
  pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

  pipelineColorBlendStateCreateInfo.attachmentCount = 1;
  pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;

  VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO
  };
  const std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  pipelineDynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

  VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo{
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
  };
  pipelineDepthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
  pipelineDepthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
  pipelineDepthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;

  VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
  graphicsPipelineCreateInfo.layout = pipelineLayout;
  graphicsPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
  graphicsPipelineCreateInfo.pStages = shaderStages.data();
  graphicsPipelineCreateInfo.pVertexInputState = &pipelineVertexInputStateCreateInfo;
  graphicsPipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
  graphicsPipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
  graphicsPipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
  graphicsPipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
  graphicsPipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
  graphicsPipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
  graphicsPipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfo;
  graphicsPipelineCreateInfo.renderPass = renderPass;
  if (vkCreateGraphicsPipelines(device, nullptr, 1u, &graphicsPipelineCreateInfo, nullptr, &pipeline) != VK_SUCCESS)
  {
    util::error(Error::GenericVulkan);
    valid = false;
    return;
  }

  // These shader modules can now be destroyed
  vkDestroyShaderModule(device, vertexShaderModule, nullptr);
  vkDestroyShaderModule(device, fragmentShaderModule, nullptr);
}

Pipeline::~Pipeline()
{
  vkDestroyPipeline(device, pipeline, nullptr);
}

void Pipeline::bind(VkCommandBuffer commandBuffer) const
{
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

bool Pipeline::isValid() const
{
  return valid;
}