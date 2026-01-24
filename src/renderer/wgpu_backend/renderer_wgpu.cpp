#include <renderer_wgpu.h>
#include <utils_wgpu.h>
#include <sdl3webgpu.h>
#include <dynamic_light_converter.h>

#include <meta_definitions.h>
#include <skl_math_utils.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <backends/imgui_impl_wgpu.h>

// Much of this was taken from https://eliemichel.github.io/LearnWebGPU

// TODO: Make such constants more configurable
constexpr u32 DefaultCascadeCount = 4;
constexpr u32 DefaultDirLightDim = 2048;
constexpr u32 DefaultPointLightDim = 512;
constexpr u32 DefaultSkyboxDim = 2048;


#pragma region Helper Functions
void WGPURenderBackend::printDeviceSpecs() {
  WGPUSupportedFeatures features;
  wgpuDeviceGetFeatures(m_wgpuCore.m_device, &features);

  LOG("Device features:");
  for (s32 iter = 0; iter < features.featureCount ; iter++) {
      LOG(" - 0x" << features.features[iter]);
  }

  WGPULimits limits = {};
  limits.nextInChain = nullptr;

  WGPUStatus success = wgpuDeviceGetLimits(m_wgpuCore.m_device, &limits);

  if (success == WGPUStatus_Success) {
      LOG("Device limits:");
      LOG(" - maxBindGroups: " << limits.maxBindGroups);
      LOG(" - maxBindGroupsPlusVertexBuffers: " << limits.maxBindGroupsPlusVertexBuffers);
      LOG(" - maxBindingsPerBindGroup: " << limits.maxBindingsPerBindGroup);
      LOG(" - maxBufferSize: " << limits.maxBufferSize);
      LOG(" - maxColorAttachmentBytesPerSample: " << limits.maxColorAttachmentBytesPerSample);
      LOG(" - maxColorAttachments: " << limits.maxColorAttachments);
      LOG(" - maxComputeInvocationsPerWorkgroup: " << limits.maxComputeInvocationsPerWorkgroup);
      LOG(" - maxComputeWorkgroupSizeX: " << limits.maxComputeWorkgroupSizeX);
      LOG(" - maxComputeWorkgroupSizeY: " << limits.maxComputeWorkgroupSizeY);
      LOG(" - maxComputeWorkgroupSizeZ: " << limits.maxComputeWorkgroupSizeZ);
      LOG(" - maxTextureDimension1D: " << limits.maxTextureDimension1D);
      LOG(" - maxTextureDimension2D: " << limits.maxTextureDimension2D);
      LOG(" - maxTextureDimension3D: " << limits.maxTextureDimension3D);
      LOG(" - maxTextureArrayLayers: " << limits.maxTextureArrayLayers);
      // [...] Extra device limits
  }
}

WGPUBindGroupLayoutEntry DefaultBindLayoutEntry() {
  return WGPUBindGroupLayoutEntry {
    .nextInChain = nullptr,
    .binding = 0,
    .visibility = WGPUShaderStage_None,
    .buffer {
      .nextInChain = nullptr,
      .type = WGPUBufferBindingType_BindingNotUsed,
      .hasDynamicOffset = false,
      .minBindingSize = 0,
    },
    .sampler {
      .nextInChain = nullptr,
      .type = WGPUSamplerBindingType_BindingNotUsed,
    },
    .texture {
      .nextInChain = nullptr,
      .sampleType = WGPUTextureSampleType_BindingNotUsed,
      .viewDimension =  WGPUTextureViewDimension_Undefined,
      .multisampled = false,
    },
    .storageTexture {
      .nextInChain = nullptr,
      .access = WGPUStorageTextureAccess_BindingNotUsed,
      .format = WGPUTextureFormat_Undefined,
      .viewDimension = WGPUTextureViewDimension_Undefined,
    },
  };
}

WGPUAdapter WGPURenderBackend::GetAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
  WGPUAdapter set = nullptr;
  bool requestEnded = false;

  WGPURequestAdapterCallbackInfo callbackInfo;
  callbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapterImpl* adapter, WGPUStringView message, void *userdata1, void *userdata2) 
  {
      if (status == WGPURequestAdapterStatus_Success) {
        *((WGPUAdapter *)userdata1) = adapter;
      } else {
          // LOG("Could not get WebGPU adapter: " << (message.data));
          LOG("Could not get WebGPU adapter: " << (message.data));
      }
      *(bool *)userdata2 = true;
  };

  callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  callbackInfo.userdata1 = &set;
  callbackInfo.userdata2 = &requestEnded;

  // Call to the WebGPU request adapter procedure
  wgpuInstanceRequestAdapter(
      instance /* equivalent of navigator.gpu */,
      options,
      callbackInfo
  );
  // We wait until userData.requestEnded gets true
  #if EMSCRIPTEN
  while (!requestEnded) {
      emscripten_sleep(100);
  }
  #endif

  ASSERT(requestEnded);

  return set;
}

WGPUDevice WGPURenderBackend::GetDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor) {
  WGPUDevice set = nullptr;
  bool requestEnded = false;

  WGPURequestDeviceCallbackInfo callbackInfo;
  callbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void * userdata1, void * userdata2) {
      if (status == WGPURequestDeviceStatus_Success) {
        *((WGPUDevice *)userdata1) = device;
      } else {
          LOG("Could not get WebGPU device: " << message.data);
      }
      *((bool *)userdata2) = true;
  };
  callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
  callbackInfo.userdata1 = &set;
  callbackInfo.userdata2 = &requestEnded;

  wgpuAdapterRequestDevice(
      adapter,
      descriptor,
      callbackInfo
  );

#ifdef __EMSCRIPTEN__
  while (!requestEnded) {
      emscripten_sleep(100);
  }
#endif // __EMSCRIPTEN__

  ASSERT(requestEnded);

  return set;
}

void WGPURenderBackend::QueueFinishCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Queued work finished with status: " << status);
  LOG("Included Message: " << message.data);
}

void WGPURenderBackend::LostDeviceCallback(WGPUDevice const * device, WGPUDeviceLostReason reason, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Device lost: reason " << reason);
  if (message.data) LOG(" (" << message.data << ")");
}

void WGPURenderBackend::ErrorCallback(WGPUDevice const * device, WGPUErrorType type, WGPUStringView message, WGPU_NULLABLE void* userdata1, WGPU_NULLABLE void* userdata2) {
  LOG("Error happened: error " << type);
  if (message.data) LOG("(" << message.data << ")");
}

bool WGPURenderBackend::InitFrame() {
  ImGui_ImplWGPU_NewFrame();

  // Gets current color texture
  wgpuSurfaceGetCurrentTexture(m_wgpuSurface, &m_surfaceTexture);

  if (m_surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
      return false;
  }

  WGPUTextureViewDescriptor viewDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Initializing texture view"),
    .format = wgpuTextureGetFormat(m_surfaceTexture.texture),
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_All,
    .usage = WGPUTextureUsage_RenderAttachment
  };

  m_surfaceTextureView = wgpuTextureCreateView(m_surfaceTexture.texture, &viewDescriptor);

  if(!m_surfaceTextureView)
  {
    return false;
  }

  return true;
}

