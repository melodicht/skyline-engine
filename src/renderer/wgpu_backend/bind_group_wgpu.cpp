#include "renderer/wgpu_backend/bind_group_wgpu.h"

void WGPUBackendBindGroup::Init(const char* label, WGPUBindGroupLayout& bindLayout) {
    m_bindGroupLayout = bindLayout;
    m_inited = false;
    m_bindGroupLabel = WGPUBackendUtils::wgpuStr(label);
    wgpuBindGroupLayoutAddRef(m_bindGroupLayout);
}

WGPUBackendBindGroup::~WGPUBackendBindGroup() {
    if (m_bindGroupLayout != nullptr) {
        wgpuBindGroupLayoutRelease(m_bindGroupLayout);
    }
    if (m_bindGroupDat != nullptr) {
        wgpuBindGroupRelease(m_bindGroupDat);
    }
}

void WGPUBackendBindGroup::UpdateBindGroup(const WGPUDevice& device) {
    if (m_inited) {
        wgpuBindGroupRelease(m_bindGroupDat);
    }
    m_inited = true;

    std::vector<WGPUBindGroupEntry> bindGroups;
    for (u32 entryIter = 0 ; entryIter < m_bindGroupEntryDescriptors.size() ; entryIter++) {
        IWGPUBackendUniformEntry* entry = m_bindGroupEntryDescriptors[entryIter];
        u32 binding = m_bindGroupEntryBinding[entryIter];
        bindGroups.push_back(entry->GetEntry(binding));
    }

    WGPUBindGroupDescriptor bindGroupDesc {
        .nextInChain = nullptr,
        .label = m_bindGroupLabel,
        .layout = m_bindGroupLayout,
        .entryCount = bindGroups.size(),
        .entries = bindGroups.data()
    };
    m_bindGroupDat = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
}

void WGPUBackendBindGroup::BindToRenderPass(WGPURenderPassEncoder& renderPass) const {
    if(!m_inited) {
        // TODO: Make sure assert is only hit in debug
        assert(true);
        return;
    }

    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroupDat, 0, nullptr);
}

void WGPUBackendBindGroup::AddEntryToBindingGroup(IWGPUBackendUniformEntry* entry, u32 binding) {
    m_bindGroupEntryDescriptors.push_back(entry);
    m_bindGroupEntryBinding.push_back(binding);
    entry->RegisterBindGroup(this);
}
