#pragma once
#include "loader.h"
#include "policy.h"
#include <atomic>
#include <functional>
namespace superman_returns::graphics::vulkan {
class Context {
public:
  Context() = default;
  explicit Context(Dispatch injected) : f(injected), injected_(true) {}
  ~Context();
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;
  bool CreateInstance(std::span<const char *> platform_extensions,
                      bool validation, Error &);
  bool EnumerateCandidates(VkSurfaceKHR, std::vector<DeviceCandidate> &,
                           Error &);
  bool OpenDevice(VkSurfaceKHR owned_surface, std::string_view uuid, Error &,
                  const VkPhysicalDeviceFeatures *requested_features = nullptr,
                  bool require_mirror_clamp=false);
  PFN_vkGetInstanceProcAddr Proc() const {
    return loader_.GetInstanceProcAddr();
  }
  void Log(const std::string &s) const;
  std::function<void(const std::string &)> logger;
  Dispatch f;
  VkInstance instance = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkPhysicalDevice physical = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkQueue graphics_queue = VK_NULL_HANDLE, present_queue = VK_NULL_HANDLE;
  uint32_t graphics_family = 0, present_family = 0;
  DeviceCandidate selected{};
  VkPhysicalDeviceProperties properties{};
  VkPhysicalDeviceMemoryProperties memory{};
  VkPhysicalDeviceFeatures enabled_features{};
  bool mirror_clamp_enabled=false;
  std::atomic<uint32_t> validation_errors{0};
  bool validation_active = false;

private:
  Loader loader_;
  bool injected_ = false;
  VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
  std::vector<VkPhysicalDevice> devices_;
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  Debug(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
        const VkDebugUtilsMessengerCallbackDataEXT *, void *);
};
} // namespace superman_returns::graphics::vulkan