void WGPURenderBackend::EndFrame() {
  if (m_surfaceTextureView) {
    wgpuTextureViewRelease(m_surfaceTextureView);
  }

  #ifndef __EMSCRIPTEN__
  wgpuSurfacePresent(m_wgpuSurface);
  wgpuInstanceProcessEvents(m_wgpuCore.m_instance);  
  #else
    
  emscripten_sleep(10);
  #endif
}

void WGPURenderBackend::DrawObjects(std::map<MeshID, u32>& meshCounts) {
  u32 startIndex = 0;
  for (std::pair<MeshID, u32> pair : meshCounts)
  {
    WGPUBackendMeshIdx& gotMesh = m_meshStore[pair.first];
    wgpuRenderPassEncoderDrawIndexed(m_renderPassEncoder, gotMesh.m_indexCount, pair.second, gotMesh.m_baseIndex, gotMesh.m_baseVertex, startIndex);
    startIndex += pair.second;
  }
}

void WGPURenderBackend::BeginColorPass() {
  WGPURenderPassColorAttachment baseColorPassAttachment {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Clear,
    .storeOp = WGPUStoreOp_Store,
    .clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 }
  };

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = m_depthTexture.m_textureView,
    .depthClearValue = 1.0f,
    .depthReadOnly = true,
    .stencilReadOnly = true,
  };

  BeginPass(
    &baseColorPassAttachment,
    &depthStencilAttachment,
    "Color Pass Command Encoder",
    "Color Pass",
    m_colorPassBindGroup,
    m_defaultPipeline);
  SetupVandIBO();
}

void WGPURenderBackend::BeginSkyboxPass() {
  WGPURenderPassColorAttachment skyboxPassAttachment {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Load,
    .storeOp = WGPUStoreOp_Store,
    .clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 }
  };

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = m_depthTexture.m_textureView,
    .depthClearValue = 1.0f,
    .depthReadOnly = true,
    .stencilReadOnly = true,
  };

  BeginPass(
    &skyboxPassAttachment,
    &depthStencilAttachment,
    "Skybox Pass Command Encoder",
    "Skybox Pass",
    m_skyboxBindGroup,
    m_skyboxPipeline);
}

void WGPURenderBackend::BeginDepthPass(WGPUTextureView depthTexture) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Depth Pass Command Encoder",
    "Depth Pass",
    m_depthBindGroup,
    m_depthPipeline);
  SetupVandIBO();
}

void WGPURenderBackend::BeginDirectionalDepthPass(WGPUTextureView depthTexture) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };
  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Directional Depth Command Encoder",
    "Directional Depth Pass",
    m_depthBindGroup, // Intentionally same as depth bind group
    m_directionDepthPipeline);
  SetupVandIBO();
}

void WGPURenderBackend::BeginPointDepthPass(WGPUTextureView depthTexture) {
  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = depthTexture,
    .depthLoadOp = WGPULoadOp_Clear,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  BeginPass(
    nullptr,
    &depthStencilAttachment,
    "Point Depth Command Encoder",
    "Point Depth Pass",
    m_pointDepthBindGroup,
    m_pointDepthPipeline);
  SetupVandIBO();
}

void WGPURenderBackend::BeginPass(
  const WGPURenderPassColorAttachment* colorPassAttachment,
  const WGPURenderPassDepthStencilAttachment* depthStencilAttachment,
  std::string&& encoderLabel,
  std::string&& passLabel,
  const WGPUBackendBindGroup& bindGroup,
  const WGPURenderPipeline& pipeline) {
  // Ensures that no two passes should be active at the same time 
  ASSERT(!m_renderPassActive);

  m_renderPassActive = true;

  // Creates command encoder
  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr(encoderLabel.c_str())
  };
  m_passCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  // Sets up pass encoder
  WGPURenderPassDescriptor passDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr(passLabel.c_str()),
    .colorAttachmentCount = colorPassAttachment != nullptr,
    .colorAttachments = colorPassAttachment,
    .depthStencilAttachment = depthStencilAttachment,
    .timestampWrites = nullptr,
  };
  m_renderPassEncoder = wgpuCommandEncoderBeginRenderPass(m_passCommandEncoder, &passDescriptor);

  // Sets render pass as fitting to certain pipeline
  wgpuRenderPassEncoderSetPipeline(m_renderPassEncoder, pipeline);
  bindGroup.BindToRenderPass(m_renderPassEncoder);
}

void WGPURenderBackend::SetupVandIBO() {
  m_meshVertexBuffer.BindToRenderPassAsVertexBuffer(m_renderPassEncoder);
  m_meshIndexBuffer.BindToRenderPassAsIndexBuffer(m_renderPassEncoder);
}
void WGPURenderBackend::EndPass() {
  m_renderPassActive = false;

  wgpuRenderPassEncoderEnd(m_renderPassEncoder);
  wgpuRenderPassEncoderRelease(m_renderPassEncoder);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
    .nextInChain = nullptr,
    .label =  WGPUBackendUtils::wgpuStr("Ending pass command buffer"),
  };

  WGPUCommandBuffer passCommand = wgpuCommandEncoderFinish(m_passCommandEncoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(m_passCommandEncoder);

  wgpuQueueSubmit(m_wgpuQueue, 1, &passCommand);
  wgpuCommandBufferRelease(passCommand);
}

void WGPURenderBackend::DrawImGui() {
  WGPUCommandEncoderDescriptor encoderDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Imgui Encoder Descriptor")
  };
  WGPUCommandEncoder imguiCommandEncoder = wgpuDeviceCreateCommandEncoder(m_wgpuCore.m_device, &encoderDesc);

  WGPURenderPassColorAttachment meshColorPass {
    .view = m_surfaceTextureView,
    .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
    .resolveTarget = nullptr,
    .loadOp = WGPULoadOp_Load,
    .storeOp = WGPUStoreOp_Store,
  };

  WGPURenderPassDepthStencilAttachment depthStencilAttachment {
    .nextInChain = nullptr,
    .view = m_depthTexture.m_textureView,
    .depthLoadOp = WGPULoadOp_Load,
    .depthStoreOp = WGPUStoreOp_Store,
    .depthClearValue = 1.0f,
    .depthReadOnly = false,
    .stencilReadOnly = true,
  };

  WGPURenderPassDescriptor meshPassDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Imgui render pass"),
    .colorAttachmentCount = 1,
    .colorAttachments = &meshColorPass,
    .depthStencilAttachment = &depthStencilAttachment,
    .timestampWrites = nullptr,
  };

  WGPURenderPassEncoder imguiPassEncoder = wgpuCommandEncoderBeginRenderPass(imguiCommandEncoder, &meshPassDesc);

  ImGui::Render();
  ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), imguiPassEncoder);

  wgpuRenderPassEncoderEnd(imguiPassEncoder);
  wgpuRenderPassEncoderRelease(imguiPassEncoder);

  WGPUCommandBufferDescriptor cmdBufferDescriptor = {
    .nextInChain = nullptr,
    .label =  WGPUBackendUtils::wgpuStr("Imgui Command Buffer"),
  };

  WGPUCommandBuffer imguiCommand = wgpuCommandEncoderFinish(imguiCommandEncoder, &cmdBufferDescriptor);
  wgpuCommandEncoderRelease(imguiCommandEncoder);

  wgpuQueueSubmit(m_wgpuQueue, 1, &imguiCommand);
  wgpuCommandBufferRelease(imguiCommand);
}
#pragma endregion

