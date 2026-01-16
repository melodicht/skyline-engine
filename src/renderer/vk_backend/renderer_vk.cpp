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


#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_vulkan.h>
#if defined(__APPLE__)
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <vma_no_warnings.h>
#include <iostream>

#include <imgui_impl_vulkan.h>

#include <asset_types.h>
#include <render_backend.h>
#include "vk_render_types.h"
#include "vk_render_utils.cpp"

#include <VkBootstrap.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>

#include <skl_math_types.h>
#include <skl_math_utils.h>
#include <meta_definitions.h>

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

AllocatedImage idImage;
VkImageView idImageView;

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
VkPipeline iconPipeline;

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

bool editor;
u32 cursorEntityIndex = UINT32_MAX;
AllocatedBuffer iconIndexBuffer;

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


    StagedCopyToBuffer(device, allocator, mainCommandPool, graphicsQueue, mesh.indexBuffer, indices, indexSize);
    StagedCopyToBuffer(device, allocator, mainCommandPool, graphicsQueue, mesh.vertBuffer, vertices, vertSize);

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
        frames[i].cameraBuffers.push_back(CreateShaderBuffer(device, allocator, sizeof(CameraData) * viewCount));
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
                                          VK_FORMAT_R8G8B8A8_SRGB, 0,
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
        .format = VK_FORMAT_R8G8B8A8_SRGB,
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

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(texImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_2_COPY_BIT);
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

    imageBarrier = ImageBarrier(texImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COPY_BIT, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
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

u32 GetIndexAtCursor()
{
    return cursorEntityIndex;
}

// Create swapchain or recreate to change size
void CreateSwapchain(u32 width, u32 height, VkSwapchainKHR oldSwapchain)
{
    // Create the swapchain
    vkb::SwapchainBuilder swapBuilder{physDevice, device, surface};

    swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;

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

    VkExtent3D imageExtent
    {
        width,
        height,
        1
    };

    depthImage = CreateImage(allocator,
                             depthFormat, 0,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                             imageExtent, 1,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

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

    if (editor)
    {
        idImage = CreateImage(allocator,
                              VK_FORMAT_R32_UINT, 0,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              imageExtent, 1,
                              VMA_MEMORY_USAGE_GPU_ONLY,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkImageViewCreateInfo idViewInfo
            {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = idImage.image,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R32_UINT,
                .subresourceRange
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    }
            };

        VK_CHECK(vkCreateImageView(device, &idViewInfo, nullptr, &idImageView));
    }
}

void DestroySwapResources()
{
    if (editor)
    {
        vkDestroyImageView(device, idImageView, nullptr);
        DestroyImage(allocator, idImage);
    }

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
    editor = info.editor;

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

    VkPhysicalDeviceFeatures feat10{.depthClamp = true};

    if (editor)
    {
        feat10.independentBlend = true;
    }

    VkPhysicalDeviceVulkan11Features feat11{.multiview = true};

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

    VkPhysicalDeviceVulkan13Features feat13{.shaderDemoteToHelperInvocation = true, .synchronization2 = true, .dynamicRendering = true};

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
    CreateSwapchain(info.startWidth, info.startHeight, VK_NULL_HANDLE);

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

    if (editor)
    {
        iconIndexBuffer = CreateBuffer(device, allocator, 6 * sizeof(u16),
                                       VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                       | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                       0,
                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        u16 indices[6] = {0, 2, 1, 1, 2, 3};
        StagedCopyToBuffer(device, allocator, mainCommandPool, graphicsQueue, iconIndexBuffer, indices, 6 * sizeof(u16));
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

VkPipelineShaderStageCreateInfo CreateStageInfo(VkShaderStageFlagBits shaderStage, VkShaderModule shaderModule)
{
    VkPipelineShaderStageCreateInfo stageInfo
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = shaderStage,
        .module = shaderModule,
        .pName = "main"
    };
    return stageInfo;
}

void InitPipelines(RenderPipelineInitInfo& info)
{
    // Create object and light buffers
    for (int i = 0; i < NUM_FRAMES; i++)
    {
        frames[i].objectBuffer = CreateShaderBuffer(device, allocator, sizeof(ObjectData) * 4096);
        frames[i].dirLightBuffer = CreateShaderBuffer(device, allocator, sizeof(VkDirLightData) * 4);
        frames[i].dirCascadeBuffer = CreateShaderBuffer(device, allocator, sizeof(LightCascade) * NUM_CASCADES * 4);
        frames[i].spotLightBuffer = CreateShaderBuffer(device, allocator, sizeof(VkSpotLightData) * 256);
        frames[i].pointLightBuffer = CreateShaderBuffer(device, allocator, sizeof(VkPointLightData) * 256);
        if (editor)
        {
            frames[i].idBuffer = CreateShaderBuffer(device, allocator, sizeof(u32) * 4096);
            frames[i].idTransferBuffer = CreateBuffer(device, allocator,
                                                      32, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                      VMA_ALLOCATION_CREATE_MAPPED_BIT
                                                      | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            frames[i].iconBuffer = CreateShaderBuffer(device, allocator, sizeof(IconData) * 1024);
        }
    }

    mainCamIndex = CreateCameraBuffer(1);

    // Create shader stages
    VkShaderModule depthShader = CreateShaderModuleFromFile("../shaderbin/depth.vert.spv");
    
    VkShaderModule shadowVertShader = CreateShaderModuleFromFile("../shaderbin/shadow.vert.spv");
    VkShaderModule shadowFragShader = CreateShaderModuleFromFile("../shaderbin/shadow.frag.spv");

    const char* colorFragPath = editor ? "../shaderbin/editor.frag.spv" : "../shaderbin/color.frag.spv";
    
    VkShaderModule colorVertShader = CreateShaderModuleFromFile("../shaderbin/color.vert.spv");
    VkShaderModule colorFragShader = CreateShaderModuleFromFile(colorFragPath);

    VkShaderModule dirShadowFragShader = CreateShaderModuleFromFile("../shaderbin/dirshadow.frag.spv");

    VkShaderModule iconVertShader = CreateShaderModuleFromFile("../shaderbin/icon.vert.spv");
    VkShaderModule iconFragShader = CreateShaderModuleFromFile("../shaderbin/icon.frag.spv");
    
    VkPipelineShaderStageCreateInfo colorVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, colorVertShader);
    VkPipelineShaderStageCreateInfo depthVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, depthShader);
    VkPipelineShaderStageCreateInfo shadowVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, shadowVertShader);
    VkPipelineShaderStageCreateInfo shadowFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, shadowFragShader);
    VkPipelineShaderStageCreateInfo colorFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, colorFragShader);
    VkPipelineShaderStageCreateInfo dirShadowFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, dirShadowFragShader);
    VkPipelineShaderStageCreateInfo iconVertStageInfo = CreateStageInfo(VK_SHADER_STAGE_VERTEX_BIT, iconVertShader);
    VkPipelineShaderStageCreateInfo iconFragStageInfo = CreateStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, iconFragShader);
    
    VkPipelineShaderStageCreateInfo colorShaderStages[] = {colorVertStageInfo, colorFragStageInfo};
    VkPipelineShaderStageCreateInfo shadowShaderStages[] = {shadowVertStageInfo, shadowFragStageInfo};
    VkPipelineShaderStageCreateInfo dirShadowShaderStages[] = {depthVertStageInfo, dirShadowFragStageInfo};
    VkPipelineShaderStageCreateInfo iconShaderStages[] = {iconVertStageInfo, iconFragStageInfo};

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

    if (editor)
    {
        pushConstants.size += sizeof(VkDeviceAddress);
    }

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
            | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending
    {
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    VkPipelineColorBlendStateCreateInfo depthBlending
    {
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 0,
        .pAttachments = nullptr,
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
        .maxDepthBounds = 1.0f
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

    VkPipelineColorBlendAttachmentState idBlendAttachment
    {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
    };

    VkPipelineColorBlendAttachmentState blendAttachments[2] = {colorBlendAttachment, idBlendAttachment};
    VkFormat colorFormats[2] = {swapchainFormat, VK_FORMAT_R32_UINT};

    if (editor)
    {
        colorBlending.attachmentCount = 2;
        colorBlending.pAttachments = blendAttachments;

        colorRenderInfo.colorAttachmentCount = 2;

        colorRenderInfo.pColorAttachmentFormats = colorFormats;
    }

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
        .pColorBlendState = &depthBlending,
        .pDynamicState = &dynamicState,
        .layout = depthPipelineLayout,
        .renderPass = VK_NULL_HANDLE, // No renderpass necessary because we are using dynamic rendering
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
    colorPipelineInfo.pColorBlendState = &colorBlending;
    colorPipelineInfo.layout = colorPipelineLayout;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &shadowPipelineInfo, nullptr, &shadowPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cascadedPipelineInfo, nullptr, &cascadedPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &cubemapPipelineInfo, nullptr, &cubemapPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &depthPipelineInfo, nullptr, &depthPipeline));
    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &colorPipelineInfo, nullptr, &colorPipeline));

    if (editor)
    {
        VkGraphicsPipelineCreateInfo iconPipelineInfo = colorPipelineInfo;
        iconPipelineInfo.pStages = iconShaderStages;
        iconPipelineInfo.pDepthStencilState = &preDepthStencil;
        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &iconPipelineInfo, nullptr, &iconPipeline));
    }

    vkDestroyShaderModule(device, depthShader, nullptr);
    vkDestroyShaderModule(device, dirShadowFragShader, nullptr);
