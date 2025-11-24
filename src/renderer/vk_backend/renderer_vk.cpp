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

#define DEFAULT_SLANG true

#include "meta_definitions.h"

#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_vulkan.h>
#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#define VOLK_IMPLEMENTATION
#include <vulkan/volk.h>

#include "vulkan/vma_no_warnings.h"
#include <iostream>

#include <backends/imgui_impl_vulkan.h>

#include "asset_types.h"
#include "renderer/render_backend.h"
#include "renderer/vk_backend/vk_render_types.h"
#include "renderer/vk_backend/vk_render_utils.cpp"

#include <vulkan/VkBootstrap.h>

#include <unordered_map>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED

#include "math/skl_math_consts.h"
#include "math/skl_math_utils.h"

// Vulkan structures
VkInstance instance;
VkDebugUtilsMessengerEXT debugMessenger;
VkSurfaceKHR surface;

VkPhysicalDevice physDevice;
VkDevice device;

VkQueue graphicsQueue;
u32 graphicsQueueFamily;

VmaAllocator allocator;

VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;
AllocatedImage depthImage;
VkImageView depthImageView;

VkFormat swapchainFormat;
VkSwapchainKHR swapchain;
VkExtent2D swapExtent;
std::vector<VkImage> swapImages;
std::vector<VkImageView> swapImageViews;

std::vector<VkSemaphore> renderSemaphores;

#define NUM_FRAMES 2
#define NUM_CASCADES 6

VkCommandPool mainCommandPool;
FrameData frames[NUM_FRAMES];

VkPipelineLayout depthPipelineLayout;
VkPipelineLayout cubemapPipelineLayout;
VkPipelineLayout colorPipelineLayout;
VkPipeline shadowPipeline;
VkPipeline cascadedPipeline;
VkPipeline cubemapPipeline;
VkPipeline depthPipeline;
VkPipeline colorPipeline;

VkDescriptorPool descriptorPool;
VkDescriptorSetLayout texDescriptorLayout;
VkDescriptorSet texDescriptorSet;

u32 frameNum;

u32 swapIndex;
bool resize = false;
u32 currentIndexCount;

VkPipelineLayout *currentLayout;
std::vector<VkImageMemoryBarrier2> imageBarriers;

MeshID currentMeshID;
std::unordered_map<MeshID,Mesh> meshes;
TextureID currentTexID;
std::unordered_map<TextureID,Texture> textures;
LightID currentLightID;
std::unordered_map<LightID,LightEntry> lights;

VkSampler shadowSampler;
VkSampler textureSampler;

u32 currentCamIndex;
u32 mainCamIndex;

// Upload a mesh to the gpu
MeshID UploadMesh(u32 vertCount, Vertex* vertices, u32 indexCount, u32* indices)
{
    currentMeshID++;
    auto iter = meshes.emplace(currentMeshID, Mesh());
    Mesh& mesh = iter.first->second;

    size_t indexSize = sizeof(u32) * indexCount;
    size_t vertSize = sizeof(Vertex) * vertCount;

    mesh.indexBuffer = CreateBuffer(device, allocator, indexSize,
                                     VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                     | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     0,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    mesh.vertBuffer = CreateBuffer(device, allocator, vertSize,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                    | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                    0,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);


    AllocatedBuffer stagingBuffer = CreateBuffer(device, allocator,
                                                 indexSize + vertSize,
                                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                 VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                 | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* stagingData = stagingBuffer.allocation->GetMappedData();
    memcpy(stagingData, indices, indexSize);
    memcpy((char*)stagingData + indexSize, vertices, vertSize);

    VkCommandBuffer cmd = BeginImmediateCommands(device, mainCommandPool);

    VkBufferCopy indexCopy{0};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = 0;
    indexCopy.size = indexSize;

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.indexBuffer.buffer, 1, &indexCopy);

    VkBufferCopy vertCopy{0};
    vertCopy.dstOffset = 0;
    vertCopy.srcOffset = indexSize;
    vertCopy.size = vertSize;

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.vertBuffer.buffer, 1, &vertCopy);

    EndImmediateCommands(device, graphicsQueue, mainCommandPool, cmd);
    DestroyBuffer(allocator, stagingBuffer);


    mesh.indexCount = indexCount;

    return currentMeshID;
}

MeshID UploadMesh(RenderUploadMeshInfo& info)
{
    return UploadMesh(info.vertSize, info.vertData, info.idxSize, info.idxData);
}

void DestroyMesh(RenderDestroyMeshInfo& info)
{
    Mesh& mesh = meshes[info.meshID];
    DestroyBuffer(allocator, mesh.indexBuffer);
    DestroyBuffer(allocator, mesh.vertBuffer);
    meshes.erase(info.meshID);
}