#pragma region Interface Impl
WGPURenderBackend::~WGPURenderBackend() {
  wgpuSurfaceUnconfigure(m_wgpuSurface);
  wgpuSurfaceRelease(m_wgpuSurface);
  wgpuQueueRelease(m_wgpuQueue);
}

void WGPURenderBackend::InitRenderer(SDL_Window *window, u32 startWidth, u32 startHeight) {
  m_screenWidth = startWidth;
  m_screenHeight = startHeight;
  // Creates instance
  WGPUInstanceDescriptor instanceDescriptor { 
    .nextInChain = nullptr
  };

  #if EMSCRIPTEN
  m_wgpuCore.m_instance = wgpuCreateInstance(nullptr);
  #else
  m_wgpuCore.m_instance = wgpuCreateInstance(&instanceDescriptor);
  #endif
  if (m_wgpuCore.m_instance == nullptr) {
    LOG_ERROR("Instance creation failed!");
    return;
  }
  LOG("Instance Created" << m_wgpuCore.m_instance);

  LOG("Requesting adapter...");

  m_wgpuSurface = SDL_GetWGPUSurface(m_wgpuCore.m_instance, window);
  WGPURequestAdapterOptions adapterOpts = {
    .nextInChain = nullptr,
    .compatibleSurface = m_wgpuSurface
  };

  WGPUAdapter adapter = GetAdapter(m_wgpuCore.m_instance, &adapterOpts);

  LOG("Got adapter: " << adapter);

  LOG("Extracting capabilities...");

  m_hardwareDepthClampingSupported = wgpuAdapterHasFeature(adapter, WGPUFeatureName_DepthClipControl);
  LOG("DEPTH CLAMPING SUPPORTED: " << m_hardwareDepthClampingSupported);

  LOG("Requesting device...");

  // General device description
  WGPULimits deviceRequirements = WGPU_LIMITS_INIT;
  deviceRequirements.maxTextureArrayLayers = 2048;

  WGPUFeatureName depthClamping = WGPUFeatureName_DepthClipControl;

  WGPUDeviceDescriptor deviceDesc = {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("My Device"),
    .requiredFeatureCount = 1,
    .requiredFeatures = &depthClamping,
    .requiredLimits = &deviceRequirements,
    .defaultQueue {
      .nextInChain = nullptr,
      .label = WGPUBackendUtils::wgpuStr("Default Queue")
    },
    .deviceLostCallbackInfo {
      .mode = WGPUCallbackMode_AllowSpontaneous,
      .callback = LostDeviceCallback
    },
    .uncapturedErrorCallbackInfo {
      .nextInChain = nullptr,
      .callback = ErrorCallback
    }
  };

  m_wgpuCore.m_device = GetDevice(adapter, &deviceDesc);

  LOG("Got device: " << m_wgpuCore.m_device);

  printDeviceSpecs();

  m_wgpuQueue = wgpuDeviceGetQueue(m_wgpuCore.m_device);

  WGPUQueueWorkDoneCallbackInfo queueDoneCallback =  WGPUQueueWorkDoneCallbackInfo {
    .mode = WGPUCallbackMode_AllowProcessEvents,
    #if EMSCRIPTEN // TODO: It seems that emdawn has split off from native for now, check frequently
    .callback = nullptr,
    #else
    .callback = QueueFinishCallback,
    #endif
  };

  wgpuQueueOnSubmittedWorkDone(m_wgpuQueue, queueDoneCallback);

  WGPUSurfaceCapabilities capabilities { };
  wgpuSurfaceGetCapabilities(m_wgpuSurface, adapter, &capabilities );
  m_wgpuTextureFormat = capabilities.formats[0];
  WGPUSurfaceConfiguration config { 
    .nextInChain = nullptr,
    .device = m_wgpuCore.m_device,
    .format = m_wgpuTextureFormat,
    .usage = WGPUTextureUsage_RenderAttachment,
    .width = startWidth,
    .height = startHeight,
    .viewFormatCount = 0,
    .viewFormats = nullptr,
    .alphaMode = WGPUCompositeAlphaMode_Auto,
    .presentMode = WGPUPresentMode_Fifo
  };

  wgpuSurfaceCapabilitiesFreeMembers( capabilities );

  wgpuSurfaceConfigure(m_wgpuSurface, &config);
  wgpuAdapterRelease(adapter);

  // Creates vertex/index buffers
  m_meshVertexBuffer.Init(m_wgpuCore.m_device, WGPUBufferUsage_Vertex, "WGPUBackendMeshIdx Vertex Buffer", m_maxMeshVertSize);
  m_meshIndexBuffer.Init(m_wgpuCore.m_device, WGPUBufferUsage_Index, "WGPUBackendMeshIdx Vertex Buffer", m_maxMeshIndexSize);

  // Creates depth texture 
  WGPUTextureDescriptor depthTextureDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Surface texture view"),
    .usage = WGPUTextureUsage_RenderAttachment,
    .dimension = WGPUTextureDimension_2D,
    .size = {startWidth, startHeight, 1},
    .format = m_wgpuDepthTextureFormat,
    .mipLevelCount = 1,
    .sampleCount = 1,
    .viewFormatCount = 1,
    .viewFormats = &m_wgpuDepthTextureFormat,
  };
  m_depthTexture.m_texture = wgpuDeviceCreateTexture(m_wgpuCore.m_device, &depthTextureDescriptor);

  WGPUTextureViewDescriptor depthViewDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Start depth view descriptor"),
    .format = m_wgpuDepthTextureFormat,
    .dimension = WGPUTextureViewDimension_2D,
    .baseMipLevel = 0,
    .mipLevelCount = 1,
    .baseArrayLayer = 0,
    .arrayLayerCount = 1,
    .aspect = WGPUTextureAspect_DepthOnly,
  };

  m_depthTexture.m_textureView = wgpuTextureCreateView(m_depthTexture.m_texture, &depthViewDescriptor);

  // Initializes imgui
  ImGui_ImplWGPU_InitInfo imguiInit;
  imguiInit.Device = m_wgpuCore.m_device;
  imguiInit.RenderTargetFormat = m_wgpuTextureFormat;
  imguiInit.DepthStencilFormat = m_wgpuDepthTextureFormat;
  imguiInit.NumFramesInFlight = 3;

  ImGui_ImplWGPU_Init(&imguiInit);

  ImGui_ImplWGPU_NewFrame();
}

