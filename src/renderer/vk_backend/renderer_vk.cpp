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

#define DEFAULT_SLANG false

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

#include "math/skl_math_types.h"
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
VkPipelineLayout shadowPipelineLayout;
VkPipelineLayout colorPipelineLayout;
VkPipeline shadowPipeline;
VkPipeline cascadedPipeline;
VkPipeline cubemapPipeline;
VkPipeline depthPipeline;
VkPipeline colorPipeline;

VkFormat shadowFormat = VK_FORMAT_D16_UNORM;

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

    VkBufferCopy indexCopy
    {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = indexSize
    };

    vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.indexBuffer.buffer, 1, &indexCopy);

    VkBufferCopy vertCopy
    {
        .srcOffset = indexSize,
        .dstOffset = 0,
        .size = vertSize
    };

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
    AllocatedImage depthTexture = CreateImage(allocator,
                             shadowFormat, 0,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
                             {width, height, 1}, 1,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthTexture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = shadowFormat,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{.imageView = depthTexView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet descriptorWrite
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = (u32)currentTexID,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    Texture texture
    {
        .texture = depthTexture,
        .imageView = depthTexView,
        .extent = {width, height},
        .descriptorIndex = (u32)currentTexID
    };

    currentTexID++;

    return texture;
}

Texture CreateDepthArray(u32 width, u32 height, u32 layers)
{
    AllocatedImage depthTexture = CreateImage(allocator,
                                              shadowFormat, 0,
                                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                              {width, height, 1}, layers,
                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthTexture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
        .format = shadowFormat,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = layers
        }
    };

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{.imageView = depthTexView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet descriptorWrite
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = (u32)currentTexID,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    Texture texture
    {
        .texture = depthTexture,
        .imageView = depthTexView,
        .extent = {width, height},
        .descriptorIndex = (u32)currentTexID
    };

    currentTexID++;

    return texture;
}

Texture CreateDepthCubemap(u32 width, u32 height)
{
    AllocatedImage depthTexture = CreateImage(allocator,
                                              shadowFormat,
                                              VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                              VK_IMAGE_USAGE_SAMPLED_BIT,
                                              {width, height, 1}, 6,
                                              VMA_MEMORY_USAGE_GPU_ONLY,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageView depthTexView;

    VkImageViewCreateInfo depthViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthTexture.image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = shadowFormat,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 6
        }
    };

    VK_CHECK(vkCreateImageView(device, &depthViewInfo, nullptr, &depthTexView));

    VkDescriptorImageInfo imageInfo{.imageView = depthTexView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet descriptorWrite
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = (u32)currentTexID,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo
    };

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    Texture texture
    {
        .texture = depthTexture,
        .imageView = depthTexView,
        .extent = {width, height},
        .descriptorIndex = (u32)currentTexID
    };

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
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkImageView texView;

    VkImageViewCreateInfo texViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = texImage.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VK_CHECK(vkCreateImageView(device, &texViewInfo, nullptr, &texView));

    VkDescriptorImageInfo imageInfo{.imageView = texView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet descriptorWrite
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texDescriptorSet,
        .dstBinding = 0,
        .dstArrayElement = (u32)currentTexID,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &imageInfo
    };

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
    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier
    };

    vkCmdPipelineBarrier2(commandBuffer, &depInfo);

    VkBufferImageCopy copyRegion
    {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource
        {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .imageExtent{info.width, info.height, 1}
    };

    vkCmdCopyBufferToImage(commandBuffer, uploadBuffer.buffer, texImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    imageBarrier = ImageBarrier(texImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkCmdPipelineBarrier2(commandBuffer, &depInfo);
    EndImmediateCommands(device, graphicsQueue, mainCommandPool, commandBuffer);

    DestroyBuffer(allocator, uploadBuffer);
    auto iter = textures.emplace(currentTexID, Texture());
    Texture& texture = iter.first->second;

    texture =
    {
        .texture = texImage,
        .imageView = texView,
        .extent = {info.width, info.height},
        .descriptorIndex = (u32)currentTexID
    };

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
    light.shadowMap = CreateDepthArray(4096, 4096, NUM_CASCADES);

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

    VkExtent3D depthImageExtent
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

    VkImageViewCreateInfo depthViewInfo
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange
        {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

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

    VkPhysicalDeviceFeatures feat10{.depthClamp = true, .shaderInt64 = true};

    VkPhysicalDeviceVulkan11Features feat11{.multiview = true, .shaderDrawParameters = true};

    VkPhysicalDeviceVulkan12Features feat12
    {
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingSampledImageUpdateAfterBind = true,
        .descriptorBindingPartiallyBound = true,
        .runtimeDescriptorArray = true,
        .scalarBlockLayout = true,
        .bufferDeviceAddress = true
    };

    VkPhysicalDeviceVulkan13Features feat13{.synchronization2 = true, .dynamicRendering = true};

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
    VmaVulkanFunctions vmaFuncs
    {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr
    };

    VmaAllocatorCreateInfo allocInfo
    {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = physDevice,
        .device = device,
        .pVulkanFunctions = &vmaFuncs,
        .instance = instance
    };

    VK_CHECK(vmaCreateAllocator(&allocInfo, &allocator));

    // Create the swapchain and associated resources at the default dimensions
    CreateSwapchain(info.startWidth, info.startHeight, nullptr);

    // Create the command pools and command buffers
    VkCommandPoolCreateInfo commandPoolInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily
    };

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &mainCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    for (int i = 0; i < NUM_FRAMES; i++)
    {
        VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &frames[i].commandPool));

        cmdAllocInfo.commandPool = frames[i].commandPool;
        VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &frames[i].commandBuffer));
    }

    // Create the sync structures
    VkFenceCreateInfo fenceInfo
    {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    VkSemaphoreCreateInfo semaphoreInfo
    {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .flags = 0
    };

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
    VkShaderModuleCreateInfo shaderInfo{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
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
    VkPipelineShaderStageCreateInfo stageInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = shaderStage,
        .module = shaderModule,
        .pName = entryPointName
    };
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

    VkShaderModule shadowFragShader = CreateShaderModuleFromFile("shaders/shadow.spv");
#else
    VkShaderModule depthShader = CreateShaderModuleFromFile("shaders/depth.vert.spv");
    
    VkShaderModule shadowVertShader = CreateShaderModuleFromFile("shaders/shadow.vert.spv");
    VkShaderModule shadowFragShader = CreateShaderModuleFromFile("shaders/shadow.frag.spv");
    
    VkShaderModule colorVertShader = CreateShaderModuleFromFile("shaders/color.vert.spv");
    VkShaderModule colorFragShader = CreateShaderModuleFromFile("shaders/color.frag.spv");

    VkShaderModule dirShadowFragShader = CreateShaderModuleFromFile("shaders/dirshadow.frag.spv");
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
    VkPipelineShaderStageCreateInfo shadowVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, shadowVertShader, vertEntryPointName);
    VkPipelineShaderStageCreateInfo colorFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, colorFragShader, fragEntryPointName);
    VkPipelineShaderStageCreateInfo shadowFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, shadowFragShader, fragEntryPointName);
    VkPipelineShaderStageCreateInfo dirShadowFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, dirShadowFragShader, fragEntryPointName);
    
    VkPipelineShaderStageCreateInfo colorShaderStages[] = {colorVertStageInfo, colorFragStageInfo};
    VkPipelineShaderStageCreateInfo shadowShaderStages[] = {shadowVertStageInfo, shadowFragStageInfo};
    VkPipelineShaderStageCreateInfo dirShadowShaderStages[] = {depthVertStageInfo, dirShadowFragStageInfo};

    // Set up descriptor pool and set for textures
    VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 512}, {VK_DESCRIPTOR_TYPE_SAMPLER, 2}};

    VkDescriptorPoolCreateInfo descriptorPoolInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
            | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };

    VK_CHECK(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

    VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorBindingFlags bindingFlags[3] = {bindlessFlags, 0, 0};
    VkDescriptorSetLayoutBinding texBinding
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 512,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutBinding shadowSamplerBinding
    {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutBinding textureSamplerBinding = shadowSamplerBinding;
    textureSamplerBinding.binding = 2;

    VkDescriptorSetLayoutBinding bindings[3] = {texBinding, shadowSamplerBinding, textureSamplerBinding};

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = 3,
        .pBindingFlags = bindingFlags
    };

    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &bindingFlagsInfo,
        .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
        .bindingCount = 3,
        .pBindings = bindings
    };

    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &texDescriptorLayout));

    VkDescriptorSetAllocateInfo descriptorAllocInfo
    {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &texDescriptorLayout
    };

    VK_CHECK(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &texDescriptorSet));

    VkSamplerCreateInfo shadowSamplerInfo
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .compareEnable = VK_TRUE,
        .compareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    };

    vkCreateSampler(device, &shadowSamplerInfo, nullptr, &shadowSampler);

    VkDescriptorImageInfo shadowSamplerDescInfo{.sampler = shadowSampler};

    VkWriteDescriptorSet shadowSamplerWrite
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = texDescriptorSet,
        .dstBinding = 1,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &shadowSamplerDescInfo
    };

    VkSamplerCreateInfo textureSamplerInfo
    {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .compareEnable = VK_FALSE
    };

    vkCreateSampler(device, &textureSamplerInfo, nullptr, &textureSampler);

    VkDescriptorImageInfo textureSamplerDescInfo{.sampler = textureSampler};

    VkWriteDescriptorSet textureSamplerWrite = shadowSamplerWrite;
    textureSamplerWrite.dstBinding = 2;
    textureSamplerWrite.pImageInfo = &textureSamplerDescInfo;

    VkWriteDescriptorSet samplerWrites[2] = {shadowSamplerWrite, textureSamplerWrite};

    vkUpdateDescriptorSets(device, 2, samplerWrites, 0, nullptr);

    // Create render pipeline layouts

    VkPushConstantRange pushConstants
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 3 * sizeof(VkDeviceAddress) + sizeof(FragPushConstants)
    };

    VkPushConstantRange shadowPushConstants
    {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 3 * sizeof(VkDeviceAddress) + sizeof(ShadowPushConstants)
    };


    VkPipelineLayoutCreateInfo depthLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants
    };

    VK_CHECK(vkCreatePipelineLayout(device, &depthLayoutInfo, nullptr, &depthPipelineLayout));

    VkPipelineLayoutCreateInfo shadowLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = nullptr,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &shadowPushConstants
    };

    VK_CHECK(vkCreatePipelineLayout(device, &shadowLayoutInfo, nullptr, &shadowPipelineLayout));

    VkPipelineLayoutCreateInfo colorLayoutInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &texDescriptorLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants
    };

    VK_CHECK(vkCreatePipelineLayout(device, &colorLayoutInfo, nullptr, &colorPipelineLayout));


    // Create render pipelines (AKA fill in 20000 info structs)
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

    // We are using vertex pulling so no need for vertex input description
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

    VkPipelineRasterizationStateCreateInfo rasterizer
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineRasterizationStateCreateInfo cubemapRasterizer = rasterizer;
    cubemapRasterizer.depthClampEnable = VK_TRUE;

    VkPipelineRasterizationStateCreateInfo shadowRasterizer = cubemapRasterizer;
    shadowRasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

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

    VkPipelineColorBlendAttachmentState colorBlendAttachment
    {
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT
            | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT
            | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    // For pre-depth pass
    VkPipelineDepthStencilStateCreateInfo preDepthStencil
    {
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    // For color pass
    VkPipelineDepthStencilStateCreateInfo colorDepthStencil
    {
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,

    };

    // For pre-depth pass
    VkPipelineRenderingCreateInfo depthRenderInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 0,
        .depthAttachmentFormat = depthFormat
    };

    VkPipelineRenderingCreateInfo shadowRenderInfo = depthRenderInfo;
    shadowRenderInfo.depthAttachmentFormat = shadowFormat;

    VkPipelineRenderingCreateInfo cascadedRenderInfo = shadowRenderInfo;
    cascadedRenderInfo.viewMask = (1 << NUM_CASCADES) - 1;

    VkPipelineRenderingCreateInfo cubemapRenderInfo = shadowRenderInfo;
    cubemapRenderInfo.viewMask = 0x3F;

    // For color pass
    VkPipelineRenderingCreateInfo colorRenderInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchainFormat,
        .depthAttachmentFormat = depthFormat
    };

    VkGraphicsPipelineCreateInfo depthPipelineInfo
    {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &depthRenderInfo,
        .stageCount = 1,
        .pStages = &depthVertStageInfo,
        .pVertexInputState = &vertexInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &preDepthStencil,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = depthPipelineLayout,
        .renderPass = nullptr, // No renderpass necessary because we are using dynamic rendering
        .subpass = 0
    };

    VkGraphicsPipelineCreateInfo shadowPipelineInfo = depthPipelineInfo;
    shadowPipelineInfo.stageCount = 2;
    shadowPipelineInfo.pStages = shadowShaderStages;
    shadowPipelineInfo.pRasterizationState = &shadowRasterizer;
    shadowPipelineInfo.layout = shadowPipelineLayout;
    shadowPipelineInfo.pNext = &shadowRenderInfo;

    VkGraphicsPipelineCreateInfo cascadedPipelineInfo = shadowPipelineInfo;
    cascadedPipelineInfo.pNext = &cascadedRenderInfo;
    cascadedPipelineInfo.pStages = dirShadowShaderStages;

    VkGraphicsPipelineCreateInfo cubemapPipelineInfo = shadowPipelineInfo;
    cubemapPipelineInfo.pRasterizationState = &cubemapRasterizer;
    cubemapPipelineInfo.layout = shadowPipelineLayout;
    cubemapPipelineInfo.pNext = &cubemapRenderInfo;

    VkGraphicsPipelineCreateInfo colorPipelineInfo = depthPipelineInfo;
    colorPipelineInfo.pNext = &colorRenderInfo;
    colorPipelineInfo.stageCount = 2;
    colorPipelineInfo.pStages = colorShaderStages;
    colorPipelineInfo.pDepthStencilState = &colorDepthStencil;
    colorPipelineInfo.layout = colorPipelineLayout;

    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &shadowPipelineInfo, nullptr, &shadowPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &cascadedPipelineInfo, nullptr, &cascadedPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &cubemapPipelineInfo, nullptr, &cubemapPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &depthPipelineInfo, nullptr, &depthPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &colorPipelineInfo, nullptr, &colorPipeline));

    vkDestroyShaderModule(device, depthShader, nullptr);
    vkDestroyShaderModule(device, dirShadowFragShader, nullptr);