u32 CreateCameraBuffer(u32 viewCount)
{
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        frames[i].cameraBuffers.push_back(CreateBuffer(device, allocator,
                                                       sizeof(CameraData) * viewCount,
                                                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                       VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                       | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
    }

    return frames[0].cameraBuffers.size() - 1;
}

Texture CreateDepthTexture(u32 width, u32 height)
{
    Texture texture;

    AllocatedImage depthTexture = CreateImage(allocator,
                             depthFormat, 0,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                             {width, height, 1}, 1,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = depthFormat;
    depthViewInfo.image = depthTexture.image;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = depthTexView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstArrayElement = currentTexID;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.dstSet = texDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    texture.texture = depthTexture;
    texture.imageView = depthTexView;
    texture.extent = {width, height};
    texture.descriptorIndex = currentTexID;

    currentTexID++;

    return texture;
}

Texture CreateDepthArray(u32 width, u32 height, u32 layers)
{
    Texture texture;

    AllocatedImage depthTexture = CreateImage(allocator,
                                              depthFormat, 0,
                                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                              {width, height, 1}, layers,
                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                              VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    depthViewInfo.format = depthFormat;
    depthViewInfo.image = depthTexture.image;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = layers;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = depthTexView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstArrayElement = currentTexID;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.dstSet = texDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    texture.texture = depthTexture;
    texture.imageView = depthTexView;
    texture.extent = {width, height};
    texture.descriptorIndex = currentTexID;

    currentTexID++;

    return texture;
}

Texture CreateDepthCubemap(u32 width, u32 height)
{
    Texture texture;

    AllocatedImage depthTexture = CreateImage(allocator,
                                              depthFormat,
                                              VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                              {width, height, 1}, 6,
                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                              VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    depthViewInfo.format = depthFormat;
    depthViewInfo.image = depthTexture.image;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 6;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = depthTexView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstArrayElement = currentTexID;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.dstSet = texDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    texture.texture = depthTexture;
    texture.imageView = depthTexView;
    texture.extent = {width, height};
    texture.descriptorIndex = currentTexID;

    currentTexID++;

    return texture;
}

TextureID UploadTexture(RenderUploadTextureInfo& info)
{
    AllocatedImage texImage = CreateImage(allocator,
                                          VK_FORMAT_R8G8B8A8_UNORM, 0,
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                          | VK_IMAGE_USAGE_SAMPLED_BIT,
                                          {info.width, info.height, 1}, 1,
                                          VMA_MEMORY_USAGE_GPU_ONLY,
                                          VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    VkImageView texView;

    VkImageViewCreateInfo texViewInfo{};
    texViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    texViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    texViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    texViewInfo.image = texImage.image;
    texViewInfo.subresourceRange.baseMipLevel = 0;
    texViewInfo.subresourceRange.levelCount = 1;
    texViewInfo.subresourceRange.baseArrayLayer = 0;
    texViewInfo.subresourceRange.layerCount = 1;
    texViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    VK_CHECK(vkCreateImageView(device, &texViewInfo, nullptr, &texView));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = texView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.dstArrayElement = currentTexID;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.dstSet = texDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    size_t dataSize = info.width * info.height * 4;
    AllocatedBuffer uploadBuffer = CreateBuffer(device, allocator, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    void* uploadData;
    vmaMapMemory(allocator, uploadBuffer.allocation, &uploadData);
    memcpy(uploadData, info.pixelData, dataSize);
    vmaUnmapMemory(allocator, uploadBuffer.allocation);

    VkCommandBuffer commandBuffer = BeginImmediateCommands(device, mainCommandPool);

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(texImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;

    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {info.width, info.height, 1};

    vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, texImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    imageBarrier = ImageBarrier(texImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    EndImmediateCommands(device, graphicsQueue, mainCommandPool, commandBuffer);

    DestroyBuffer(allocator, uploadBuffer);
    auto iter = textures.emplace(currentTexID, Texture());
    Texture& texture = iter.first->second;

    texture.texture = texImage;
    texture.imageView = texView;
    texture.extent = {info.width, info.height};
    texture.descriptorIndex = currentTexID;

    return currentTexID++;
}

void DestroyTexture(TextureID texID)
{
    Texture& texture = textures[texID];
    vkDestroyImageView(device, texture.imageView, nullptr);
    DestroyImage(allocator, texture.texture);
    textures.erase(texID);
}

LightID AddDirLight()
{
    currentLightID++;
    auto iter = lights.emplace(currentLightID, LightEntry());
    LightEntry& light = iter.first->second;
    light.cameraIndex = CreateCameraBuffer(NUM_CASCADES);
    light.shadowMap = CreateDepthArray(2048, 2048, NUM_CASCADES);

    return currentLightID;
}

LightID AddSpotLight()
{
    currentLightID++;
    auto iter = lights.emplace(currentLightID, LightEntry());
    LightEntry& light = iter.first->second;
    light.cameraIndex = CreateCameraBuffer(1);
    light.shadowMap = CreateDepthTexture(1024, 1024);

    return currentLightID;
}

LightID AddPointLight()
{
    currentLightID++;
    auto iter = lights.emplace(currentLightID, LightEntry());
    LightEntry& light = iter.first->second;
    light.cameraIndex = CreateCameraBuffer(6);
    light.shadowMap = CreateDepthCubemap(512, 512);

    return currentLightID;
}

void DestroyDirLight(LightID lightID)
{

}

void DestroySpotLight(LightID lightID)
{

}

void DestroyPointLight(LightID lightID)
{

}

// Create swapchain or recreate to change size
void CreateSwapchain(u32 width, u32 height, VkSwapchainKHR oldSwapchain)
{
    // Create the swapchain
    vkb::SwapchainBuilder swapBuilder{physDevice, device, surface};

    swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapBuilder
            .set_desired_format(VkSurfaceFormatKHR{.format = swapchainFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .set_old_swapchain(oldSwapchain)
            .build().value();

    swapchain = vkbSwapchain.swapchain;
    swapExtent = vkbSwapchain.extent;
    swapImages = vkbSwapchain.get_images().value();
    swapImageViews = vkbSwapchain.get_image_views().value();


    // Create the depth buffer

    VkExtent3D depthImageExtent =
            {
                    swapExtent.width,
                    swapExtent.height,
                    1
            };

    depthImage = CreateImage(allocator,
                             depthFormat, 0,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             depthImageExtent, 1,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = depthFormat;
    depthViewInfo.image = depthImage.image;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthImageView));
}

void DestroySwapResources()
{
    vkDestroyImageView(device, depthImageView, nullptr);
    DestroyImage(allocator, depthImage);

    for (VkImageView imageView : swapImageViews)
    {
        vkDestroyImageView(device, imageView, nullptr);
    }
}

void RecreateSwapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &surfaceCapabilities);

    u32 width = surfaceCapabilities.currentExtent.width;
    u32 height = surfaceCapabilities.currentExtent.height;

    if (width == 0 || height == 0)
    {
        return;
    }

    vkDeviceWaitIdle(device);

    DestroySwapResources();

    VkSwapchainKHR old = swapchain;
    CreateSwapchain(width, height, old);
    vkDestroySwapchainKHR(device, old, nullptr);
}

SDL_WindowFlags GetRenderWindowFlags()
{
    return SDL_WINDOW_VULKAN;
}

// Initialize the rendering API
void InitRenderer(RenderInitInfo& info)
{
    volkInitializeCustom((PFN_vkGetInstanceProcAddr)SDL_Vulkan_GetVkGetInstanceProcAddr());

    // Create Vulkan instance
    vkb::InstanceBuilder builder{vkGetInstanceProcAddr};
    vkb::Instance vkbInstance = builder
            .set_app_name("Skyline Engine")
            .request_validation_layers()
            .use_default_debug_messenger()
            .require_api_version(1, 3)
            .build().value();


    instance = vkbInstance.instance;
    volkLoadInstanceOnly(instance);
    debugMessenger = vkbInstance.debug_messenger;


    // Create window surface
    SDL_Vulkan_CreateSurface(info.window, instance, vkbInstance.allocation_callbacks, &surface);

    VkPhysicalDeviceFeatures feat10{};
    feat10.depthClamp = true;
    feat10.shaderInt64 = true;

    VkPhysicalDeviceVulkan11Features feat11{};
    feat11.shaderDrawParameters = true;
    feat11.multiview = true;

    VkPhysicalDeviceVulkan12Features feat12{};
    feat12.bufferDeviceAddress = true;
    feat12.descriptorIndexing = true;
    feat12.descriptorBindingPartiallyBound = true;
    feat12.descriptorBindingSampledImageUpdateAfterBind = true;
    feat12.runtimeDescriptorArray = true;
    feat12.scalarBlockLayout = true;

    VkPhysicalDeviceVulkan13Features feat13{};
    feat13.dynamicRendering = true;
    feat13.synchronization2 = true;


    // Select which GPU to use
    vkb::PhysicalDeviceSelector selector{vkbInstance};
    vkb::PhysicalDevice vkbPhysDevice = selector
            .set_surface(surface)
            .set_minimum_version(1, 3)
            .set_required_features(feat10)
            .set_required_features_11(feat11)
            .set_required_features_12(feat12)
            .set_required_features_13(feat13)
            .prefer_gpu_device_type()
            .select().value();
    physDevice = vkbPhysDevice.physical_device;


    // Create the logical GPU device
    vkb::DeviceBuilder deviceBuilder{vkbPhysDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();
    device = vkbDevice.device;
    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
    volkLoadDevice(device);


    // Create the VMA allocator
    VmaVulkanFunctions vmaFuncs{};
    vmaFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vmaFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocInfo = {};
    allocInfo.physicalDevice = physDevice;
    allocInfo.device = device;
    allocInfo.instance = instance;
    allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocInfo.pVulkanFunctions = &vmaFuncs;

    VK_CHECK(vmaCreateAllocator(&allocInfo, &allocator));

    // Create the swapchain and associated resources at the default dimensions
    CreateSwapchain(info.startWidth, info.startHeight, nullptr);

    // Create the command pools and command buffers
    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    commandPoolInfo.queueFamilyIndex = graphicsQueueFamily;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &mainCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = {};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.pNext = nullptr;
    cmdAllocInfo.commandBufferCount = 1;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

        cmdAllocInfo.commandPool = frames[i].commandPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].commandBuffer));
    }

    // Create the sync structures
    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frames[i].acquireSemaphore));

    }

    renderSemaphores = std::vector<VkSemaphore>(swapImages.size());

    for (int i = 0; i < swapImages.size(); i++)
    {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderSemaphores[i]));
    }
}