WGPUShaderModule loadShader(const WGPUDevice& device, std::string fileName, std::string shaderLabel) {
  // Loads in shader module
  size_t loadedDatSize;
  auto loadedDat = SDL_LoadFile(fileName.data(), &loadedDatSize);

  // Makes sure data actually gets loaded in
  ASSERT(loadedDat);

  WGPUShaderModuleWGSLDescriptor wgslShaderDesc {
    .chain {
      .next = nullptr,
      .sType = WGPUSType_ShaderSourceWGSL,
    },
    .code{
      .data = reinterpret_cast<const char *>(loadedDat),
      .length = loadedDatSize,
    },
  };

  WGPUShaderModuleDescriptor shaderDesc {
    .nextInChain = &wgslShaderDesc.chain,
    .label = WGPUBackendUtils::wgpuStr(shaderLabel.data()),
  };

  WGPUShaderModule retModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
  SDL_free(loadedDat);
  return retModule;
}

void WGPURenderBackend::InitPipelines()
{
  WGPUShaderModule shaderModule = loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/color_shader.wgsl", "Color Pass Shader");

  WGPUShaderModule depthShaderModule = loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/depth_shader.wgsl", "Depth Pass Shader");

  WGPUShaderModule pointDepthShaderModule = loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/point_depth_shader.wgsl", "Point Depth Pass Shader");

  WGPUShaderModule skyboxShaderModule = loadShader(m_wgpuCore.m_device, SKL_BASE_PATH "/shaderbin/skybox_shader.wgsl", "Skybox Pass Shader");

  // Configures z-buffer
  
  WGPUDepthStencilState depthStencilState {
    .nextInChain = nullptr,
    .format = m_wgpuDepthTextureFormat,
    .depthWriteEnabled = WGPUOptionalBool_True,
    .depthCompare = WGPUCompareFunction_LessEqual,
    .stencilFront {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilBack {
      .compare = WGPUCompareFunction_Always,
      .failOp = WGPUStencilOperation_Keep,
      .depthFailOp = WGPUStencilOperation_Keep,
      .passOp = WGPUStencilOperation_Keep
    },
    .stencilReadMask = 0,
    .stencilWriteMask = 0,
    .depthBias = 0,
    .depthBiasSlopeScale = 0,
    .depthBiasClamp = 0,
  };
  
  WGPUDepthStencilState depthStencilSetSlopeBiased = depthStencilState;
  depthStencilSetSlopeBiased.depthBias = 0.000015;
  depthStencilSetSlopeBiased.depthBiasSlopeScale = 4.5f; // TODO: Make less arbitrary allow to pass in

  WGPUDepthStencilState depthStencilReadOnlyState = depthStencilState;
  depthStencilReadOnlyState.depthWriteEnabled = WGPUOptionalBool_False;
  
  
  WGPUBlendState blendState {
    .color {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_SrcAlpha,
      .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
    },
    .alpha {
      .operation = WGPUBlendOperation_Add,
      .srcFactor = WGPUBlendFactor_Zero,
      .dstFactor = WGPUBlendFactor_One,
    },
  };

  WGPUColorTargetState colorTarget {
    .nextInChain = nullptr,
    .format = m_wgpuTextureFormat,
    .blend = &blendState,
    .writeMask = WGPUColorWriteMask_All,
  };

  WGPUFragmentState fragState {
    .module = shaderModule,
    .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
    .constantCount = 0,
    .constants = nullptr,
    .targetCount = 1,
    .targets = &colorTarget,
  };

  WGPUFragmentState pointDepthDummyFragState {
    .module = pointDepthShaderModule,
    .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
    .constantCount = 0,
    .constants = nullptr,
    .targetCount = 0,
    .targets = nullptr,
  };

  WGPUFragmentState skyboxFragState {
    .module = skyboxShaderModule,
    .entryPoint = WGPUBackendUtils::wgpuStr("fsMain"),
    .constantCount = 0,
    .constants = nullptr,
    .targetCount = 1,
    .targets = &colorTarget,
  };
  
  std::vector<WGPUVertexAttribute> vertexAttributes{ };
  std::vector<WGPUVertexAttribute> depthVertexAttributes{ };
  std::vector<WGPUVertexAttribute> pointDepthVertexAttributes{ };
  std::vector<WGPUVertexAttribute> skyboxVertexAttributes{ };

  WGPUVertexAttribute posVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32x3,
    .offset = 0,
    .shaderLocation = 0,
  };
  vertexAttributes.push_back(posVertAttribute);
  depthVertexAttributes.push_back(posVertAttribute);
  pointDepthVertexAttributes.push_back(posVertAttribute);

  WGPUVertexAttribute uvXVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32,
    .offset = sizeof(glm::vec3),
    .shaderLocation = 1,
  };
  vertexAttributes.push_back(uvXVertAttribute);

  WGPUVertexAttribute normVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32x3,
    .offset =  sizeof(glm::vec3) + sizeof(float),
    .shaderLocation = 2,
  };
  vertexAttributes.push_back(normVertAttribute);
  depthVertexAttributes.push_back(normVertAttribute);
  pointDepthVertexAttributes.push_back(normVertAttribute);

  WGPUVertexAttribute uvYVertAttribute {
    .nextInChain = nullptr,
    .format = WGPUVertexFormat_Float32,
    .offset =  sizeof(glm::vec3) * 2 + sizeof(float),
    .shaderLocation = 3,
  };
  vertexAttributes.push_back(uvYVertAttribute);

  // Set up layout (maybe we want to abstract this later)

  WGPUVertexBufferLayout bufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = vertexAttributes.size(),
    .attributes = vertexAttributes.data(),
  };

  WGPUVertexBufferLayout depthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = depthVertexAttributes.size(),
    .attributes = depthVertexAttributes.data(),
  };

  WGPUVertexBufferLayout pointDepthBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec3) * 2 + sizeof(float) * 2,
    .attributeCount = pointDepthVertexAttributes.size(),
    .attributes = pointDepthVertexAttributes.data(),
  };

  WGPUVertexBufferLayout skyboxBufferLayout {
    .nextInChain = nullptr,
    .stepMode = WGPUVertexStepMode_Vertex,
    .arrayStride = sizeof(glm::vec4),
    .attributeCount = pointDepthVertexAttributes.size(),
    .attributes = pointDepthVertexAttributes.data(),
  };
  
  WGPUBindGroupLayoutEntry fixedColorPassBind = DefaultBindLayoutEntry();
  fixedColorPassBind.binding = 0;
  fixedColorPassBind.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  fixedColorPassBind.buffer.type = WGPUBufferBindingType_Uniform;
  fixedColorPassBind.buffer.minBindingSize = sizeof(WGPUBackendColorPassFixedData);

  WGPUBindGroupLayoutEntry cameraSpaceBind = DefaultBindLayoutEntry();
  cameraSpaceBind.binding = 0;
  cameraSpaceBind.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
  cameraSpaceBind.buffer.type = WGPUBufferBindingType_Uniform;
  cameraSpaceBind.buffer.minBindingSize = sizeof(glm::mat4x4);

  WGPUBindGroupLayoutEntry objDatBind = DefaultBindLayoutEntry();
  objDatBind.visibility = WGPUShaderStage_Vertex;
  objDatBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  objDatBind.buffer.minBindingSize = sizeof(WGPUBackendObjectData);

  WGPUBindGroupLayoutEntry fixedPointDepthPassBind = DefaultBindLayoutEntry();
  fixedPointDepthPassBind.visibility = WGPUShaderStage_Fragment;
  fixedPointDepthPassBind.buffer.type = WGPUBufferBindingType_Uniform;
  fixedPointDepthPassBind.buffer.minBindingSize = sizeof(WGPUBackendPointDepthPassFixedData);

  WGPUBindGroupLayoutEntry dynamicShadowedDirLightBind = DefaultBindLayoutEntry();
  dynamicShadowedDirLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedDirLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedDirLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedDirLightData);

  WGPUBindGroupLayoutEntry dynamicShadowedPointLightBind = DefaultBindLayoutEntry();
  dynamicShadowedPointLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedPointLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedPointLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedPointLightData);

  WGPUBindGroupLayoutEntry dynamicShadowedSpotLightBind = DefaultBindLayoutEntry();
  dynamicShadowedSpotLightBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowedSpotLightBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowedSpotLightBind.buffer.minBindingSize = sizeof(WGPUBackendDynamicShadowedSpotLightData);

  WGPUBindGroupLayoutEntry dynamicShadowLightSpacesBind = DefaultBindLayoutEntry();
  dynamicShadowLightSpacesBind.visibility = WGPUShaderStage_Fragment;
  dynamicShadowLightSpacesBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicShadowLightSpacesBind.buffer.minBindingSize = sizeof(glm::mat4x4);

  WGPUBindGroupLayoutEntry dynamicDirLightShadowMapBind = DefaultBindLayoutEntry();
  dynamicDirLightShadowMapBind.visibility = WGPUShaderStage_Fragment;
  dynamicDirLightShadowMapBind.texture = {
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Depth,
    .viewDimension = WGPUTextureViewDimension_2DArray,
    .multisampled = false
  };

  WGPUBindGroupLayoutEntry dynamicPointLightShadowMapBind = DefaultBindLayoutEntry();
  dynamicPointLightShadowMapBind.visibility = WGPUShaderStage_Fragment;
  dynamicPointLightShadowMapBind.texture = {
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Depth,
    .viewDimension = WGPUTextureViewDimension_CubeArray,
    .multisampled = false
  };

  WGPUBindGroupLayoutEntry dynamicDirLightCascadeRatiosBind = DefaultBindLayoutEntry();
  dynamicDirLightCascadeRatiosBind.visibility = WGPUShaderStage_Fragment;
  dynamicDirLightCascadeRatiosBind.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
  dynamicDirLightCascadeRatiosBind.buffer.minBindingSize = sizeof(float);

  WGPUBindGroupLayoutEntry shadowMapSamplerMapBind = DefaultBindLayoutEntry();
  shadowMapSamplerMapBind.visibility = WGPUShaderStage_Fragment;
  shadowMapSamplerMapBind.sampler = {
    .nextInChain = nullptr,
    .type = WGPUSamplerBindingType_Comparison
  };

  WGPUBindGroupLayoutEntry skyboxTextureBind = DefaultBindLayoutEntry();
  skyboxTextureBind.visibility = WGPUShaderStage_Fragment;
  skyboxTextureBind.texture = {
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Float,
    .viewDimension = WGPUTextureViewDimension_Cube,
    .multisampled = false
  };

  WGPUBindGroupLayoutEntry skyboxSamplerBind = DefaultBindLayoutEntry();
  skyboxSamplerBind.visibility = WGPUShaderStage_Fragment;
  skyboxSamplerBind.sampler = {
    .nextInChain = nullptr,
    .type = WGPUSamplerBindingType_NonFiltering
  };


  std::vector<WGPUBindGroupLayoutEntry> colorBindEntries;
  std::vector<WGPUBindGroupLayoutEntry> depthBindEntities;
  std::vector<WGPUBindGroupLayoutEntry> pointDepthBindEntities;
  std::vector<WGPUBindGroupLayoutEntry> skyboxBindEntities;

  InsertEntry(colorBindEntries,fixedColorPassBind,0);
  InsertEntry(colorBindEntries, objDatBind, 1);
  InsertEntry(colorBindEntries, dynamicShadowedDirLightBind, 2);
  InsertEntry(colorBindEntries, dynamicShadowedPointLightBind, 3);
  InsertEntry(colorBindEntries, dynamicShadowedSpotLightBind, 4);
  InsertEntry(colorBindEntries, dynamicShadowLightSpacesBind, 5);
  InsertEntry(colorBindEntries, dynamicDirLightShadowMapBind, 6);
  InsertEntry(colorBindEntries, dynamicPointLightShadowMapBind, 7);
  InsertEntry(colorBindEntries, dynamicDirLightCascadeRatiosBind, 8);   
  InsertEntry(colorBindEntries, shadowMapSamplerMapBind, 9);  

  InsertEntry(depthBindEntities, cameraSpaceBind, 0);
  InsertEntry(depthBindEntities, objDatBind, 1);

  InsertEntry(pointDepthBindEntities, cameraSpaceBind, 0);
  InsertEntry(pointDepthBindEntities, objDatBind, 1);
  InsertEntry(pointDepthBindEntities, fixedPointDepthPassBind, 2);

  InsertEntry(skyboxBindEntities, cameraSpaceBind, 0);
  InsertEntry(skyboxBindEntities, skyboxTextureBind, 1);
  InsertEntry(skyboxBindEntities, skyboxSamplerBind, 2);


  WGPUBindGroupLayoutDescriptor bindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Bind Layout"),
    .entryCount = colorBindEntries.size(), 
    .entries = colorBindEntries.data(),
  };

  WGPUBindGroupLayoutDescriptor depthBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pass Bind Layout"),
    .entryCount = depthBindEntities.size(), 
    .entries = depthBindEntities.data(),
  };

  WGPUBindGroupLayoutDescriptor pointDepthBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pass Bind Layout"),
    .entryCount = pointDepthBindEntities.size(),
    .entries = pointDepthBindEntities.data(),
  };

  WGPUBindGroupLayoutDescriptor skyboxBindLayoutDescriptor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Skybox Pass Bind Layout"),
    .entryCount = skyboxBindEntities.size(),
    .entries = skyboxBindEntities.data(),
  };

  WGPUBindGroupLayout bindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &bindLayoutDescriptor);
  WGPUBindGroupLayout depthBindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &depthBindLayoutDescriptor);
  WGPUBindGroupLayout pointDepthBindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &pointDepthBindLayoutDescriptor);
  WGPUBindGroupLayout skyboxBindLayout = wgpuDeviceCreateBindGroupLayout(m_wgpuCore.m_device, &skyboxBindLayoutDescriptor);
  
  WGPUPipelineLayoutDescriptor pipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pass Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &bindLayout,
  };

  WGPUPipelineLayoutDescriptor depthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &depthBindLayout,
  };

  WGPUPipelineLayoutDescriptor pointDepthPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &pointDepthBindLayout,
  };

  WGPUPipelineLayoutDescriptor skyboxPipelineLayoutConstructor {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Skybox Pipeline Layout"),
    .bindGroupLayoutCount = 1,
    .bindGroupLayouts = &skyboxBindLayout,
  };

  WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pipelineLayoutConstructor);
  WGPUPipelineLayout depthPipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &depthPipelineLayoutConstructor);
  WGPUPipelineLayout pointDepthPipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &pointDepthPipelineLayoutConstructor);
  WGPUPipelineLayout skyboxPipelineLayout = wgpuDeviceCreatePipelineLayout(m_wgpuCore.m_device, &skyboxPipelineLayoutConstructor);

  WGPURenderPipelineDescriptor skyboxPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Skybox Pipeline"),
    .layout = skyboxPipelineLayout,
    .vertex {
      .module = skyboxShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 0,
      .buffers = &skyboxBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilReadOnlyState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &skyboxFragState,
  };

  WGPURenderPipelineDescriptor depthPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Depth Pipeline"),
    .layout = depthPipelineLayout,
    .vertex {
      .module = depthShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &depthBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = nullptr,
  };

  // Ideally ensures that depth clamping logic still goes through
  // even when not supported by hardware
  WGPUConstantEntry enableManualClampingEntry {
    .key = WGPUBackendUtils::wgpuStr("manualClamping"),
    .value = static_cast<double>(!m_hardwareDepthClampingSupported)
  };

  WGPURenderPipelineDescriptor directionalDepthPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Directional Depth Pipeline"),
    .layout = depthPipelineLayout,
    .vertex {
      .module = depthShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 1,
      .constants = &enableManualClampingEntry,
      .bufferCount = 1,
      .buffers = &depthBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back,
      .unclippedDepth = m_hardwareDepthClampingSupported
    },
    .depthStencil = &depthStencilSetSlopeBiased,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = nullptr,
  };

  WGPURenderPipelineDescriptor pointDepthPipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Point Depth Pipeline "),
    .layout = pointDepthPipelineLayout,
    .vertex {
      .module = pointDepthShaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &pointDepthBufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &pointDepthDummyFragState,
  };

  WGPURenderPipelineDescriptor pipelineDesc {
    .nextInChain = nullptr,
    .label = WGPUBackendUtils::wgpuStr("Color Pipeline"),
    .layout = pipelineLayout,
    .vertex {
      .module = shaderModule,
      .entryPoint = WGPUBackendUtils::wgpuStr("vtxMain"),
      .constantCount = 0,
      .constants = nullptr,
      .bufferCount = 1,
      .buffers = &bufferLayout
    },
    .primitive {
      .topology = WGPUPrimitiveTopology_TriangleList,
      .stripIndexFormat = WGPUIndexFormat_Undefined,
      .frontFace = WGPUFrontFace_CCW,
      .cullMode = WGPUCullMode_Back
    },
    .depthStencil = &depthStencilReadOnlyState,
    .multisample {
      .count = 1,
      .mask = ~0u,
      .alphaToCoverageEnabled = false,
    },
    .fragment = &fragState,
  };

  m_defaultPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pipelineDesc);
  m_depthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &depthPipelineDesc);
  m_directionDepthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &directionalDepthPipelineDesc);
  m_pointDepthPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &pointDepthPipelineDesc);
  m_skyboxPipeline = wgpuDeviceCreateRenderPipeline(m_wgpuCore.m_device, &skyboxPipelineDesc); 

  m_cameraSpaceBuffer.Init(m_wgpuCore.m_device, "Camera Space Buffer");
  m_fixedColorPassDatBuffer.Init(m_wgpuCore.m_device, "Color Pass Fixed Data Buffer");
  m_instanceDatBuffer.Init(m_wgpuCore.m_device, "Instance Buffer", m_maxObjArraySize);
  m_fixedPointDepthPassDatBuffer.Init(m_wgpuCore.m_device, "Point Depth Pass Fixed Data Buffer");
  m_dynamicShadowedDirLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Direction Light Buffer", m_maxDynamicShadowedDirLights);
  m_dynamicShadowedPointLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Point Light Buffer", m_maxDynamicShadowedPointLights);
  m_dynamicShadowedSpotLightBuffer.Init(m_wgpuCore.m_device, "Dynamic Shadowed Dir Light Buffer", m_maxDynamicShadowedSpotLights);
  m_dynamicShadowLightSpaces.Init(m_wgpuCore.m_device, "Dynamic Shadow Light Spaces", m_maxDynamicShadowLightSpaces);
  m_dynamicShadowedDirLightCascadeRatiosBuffer.Init(m_wgpuCore.m_device, "Shadowed Dynamic Directional Light Cascade Ratios", DefaultCascadeCount);
  
  m_dynamicDirLightShadowMapTexture.Init(
    m_wgpuCore.m_device, 
    DefaultDirLightDim, 
    DefaultDirLightDim, 
    32, 
    DefaultCascadeCount, 
    "Dynamic Direction Light Shadow Maps", 
    "Dynamic Direction Light Shadow Maps Whole", 
    "Dynamic Direction Light Shadow Maps Layer",
    false);
  m_dynamicPointLightShadowMapTexture.Init(
    m_wgpuCore.m_device, 
    DefaultPointLightDim, 
    DefaultPointLightDim, 
    2048, 
    6,
    "Dynamic Point Light Shadow Maps", 
    "Dynamic Point Light Shadow Maps Cube Arrays", 
    "Dynamic Point Light Shadow Maps Texture Layer",
    true);
  m_shadowMapSampler.InitOrUpdate(
    m_wgpuCore.m_device, 
    m_wgpuQueue, 
    WGPUAddressMode_ClampToEdge, 
    WGPUFilterMode_Linear, 
    WGPUFilterMode_Linear, 
    WGPUMipmapFilterMode_Nearest, 
    0.0, 
    0.0, 
    WGPUCompareFunction_Less, 
    1, 
    "Shadow Map Sampler");
  m_skyboxTexture.Init(
    m_wgpuCore.m_device,
    DefaultSkyboxDim,
    DefaultSkyboxDim,
    "Skybox Texture",
    "Skybox Texture Cubemap View"
  );
  m_skyboxSampler.InitOrUpdate(
    m_wgpuCore.m_device, 
    m_wgpuQueue, 
    WGPUAddressMode_ClampToEdge, 
    WGPUFilterMode_Nearest, 
    WGPUFilterMode_Nearest, 
    WGPUMipmapFilterMode_Nearest, 
    0.0, 
    0.0, 
    WGPUCompareFunction_Undefined, 
    1, 
    "Skybox Sampler"
  );

  m_colorPassBindGroup.Init("Color Pass Pipeline Bind Group", bindLayout);
  m_depthBindGroup.Init("Depth Pipeline Pass Bind Group", depthBindLayout);
  m_pointDepthBindGroup.Init("Point Depth Pass Bind Group", pointDepthBindLayout);
  m_skyboxBindGroup.Init("Skybox pass bind group", skyboxBindLayout);

  m_colorPassBindGroup.AddEntryToBindingGroup(&m_fixedColorPassDatBuffer, 0);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_instanceDatBuffer, 1);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicShadowedDirLightBuffer, 2);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicShadowedPointLightBuffer, 3);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicShadowedSpotLightBuffer, 4);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicShadowLightSpaces, 5);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicDirLightShadowMapTexture, 6);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicPointLightShadowMapTexture, 7);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_dynamicShadowedDirLightCascadeRatiosBuffer, 8);
  m_colorPassBindGroup.AddEntryToBindingGroup(&m_shadowMapSampler, 9);

  m_depthBindGroup.AddEntryToBindingGroup(&m_cameraSpaceBuffer, 0);
  m_depthBindGroup.AddEntryToBindingGroup(&m_instanceDatBuffer, 1);

  m_pointDepthBindGroup.AddEntryToBindingGroup(&m_cameraSpaceBuffer, 0);
  m_pointDepthBindGroup.AddEntryToBindingGroup(&m_instanceDatBuffer, 1);
  m_pointDepthBindGroup.AddEntryToBindingGroup(&m_fixedPointDepthPassDatBuffer, 2);

  m_skyboxBindGroup.AddEntryToBindingGroup(&m_cameraSpaceBuffer, 0);
  m_skyboxBindGroup.AddEntryToBindingGroup(&m_skyboxTexture, 1);
  m_skyboxBindGroup.AddEntryToBindingGroup(&m_skyboxSampler, 2);
  
  m_colorPassBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_depthBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_pointDepthBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
  m_skyboxBindGroup.UpdateBindGroup(m_wgpuCore.m_device);
    
  wgpuPipelineLayoutRelease(skyboxPipelineLayout);
  wgpuPipelineLayoutRelease(pointDepthPipelineLayout);
  wgpuPipelineLayoutRelease(depthPipelineLayout);
  wgpuPipelineLayoutRelease(pipelineLayout);
  wgpuShaderModuleRelease(skyboxShaderModule);
  wgpuShaderModuleRelease(pointDepthShaderModule);
  wgpuShaderModuleRelease(depthShaderModule);
  wgpuShaderModuleRelease(shaderModule);
  wgpuBindGroupLayoutRelease(skyboxBindLayout);
  wgpuBindGroupLayoutRelease(pointDepthBindLayout);
  wgpuBindGroupLayoutRelease(depthBindLayout);
  wgpuBindGroupLayoutRelease(bindLayout);
}

