#pragma once

#include <webgpu/webgpu.h>

class WGPUBackendUtils {
public:
    static WGPUStringView wgpuStr(const char* string);
};