#if DEFAULT_SLANG
    vkDestroyShaderModule(device, colorShader, nullptr);
    vkDestroyShaderModule(device, cubemapShader, nullptr);
#else
    vkDestroyShaderModule(device, colorVertShader, nullptr);
    vkDestroyShaderModule(device, colorFragShader, nullptr);
    vkDestroyShaderModule(device, shadowVertShader, nullptr);
    vkDestroyShaderModule(device, shadowFragShader, nullptr);
#endif

#if SKL_ENABLED_EDITOR
    // Initialize ImGui
    ImGui_ImplVulkan_InitInfo imGuiInfo
    {
        .ApiVersion = VK_API_VERSION_1_3,
        .Instance = instance,
        .PhysicalDevice = physDevice,
        .Device = device,
        .QueueFamily = graphicsQueueFamily,
        .Queue = graphicsQueue,
        .MinImageCount = 2,
        .ImageCount = (u32)swapImages.size(),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = colorRenderInfo
    };
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
    VkCommandBufferBeginInfo beginInfo
    {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return true;
}

void BeginDepthPass(VkImageView depthView, VkExtent2D extent, CullMode cullMode, u32 layerCount)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkRect2D scissor
    {
        .offset = {0, 0},
        .extent = {extent.width, extent.height}
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetCullMode(cmd, GetCullModeFlags(cullMode));

    VkRenderingAttachmentInfo depthAttachment
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue{.depthStencil{.depth = 1.0f}}
    };

    VkRenderingInfo renderInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea
        {
            .offset = {0, 0},
            .extent = extent
        },
        .layerCount = layerCount,
        .viewMask = (u32)(layerCount > 1 ? (1 << layerCount) - 1 : 0),
        .colorAttachmentCount = 0,
        .pColorAttachments = nullptr,
        .pDepthAttachment = &depthAttachment,
        .pStencilAttachment = nullptr
    };

    vkCmdBeginRendering(cmd, &renderInfo);
}