MeshID WGPURenderBackend::UploadMesh(u32 vertCount, Vertex* vertices, u32 indexCount, u32* indices) {
  u32 retInt = m_nextMeshID;
  m_meshStore.emplace(std::pair<u32, WGPUBackendMeshIdx>(retInt, WGPUBackendMeshIdx(m_meshTotalIndices, m_meshTotalVertices, indexCount, vertCount)));
  
  m_meshVertexBuffer.AppendToBack(m_wgpuCore.m_device, m_wgpuQueue, vertices, vertCount);
  m_meshIndexBuffer.AppendToBack(m_wgpuCore.m_device, m_wgpuQueue, indices, indexCount);

  m_nextMeshID++;
  m_meshTotalIndices += indexCount; 
  m_meshTotalVertices += vertCount;

  return retInt;
}

void WGPURenderBackend::DestroyMesh(MeshID meshID) {
  WGPUBackendMeshIdx& gotMesh = m_meshStore[meshID];

  // Removes mesh gpu side informations
  m_meshVertexBuffer.EraseRange(m_wgpuCore.m_device, m_wgpuQueue, gotMesh.m_baseVertex , gotMesh.m_vertexCount);
  m_meshIndexBuffer.EraseRange(m_wgpuCore.m_device, m_wgpuQueue, gotMesh.m_baseIndex , gotMesh.m_indexCount);

  // Readjusts mesh cpu side descriptors
  for (std::pair<MeshID, WGPUBackendMeshIdx> meshIter : m_meshStore) {
    if(meshIter.first > meshID) {
      WGPUBackendMeshIdx& editMesh = meshIter.second;
      editMesh.m_baseIndex -= gotMesh.m_indexCount;
      editMesh.m_baseVertex -= gotMesh.m_vertexCount;
    }
  }

  m_meshTotalVertices -= gotMesh.m_indexCount;
  m_meshTotalVertices -= gotMesh.m_vertexCount;

  // Removes mesh cpu side descriptors
  m_meshStore.erase(meshID);
}