// Also checks that the creation of the shader module is good.
VkShaderModule CreateShaderModuleFromFile(const char *FilePath)
{
    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    void *shaderFile = SDL_LoadFile(FilePath, &shaderInfo.codeSize);
    if (!shaderFile)
    {
        std::cerr << "Failed to load shader file: " << SDL_GetError() << std::endl;
        return VK_NULL_HANDLE;
    }
    shaderInfo.pCode = reinterpret_cast<const u32*>(shaderFile);
    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &shaderInfo, nullptr, &shaderModule));
    return shaderModule;
}

VkPipelineShaderStageCreateInfo CreateStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule, const char *entryPointName)
{
    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = shaderStage;
    stageInfo.module = shaderModule;
    stageInfo.pName = entryPointName;
    return stageInfo;
}

void InitPipelines(RenderPipelineInitInfo& info)
{
    // Create object and light buffers
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        frames[i].objectBuffer = CreateBuffer(device, allocator,
                                              sizeof(ObjectData) * 4096,
                                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                              VMA_ALLOCATION_CREATE_MAPPED_BIT
                                              | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        frames[i].dirLightBuffer = CreateBuffer(device, allocator,
                                                sizeof(VkDirLightData) * 4,
                                                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        frames[i].dirCascadeBuffer = CreateBuffer(device, allocator,
                                                  sizeof(LightCascade) * NUM_CASCADES * 4,
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                  | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        frames[i].spotLightBuffer = CreateBuffer(device, allocator,
                                                 sizeof(VkSpotLightData) * 256,
                                                 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                 VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                 | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        frames[i].pointLightBuffer = CreateBuffer(device, allocator,
                                                  sizeof(VkPointLightData) * 256,
                                                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                  VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                  | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    }

    mainCamIndex = CreateCameraBuffer(1);

    // Create shader stages
#if DEFAULT_SLANG
    VkShaderModule depthShader = CreateShaderModuleFromFile("shaders/depth.spv");
    
    VkShaderModule cubemapShader = CreateShaderModuleFromFile("shaders/cubemap.spv");
    VkShaderModule cubemapVertShader = cubemapShader;
    VkShaderModule cubemapFragShader = cubemapShader;

    VkShaderModule colorShader = CreateShaderModuleFromFile("shaders/color.spv");
    VkShaderModule colorVertShader = colorShader;
    VkShaderModule colorFragShader = colorShader;
#else
    VkShaderModule depthShader = CreateShaderModuleFromFile("shaders/depth.vert.spv");
    
    VkShaderModule cubemapVertShader = CreateShaderModuleFromFile("shaders/cubemap.vert.spv");
    VkShaderModule cubemapFragShader = CreateShaderModuleFromFile("shaders/cubemap.frag.spv");
    
    VkShaderModule colorVertShader = CreateShaderModuleFromFile("shaders/color.vert.spv");
    VkShaderModule colorFragShader = CreateShaderModuleFromFile("shaders/color.frag.spv");
#endif

#if DEFAULT_SLANG
    const char *vertEntryPointName = "vertexMain";
    const char *fragEntryPointName = "fragmentMain";
#else
    const char *vertEntryPointName = "main";
    const char *fragEntryPointName = "main";
#endif
    
    VkPipelineShaderStageCreateInfo colorVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, colorVertShader, vertEntryPointName);
    VkPipelineShaderStageCreateInfo depthVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, depthShader, vertEntryPointName);
    VkPipelineShaderStageCreateInfo cubemapVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, cubemapVertShader, vertEntryPointName);
    VkPipelineShaderStageCreateInfo colorFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, colorFragShader, fragEntryPointName);
    VkPipelineShaderStageCreateInfo cubemapFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, cubemapFragShader, fragEntryPointName);
    
    VkPipelineShaderStageCreateInfo colorShaderStages[] = {colorVertStageInfo, colorFragStageInfo};
    VkPipelineShaderStageCreateInfo cubemapShaderStages[] = {cubemapVertStageInfo, cubemapFragStageInfo};

    // Set up descriptor pool and set for textures
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 512}, {VK_DESCRIPTOR_TYPE_SAMPLER, 2}};

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
                               | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    descriptorPoolInfo.maxSets = 1;
    descriptorPoolInfo.poolSizeCount = 2;
    descriptorPoolInfo.pPoolSizes = poolSizes;

    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorBindingFlags bindingFlags[3] = {bindlessFlags, 0, 0};
    VkDescriptorSetLayoutBinding texBinding{};
    texBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    texBinding.descriptorCount = 512;
    texBinding.binding = 0;
    texBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    texBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding shadowSamplerBinding{};
    shadowSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    shadowSamplerBinding.descriptorCount = 1;
    shadowSamplerBinding.binding = 1;
    shadowSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    shadowSamplerBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding textureSamplerBinding = shadowSamplerBinding;
    textureSamplerBinding.binding = 2;

    VkDescriptorSetLayoutBinding bindings[3] = {texBinding, shadowSamplerBinding, textureSamplerBinding};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
    bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlagsInfo.bindingCount = 3;
    bindingFlagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 3;
    descriptorLayoutInfo.pBindings = bindings;
    descriptorLayoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    descriptorLayoutInfo.pNext = &bindingFlagsInfo;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &texDescriptorLayout));

    VkDescriptorSetAllocateInfo descriptorAllocInfo{};
    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = descriptorPool;
    descriptorAllocInfo.descriptorSetCount = 1;
    descriptorAllocInfo.pSetLayouts = &texDescriptorLayout;

    VK_CHECK(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &texDescriptorSet));

    VkSamplerCreateInfo shadowSamplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    shadowSamplerInfo.magFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.minFilter = VK_FILTER_LINEAR;
    shadowSamplerInfo.compareEnable = VK_TRUE;
    shadowSamplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowSampler);

    VkDescriptorImageInfo shadowSamplerDescInfo{};
    shadowSamplerDescInfo.sampler = shadowSampler;

    VkWriteDescriptorSet shadowSamplerWrite{};
    shadowSamplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadowSamplerWrite.descriptorCount = 1;
    shadowSamplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    shadowSamplerWrite.dstSet = texDescriptorSet;
    shadowSamplerWrite.dstBinding = 1;
    shadowSamplerWrite.pImageInfo = &shadowSamplerDescInfo;

    VkSamplerCreateInfo textureSamplerInfo = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    textureSamplerInfo.magFilter = VK_FILTER_NEAREST;
    textureSamplerInfo.minFilter = VK_FILTER_NEAREST;
    textureSamplerInfo.compareEnable = VK_FALSE;
    vkCreateSampler(device, &textureSamplerInfo, nullptr, &textureSampler);

    VkDescriptorImageInfo textureSamplerDescInfo{};
    textureSamplerDescInfo.sampler = textureSampler;

    VkWriteDescriptorSet textureSamplerWrite = shadowSamplerWrite;
    textureSamplerWrite.dstBinding = 2;
    textureSamplerWrite.pImageInfo = &textureSamplerDescInfo;

    VkWriteDescriptorSet samplerWrites[2] = {shadowSamplerWrite, textureSamplerWrite};

    vkUpdateDescriptorSets(device, 2, samplerWrites, 0, nullptr);

    // Create render pipeline layouts

    VkPushConstantRange pushConstants;
    pushConstants.offset = 0;
    pushConstants.size = sizeof(VkDeviceAddress) + sizeof(VertPushConstants) + sizeof(FragPushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPushConstantRange cubeVertPushConstants;
    cubeVertPushConstants.offset = 0;
    cubeVertPushConstants.size = sizeof(VkDeviceAddress) + sizeof(VertPushConstants) + sizeof(CubemapPushConstants);
    cubeVertPushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPushConstantRange cubeFragPushConstants;
    cubeFragPushConstants.offset = sizeof(VkDeviceAddress) + sizeof(VertPushConstants);
    cubeFragPushConstants.size =  sizeof(CubemapPushConstants);
    cubeFragPushConstants.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkPushConstantRange cubePushConstants[2] = {cubeVertPushConstants, cubeFragPushConstants};

    VkPipelineLayoutCreateInfo depthLayoutInfo{};
    depthLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    depthLayoutInfo.setLayoutCount = 0;
    depthLayoutInfo.pSetLayouts = nullptr;
    depthLayoutInfo.pushConstantRangeCount = 1;
    depthLayoutInfo.pPushConstantRanges = &pushConstants;

    VK_CHECK(vkCreatePipelineLayout(device, &depthLayoutInfo, nullptr, &depthPipelineLayout));

    VkPipelineLayoutCreateInfo cubemapLayoutInfo{};
    cubemapLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    cubemapLayoutInfo.setLayoutCount = 0;
    cubemapLayoutInfo.pSetLayouts = nullptr;
    cubemapLayoutInfo.pushConstantRangeCount = 2;
    cubemapLayoutInfo.pPushConstantRanges = cubePushConstants;

    VK_CHECK(vkCreatePipelineLayout(device, &cubemapLayoutInfo, nullptr, &cubemapPipelineLayout));

    VkPipelineLayoutCreateInfo colorLayoutInfo{};
    colorLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    colorLayoutInfo.setLayoutCount = 1;
    colorLayoutInfo.pSetLayouts = &texDescriptorLayout;
    colorLayoutInfo.pushConstantRangeCount = 1;
    colorLayoutInfo.pPushConstantRanges = &pushConstants;

    VK_CHECK(vkCreatePipelineLayout(device, &colorLayoutInfo, nullptr, &colorPipelineLayout));


    // Create render pipelines (AKA fill in 20000 info structs)
    VkDynamicState dynamicStates[] =
            {
                    VK_DYNAMIC_STATE_VIEWPORT,
                    VK_DYNAMIC_STATE_SCISSOR,
                    VK_DYNAMIC_STATE_CULL_MODE
            };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 3;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineVertexInputStateCreateInfo vertexInfo{};  // We are using vertex pulling so no need for vertex input description
    vertexInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = nullptr;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo cubemapRasterizer = rasterizer;
    cubemapRasterizer.depthClampEnable = VK_TRUE;

    VkPipelineRasterizationStateCreateInfo shadowRasterizer = cubemapRasterizer;
    shadowRasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    // For pre-depth pass
    VkPipelineDepthStencilStateCreateInfo preDepthStencil{};
    preDepthStencil.depthTestEnable = VK_TRUE;
    preDepthStencil.depthWriteEnable = VK_TRUE;
    preDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    preDepthStencil.depthBoundsTestEnable = VK_FALSE;
    preDepthStencil.minDepthBounds = 0.0f;
    preDepthStencil.maxDepthBounds = 1.0f;
    preDepthStencil.stencilTestEnable = VK_FALSE;
    preDepthStencil.front = {};
    preDepthStencil.back = {};

    // For color pass
    VkPipelineDepthStencilStateCreateInfo colorDepthStencil{};
    colorDepthStencil.depthTestEnable = VK_TRUE;
    colorDepthStencil.depthWriteEnable = VK_FALSE;
    colorDepthStencil.depthCompareOp = VK_COMPARE_OP_EQUAL;
    colorDepthStencil.depthBoundsTestEnable = VK_FALSE;
    colorDepthStencil.minDepthBounds = 0.0f;
    colorDepthStencil.maxDepthBounds = 1.0f;
    colorDepthStencil.stencilTestEnable = VK_FALSE;
    colorDepthStencil.front = {};
    colorDepthStencil.back = {};

    // For pre-depth pass
    VkPipelineRenderingCreateInfo depthRenderInfo{};
    depthRenderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    depthRenderInfo.colorAttachmentCount = 0;
    depthRenderInfo.depthAttachmentFormat = depthFormat;

    VkPipelineRenderingCreateInfo cascadedRenderInfo = depthRenderInfo;
    cascadedRenderInfo.viewMask = (1 << NUM_CASCADES) - 1;

    VkPipelineRenderingCreateInfo cubemapRenderInfo = depthRenderInfo;
    cubemapRenderInfo.viewMask = 0x3F;

    // For color pass
    VkPipelineRenderingCreateInfo colorRenderInfo{};
    colorRenderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    colorRenderInfo.colorAttachmentCount = 1;
    colorRenderInfo.pColorAttachmentFormats = &swapchainFormat;
    colorRenderInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo depthPipelineInfo{};
    depthPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    depthPipelineInfo.stageCount = 1;
    depthPipelineInfo.pStages = &depthVertStageInfo;
    depthPipelineInfo.pVertexInputState = &vertexInfo;
    depthPipelineInfo.pInputAssemblyState = &inputAssembly;
    depthPipelineInfo.pViewportState = &viewportState;
    depthPipelineInfo.pRasterizationState = &rasterizer;
    depthPipelineInfo.pMultisampleState = &multisampling;
    depthPipelineInfo.pDepthStencilState = &preDepthStencil;
    depthPipelineInfo.pColorBlendState = &colorBlending;
    depthPipelineInfo.pDynamicState = &dynamicState;
    depthPipelineInfo.layout = depthPipelineLayout;
    depthPipelineInfo.renderPass = nullptr; // No renderpass necessary because we are using dynamic rendering
    depthPipelineInfo.subpass = 0;
    depthPipelineInfo.pNext = &depthRenderInfo;

    VkGraphicsPipelineCreateInfo shadowPipelineInfo = depthPipelineInfo;
    shadowPipelineInfo.pRasterizationState = &shadowRasterizer;

    VkGraphicsPipelineCreateInfo cascadedPipelineInfo = shadowPipelineInfo;
    cascadedPipelineInfo.pNext = &cascadedRenderInfo;

    VkGraphicsPipelineCreateInfo cubemapPipelineInfo = shadowPipelineInfo;
    cubemapPipelineInfo.stageCount = 2;
    cubemapPipelineInfo.pStages = cubemapShaderStages;
    cubemapPipelineInfo.pRasterizationState = &cubemapRasterizer;
    cubemapPipelineInfo.pNext = &cubemapRenderInfo;

    VkGraphicsPipelineCreateInfo colorPipelineInfo{};
    colorPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    colorPipelineInfo.stageCount = 2;
    colorPipelineInfo.pStages = colorShaderStages;
    colorPipelineInfo.pVertexInputState = &vertexInfo;
    colorPipelineInfo.pInputAssemblyState = &inputAssembly;
    colorPipelineInfo.pViewportState = &viewportState;
    colorPipelineInfo.pRasterizationState = &rasterizer;
    colorPipelineInfo.pMultisampleState = &multisampling;
    colorPipelineInfo.pDepthStencilState = &colorDepthStencil;
    colorPipelineInfo.pColorBlendState = &colorBlending;
    colorPipelineInfo.pDynamicState = &dynamicState;
    colorPipelineInfo.layout = colorPipelineLayout;
    colorPipelineInfo.renderPass = nullptr; // No renderpass necessary because we are using dynamic rendering
    colorPipelineInfo.subpass = 0;
    colorPipelineInfo.pNext = &colorRenderInfo;

    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &shadowPipelineInfo, nullptr, &shadowPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &cascadedPipelineInfo, nullptr, &cascadedPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &cubemapPipelineInfo, nullptr, &cubemapPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &depthPipelineInfo, nullptr, &depthPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &colorPipelineInfo, nullptr, &colorPipeline));

    vkDestroyShaderModule(device, depthShader, nullptr);