void BeginDepthPass(CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    //set dynamic viewport and scissor
    VkViewport viewport
    {
        .x = 0,
        .y = (f32)swapExtent.height,
        .width = (f32)swapExtent.width,
        .height = -(f32)swapExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

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

    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport
    {
        .x = 0,
        .y = 0,
        .width = (f32)swapExtent.width,
        .height = (f32)swapExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

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

    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport
    {
        .x = 0,
        .y = 0,
        .width = (f32)swapExtent.width,
        .height = (f32)swapExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

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

    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &imageBarrier
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);

    VkExtent2D extent = target.extent;

    //set dynamic viewport and scissor
    VkViewport viewport
    {
        .x = 0,
        .y = (f32)swapExtent.height,
        .width = (f32)swapExtent.width,
        .height = -(f32)swapExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipeline);
    currentLayout = &shadowPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
}

void BeginColorPass(CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    imageBarriers.push_back(ImageBarrier(swapImages[swapIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = (u32)imageBarriers.size(),
        .pImageMemoryBarriers = imageBarriers.data()
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);

    //set dynamic viewport and scissor
    VkViewport viewport
    {
        .x = 0,
        .y = (f32)swapExtent.height,
        .width = (f32)swapExtent.width,
        .height = -(f32)swapExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor
    {
        .offset = {0, 0},
        .extent = {swapExtent.width, swapExtent.height}
    };

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetCullMode(cmd, GetCullModeFlags(cullMode));
    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    vkCmdSetDepthBiasEnable(cmd, false);

    VkClearValue clearValue{.color = {{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderingAttachmentInfo colorAttachment
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapImageViews[swapIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValue
    };

    VkRenderingAttachmentInfo depthAttachment
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImageView,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_NONE,
    };

    VkRenderingInfo renderInfo
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea
        {
            .offset = {0, 0},
            .extent = swapExtent
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment,
        .pStencilAttachment = nullptr
    };

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

void SetShadowInfo(glm::vec3 lightPos, f32 farPlane)
{
    ShadowPushConstants pushConstants = {lightPos, farPlane};
    vkCmdPushConstants(frames[frameNum].commandBuffer, shadowPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       3 * sizeof(VkDeviceAddress), sizeof(ShadowPushConstants),
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
                       3 * sizeof(VkDeviceAddress), sizeof(FragPushConstants), &pushConstants);
}

// Send the matrices of the models to render (Must be called between InitFrame and EndFrame)
void SetObjectData(std::vector<ObjectData>& objects)
{
    AllocatedBuffer& objectBuffer = frames[frameNum].objectBuffer;
    void* objectData = objectBuffer.allocation->GetMappedData();
    memcpy(objectData, objects.data(), sizeof(ObjectData) * objects.size());
    // Send addresses to instance buffer as a push constant
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;
    vkCmdPushConstants(cmd, *currentLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &objectBuffer.address);
}

// Set the mesh currently being rendered (Must be called between InitFrame and EndFrame)
void SetMesh(MeshID meshIndex)
{
    Mesh* mesh = &meshes[meshIndex];

    // Send addresses to vertex buffer as a push constant
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;
    vkCmdPushConstants(cmd, *currentLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       2 * sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &mesh->vertBuffer.address);
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

    //Submit commands
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submit
    {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frames[frameNum].acquireSemaphore,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderSemaphores[swapIndex]
    };

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submit, frames[frameNum].renderFence));

    //Draw to screen
    VkPresentInfoKHR presentInfo
    {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderSemaphores[swapIndex],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &swapIndex
    };

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

    currentLayout = &depthPipelineLayout;
    SetObjectData(objects);

    Transform3D* cameraTransform = info.cameraTransform;
    glm::mat4 view = cameraTransform->GetViewMatrix();
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

        Transform3D* dirTransform = dirInfo.transform;
        glm::mat4 dirView = dirTransform->GetViewMatrix();

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

            f32 width = abs(maxX - minX);
            f32 height = abs(maxY - minY);
            f32 pixelWidth = width / 2048;
            f32 pixelHeight = height / 2048;
            f32 snappedMinX = minX - fmod(minX, pixelWidth);
            f32 snappedMaxX = maxX - fmod(maxX, pixelWidth);
            f32 snappedMinY = minY - fmod(minY, pixelHeight);
            f32 snappedMaxY = maxY - fmod(maxY, pixelHeight);

            glm::mat4 dirProj = glm::ortho(snappedMinX, snappedMaxX, snappedMinY, snappedMaxY, minZ, maxZ);

            dirViews[i] = {dirView, dirProj, {}};

            cascades.push_back({dirProj * dirView, currentNear});
        }

        glm::vec3 lightDir = dirTransform->GetForwardVector();

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

        dirLightData.push_back({dirTransform->GetForwardVector(),
                                lightEntry.shadowMap.descriptorIndex,
                                dirInfo.diffuse, dirInfo.specular});
    }


    std::vector<VkSpotLightData> spotLightData;

    for (SpotLightRenderInfo spotInfo : info.spotLights)
    {
        Transform3D* spotTransform = spotInfo.transform;
        glm::mat4 spotView = spotTransform->GetViewMatrix();
        glm::mat4 spotProj = glm::perspective(glm::radians(spotInfo.outerCone * 2), 1.0f, 0.01f, spotInfo.range);
        glm::vec3 spotPos = spotTransform->GetWorldTransform() * glm::vec4(0, 0, 0, 1);
        LightEntry lightEntry = lights[spotInfo.lightID];

        if (spotInfo.needsUpdate)
        {
            CameraData spotCamData = {spotView, spotProj, spotPos};

            BeginShadowPass(lightEntry.shadowMap, CullMode::BACK);

            SetCamera(lightEntry.cameraIndex);
            UpdateCamera(1, &spotCamData);

            SetShadowInfo(spotPos, spotInfo.range);

            startIndex = 0;
            for (std::pair<MeshID, u32> pair: meshCounts)
            {
                SetMesh(pair.first);
                DrawObjects(pair.second, startIndex);
                startIndex += pair.second;
            }
            EndPass();
        }


        spotLightData.push_back({spotProj * spotView, spotPos, spotTransform->GetForwardVector(),
                                 lightEntry.shadowMap.descriptorIndex, spotInfo.diffuse, spotInfo.specular,
                                 cosf(glm::radians(spotInfo.innerCone)), cosf(glm::radians(spotInfo.outerCone)),
                                 spotInfo.range});
    }

    std::vector<VkPointLightData> pointLightData;

    for (PointLightRenderInfo pointInfo : info.pointLights)
    {
        Transform3D* pointTransform = pointInfo.transform;
        glm::vec3 pointPos = pointTransform->GetWorldTransform() * glm::vec4(0, 0, 0, 1);
        LightEntry lightEntry = lights[pointInfo.lightID];

        if (pointInfo.needsUpdate)
        {
            CameraData pointCamData[6];

            glm::mat4 pointProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, pointInfo.maxRange);
            glm::mat4 pointViews[6];

            pointTransform->GetPointViews(pointViews);

            for (int i = 0; i < 6; i++)
            {
                pointCamData[i] = {pointViews[i], pointProj, pointPos};
            }

            BeginCubemapShadowPass(lightEntry.shadowMap, CullMode::BACK);

            SetCamera(lightEntry.cameraIndex);
            UpdateCamera(6, pointCamData);

            SetShadowInfo(pointPos, pointInfo.maxRange);

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
    CameraData mainCamData = {view, proj, cameraTransform->GetLocalPosition()};
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
