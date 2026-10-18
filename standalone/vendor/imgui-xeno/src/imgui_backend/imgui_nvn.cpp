#include "imgui_nvn.h"
#include "helpers/InputHelper.h"
#include "helpers/memoryHelper.h"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "imgui_backend_config.h"
#include "logger/Logger.hpp"
#include "nvn_CppFuncPtrImpl.h"
#include "nx/abort.h"
extern "C" long long mhr_get_tick();

nvn::Device *nvnDevice;
nvn::Queue *nvnQueue;
nvn::CommandBuffer *nvnCmdBuf;

static nvn::CommandBuffer *__cmdBuf = nullptr;
static const nvn::TexturePool *__texturePool = nullptr;
static const nvn::SamplerPool *__samplerPool = nullptr;

nvn::DeviceGetProcAddressFunc tempGetProcAddressFuncPtr;

nvn::CommandBufferInitializeFunc tempBufferInitFuncPtr;
nvn::DeviceInitializeFunc tempDeviceInitFuncPtr;
nvn::QueueInitializeFunc tempQueueInitFuncPtr;
nvn::QueuePresentTextureFunc tempPresentTexFunc;
nvn::WindowSetCropFunc tempSetCropFunc;

nvn::CommandBufferSetTexturePoolFunc tempCommandSetTexturePoolFunc;
nvn::CommandBufferSetSamplerPoolFunc tempCommandSetSamplerPoolFunc;

bool hasInitImGui = false;
int commandBufferCount = 0;
static nvn::CommandBuffer hudCommandBuffer;
static nvn::Queue hudQueue;
static nvn::Window *hudWindow;
static const nvn::Texture *hudTextures[8];
static int hudTextureCount;
static nvn::WindowInitializeFunc originalWindowInit;

NVNboolean windowInit(nvn::Window *window, const nvn::WindowBuilder *builder) {
  NVNboolean result = originalWindowInit(window, builder);
  if (result) {
    hudWindow = window;
    hudTextureCount = builder->GetNumTextures();
    if (hudTextureCount > 8) hudTextureCount = 0;
    for (int i = 0; i < hudTextureCount; ++i) hudTextures[i] = builder->GetTexture(i);
    Logger::log("[MHR-HUD] window captured with %d presentation textures\n", hudTextureCount);
  }
  return result;
}

namespace nvnImGui {
  ImVector<ProcDrawFunc> drawQueue;
  ImVector<InitFunc> preInitQueue;
  ImVector<InitFunc> postInitQueue;
  ImVector<NewFrameFunc> newFrameQueue;
}

void setTexturePool(nvn::CommandBuffer *cmdBuf, const nvn::TexturePool *pool) {
  __cmdBuf = cmdBuf;
  __texturePool = pool;

  tempCommandSetTexturePoolFunc(cmdBuf, pool);
}

void setSamplerPool(nvn::CommandBuffer *cmdBuf, const nvn::SamplerPool *pool) {
  __samplerPool = pool;

  tempCommandSetSamplerPoolFunc(cmdBuf, pool);
}

void setCrop(nvn::Window *window, int x, int y, int w, int h) {

  Logger::log("Window Crop: x: %d y: %d w: %d h: %d\n", x, y, w, h);

  tempSetCropFunc(window, x, y, w, h);

  if (hasInitImGui)
    ImGui::GetIO().DisplaySize = ImVec2(w - x, h - y);
}

void presentTexture(nvn::Queue *queue, nvn::Window *window, int texIndex) {

  static unsigned presents = 0;
  if (++presents <= 256 && !(presents & (presents - 1))) {
    Logger::log("[MHR-HUD] present %u ready=%d buffer=%p texture=%p sampler=%p\n",
        presents, hasInitImGui, __cmdBuf, __texturePool, __samplerPool);
  }

  if (!hasInitImGui || window != hudWindow || texIndex < 0 || texIndex >= hudTextureCount) {
    tempPresentTexFunc(queue, window, texIndex);
    return;
  }

  auto *bd = ImguiNvnBackend::getBackendData();
  bd->gameQueue = queue;
  bd->presentTarget = hudTextures[texIndex];
  ImGui::GetIO().DisplaySize = ImVec2(bd->presentTarget->GetWidth(), bd->presentTarget->GetHeight());
  long long begin = mhr_get_tick();
  nvnImGui::procDraw();
  long long end = mhr_get_tick();
  static long long reportAt = 0, elapsed = 0, peak = 0;
  static unsigned sampled = 0, visible = 0;
  if (!reportAt) reportAt = end;
  ++sampled;
  if (ImGui::GetDrawData()->CmdListsCount) {
    ++visible;
    elapsed += end - begin;
    if (end - begin > peak) peak = end - begin;
  }
  if (end - reportAt >= 15LL * 19200000) {
    Logger::log("[MHR-HUD] perf frames=%u visible=%u hud_us_avg=%llu hud_us_peak=%llu reuse_waits=%u reuse_wait_us=%llu skipped=%u sync_fallbacks=%u\n",
        sampled, visible, visible ? elapsed * 1000000ULL / 19200000 / visible : 0,
        peak * 1000000ULL / 19200000, bd->reuseWaits,
        bd->reuseWaitTicks * 1000000ULL / 19200000, bd->skippedFrames, bd->syncFallbacks);
    reportAt = end; elapsed = peak = 0; sampled = visible = 0;
    bd->reuseWaits = bd->skippedFrames = bd->syncFallbacks = 0; bd->reuseWaitTicks = 0;
  }

  tempPresentTexFunc(queue, window, texIndex);
}

