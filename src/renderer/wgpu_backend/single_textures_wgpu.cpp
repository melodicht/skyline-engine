#pragma once

#include "single_textures_wgpu.h"

void WebGPUBackendCubemapTextureBuffer::ClearBuffers() {
    wgpuTextureViewRelease(m_wholeTextureDataView);
    wgpuTextureDestroy(m_textureBuffer);
}

WebGPUBackendCubemapTextureBuffer::WebGPUBackendCubemapTextureBuffer() {
}

void WebGPUBackendCubemapTextureBuffer::UpdateBindGroups() {

}

WebGPUBackendCubemapTextureBuffer::~WebGPUBackendCubemapTextureBuffer() {
    ClearBuffers();
}

WGPUTextureView WebGPUBackendCubemapTextureBuffer::getView() {

}

// Used to update bind group on underlying texture change
WGPUBindGroupEntry WebGPUBackendCubemapTextureBuffer::GetEntry() {
    return m_currentBindGroupEntry;
}
void WebGPUBackendCubemapTextureBuffer::RegisterBindGroup(WGPUBackendBindGroup& bindGroup) {
    bindGroup.
}