void WGPURenderBackend::SetSkybox(u32 width, u32 height, const std::array<u32*,6>& faceData) {
  m_skyboxTexture.Insert(m_wgpuCore.m_device, m_wgpuQueue, width, height, faceData);
}

void WGPURenderBackend::RenderUpdate(RenderFrameInfo& state) {
  // Inits frame and checks if frame is able to be rendered
  if (!InitFrame())
  {
      return;
  }

  // >>> Begins processing frame information to be ran by renderer <<<

  // Inserts mesh instance information into a single objData vector
  std::map<MeshID, u32> meshCounts;
  for (MeshRenderInfo meshInstance: state.meshes)
  {
    meshCounts[meshInstance.mesh] += 1;
  }

  u32 totalCount = 0;
  std::unordered_map<MeshID, u32> offsets;
  for (std::pair<MeshID,u32> meshType : meshCounts) 
  {
    offsets[meshType.first] = totalCount;
    totalCount += meshType.second;
  }

  std::vector<WGPUBackendObjectData> objData(totalCount);
  for (MeshRenderInfo meshInstance: state.meshes)
  {
      u32 instanceIdx = offsets[meshInstance.mesh]++;
      glm::mat4x4 normMat = glm::transpose(glm::inverse(meshInstance.matrix));
      objData[instanceIdx] = {meshInstance.matrix, normMat, glm::vec4(meshInstance.rgbColor, 1)};
  }

  // Prepares camera to be rendered through
  float mainCamAspectRatio = (float)m_screenWidth / (float)m_screenHeight;
  glm::mat4x4 mainCamProj = glm::perspective(glm::radians(state.cameraFov), mainCamAspectRatio, state.cameraNear, state.cameraFar);
  glm::mat4x4 mainCamView = state.cameraTransform->GetViewMatrix();
  glm::mat4x4 camSpace = mainCamProj * mainCamView;
    
  // Prepares dynamic shadowed lights to be rendered
  std::vector<glm::mat4x4> dirLightSpaces;
  std::vector<glm::mat4x4> pointLightSpaces;

  // TODO: Make cascade ratios more adjustable
  std::vector<float> cascadeRatios = {0.25, 0.50, 0.75, 1.00};
  const std::vector<WGPUBackendDynamicShadowedDirLightData> shadowedDirLightData = ConvertDirLights(state.dirLights, dirLightSpaces, 4, camSpace, cascadeRatios, 0.05, state.cameraFar);
  const std::vector<WGPUBackendDynamicShadowedPointLightData> shadowedPointLightData = ConvertPointLights(state.pointLights, pointLightSpaces, DefaultPointLightDim, DefaultPointLightDim);
  const std::vector<WGPUBackendDynamicShadowedSpotLightData> shadowedSpotLightData = ConvertSpotLights(state.spotLights);
  // >>> Actually begins sending off information to be rendered <<<

  // Sends in the attributes of individual mesh instances
  m_instanceDatBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, objData.data(), (u32)objData.size());

  // Begins writing in shadow mapping passes and inserting data for shadowed lights
  for (u8 cascadeIter = 0 ; cascadeIter < DefaultCascadeCount ; cascadeIter++) {
    for (u32 dirShadowIdx = 0 ; dirShadowIdx < shadowedDirLightData.size() ; dirShadowIdx++) {
      m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, dirLightSpaces[dirShadowIdx + cascadeIter * shadowedDirLightData.size()]);
      BeginDirectionalDepthPass(m_dynamicDirLightShadowMapTexture.GetView(dirShadowIdx * DefaultCascadeCount + cascadeIter));
      DrawObjects(meshCounts);
      EndPass();
    }
  }

  for (u32 pointLightIdx = 0 ; pointLightIdx < shadowedPointLightData.size() ; pointLightIdx++) {
    const WGPUBackendDynamicShadowedPointLightData& pointLight = shadowedPointLightData[pointLightIdx];
    m_fixedPointDepthPassDatBuffer.WriteBuffer(m_wgpuQueue, {pointLight.m_position, pointLight.m_radius});
    for (u32 pointShadowIdx = pointLightIdx * 6 ; pointShadowIdx < (pointLightIdx + 1) * 6 ; pointShadowIdx++) {
      m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, pointLightSpaces[pointShadowIdx]);
      BeginPointDepthPass(m_dynamicPointLightShadowMapTexture.GetView(pointShadowIdx));
      DrawObjects(meshCounts);
      EndPass();
    }
  }

  // Edits cascade ratios to be put in world space
  float camNearFarDiff = state.cameraFar - state.cameraNear;
  for (float& ratio : cascadeRatios) {
    ratio = state.cameraNear + camNearFarDiff * ratio;
  }
  m_dynamicShadowedDirLightCascadeRatiosBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, cascadeRatios.data(), DefaultCascadeCount);

  // Begins writing in dynamic lights
  m_dynamicShadowedDirLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedDirLightData.data(), (u32)shadowedDirLightData.size());

  m_dynamicShadowedPointLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedPointLightData.data(), (u32)shadowedPointLightData.size());

  m_dynamicShadowedSpotLightBuffer.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, shadowedSpotLightData.data(), (u32)shadowedSpotLightData.size());

  m_dynamicShadowLightSpaces.WriteBuffer(m_wgpuCore.m_device, m_wgpuQueue, dirLightSpaces.data(), (u32)dirLightSpaces.size());

  // Sets fixed data
  WGPUBackendColorPassFixedData colorPassState {
    .m_combined = camSpace,
    .m_view = mainCamView,
    .m_proj = mainCamProj,
    .m_pos = state.cameraTransform->position,
    .m_dirLightCount = (u32)shadowedDirLightData.size(),
    .m_pointLightCount = (u32)shadowedPointLightData.size(),
    .m_spotLightCount = (u32)shadowedSpotLightData.size(),
    .m_dirLightCascadeCount = DefaultCascadeCount,
    .m_dirLightMapPixelDimension = DefaultDirLightDim,
    .m_pointLightMapPixelDimension = DefaultPointLightDim,
    .m_pcsRange = 2
  };
  m_fixedColorPassDatBuffer.WriteBuffer(m_wgpuQueue, colorPassState);

  // Sets the orientation of the view camera for depth pre pass
  m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, camSpace);

  BeginDepthPass(m_depthTexture.m_textureView);
  DrawObjects(meshCounts);
  EndPass();

  BeginColorPass();
  DrawObjects(meshCounts);
  EndPass();

  m_cameraSpaceBuffer.WriteBuffer(m_wgpuQueue, glm::inverse(mainCamProj * glm::mat4x4(glm::mat3x3(mainCamView))));
  BeginSkyboxPass();
  wgpuRenderPassEncoderDraw(m_renderPassEncoder, 3, 1, 0, 0);
  EndPass();

  DrawImGui();
  EndFrame();
}

