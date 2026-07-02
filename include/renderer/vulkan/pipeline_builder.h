#pragma once

#include <vector>

class PipelineBuilder
{
public:
    VkPipelineRenderingCreateInfo renderInfo;

    PipelineBuilder(VkDevice device);

    PipelineBuilder& Shader(const char* path, VkShaderStageFlagBits stage);
    PipelineBuilder& Multiview(u32 viewCount);
    PipelineBuilder& Rasterizer(VkFrontFace frontFace, VkBool32 depthClamp);
    PipelineBuilder& DepthTarget(VkFormat format, VkBool32 write, VkCompareOp op);
    PipelineBuilder& ColorTarget(VkFormat format, VkColorComponentFlags writeMask);

    VkPipelineRenderingCreateInfo GetRenderingInfo();

    VkPipeline Build(VkPipelineLayout layout);

private:
    VkDevice device;
    std::vector<VkPipelineShaderStageCreateInfo> shaders;
    VkPipelineRasterizationStateCreateInfo rasterInfo;
    VkPipelineDepthStencilStateCreateInfo depthInfo;
    bool hasDepth;
    std::vector<VkFormat> colorFormats;
    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
};