#if DEFAULT_SLANG
    vkDestroyShaderModule(device, colorShader, nullptr);
    vkDestroyShaderModule(device, cubemapShader, nullptr);
#else
    vkDestroyShaderModule(device, colorVertShader, nullptr);
    vkDestroyShaderModule(device, colorFragShader, nullptr);
    vkDestroyShaderModule(device, cubemapVertShader, nullptr);
    vkDestroyShaderModule(device, cubemapFragShader, nullptr);
#endif

#if SKL_ENABLED_EDITOR
    // Initialize ImGui
    ImGui_ImplVulkan_InitInfo imGuiInfo{};
    imGuiInfo.ApiVersion = VK_API_VERSION_1_3;
    imGuiInfo.Instance = instance;
    imGuiInfo.PhysicalDevice = physDevice;
    imGuiInfo.Device = device;
    imGuiInfo.QueueFamily = graphicsQueueFamily;
    imGuiInfo.Queue = graphicsQueue;
    imGuiInfo.MinImageCount = 2;
    imGuiInfo.ImageCount = swapImages.size();
    imGuiInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imGuiInfo.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1;
    imGuiInfo.UseDynamicRendering = true;
    imGuiInfo.PipelineRenderingCreateInfo = colorRenderInfo;
    ImGui_ImplVulkan_Init(&imGuiInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
#endif
}