#if DEFAULT_SLANG
    vkDestroyShaderModule(device, colorShader, nullptr);
    vkDestroyShaderModule(device, cubemapShader, nullptr);
#else
    vkDestroyShaderModule(device, colorVertShader, nullptr);
    vkDestroyShaderModule(device, colorFragShader, nullptr);
    vkDestroyShaderModule(device, depthShader, nullptr);
    vkDestroyShaderModule(device, dirShadowFragShader, nullptr);
    vkDestroyShaderModule(device, shadowVertShader, nullptr);
    vkDestroyShaderModule(device, shadowFragShader, nullptr);
    vkDestroyShaderModule(device, iconVertShader, nullptr);
    vkDestroyShaderModule(device, iconFragShader, nullptr);
#endif

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
        .ImageCount = 2,
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .Editor = editor,
        .DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE + 1,
        .UseDynamicRendering = true,
        .PipelineRenderingCreateInfo = colorRenderInfo
    };
    ImGui_ImplVulkan_Init(&imGuiInfo);

    ImGui_ImplVulkan_CreateFontsTexture();
}

// Set up frame and begin capturing draw calls
bool InitFrame()
{
    ImGui_ImplVulkan_NewFrame();

    imageBarriers.clear();

    //Set up commands
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    //Synchronize and get images
    VK_CHECK(vkWaitForFences(device, 1, &frames[frameNum].renderFence, true, 1000000000));

    VkResult acquireResult = vkAcquireNextImageKHR(device, swapchain, 1000000000, frames[frameNum].acquireSemaphore, VK_NULL_HANDLE, &swapIndex);
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

    vkCmdPushConstants(cmd, *currentLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &frames[frameNum].objectBuffer.address);

    if (editor)
    {
        void* idData = frames[frameNum].idTransferBuffer.allocation->GetMappedData();
        memcpy(&cursorEntityIndex, idData, sizeof(u32));
    }

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

    imageBarriers.push_back(ImageBarrier(depthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT));
}

void BeginShadowPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

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
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, 1);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline);
    currentLayout = &depthPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
}

void BeginCascadedPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

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
        .width = (f32)extent.width,
        .height = (f32)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, NUM_CASCADES);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cascadedPipeline);
    currentLayout = &depthPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
}

void BeginCubemapShadowPass(Texture target, CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);

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
        .y = (f32)extent.height,
        .width = (f32)extent.width,
        .height = -(f32)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    BeginDepthPass(target.imageView, extent, cullMode, 6);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubemapPipeline);
    currentLayout = &shadowPipelineLayout;

    imageBarriers.push_back(ImageBarrier(target.texture.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT));
}

void BeginColorPass(CullMode cullMode)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    imageBarriers.push_back(ImageBarrier(swapImages[swapIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT));

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
    VkClearValue idClearValue{.color = {.uint32 = {UINT32_MAX, 0, 0, 0}}};

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
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
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

    VkRenderingAttachmentInfo idAttachment
    {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = idImageView,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = idClearValue
    };

    VkRenderingAttachmentInfo attachments[2] = {colorAttachment, idAttachment};

    if (editor)
    {
        renderInfo.colorAttachmentCount = 2;
        renderInfo.pColorAttachments = attachments;

        imageBarriers.push_back(ImageBarrier(idImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT));

        vkCmdPushConstants(cmd, *currentLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           3 * sizeof(VkDeviceAddress), sizeof(VkDeviceAddress), &frames[frameNum].idBuffer.address);
    }

    VkDependencyInfo depInfo
    {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = (u32)imageBarriers.size(),
        .pImageMemoryBarriers = imageBarriers.data()
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);

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

    u32 offset = editor ? 4 : 3;

    vkCmdPushConstants(cmd, colorPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       offset * sizeof(VkDeviceAddress), sizeof(FragPushConstants), &pushConstants);
}

