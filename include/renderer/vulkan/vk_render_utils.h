#pragma once

#include "vk_render_types.h"

#define VK_CHECK(x)                                                 \
    do                                                              \
    {                                                               \
        VkResult err = (x);                                         \
        if (err)                                                    \
        {                                                           \
            std::cout << "Detected Vulkan error: " << err <<        \
            " at line " <<  __LINE__ << " in file " << __FILE__;    \
            abort();                                                \
        }                                                           \
    } while (0)

AllocatedBuffer CreateBuffer(VkDevice device, VmaAllocator allocator, size_t allocSize,
    VkBufferUsageFlags usage, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags);

AllocatedBuffer CreateShaderBuffer(VkDevice device, VmaAllocator allocator, size_t allocSize);

void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer);

AllocatedImage CreateImage(VmaAllocator allocator, VkFormat format, VkImageCreateFlags createFlags,
                           VkImageUsageFlags usageFlags, VkExtent3D extent, u32 layers,
                           VmaMemoryUsage memUsage, VkMemoryPropertyFlags memFlags);

void DestroyImage(VmaAllocator allocator, AllocatedImage image);

VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout,
    VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage);

VkCommandBuffer BeginImmediateCommands(VkDevice device, VkCommandPool commandPool);

void EndImmediateCommands(VkDevice device, VkQueue graphicsQueue,
    VkCommandPool commandPool, VkCommandBuffer commandBuffer);

void StagedCopyToBuffer(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool,
    VkQueue graphicsQueue, AllocatedBuffer buffer, void* src, size_t size);

VkCullModeFlags GetCullModeFlags(CullMode cullMode);

inline f32 sRGBToLinear(f32 sRGB)
{
    return sRGB > 0.04045f ? std::powf((sRGB + 0.055f) / 1.055, 2.4f) : sRGB / 12.92f;
}

inline f32 linearToSRGB(f32 linear)
{
    return linear > 0.0031308f ? 1.055f * std::powf(linear, 1.0f / 2.4f) - 0.055f : linear * 12.92f;
}

inline glm::vec3 sRGBToLinear(glm::vec3 sRGB)
{
    return {sRGBToLinear(sRGB.x), sRGBToLinear(sRGB.y), sRGBToLinear(sRGB.z)};
}

inline glm::vec3 linearToSRGB(glm::vec3 linear)
{
    return {linearToSRGB(linear.x), linearToSRGB(linear.y), linearToSRGB(linear.z)};
}

inline glm::vec4 sRGBToLinear(glm::vec4 sRGB)
{
    return {sRGBToLinear(sRGB.x), sRGBToLinear(sRGB.y), sRGBToLinear(sRGB.z), sRGB.a};
}

inline glm::vec4 linearToSRGB(glm::vec4 linear)
{
    return {linearToSRGB(linear.x), linearToSRGB(linear.y), linearToSRGB(linear.z), linear.a};
}