NVNboolean deviceInit(nvn::Device *device, const nvn::DeviceBuilder *builder) {
  NVNboolean result = tempDeviceInitFuncPtr(device, builder);
  nvnDevice = device;
  nvn::nvnLoadCPPProcs(nvnDevice, tempGetProcAddressFuncPtr);
  return result;
}

NVNboolean queueInit(nvn::Queue *queue, const nvn::QueueBuilder *builder) {
  NVNboolean result = tempQueueInitFuncPtr(queue, builder);
  nvnQueue = queue;
  return result;
}


NVNboolean cmdBufInit(nvn::CommandBuffer *buffer, nvn::Device *device) {
  NVNboolean result = tempBufferInitFuncPtr(buffer, device);

  if (!hasInitImGui) {
    // XENO: wait for the right command buffer
    if (commandBufferCount++ != IMGUI_XENO_COMMAND_BUFFER_ID) {
      return result;
    }
    nvnCmdBuf = buffer;
    hasInitImGui = nvnImGui::InitImGui();
  }

  return result;
}

nvn::GenericFuncPtrFunc getProc(nvn::Device *device, const char *procName) {
  nvn::GenericFuncPtrFunc ptr = tempGetProcAddressFuncPtr(nvnDevice, procName);

  if (strcmp(procName, "nvnWindowInitialize") == 0) {
    originalWindowInit = (nvn::WindowInitializeFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &windowInit;
  } else if (strcmp(procName, "nvnQueueInitialize") == 0) {
    tempQueueInitFuncPtr = (nvn::QueueInitializeFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &queueInit;
  } else if (strcmp(procName, "nvnCommandBufferInitialize") == 0) {
    tempBufferInitFuncPtr = (nvn::CommandBufferInitializeFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &cmdBufInit;
  } else if (strcmp(procName, "nvnQueuePresentTexture") == 0) {
    tempPresentTexFunc = (nvn::QueuePresentTextureFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &presentTexture;
  } else if (strcmp(procName, "nvnDeviceInitialize") == 0) {
    tempDeviceInitFuncPtr = (nvn::DeviceInitializeFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &deviceInit;
  } else if (strcmp(procName, "nvnCommandBufferSetSamplerPool") == 0) {
    tempCommandSetSamplerPoolFunc = (nvn::CommandBufferSetSamplerPoolFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &setSamplerPool;
  } else if (strcmp(procName, "nvnCommandBufferSetTexturePool") == 0) {
    tempCommandSetTexturePoolFunc = (nvn::CommandBufferSetTexturePoolFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &setTexturePool;
  } else if (strcmp(procName, "nvnWindowSetCrop") == 0) {
    tempSetCropFunc = (nvn::WindowSetCropFunc) ptr;
    return (nvn::GenericFuncPtrFunc) &setCrop;
  }

  return ptr;
}

void disableButtons(nn::hid::NpadBaseState *state) {
  if (!InputHelper::isReadInputs() && InputHelper::isInputToggled()) {
    // clear out the data within the state (except for the sampling number and attributes)
    state->mButtons = nn::hid::NpadButtonSet();
    state->mAnalogStickL = nn::hid::AnalogStickState();
    state->mAnalogStickR = nn::hid::AnalogStickState();
  }
}

void* nvnImGui::NvnBootstrapHook(const char *funcName, OrigNvnBootstrap origFn) {
  void *result = origFn(funcName);

  Logger::log("Getting Proc from Bootstrap: %s\n", funcName);

  if (strcmp(funcName, "nvnDeviceInitialize") == 0) {
    tempDeviceInitFuncPtr = (nvn::DeviceInitializeFunc) result;
    return (void *) &deviceInit;
  }
  if (strcmp(funcName, "nvnDeviceGetProcAddress") == 0) {
    tempGetProcAddressFuncPtr = (nvn::DeviceGetProcAddressFunc) result;
    return (void *) &getProc;
  }

  return result;
}

void nvnImGui::addDrawFunc(ProcDrawFunc func) {
  XENO_ASSERT(func != nullptr, "Function cannot be nullptr!");
  XENO_ASSERT(!drawQueue.contains(func), "Function has already been added to queue!");

  drawQueue.push_back(func);
}

void nvnImGui::addPreInitFunc(InitFunc func) {
  XENO_ASSERT(func != nullptr, "Function cannot be nullptr!");
  XENO_ASSERT(!preInitQueue.contains(func), "Function has already been added to queue!");

  preInitQueue.push_back(func);
}

void nvnImGui::addPostInitFunc(InitFunc func) {
  XENO_ASSERT(func != nullptr, "Function cannot be nullptr!");
  XENO_ASSERT(!postInitQueue.contains(func), "Function has already been added to queue!");

  postInitQueue.push_back(func);
}

void nvnImGui::addNewFrameFunc(NewFrameFunc func) {
  XENO_ASSERT(func != nullptr, "Function cannot be nullptr!");
  XENO_ASSERT(!newFrameQueue.contains(func), "Function has already been added to queue!");

  newFrameQueue.push_back(func);
}

void nvnImGui::procDraw() {
  for (auto newFrameFunc: newFrameQueue) {
    newFrameFunc();
  }

  ImguiNvnBackend::newFrame();
  ImGui::NewFrame();
  ImGui::GetIO().MouseDrawCursor = InputHelper::toggleInput;
  if (ImGui::GetMousePos().x < 1 || ImGui::GetMousePos().y < 1) {
    ImGui::GetIO().MouseDrawCursor = false;
  }

  for (auto drawFunc: drawQueue) {
    drawFunc();
  }

  ImGui::Render();
  ImguiNvnBackend::renderDrawData(ImGui::GetDrawData());
}

void nvnImGui::InstallHooks() {
  //NvnBootstrapHook::InstallAtSymbol("nvnBootstrapLoader"); // TODO xeno
  /*
  DisableFullKeyState::InstallAtSymbol("_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadFullKeyStateEiRKj");
  DisableHandheldState::InstallAtSymbol("_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_17NpadHandheldStateEiRKj");
  DisableJoyDualState::InstallAtSymbol("_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadJoyDualStateEiRKj");
  DisableJoyLeftState::InstallAtSymbol("_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_16NpadJoyLeftStateEiRKj");
  DisableJoyRightState::InstallAtSymbol("_ZN2nn3hid6detail13GetNpadStatesEPiPNS0_17NpadJoyRightStateEiRKj");*/ // TODO xeno
}

bool nvnImGui::InitImGui() {
  if (nvnDevice && nvnQueue && nvnCmdBuf) {

    Logger::log("Creating ImGui with Ver.\n");

    IMGUI_CHECKVERSION();

    Mem::Init();

    ImGuiMemAllocFunc allocFunc = [](size_t size, void *user_data) {
      return Mem::Allocate(size);
    };

    ImGuiMemFreeFunc freeFunc = [](void *ptr, void *user_data) {
      Mem::Deallocate(ptr);
    };

    ImGui::SetAllocatorFunctions(allocFunc, freeFunc, nullptr);

    Logger::log("Creating ImGui context.\n");
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;
    Logger::log("Created ImGui context.\n");

    ImGui::StyleColorsDark();

    nvn::QueueBuilder queueBuilder;
    queueBuilder.SetDefaults().SetDevice(nvnDevice);
    if (!tempQueueInitFuncPtr(&hudQueue, &queueBuilder)) {
      Logger::log("[MHR-HUD] isolated GPU queue could not initialize\n");
      return false;
    }
    Logger::log("[MHR-HUD] isolated GPU queue initialized; game queue=%p HUD queue=%p\n", nvnQueue, &hudQueue);
    if (!tempBufferInitFuncPtr(&hudCommandBuffer, nvnDevice)) return false;
    ImguiNvnBackend::NvnBackendInitInfo initInfo = {
        .device = nvnDevice,
        .queue = &hudQueue,
        .cmdBuf = &hudCommandBuffer
    };

    for (auto init: preInitQueue) {
      init();
    }

    Logger::log("Initializing Backend.\n");

    ImguiNvnBackend::InitBackend(initInfo);
    if (!ImguiNvnBackend::getBackendData()->isInitialized) {
      Logger::log("[MHR-HUD] NVN backend setup failed; drawing disabled\n");
      return false;
    }

    InputHelper::initKBM();

    // set input helpers default port
    InputHelper::setPort(IMGUI_XENO_DEFAULT_INPUT_PORT);

    for (auto init: postInitQueue) {
      init();
    }

#if IMGUI_XENO_DRAW_DEMO
    addDrawFunc([]() { ImGui::ShowDemoWindow(); });
#endif

    return true;

  } else {
    Logger::log("Unable to create ImGui Renderer!\n");

    return false;
  }
}