// Send the matrices of the models to render (Must be called between InitFrame and EndFrame)
void SetObjectData(std::vector<ObjectData>& objects, std::vector<u32>& ids)
{
    AllocatedBuffer& objectBuffer = frames[frameNum].objectBuffer;
    void* objectData = objectBuffer.allocation->GetMappedData();
    memcpy(objectData, objects.data(), sizeof(ObjectData) * objects.size());

    if (editor)
    {
        AllocatedBuffer& idBuffer = frames[frameNum].idBuffer;
        void* idData = idBuffer.allocation->GetMappedData();
        memcpy(idData, ids.data(), sizeof(u32) * ids.size());
    }
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
void EndFrame(glm::ivec2 cursorPos)
{
    // End dynamic rendering and commands
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    VkImageMemoryBarrier2 imageBarrier = ImageBarrier(swapImages[swapIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    VkImageMemoryBarrier2 idBarrier = ImageBarrier(idImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_2_COPY_BIT);

    VkImageMemoryBarrier2 barriers[2] = {imageBarrier, idBarrier};

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    if (editor)
    {
        depInfo.imageMemoryBarrierCount = 2;
        depInfo.pImageMemoryBarriers = barriers;
    }

    vkCmdPipelineBarrier2(cmd, &depInfo);

    if (editor)
    {
        cursorPos = glm::clamp(cursorPos, {0, 0}, {swapExtent.width - 1, swapExtent.height - 1});
        VkBufferImageCopy idCopy
        {
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .imageSubresource
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .imageOffset = {cursorPos.x, cursorPos.y, 0},
            .imageExtent = {1, 1, 1}
        };

        vkCmdCopyImageToBuffer(cmd, idImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, frames[frameNum].idTransferBuffer.buffer, 1, &idCopy);
    }

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

void DrawIcons(std::vector<IconRenderInfo>& icons)
{
    VkCommandBuffer& cmd = frames[frameNum].commandBuffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, iconPipeline);
    vkCmdBindIndexBuffer(cmd, iconIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

    AllocatedBuffer& objectBuffer = frames[frameNum].iconBuffer;
    void* objectData = objectBuffer.allocation->GetMappedData();
    memcpy(objectData, icons.data(), sizeof(IconData) * icons.size());

    f32 aspect = (f32)swapExtent.height / swapExtent.width;
    glm::vec2 iconScale = {0.0625 * aspect, 0.0625};

    FrameData& frame = frames[frameNum];
    IconPushConstants pushConstants
    {
        .objectAddress = objectBuffer.address,
        .cameraAddress = frame.cameraBuffers[currentCamIndex].address,
        .iconScale = iconScale,
    };

    vkCmdPushConstants(cmd, colorPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(IconPushConstants), &pushConstants);

    vkCmdDrawIndexed(cmd, 6, icons.size(), 0, 0, 0);
}

void RenderUpdate(RenderFrameInfo& info)
{
    currentLayout = &depthPipelineLayout;

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
    std::vector<u32> ids(totalCount);

    // 4. Iterate through scene view once more and fill in the fixed size array.
    for (MeshRenderInfo meshInfo : info.meshes)
    {
        glm::mat4 model = meshInfo.matrix;
        MeshID mesh = meshInfo.mesh;
        TextureID tex = meshInfo.texture;
        glm::vec3 color = meshInfo.rgbColor;

        objects[offsets[mesh]] = {model, tex, sRGBToLinear(glm::vec4(color.r, color.g, color.b, 1.0f))};
        ids[offsets[mesh]++] = meshInfo.id;
    }

    SetObjectData(objects, ids);

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
                                sRGBToLinear(dirInfo.diffuse), sRGBToLinear(dirInfo.specular)});
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
                                 lightEntry.shadowMap.descriptorIndex, sRGBToLinear(spotInfo.diffuse), sRGBToLinear(spotInfo.specular),
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

            glm::mat4 pointProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, pointInfo.radius);
            glm::mat4 pointViews[6];

            pointTransform->GetPointViews(pointViews);

            for (int i = 0; i < 6; i++)
            {
                pointCamData[i] = {pointViews[i], pointProj, pointPos};
            }

            BeginCubemapShadowPass(lightEntry.shadowMap, CullMode::BACK);

            SetCamera(lightEntry.cameraIndex);
            UpdateCamera(6, pointCamData);

            SetShadowInfo(pointPos, pointInfo.radius);

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
                                  sRGBToLinear(pointInfo.diffuse), sRGBToLinear(pointInfo.specular),
                                  pointInfo.radius, pointInfo.falloff});
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

    SetLights(sRGBToLinear({0.1f, 0.1f, 0.1f}),
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

    if (editor)
    {
        DrawIcons(info.icons);
    }

    DrawImGui();
    EndPass();
    EndFrame(info.cursorPos);
}

void SetSkyboxTexture(RenderSetSkyboxInfo& info)
{

}