// Set up frame and begin capturing draw calls
bool InitFrame()
{
#if SKL_ENABLED_EDITOR
    ImGui_ImplVulkan_NewFrame();
#endif

    imageBarriers.clear();

    //Set up commands
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    //Synchronize and get images
    VK_CHECK(vkWaitForFences(device, 1, &frames[frameNum].renderFence, true, 1000000000));

    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, 1000000000, frames[frameNum].acquireSemaphore, nullptr, &swapIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapchain();
        return false;
    }
    if (acquireResult == VK_SUBOPTIMAL_KHR)
    {
        resize = true;
    }

    VK_CHECK(vkResetFences(device, 1, &frames[frameNum].renderFence));

    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //Begin commands and rendering
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return true;
}

void BeginDepthPass(VkImageView depthView, VkExtent2D extent, CullMode cullMode, u32 layerCount)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = extent.width;
    scissor.extent.height = extent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetCullMode(cmd, GetCullModeFlags(cullMode));

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue.depthStencil.depth = 1.0f;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = extent;
    renderInfo.viewMask = layerCount > 1 ? (1 << layerCount) - 1 : 0;
    renderInfo.layerCount = layerCount;
    renderInfo.colorAttachmentCount = 0;
    renderInfo.pColorAttachments = nullptr;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);
}

