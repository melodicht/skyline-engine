#include "profiler_wgpu.h"


// const std::vector<std::string> m_zones;
// const WGPUBuffer m_currentBuffer;
// const WGPUQuerySet m_currentQuerySet;
// const uint64_t m_maxFrames;

// std::vector<uint32_t> m_zoneIds;
// std::vector<uint64_t> m_frameEndIndices;

// uint64_t m_currentFramesUsed;
// uint64_t m_totalFrameCounter; 

// #if SKL_ENABLED_PROFILING

#include "utils_wgpu.h"
#include "skl_debug.h"

uint32_t WebGpuProfiler::RegisterZone(const std::string zone) {
    auto matchingZone = std::find(m_zones.begin(), m_zones.end(), zone);
    if (matchingZone == m_zones.end()) {
        ASSERT_PRINT(m_zones.size() < m_maxZoneTypes, "More zone types added to profiler than specified");
        m_zones.push_back(zone);
        return m_zones.size() - 1;
    }

    return std::distance(m_zones.begin(), matchingZone);
}
void WebGpuProfiler::AsyncFillFrameBuffer(WGPUDevice device, WGPUQueue queue) {
    // Ensures that frame buffers are simply discared if previous buffers are still being transferred
    // Should be vanishingly unlikely to happen but prevents race condition
    if (!m_syncData->m_filledFrameBufferInTransit) {
        WGPUCommandEncoderDescriptor flushCommandEncoderDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("WebGpuProfiler flushing operation command encoder")
        };
        WGPUCommandEncoder flushCommandEncoder = wgpuDeviceCreateCommandEncoder(device, &flushCommandEncoderDesc);

        WGPUCommandBufferDescriptor flushCommandBufferDesc {
            .nextInChain = nullptr,
            .label = WGPUBackendUtils::wgpuStr("WebGpuProfiler flushing operation command buffer")
        };

        wgpuCommandEncoderResolveQuerySet(
            flushCommandEncoder, 
            m_querySet,
            0,
            m_zoneIds.size(),
            m_queryResolveBuffer,
            0);

        wgpuCommandEncoderCopyBufferToBuffer(
            flushCommandEncoder,
            m_queryResolveBuffer,
            0,
            m_cpuMappingBuffer,
            0,
            m_zoneIds.size() * sizeof(uint64_t));

        WGPUCommandBuffer flushCommandBuffer = wgpuCommandEncoderFinish(flushCommandEncoder, &flushCommandBufferDesc);
        wgpuCommandEncoderRelease(flushCommandEncoder);
        wgpuQueueSubmit(queue, 1, &flushCommandBuffer);
        wgpuCommandBufferRelease(flushCommandBuffer);
        
        WGPUBufferMapCallback callback = FillFrameBuffer;

        FillFrameBufferInfo* callbackData = new FillFrameBufferInfo {
            .m_zoneIdsCpy = m_zoneIds,
            .m_frameEndIndicesCpy = m_frameEndIndices,
            .m_zones = m_zones,
            .m_syncData = m_syncData,
            .m_mappedBuffer = &m_cpuMappingBuffer,
            .m_frameBuffer = &m_frameBuffer
        };

        WGPUBufferMapCallbackInfo callbackInfo = {
            .nextInChain = nullptr,
            .callback = callback,
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .userdata1 = callbackData,
            .userdata2 = nullptr
        };
        wgpuBufferMapAsync(
            m_cpuMappingBuffer,
            WGPUMapMode_Read,
            0,
            m_zoneIds.size() * sizeof(uint64_t),
            callbackInfo);
    }

    m_currentFramesUsed = 0;
    m_zoneIds.clear();
    m_zoneBitMask.assign(m_zoneBitMask.size(), false);
}
void WebGpuProfiler::FillFrameBuffer(
    WGPUMapAsyncStatus status,
    WGPUStringView message,
    void* info,
    void* _) {
    FillFrameBufferInfo* fillInfo = static_cast<FillFrameBufferInfo*>(info);

    // Checks for destruction or failure
    std::lock_guard<std::mutex>(fillInfo->m_syncData->m_fillBufferMut);
    if (fillInfo->m_syncData->m_profilerDestructed) {
        delete fillInfo;
        return;
    }
    
    ASSERT_PRINT(!fillInfo->m_syncData->m_filledFrameBufferInTransit, "Fill logic set as not in transit despite being in transit");

    // Checks for callback failure
    if (status != WGPUMapAsyncStatus_Success) {
        fillInfo->m_syncData->m_filledFrameBufferInTransit = false;
        delete fillInfo;
        return;
    }

    WGPUBuffer buffer = static_cast<WGPUBuffer>(*fillInfo->m_mappedBuffer);
    std::vector<uint32_t> vecIds(std::move(fillInfo->m_zoneIdsCpy));
    std::vector<uint64_t> frameEndIndices(std::move(fillInfo->m_frameEndIndicesCpy));
    std::vector<std::string> zoneNames(std::move(fillInfo->m_zones));
    uint64_t frames = frameEndIndices.size(); 
    uint64_t queries = vecIds.size();

    const uint64_t* timestamps =
        static_cast<const uint64_t*>(
            wgpuBufferGetConstMappedRange(
                buffer,
                0,
                queries * sizeof(uint64_t)
            )
        );
    
    std::vector<RenderFramePerformanceInfo> ret;
    ret.reserve(frames);
    for (uint64_t i = 0 ; i < frames ; i++) {
        RenderFramePerformanceInfo frameInfo;
        frameInfo.zoneNames = zoneNames;

        std::vector<uint64_t> frameZoneTimes(zoneNames.size(), 0);
        uint64_t iter = i == 0 ? 0 : frameEndIndices[i - 1];
        uint64_t end = i == frameEndIndices[i];
        while (iter < end) {
            uint64_t& time = frameZoneTimes[vecIds[iter]];
            if (time == 0) {
                time = -timestamps[iter];
            }
            else {
                time += timestamps[iter];
            }
        }
        ret.push_back(frameInfo);
    }
    fillInfo->m_frameBuffer->insert(fillInfo->m_frameBuffer->end(), ret.begin(), ret.end());

    
    wgpuBufferUnmap(*fillInfo->m_mappedBuffer);

    fillInfo->m_syncData->m_filledFrameBufferInTransit = false;
    delete fillInfo;
}

