#include "MemoryPoolMaker.h"
#include "helpers/memoryHelper.h"
#include "imgui_impl_nvn.hpp"
#include "logger/Logger.hpp"

bool MemoryPoolMaker::createPool(nvn::MemoryPool *result, size_t size, const nvn::MemoryPoolFlags &flags) {

  auto bd = ImguiNvnBackend::getBackendData();

  void *poolPtr = Mem::AllocateAlign(0x1000, size);
  if (!poolPtr) {
    Logger::log("[MHR-HUD] aligned pool allocation failed: size=%lu\n", size);
    return false;
  }

  nvn::MemoryPoolBuilder memPoolBuilder{};
  memPoolBuilder.SetDefaults().SetDevice(bd->device).SetFlags(flags).SetStorage(poolPtr, size);

  if (!result->Initialize(&memPoolBuilder)) {
    Logger::log("[MHR-HUD] NVN memory pool init failed: size=%lu ptr=%p\n", size, poolPtr);
    return false;
  }

  return true;
}
