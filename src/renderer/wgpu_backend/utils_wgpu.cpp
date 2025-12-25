#include "renderer/wgpu_backend/utils_wgpu.h"

#include <string>

WGPUStringView WGPUBackendUtils::wgpuStr(const char* string) {
  WGPUStringView retString {
    .data = string,
    .length = std::strlen(string)
  };
  return retString;
}