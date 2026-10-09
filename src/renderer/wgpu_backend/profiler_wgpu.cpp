#include "profiler_wgpu.h"

#if SKL_ENABLED_PROFILING

#include <algorithm>
#include "utils_wgpu.h"
#include "skl_debug.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    bool inTransit = m_syncData->m_filledFrameBufferInTransit;
    if (!inTransit && m_zoneIds.empty()) {
        // Empty frames have no timestamps to map (zero-size maps are invalid).
        std::lock_guard<std::mutex> lock(m_syncData->m_fillBufferMut);
        for (size_t i = 0; i < m_frameEndIndices.size(); ++i) {
            m_frameBuffer.push_back({std::vector<uint64_t>(m_zones.size(), 0), m_zones});
        }
    }
    else if (!inTransit) {
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
            .mode = WGPUCallbackMode_AllowSpontaneous,
            .callback = callback,
            .userdata1 = callbackData,
            .userdata2 = nullptr
        };
        m_syncData->m_filledFrameBufferInTransit = true;
        wgpuBufferMapAsync(
            m_cpuMappingBuffer,
            WGPUMapMode_Read,
            0,
            m_zoneIds.size() * sizeof(uint64_t),
            callbackInfo);
    }

    m_currentFramesUsed = 0;
    m_zoneIds.clear();
    m_frameEndIndices.clear();
    m_zoneBitMask.assign(m_zoneBitMask.size(), false);
}
void WebGpuProfiler::FillFrameBuffer(
    WGPUMapAsyncStatus status,
    WGPUStringView message,
    void* info,
    void* _) {
    std::unique_ptr<FillFrameBufferInfo> fillInfo(static_cast<FillFrameBufferInfo*>(info));

    // Checks for destruction or failure
    std::lock_guard<std::mutex> lock(fillInfo->m_syncData->m_fillBufferMut);
    if (fillInfo->m_syncData->m_profilerDestructed) {
        return;
    }
    
    // Checks for callback failure
    if (status != WGPUMapAsyncStatus_Success) {
        fillInfo->m_syncData->m_filledFrameBufferInTransit = false;
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
        std::vector<uint64_t> starts(zoneNames.size(), 0);
        std::vector<bool> active(zoneNames.size(), false);
        uint64_t iter = i == 0 ? 0 : frameEndIndices[i - 1];
        uint64_t end = frameEndIndices[i];
        while (iter < end) {
            uint32_t zone = vecIds[iter];
            if (!active[zone]) {
                starts[zone] = timestamps[iter];
            }
            else {
                frameZoneTimes[zone] += timestamps[iter] - starts[zone];
            }
            active[zone] = !active[zone];
            ++iter;
        }
        frameInfo.zoneTimes = std::move(frameZoneTimes);
        ret.push_back(frameInfo);
    }
    fillInfo->m_frameBuffer->insert(fillInfo->m_frameBuffer->end(), ret.begin(), ret.end());

    
    wgpuBufferUnmap(*fillInfo->m_mappedBuffer);

    fillInfo->m_syncData->m_filledFrameBufferInTransit = false;
}

WebGpuProfiler::WebGpuProfiler(WGPUDevice device, uint32_t maxZoneTypes, uint32_t frameBufferSize) :
    m_maxFrames(frameBufferSize),
    m_maxZoneTypes(maxZoneTypes)
    {
    uint64_t maxTotalZones = 2ull * maxZoneTypes * frameBufferSize;
    m_zones.reserve(maxZoneTypes);
    m_frameEndIndices.reserve(frameBufferSize);
    m_zoneIds.reserve(maxTotalZones);
    m_zoneBitMask = std::vector(maxZoneTypes, false);

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
        .count = static_cast<uint32_t>(maxTotalZones)
    };

    m_querySet = wgpuDeviceCreateQuerySet(device, &querySetDesc);

}
WebGpuProfiler::~WebGpuProfiler() {
    {
        std::lock_guard<std::mutex> lock(m_syncData->m_fillBufferMut);
        m_syncData->m_profilerDestructed = true;
    }

    if (m_cpuMappingBuffer) {
        wgpuBufferUnmap(m_cpuMappingBuffer);
        wgpuBufferDestroy(m_cpuMappingBuffer);
        wgpuBufferRelease(m_cpuMappingBuffer);
    }
    if (m_queryResolveBuffer) {
        wgpuBufferDestroy(m_queryResolveBuffer);
        wgpuBufferRelease(m_queryResolveBuffer);
    }
    if (m_querySet) {
        wgpuQuerySetDestroy(m_querySet);
        wgpuQuerySetRelease(m_querySet);
    }
}

