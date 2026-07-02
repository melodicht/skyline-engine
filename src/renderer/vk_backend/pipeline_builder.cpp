#include <format>

#include <volk.h>
#include <SDL3/SDL.h>

#include "meta_definitions.h"
#include "vk_render_utils.h"
#include "pipeline_builder.h"

PipelineBuilder::PipelineBuilder(VkDevice device)
{
    this->device = device;
    this->rasterInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };
    this->renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    this->depthInfo = {};
    hasDepth = false;
}

PipelineBuilder& PipelineBuilder::Shader(const char* path, VkShaderStageFlagBits stage)
{
    VkShaderModuleCreateInfo shaderInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    void *shaderFile = SDL_LoadFile(std::format(SKL_BASE_PATH "/shaderbin/{}.spv", path).c_str(), &shaderInfo.codeSize);
    if (!shaderFile)
    {
        std::cerr << "Failed to load shader file: " << SDL_GetError() << std::endl;
        return *this;
    }
    shaderInfo.pCode = static_cast<const u32*>(shaderFile);
    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &shaderInfo, nullptr, &module));

    VkPipelineShaderStageCreateInfo stageInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = stage,
        .module = module,
        .pName = "main"
    };
    shaders.push_back(stageInfo);

    return *this;
}

PipelineBuilder& PipelineBuilder::Multiview(u32 viewCount)
{
    renderInfo.viewMask = (1 << viewCount) - 1;
    return *this;
}

PipelineBuilder& PipelineBuilder::Rasterizer(VkFrontFace frontFace, VkBool32 depthClamp)
{
    rasterInfo.frontFace = frontFace;
    rasterInfo.depthClampEnable = depthClamp;
    return *this;
}

PipelineBuilder& PipelineBuilder::DepthTarget(VkFormat format, VkBool32 write, VkCompareOp op)
{
    renderInfo.depthAttachmentFormat = format;

    depthInfo =
    {
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = write,
        .depthCompareOp = op,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    hasDepth = true;

    return *this;
}

PipelineBuilder& PipelineBuilder::ColorTarget(VkFormat format, VkColorComponentFlags writeMask)
{
    VkPipelineColorBlendAttachmentState blendState
    {
        .blendEnable = VK_FALSE,
        .colorWriteMask = writeMask
    };

    colorFormats.push_back(format);
    blendAttachments.push_back(blendState);

    return *this;
}

VkPipeline PipelineBuilder::Build(VkPipelineLayout layout)
{
    VkPipelineVertexInputStateCreateInfo vertexInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssembly
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewportState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    VkPipelineMultisampleStateCreateInfo multisampling
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineColorBlendStateCreateInfo blendState
    {
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = (u32)blendAttachments.size(),
        .pAttachments = blendAttachments.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE
    };

    VkPipelineDynamicStateCreateInfo dynamicState
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 3,
        .pDynamicStates = dynamicStates
    };

    renderInfo.colorAttachmentCount = (u32)colorFormats.size();
    renderInfo.pColorAttachmentFormats = colorFormats.data();

    VkGraphicsPipelineCreateInfo pipelineInfo
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = (u32)shaders.size(),
        .pStages = shaders.data(),
        .pVertexInputState = &vertexInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterInfo,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = hasDepth ? &depthInfo : nullptr,
        .pColorBlendState = &blendState,
        .pDynamicState = &dynamicState,
        .layout = layout,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0
    };

    VkPipeline pipeline = VK_NULL_HANDLE;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    for (VkPipelineShaderStageCreateInfo shaderInfo : shaders)
    {
        vkDestroyShaderModule(device, shaderInfo.module, nullptr);
    }

    return pipeline;
}

