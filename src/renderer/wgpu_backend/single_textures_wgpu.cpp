#include <single_textures_wgpu.h>

#include <array>

// >>> Private Helpers <<<

void WebGPUBackendCubemapTextureBuffer::ClearBuffers() {
    wgpuTextureViewRelease(m_textureCubemapView);
    wgpuTextureDestroy(m_textureData);
}

void WebGPUBackendCubemapTextureBuffer::UpdateBindGroups(const WGPUDevice& device) {
    for (WGPUBackendBindGroup* bindGroup : m_bindGroups) {
        bindGroup->UpdateBindGroup(device);
    }
}

// >>> Public Interface <<<

WebGPUBackendCubemapTextureBuffer::WebGPUBackendCubemapTextureBuffer() {
}
WebGPUBackendCubemapTextureBuffer::~WebGPUBackendCubemapTextureBuffer() {
    ClearBuffers();
}

bool WebGPUBackendCubemapTextureBuffer::GetInitiated() {
    return m_inited;
}

// Assumes u32 will correspond to WGPUTextureFormat_RGBA8Unorm
void WebGPUBackendCubemapTextureBuffer::Init(
    const WGPUDevice& device, 
    u32 texturesWidth, 
    u32 texturesHeight, 
    std::string&& label,
    std::string&& viewLabel){
    ASSERT(!m_inited);
    m_inited = true;
    m_width = texturesWidth;
    m_height = texturesHeight;

    m_label = std::move(label);
    m_viewLabel = std::move(viewLabel);

    // Initializes texture
    WGPUTextureDescriptor textureDesc {
        .nextInChain = nullptr,
        .label = WGPUBackendUtils::wgpuStr(m_label.data()),
        .usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
        .dimension = WGPUTextureDimension_2D,
        .size = {
            .width = texturesWidth,
            .height = texturesHeight,
            .depthOrArrayLayers = 6  
        },
        .format = WGPUTextureFormat_RGBA8Unorm,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 0,
        .viewFormats = nullptr
    };

    m_textureData = wgpuDeviceCreateTexture(device, &textureDesc);

    // Provides view of texture
    WGPUTextureViewDescriptor textureViewDesc {
        .nextInChain = nullptr,
        .label = WGPUBackendUtils::wgpuStr(m_viewLabel.data()),
        .format = WGPUTextureFormat_RGBA8Unorm,
        .dimension = WGPUTextureViewDimension_Cube,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 6,
        .aspect = WGPUTextureAspect_All,
        .usage = WGPUTextureUsage_TextureBinding
    };

   m_textureCubemapView = wgpuTextureCreateView(m_textureData, &textureViewDesc);

   // Updates entry
   m_currentBindGroupEntry = {
    .nextInChain = nullptr, 
    .offset = 0,
    .size = 0,
    .sampler = nullptr,
    .textureView = m_textureCubemapView
   };
   UpdateBindGroups(device);
}

void WebGPUBackendCubemapTextureBuffer::Reinit(
    const WGPUDevice& device, 
    u32 texturesWidth, 
    u32 texturesHeight, 
    std::string&& label,
    std::string&& viewLabel) {
    WebGPUBackendCubemapTextureBuffer::ClearBuffers();
    m_inited = false;
    Init(
        device,
        texturesWidth, 
        texturesHeight, 
        std::move(label),
        std::move(viewLabel));
}

void WebGPUBackendCubemapTextureBuffer::Insert(
    const WGPUDevice& device, 
    const WGPUQueue& queue,
    u32 texturesWidth, 
    u32 texturesHeight, 
    std::array<u32*,6> faceData) {
    ASSERT(m_inited);

    // Recreates texture at different size
    if (texturesWidth != m_width && texturesHeight != m_height) {
        Reinit(device, texturesWidth, texturesHeight, std::move(m_label), std::move(m_viewLabel));
    }

    // Copies relevant information into new texture
    size_t imageTexelAmount = texturesHeight * texturesWidth;

    WGPUTexelCopyTextureInfo copyInfo {
        .texture = m_textureData,
        .mipLevel = 0,
        .origin = {0, 0 ,0},
        .aspect = WGPUTextureAspect_All
    };

    WGPUTexelCopyBufferLayout bufferInfo{
        .offset = 0,
        .bytesPerRow = static_cast<u32>(sizeof(u32)) * texturesWidth,
        .rowsPerImage = texturesHeight
    };

    WGPUExtent3D extentInfo {
        .width = texturesWidth,
        .height = texturesHeight,
        .depthOrArrayLayers = 1
    };

    for (u32 dataIter = 0 ; dataIter < 6 ; dataIter++) {
        copyInfo.origin.z = dataIter;
        wgpuQueueWriteTexture(queue, &copyInfo, faceData[dataIter], imageTexelAmount * sizeof(u32), &bufferInfo, &extentInfo);
    }
}

WGPUTextureView WebGPUBackendCubemapTextureBuffer::getView() {
    return m_textureCubemapView;
}

// Used to update bind group on underlying texture change
WGPUBindGroupEntry WebGPUBackendCubemapTextureBuffer::GetEntry(u32 binding) {
    m_currentBindGroupEntry.binding = binding;
    return m_currentBindGroupEntry;
}
void WebGPUBackendCubemapTextureBuffer::RegisterBindGroup(WGPUBackendBindGroup* bindGroup) {
    m_bindGroups.push_back(bindGroup);
}


