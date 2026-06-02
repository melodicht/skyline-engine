#include <bind_group_wgpu.h>

WGPUBindGroupEntry WGPUBackendBindGroup::WGPUBackendBindGroupEntry::GetEntry(u32 binding) const {
    WGPUBindGroupEntry entry = m_currentBindGroupEntry;
    entry.binding = binding;
    return entry;
}

void WGPUBackendBindGroup::WGPUBackendBindGroupEntry::RegisterBindGroup(WGPUBackendBindGroup* bindGroup, u32 binding) {
    bindGroup->AddEntryToBindingGroup(this, binding);
}

void WGPUBackendBindGroup::DirtyMarkingBindGroupEntry::DirtyBindingGroups() {
    for (int i = 0 ; i < m_registeredBindGroups.size() ; i++) {
        m_registeredBindGroups[i]->DirtyBindGroup();
    }
}
//virtual void RegisterBindGroup(WGPUBackendBindGroup* bindGroup, u32 binding) override;
void WGPUBackendBindGroup::DirtyMarkingBindGroupEntry::RegisterBindGroup(WGPUBackendBindGroup* bindGroup, u32 binding) {
    WGPUBackendBindGroup::WGPUBackendBindGroupEntry::RegisterBindGroup(bindGroup, binding);
    m_registeredBindGroups.push_back(bindGroup);
}

void WGPUBackendBindGroup::Init(const char* label, const WGPUBindGroupLayout& bindLayout, u32 dynamicStride) {
    m_bindGroupLayout = bindLayout;
    m_dirty = true;
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

void WGPUBackendBindGroup::DirtyBindGroup() {
    m_dirty = true;
}

std::vector<WGPUBindGroupEntry> WGPUBackendBindGroup::GetEntries() const {
    std::vector<WGPUBindGroupEntry> bindGroupEntryDescriptors;
    for (u32 entryIter = 0 ; entryIter < m_bindGroupEntries.size() ; entryIter++) {
        WGPUBackendBindedEntry entry = m_bindGroupEntries[entryIter];
        bindGroupEntryDescriptors.push_back(entry.m_entry->GetEntry(entry.m_entryBind));
    }
    return bindGroupEntryDescriptors;
}

void WGPUBackendBindGroup::UpdateBindGroup(const WGPUDevice& device) {
    if (m_dirty) {
        if (m_bindGroupDat != nullptr) {
            wgpuBindGroupRelease(m_bindGroupDat);
        }
        m_dirty = false;

        std::vector<WGPUBindGroupEntry> bindGroupEntryDescriptors = GetEntries();

        WGPUBindGroupDescriptor bindGroupDesc {
            .nextInChain = nullptr,
            .label = m_bindGroupLabel,
            .layout = m_bindGroupLayout,
            .entryCount = bindGroupEntryDescriptors.size(),
            .entries = bindGroupEntryDescriptors.data()
        };
        m_bindGroupDat = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);
    }
}

void WGPUBackendBindGroup::BindToRenderPass(u32 groupIdx, const WGPURenderPassEncoder& renderPass) const {
    ASSERT(!m_dirty);

    wgpuRenderPassEncoderSetBindGroup(renderPass, groupIdx, m_bindGroupDat, 0, nullptr);
}

void WGPUBackendBindGroup::AddEntryToBindingGroup(const WGPUBackendBindGroupEntry* entry, u32 binding) {
    m_bindGroupEntries.push_back({entry, binding});
}

WGPUBindGroupEntry WGPUBackendDynamicUniformEntry::GetDynamicUniformEntry(u32 binding) const {
    WGPUBindGroupEntry entry = m_currentDynamicUniformEntry;
    entry.binding = binding;
    return entry;
}
