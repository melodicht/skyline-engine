#include <volk.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <cstring>
#include <iostream>

#include "meta_definitions.h"
#include "render_types.h"
#include "vk_render_types.h"
#include "vk_render_utils.h"

//Create a buffer using VMA
AllocatedBuffer CreateBuffer(VkDevice device, VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags)
{
    VkBufferCreateInfo bufferInfo
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = usage
    };

    VmaAllocationCreateInfo allocInfo
    {
        .flags = allocFlags,
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = requiredFlags
    };

    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = newBuffer.buffer
        };
        newBuffer.address = vkGetBufferDeviceAddress(device, &addressInfo);
    }

    return newBuffer;
}

AllocatedBuffer CreateShaderBuffer(VkDevice device, VmaAllocator allocator, size_t allocSize)
{
    return CreateBuffer(device, allocator, allocSize,
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
}

void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage CreateImage(VmaAllocator allocator, VkFormat format, VkImageCreateFlags createFlags,
                           VkImageUsageFlags usageFlags, VkExtent3D extent, u32 layers,
                           VmaMemoryUsage memUsage, VkMemoryPropertyFlags memFlags)
{
    VkImageCreateInfo info
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = createFlags,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = 1,
        .arrayLayers = layers,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usageFlags
    };

    VmaAllocationCreateInfo allocInfo
    {
        .usage = memUsage,
        .requiredFlags = memFlags
    };

    AllocatedImage image;

    VK_CHECK(vmaCreateImage(allocator, &info, &allocInfo, &image.image, &image.allocation, nullptr));

    return image;
}

void DestroyImage(VmaAllocator allocator, AllocatedImage image)
{
    vmaDestroyImage(allocator, image.image, image.allocation);
}

//Change layout of image
VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
{
    VkImageAspectFlags aspectMask = newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            || currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
            ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageSubresourceRange subImage
    {
        .aspectMask = aspectMask,
        .levelCount = VK_REMAINING_MIP_LEVELS,
        .layerCount = VK_REMAINING_ARRAY_LAYERS
    };

    VkImageMemoryBarrier2 imageBarrier
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStage,
        .srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
        .dstStageMask = dstStage,
        .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT,
        .oldLayout = currentLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = subImage
    };

    return imageBarrier;
}

//Create a temporary command buffer for executing commands outside of the rendering loop
VkCommandBuffer BeginImmediateCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };


    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
};

//Delete a command buffer and submit the commands in it
void EndImmediateCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo commandInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer
    };

    VkSubmitInfo2 submitInfo
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandInfo
    };

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(graphicsQueue));

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
};

void StagedCopyToBuffer(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool,
    VkQueue graphicsQueue, AllocatedBuffer buffer, void* src, size_t size)
{
    AllocatedBuffer stagingBuffer = CreateBuffer(device, allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* stagingData = stagingBuffer.info.pMappedData;
    memcpy(stagingData, src, size);

    VkCommandBuffer cmd = BeginImmediateCommands(device, commandPool);

    VkBufferCopy copy
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, buffer.buffer, 1, &copy);

    EndImmediateCommands(device, graphicsQueue, commandPool, cmd);
    DestroyBuffer(allocator, stagingBuffer);
}

VkCullModeFlags GetCullModeFlags(CullMode cullMode)
{
    switch (cullMode)
    {
    case FRONT:
        return VK_CULL_MODE_FRONT_BIT;
    case BACK:
        return VK_CULL_MODE_BACK_BIT;
    default:
        return VK_CULL_MODE_NONE;
    }
};