// Adds dynamic lights into scene
LightID WGPURenderBackend::AddDirLight() {
  LightID lightID = m_dynamicShadowedDirLightNextID;
  m_dynamicShadowedDirLightNextID++;
  m_dynamicDirLightShadowMapTexture.RegisterShadow(m_wgpuCore.m_device, m_wgpuQueue);
  return lightID;
}
LightID WGPURenderBackend::AddSpotLight() {
  LightID lightID = m_dynamicShadowedSpotLightNextID;
  m_dynamicShadowedSpotLightNextID++;
  return lightID;
}
LightID WGPURenderBackend::AddPointLight() {
  LightID lightID = m_dynamicShadowedPointLightNextID;
  m_dynamicShadowedPointLightNextID++;
  m_dynamicPointLightShadowMapTexture.RegisterShadow(m_wgpuCore.m_device, m_wgpuQueue);
  return lightID;
}

void WGPURenderBackend::DestroyDirLight(LightID lightID) {
  m_dynamicDirLightShadowMapTexture.UnregisterShadow();
}
// TODO Actually implement
void WGPURenderBackend::DestroySpotLight(LightID lightID) {
  
}
void WGPURenderBackend::DestroyPointLight(LightID lightID) {
  m_dynamicPointLightShadowMapTexture.UnregisterShadow();
}
#pragma endregion