WebGpuProfiler::WebGpuProfiler(WGPUDevice device, uint32_t maxZoneTypes, uint32_t frameBufferSize) :
    m_maxFrames(frameBufferSize),
    m_maxZoneTypes(maxZoneTypes)
    {
    
    uint64_t maxTotalZones = maxZoneTypes * frameBufferSize;
    WGPUBufferDescriptor bufferDesc = {
        .nextInChain = nullptr,
        .label = WGPUBackendUtils::wgpuStr("Profiler timestamp buffer"),
        .usage = WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
        .size = maxTotalZones * sizeof(uint64_t),
        .mappedAtCreation = false
    };

    m_queryResolveBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);
    bufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    m_cpuMappingBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    WGPUQuerySetDescriptor querySetDesc = {
        .nextInChain = nullptr,
        .label = WGPUBackendUtils::wgpuStr("Query set buffer"),
        .type = WGPUQueryType_Timestamp,
        .count = maxTotalZones
    };

    m_querySet = wgpuDeviceCreateQuerySet(device, &querySetDesc);

    m_zones.reserve(maxZoneTypes);
    m_frameEndIndices.reserve(frameBufferSize);
    m_zoneIds.reserve(maxTotalZones);

    m_zoneBitMask = std::vector(maxZoneTypes, false);
}
WebGpuProfiler::~WebGpuProfiler() {
    std::lock_guard<std::mutex> lock(m_syncData->m_fillBufferMut);
    m_syncData->m_profilerDestructed = true;

    wgpuBufferUnmap(m_cpuMappingBuffer);
    wgpuBufferDestroy(m_cpuMappingBuffer);
    wgpuBufferDestroy(m_queryResolveBuffer);
    wgpuQuerySetDestroy(m_querySet);    
}

// Allows for the starting and ending of zones in command encoders
void WebGpuProfiler::StartZone(WGPUCommandEncoder encoder, const std::string zoneName) {
    if (m_currentFramesUsed > m_maxFrames) {
        return;
    }
    
    uint32_t zoneId = RegisterZone(zoneName);

    if (m_zoneBitMask[zoneId]) {
        ASSERT_PRINT(false, "A zone has been restarted without closing.");
        return;
    }

    m_zoneBitMask[zoneId] = true;
    wgpuCommandEncoderWriteTimestamp(encoder, m_querySet, m_zoneIds.size());
    m_zoneIds.push_back(zoneId);
}
void WebGpuProfiler::EndZone(WGPUCommandEncoder encoder, const std::string zoneName) {
    if (m_currentFramesUsed > m_maxFrames) {
        return;
    }

    uint32_t zoneId = RegisterZone(zoneName);

    if (!m_zoneBitMask[zoneId]) {
        ASSERT_PRINT(false, "A zone has been closed without actually starting");
        return;
    }

    m_zoneBitMask[zoneId] = false;
    wgpuCommandEncoderWriteTimestamp(encoder, m_querySet, m_zoneIds.size());
    m_zoneIds.push_back(zoneId);
}

// Marks that no zones should still be active and that a frame has been finished 
void WebGpuProfiler::MarkFrameEnd(WGPUDevice device, WGPUQueue queue) {
    if (m_currentFramesUsed > m_maxFrames) {
        return;
    }

    ASSERT_PRINT(m_zoneBitMask == std::vector<bool>(m_maxZoneTypes, false), "Zones still in transit across frames.");

    m_zoneBitMask.assign(m_zoneBitMask.size(), false);
    m_totalFrameCounter += 1;
    m_currentFramesUsed += 1;
    m_frameEndIndices.push_back(m_zoneIds.size());
}

std::vector<RenderFramePerformanceInfo> WebGpuProfiler::FlushRecordedTimes() {
    std::lock_guard<std::mutex> lock(m_syncData->m_fillBufferMut);
    std::vector<RenderFramePerformanceInfo> ret(std::move(m_frameBuffer));
    m_frameBuffer = {};
    return ret;
}

// #else
// WebGpuProfiler::WebGpuProfiler(WGPUDevice device, std::vector<std::string>&& zoneNames, uint64_t size) {

// }

// // Allows for the starting and ending of zones on
// void WebGpuProfiler::StartZone(WGPUCommandEncoder encoder, const std::string zoneName) {}
// void WebGpuProfiler::EndZone(WGPUCommandEncoder encoder, const std::string zoneName) {}

// // Marks that no zones should still be active and that a frame has been finished 
// void WebGpuProfiler::MarkFrameEnd() {}

// std::vector<RenderFramePerformanceInfo> FlushRecordedTimes();
// #endif