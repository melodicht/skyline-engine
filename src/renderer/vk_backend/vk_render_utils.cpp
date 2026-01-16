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
VkImageMemoryBarrier2 ImageBarrier(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    imageBarrier.pNext = nullptr;

    imageBarrier.srcStageMask = srcStage;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = dstStage;
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

void StagedCopyToBuffer(VkDevice device, VmaAllocator allocator, VkCommandPool commandPool,
    VkQueue graphicsQueue, AllocatedBuffer buffer, void* src, size_t size)
{
    AllocatedBuffer stagingBuffer = CreateBuffer(device, allocator, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_MAPPED_BIT
        | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* stagingData = stagingBuffer.allocation->GetMappedData();
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
        case NONE:
            return VK_CULL_MODE_NONE;
        case FRONT:
            return VK_CULL_MODE_FRONT_BIT;
        case BACK:
            return VK_CULL_MODE_BACK_BIT;
    }
};

f32 sRGBToLinear(f32 sRGB)
{
    return sRGB > 0.04045f ? std::powf((sRGB + 0.055f) / 1.055, 2.4f) : sRGB / 12.92f;
}

f32 linearToSRGB(f32 linear)
{
    return linear > 0.0031308f ? 1.055f * std::powf(linear, 1.0f / 2.4f) - 0.055f : linear * 12.92f;
}

glm::vec3 sRGBToLinear(glm::vec3 sRGB)
{
    return {sRGBToLinear(sRGB.x), sRGBToLinear(sRGB.y), sRGBToLinear(sRGB.z)};
}

glm::vec3 linearToSRGB(glm::vec3 linear)
{
    return {linearToSRGB(linear.x), linearToSRGB(linear.y), linearToSRGB(linear.z)};
}

glm::vec4 sRGBToLinear(glm::vec4 sRGB)
{
    return {sRGBToLinear(sRGB.x), sRGBToLinear(sRGB.y), sRGBToLinear(sRGB.z), sRGB.a};
}

glm::vec4 linearToSRGB(glm::vec4 linear)
{
    return {linearToSRGB(linear.x), linearToSRGB(linear.y), linearToSRGB(linear.z), linear.a};
}