void BeginDepthPass(CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = swapExtent.height;
    viewport.width = (float)swapExtent.width;
    viewport.height = -(float)swapExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(depthImageView, swapExtent, cullMode, 1);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, depthPipeline);
    currentLayout = &depthPipelineLayout;

    imageBarriers.push_back(ImageBarrier(depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL));
}

void BeginShadowPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, 1);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
    currentLayout = &depthPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void BeginCascadedPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)extent.width;
    viewport.height = (float)extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, NUM_CASCADES);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedPipeline);
    currentLayout = &depthPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void BeginCubemapShadowPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = extent.height;
    viewport.width = (float)extent.width;
    viewport.height = -(float)extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipeline);
    currentLayout = &cubemapPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void BeginColorPass(CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    imageBarriers.push_back(ImageBarrier(swapImages[swapIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = imageBarriers.size();
    depInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(cmd, &depInfo);

    //set dynamic viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = swapExtent.height;
    viewport.width = (float)swapExtent.width;
    viewport.height = -(float)swapExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = swapExtent.width;
    scissor.extent.height = swapExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetCullMode(cmd, GetCullModeFlags(cullMode));
    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthBiasEnable(cmd, false);

    VkClearValue clearValue;
    clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapImageViews[swapIndex];
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clearValue;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea.offset = {0, 0};
    renderInfo.renderArea.extent = swapExtent;
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;
    renderInfo.pDepthAttachment = &depthAttachment;
    renderInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, colorPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, colorPipelineLayout, 0, 1, &texDescriptorSet, 0, nullptr);
    currentLayout = &colorPipelineLayout;
}

void EndPass()
{
    vkCmdEndRendering(frames[frameNum].commandBuffer);
}

void DrawImGui()
{
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frames[frameNum].commandBuffer);
}

// Set the matrices of the camera (Must be called between InitFrame and EndFrame)
void SetCamera(u32 index)
{
    currentCamIndex = index;
    vkCmdPushConstants(frames[frameNum].commandBuffer, *currentLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(VkDeviceAddress), &frames[frameNum].cameraBuffers[index].address);
}

void UpdateCamera(u32 viewCount, CameraData* views)
{
    void* cameraData = frames[frameNum].cameraBuffers[currentCamIndex].allocation->GetMappedData();
    memcpy(cameraData, views, sizeof(CameraData) * viewCount);
}

void SetCubemapInfo(glm::vec3 lightPos, f32 farPlane)
{
    CubemapPushConstants pushConstants = {lightPos, farPlane};
    vkCmdPushConstants(frames[frameNum].commandBuffer, cubemapPipelineLayout,
                       VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(VkDeviceAddress) + sizeof(VertPushConstants), sizeof(CubemapPushConstants),
                       &pushConstants);
}

void SetLights(glm::vec3 ambientLight,
               u32 dirCount, VkDirLightData* dirData, LightCascade* dirCascades,
               u32 spotCount, VkSpotLightData* spotData,
               u32 pointCount, VkPointLightData* pointData)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    if (dirCount > 0)
    {
        void* dirLightData = frames[frameNum].dirLightBuffer.allocation->GetMappedData();
        memcpy(dirLightData, dirData, sizeof(VkDirLightData) * dirCount);
        void* dirCascadeData = frames[frameNum].dirCascadeBuffer.allocation->GetMappedData();
        memcpy(dirCascadeData, dirCascades, sizeof(LightCascade) * NUM_CASCADES);
    }

    if (spotCount > 0)
    {
        void* spotLightData = frames[frameNum].spotLightBuffer.allocation->GetMappedData();
        memcpy(spotLightData, spotData, sizeof(VkSpotLightData) * spotCount);
    }

    if (pointCount > 0)
    {
        void* pointLightData = frames[frameNum].pointLightBuffer.allocation->GetMappedData();
        memcpy(pointLightData, pointData, sizeof(VkPointLightData) * pointCount);
    }

    FragPushConstants pushConstants = {frames[frameNum].dirLightBuffer.address,
                                       frames[frameNum].dirCascadeBuffer.address,
                                       frames[frameNum].spotLightBuffer.address,
                                       frames[frameNum].pointLightBuffer.address,
                                       dirCount, NUM_CASCADES, spotCount, pointCount,
                                       ambientLight};

    vkCmdPushConstants(cmd, colorPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(VkDeviceAddress) + sizeof(VertPushConstants), sizeof(FragPushConstants), &pushConstants);
}