// Use real passes: Metal does not reliably write timestamps for empty encoders.
WGPUPassTimestampWrites WebGpuProfiler::StartZone(const std::string zoneName) {
    uint32_t zoneId = RegisterZone(zoneName);
    ASSERT_PRINT(!m_zoneBitMask[zoneId], "A zone has been restarted without closing.");

    m_zoneBitMask[zoneId] = true;
    uint32_t queryIndex = static_cast<uint32_t>(m_zoneIds.size());
    m_zoneIds.push_back(zoneId);
    return {
        .querySet = m_querySet,
        .beginningOfPassWriteIndex = queryIndex,
        .endOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED
    };
}
WGPUPassTimestampWrites WebGpuProfiler::EndZone(const std::string zoneName) {
    uint32_t zoneId = RegisterZone(zoneName);
    ASSERT_PRINT(m_zoneBitMask[zoneId], "A zone has been closed without actually starting");

    m_zoneBitMask[zoneId] = false;
    uint32_t queryIndex = static_cast<uint32_t>(m_zoneIds.size());
    m_zoneIds.push_back(zoneId);
    return {
        .querySet = m_querySet,
        .beginningOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED,
        .endOfPassWriteIndex = queryIndex
    };
}

// Marks that no zones should still be active and that a frame has been finished 
void WebGpuProfiler::MarkFrameEnd(WGPUDevice device, WGPUQueue queue) {
    ASSERT_PRINT(std::find(m_zoneBitMask.begin(), m_zoneBitMask.end(), true) == m_zoneBitMask.end(),
        "Zones still in transit across frames.");

    m_zoneBitMask.assign(m_zoneBitMask.size(), false);
    m_totalFrameCounter += 1;
    m_currentFramesUsed += 1;
    m_frameEndIndices.push_back(m_zoneIds.size());
    if (m_currentFramesUsed == m_maxFrames) {
        // Call after submitting all encoders containing this frame's zones.
        AsyncFillFrameBuffer(device, queue);
    }
}

std::vector<RenderFramePerformanceInfo> WebGpuProfiler::FlushRecordedTimes() {
    std::lock_guard<std::mutex> lock(m_syncData->m_fillBufferMut);
    std::vector<RenderFramePerformanceInfo> ret(std::move(m_frameBuffer));
    m_frameBuffer = {};
    return ret;
}

#else
WebGpuProfiler::WebGpuProfiler(WGPUDevice device, uint32_t maxZoneTypes, uint32_t frameBufferSize) {}
WebGpuProfiler::~WebGpuProfiler() = default;

WGPUPassTimestampWrites WebGpuProfiler::StartZone(const std::string zoneName) {
    return { .beginningOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED,
             .endOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED };
}
WGPUPassTimestampWrites WebGpuProfiler::EndZone(const std::string zoneName) {
    return { .beginningOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED,
             .endOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED };
}

void WebGpuProfiler::MarkFrameEnd(WGPUDevice device, WGPUQueue queue) {}

std::vector<RenderFramePerformanceInfo> WebGpuProfiler::FlushRecordedTimes() { return {}; }
#endif

std::unique_ptr<WebGpuProfiler> CreateWebGpuProfiler(
    WGPUDevice device, uint32_t maxZoneTypes, uint32_t frameBufferSize) {
#if SKL_ENABLED_PROFILING
    if (!device || !wgpuDeviceHasFeature(device, WGPUFeatureName_TimestampQuery) ||
        frameBufferSize == 0 || maxZoneTypes == 0 || maxZoneTypes > 2048 / frameBufferSize) {
        return nullptr;
    }
    // WebGPU may return a non-null error object on creation failure.
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Internal);
    wgpuDevicePushErrorScope(device, WGPUErrorFilter_OutOfMemory);
    std::unique_ptr<WebGpuProfiler> profiler;
    try {
        profiler.reset(new WebGpuProfiler(device, maxZoneTypes, frameBufferSize));
    }
    catch (const std::bad_alloc&) {}

    struct InitializationResult {
        std::atomic<uint32_t> pending{3};
        std::atomic<bool> failed{false};
    } result;
    WGPUPopErrorScopeCallbackInfo callbackInfo {
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .callback = [](WGPUPopErrorScopeStatus status, WGPUErrorType type, WGPUStringView, void* info, void*) {
            auto* result = static_cast<InitializationResult*>(info);
            if (status != WGPUPopErrorScopeStatus_Success || type != WGPUErrorType_NoError) {
                result->failed = true;
            }
            --result->pending;
        },
        .userdata1 = &result
    };
    for (uint32_t i = 0; i < 3; ++i) {
        wgpuDevicePopErrorScope(device, callbackInfo);
    }
    while (result.pending) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(1);
#else
        std::this_thread::yield();
#endif
    }
    if (result.failed || !profiler || !profiler->m_queryResolveBuffer ||
        !profiler->m_cpuMappingBuffer || !profiler->m_querySet) {
        return nullptr;
    }
    return profiler;
#else
    return nullptr;
#endif
}
