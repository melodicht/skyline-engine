//Create a buffer using VMA
AllocatedBuffer CreateBuffer(VkDevice device, VmaAllocator allocator, size_t allocSize, VkBufferUsageFlags usage, VmaAllocationCreateFlags allocFlags, VkMemoryPropertyFlags requiredFlags)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;

    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.flags = allocFlags;
    allocInfo.requiredFlags = requiredFlags;
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &newBuffer.buffer, &newBuffer.allocation, nullptr));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = newBuffer.buffer};
        newBuffer.address = vkGetBufferDeviceAddress(device, &addressInfo);
    }

    return newBuffer;
}

void DestroyBuffer(VmaAllocator allocator, AllocatedBuffer buffer)
{
    vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage CreateImage(VmaAllocator allocator, VkFormat format, VkImageCreateFlags createFlags,
                           VkImageUsageFlags usageFlags, VkExtent3D extent, u32 layers,
                           VmaMemoryUsage memUsage, VkMemoryPropertyFlags memFlags)
{
    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = format;
    info.flags = createFlags;
    info.extent = extent;

    info.mipLevels = 1;
    info.arrayLayers = layers;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usageFlags;

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = memUsage;
    allocInfo.requiredFlags = memFlags;

    AllocatedImage image;

    VK_CHECK(vmaCreateImage(allocator, &info, &allocInfo, &image.image, &image.allocation, nullptr));

    return image;
}

void DestroyImage(VmaAllocator allocator, AllocatedImage image)
{
    vmaDestroyImage(allocator, image.image, image.allocation);
}

//Change layout of image
VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || currentLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
            ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageSubresourceRange subImage {};
    subImage.aspectMask = aspectMask;
    subImage.baseMipLevel = 0;
    subImage.levelCount = VK_REMAINING_MIP_LEVELS;
    subImage.baseArrayLayer = 0;
    subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

    imageBarrier.subresourceRange = subImage;
    imageBarrier.image = image;

    return imageBarrier;
}

//Create a temporary command buffer for executing commands outside of the rendering loop
VkCommandBuffer BeginImmediateCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    return commandBuffer;
};

//Delete a command buffer and submit the commands in it
void EndImmediateCommands(VkDevice device, VkQueue graphicsQueue, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkCommandBufferSubmitInfo commandInfo{};
    commandInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandInfo.commandBuffer = commandBuffer;

    VkSubmitInfo2 submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &commandInfo;

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(graphicsQueue));

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
};

VkCullModeFlags GetCullModeFlags(CullMode cullMode)
{
    switch (cullMode)
    {
        case NONE:
            return VK_CULL_MODE_NONE;
        case FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case BACK:
            return VK_CULL_MODE_BACK_BIT;
    }
};