// Send the matrices of the models to render (Must be called between InitFrame and EndFrame)
void SendObjectData(std::vector<ObjectData>& objects)
{
    void* objectData = frames[frameNum].objectBuffer.allocation->GetMappedData();
    memcpy(objectData, objects.data(), sizeof(ObjectData) * objects.size());
}

// Set the mesh currently being rendered (Must be called between InitFrame and EndFrame)
void SetMesh(MeshID meshIndex)
{
    Mesh* mesh = &meshes[meshIndex];

    // Send addresses to camera, object, and vertex buffers as push constants
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;
    VertPushConstants pushConstants = {frames[frameNum].objectBuffer.address, mesh->vertBuffer.address};
    vkCmdPushConstants(cmd, *currentLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(VkDeviceAddress), sizeof(VertPushConstants), &pushConstants);
    // Bind the index buffer
    vkCmdBindIndexBuffer(cmd, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    currentIndexCount = mesh->indexCount;
}

// Draw multiple objects to the screen (Must be called between InitFrame and EndFrame and after SetMesh)
void DrawObjects(int count, int startIndex)
{
    vkCmdDrawIndexed(frames[frameNum].commandBuffer, currentIndexCount, count, 0, 0, startIndex);
}

// End the frame and present it to the screen
void EndFrame()
{
    // End dynamic rendering and commands
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(swapImages[swapIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.pNext = nullptr;

    //Submit commands
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    submit.pWaitDstStageMask = &waitStage;
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &frames[frameNum].acquireSemaphore;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderSemaphores[swapIndex];
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, frames[frameNum].renderFence));

    //Draw to screen
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &renderSemaphores[swapIndex];
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapIndex;

    VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR || resize)
    {
        resize = false;
        RecreateSwapchain();
    }

    frameNum++;
    frameNum %= NUM_FRAMES;
}


