#pragma once

#include <d3d11.h>
#include <mutex>
#include <atomic>

namespace wallpaper {
namespace d3d11_interop {

inline std::mutex g_sharedDeviceMutex;
inline ID3D11Device* g_sharedDevice = nullptr;
inline std::atomic<std::uint64_t> g_sharedDeviceRevision{1U};

inline void SetSharedDevice(ID3D11Device* const device) noexcept {
  std::lock_guard<std::mutex> lock(g_sharedDeviceMutex);
  ID3D11Device*& slot = g_sharedDevice;
  if (slot == device) {
    return;
  }
  if (slot != nullptr) {
    slot->Release();
    slot = nullptr;
  }
  if (device != nullptr) {
    device->AddRef();
    slot = device;
  }
  g_sharedDeviceRevision.fetch_add(1U, std::memory_order_release);
}

inline void ClearSharedDevice(ID3D11Device* const device) noexcept {
  std::lock_guard<std::mutex> lock(g_sharedDeviceMutex);
  ID3D11Device*& slot = g_sharedDevice;
  if (slot == nullptr) {
    return;
  }
  if (device != nullptr && slot != device) {
    return;
  }
  slot->Release();
  slot = nullptr;
  g_sharedDeviceRevision.fetch_add(1U, std::memory_order_release);
}

inline ID3D11Device* AcquireSharedDevice() noexcept {
  std::lock_guard<std::mutex> lock(g_sharedDeviceMutex);
  ID3D11Device* const slot = g_sharedDevice;
  if (slot != nullptr) {
    slot->AddRef();
  }
  return slot;
}

inline std::uint64_t QuerySharedDeviceRevision() noexcept {
  return g_sharedDeviceRevision.load(std::memory_order_acquire);
}

}  // namespace d3d11_interop
}  // namespace wallpaper