void RenderUpdate(RenderFrameInfo& info)
{
    if (!InitFrame())
    {
        return;
    }

    // 1. Gather counts of each unique mesh pointer.
    std::map<MeshID, u32> meshCounts;
    for (MeshRenderInfo meshInfo : info.meshes)
    {
        ++meshCounts[meshInfo.mesh];
    }

    // 2. Create, with fixed size, the list of Mat4s, by adding up all of the counts.
    // 3. Get pointers to the start of each segment of unique mesh pointer.
    u32 totalCount = 0;
    std::unordered_map<MeshID, u32> offsets;
    for (std::pair<MeshID, u32> pair: meshCounts)
    {
        offsets[pair.first] = totalCount;
        totalCount += pair.second;
    }

    std::vector<ObjectData> objects(totalCount);

    // 4. Iterate through scene view once more and fill in the fixed size array.
    for (MeshRenderInfo meshInfo : info.meshes)
    {
        glm::mat4 model = meshInfo.matrix;
        MeshID mesh = meshInfo.mesh;
        TextureID tex = meshInfo.texture;
        glm::vec3 color = meshInfo.rgbColor;

        objects[offsets[mesh]++] = {model, tex, glm::vec4(color.r, color.g, color.b, 1.0f)};
    }

    SendObjectData(objects);

    Transform3D cameraTransform = info.cameraTransform;
    glm::mat4 view = GetViewMatrix(&cameraTransform);
    f32 aspect = (f32)swapExtent.width / (f32)swapExtent.height;

    glm::mat4 proj = glm::perspective(glm::radians(info.cameraFov), aspect, info.cameraNear, info.cameraFar);

    // Calculate cascaded shadow views

    CameraData dirViews[NUM_CASCADES];

    f32 subFrustumSize = (info.cameraFar - info.cameraNear) / NUM_CASCADES;

    std::vector<VkDirLightData> dirLightData;

    std::vector<LightCascade> cascades;

    u32 startIndex;

    for (DirLightRenderInfo dirInfo : info.dirLights)
    {
        f32 currentNear = info.cameraNear;

        Transform3D dirTransform = dirInfo.transform;
        glm::mat4 dirView = GetViewMatrix(&dirTransform);

        for (int i = 0; i < NUM_CASCADES; i++)
        {
            glm::mat4 subProj = glm::perspective(glm::radians(info.cameraFov), aspect,
                                                 currentNear, currentNear + subFrustumSize);
            currentNear += subFrustumSize;

            f32 minX = std::numeric_limits<f32>::max();
            f32 maxX = std::numeric_limits<f32>::lowest();
            f32 minY = std::numeric_limits<f32>::max();
            f32 maxY = std::numeric_limits<f32>::lowest();
            f32 minZ = std::numeric_limits<f32>::max();
            f32 maxZ = std::numeric_limits<f32>::lowest();

            std::vector<glm::vec4> corners = GetFrustumCorners(subProj, view);

            for (const glm::vec3& v : corners)
            {
                const glm::vec4 trf = dirView * glm::vec4(v, 1.0);
                minX = std::min(minX, trf.x);
                maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y);
                maxY = std::max(maxY, trf.y);
                minZ = std::min(minZ, trf.z);
                maxZ = std::max(maxZ, trf.z);
            }

            glm::mat4 dirProj = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

            dirViews[i] = {dirView, dirProj, {}};

            cascades.push_back({dirProj * dirView, currentNear});
        }

        glm::vec3 lightDir = GetForwardVector(&dirTransform);

        LightEntry lightEntry = lights[dirInfo.lightID];

        BeginCascadedPass(lightEntry.shadowMap, CullMode::BACK);

        SetCamera(lightEntry.cameraIndex);
        UpdateCamera(NUM_CASCADES, dirViews);

        startIndex = 0;
        for (std::pair<MeshID, u32> pair: meshCounts)
        {
            SetMesh(pair.first);
            DrawObjects(pair.second, startIndex);
            startIndex += pair.second;
        }
        EndPass();

        dirLightData.push_back({GetForwardVector(&dirTransform),
                                lightEntry.shadowMap.descriptorIndex,
                                dirInfo.diffuse, dirInfo.specular});
    }


    std::vector<VkSpotLightData> spotLightData;

    for (SpotLightRenderInfo spotInfo : info.spotLights)
    {
        Transform3D spotTransform = spotInfo.transform;
        glm::mat4 spotView = GetViewMatrix(&spotTransform);
        glm::mat4 spotProj = glm::perspective(glm::radians(spotInfo.outerCone * 2), 1.0f, 0.01f, spotInfo.range);
        LightEntry lightEntry = lights[spotInfo.lightID];

        if (spotInfo.needsUpdate)
        {
            CameraData spotCamData = {spotView, spotProj, spotTransform.position};

            BeginShadowPass(lightEntry.shadowMap, CullMode::BACK);

            SetCamera(lightEntry.cameraIndex);
            UpdateCamera(1, &spotCamData);

            startIndex = 0;
            for (std::pair<MeshID, u32> pair: meshCounts)
            {
                SetMesh(pair.first);
                DrawObjects(pair.second, startIndex);
                startIndex += pair.second;
            }
            EndPass();
        }


        spotLightData.push_back({spotProj * spotView, spotTransform.position, GetForwardVector(&spotTransform),
                                 lightEntry.shadowMap.descriptorIndex, spotInfo.diffuse, spotInfo.specular,
                                 cosf(glm::radians(spotInfo.innerCone)), cosf(glm::radians(spotInfo.outerCone)),
                                 spotInfo.range});
    }

    std::vector<VkPointLightData> pointLightData;

    for (PointLightRenderInfo pointInfo : info.pointLights)
    {
        Transform3D pointTransform = pointInfo.transform;
        glm::vec3 pointPos = pointTransform.position;
        LightEntry lightEntry = lights[pointInfo.lightID];

        if (pointInfo.needsUpdate)
        {
            CameraData pointCamData[6];

            glm::mat4 pointProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.5f, pointInfo.maxRange);
            glm::mat4 pointViews[6];

            GetPointViews(&pointTransform, pointViews);

            for (int i = 0; i < 6; i++)
            {
                pointCamData[i] = {pointViews[i], pointProj, pointPos};
            }

            BeginCubemapShadowPass(lightEntry.shadowMap, CullMode::BACK);

            SetCamera(lightEntry.cameraIndex);
            UpdateCamera(6, pointCamData);

            SetCubemapInfo(pointPos, pointInfo.maxRange);

            startIndex = 0;
            for (std::pair<MeshID, u32> pair: meshCounts)
            {
                SetMesh(pair.first);
                DrawObjects(pair.second, startIndex);
                startIndex += pair.second;
            }
            EndPass();
        }


        pointLightData.push_back({pointPos, lightEntry.shadowMap.descriptorIndex,
                                  pointInfo.diffuse, pointInfo.specular,
                                  pointInfo.constant, pointInfo.linear, pointInfo.quadratic,
                                  pointInfo.maxRange});
    }

    BeginDepthPass(CullMode::BACK);

    SetCamera(mainCamIndex);
    CameraData mainCamData = {view, proj, cameraTransform.position};
    UpdateCamera(1, &mainCamData);

    startIndex = 0;
    for (std::pair<MeshID, u32> pair: meshCounts)
    {
        SetMesh(pair.first);
        DrawObjects(pair.second, startIndex);
        startIndex += pair.second;
    }
    EndPass();

    BeginColorPass(CullMode::BACK);

    SetLights({0.1f, 0.1f, 0.1f},
              dirLightData.size(), dirLightData.data(), cascades.data(),
              spotLightData.size(), spotLightData.data(),
              pointLightData.size(), pointLightData.data());

    startIndex = 0;
    for (std::pair<MeshID, u32> pair: meshCounts)
    {
        SetMesh(pair.first);
        DrawObjects(pair.second, startIndex);
        startIndex += pair.second;
    }
#if SKL_ENABLED_EDITOR
    DrawImGui();
#endif
    EndPass();
    EndFrame();
}
