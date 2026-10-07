#pragma once
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
namespace superman_returns::graphics::vulkan {
struct QueueFamily {
  VkQueueFlags flags;
  bool present;
};
struct DeviceCandidate {
  std::string name, uuid;
  uint32_t api_version;
  VkPhysicalDeviceType type;
  bool swapchain;
  std::vector<QueueFamily> queues;
};
struct DeviceSelection {
  std::optional<size_t> index;
  uint32_t graphics = 0, present = 0;
  std::string error;
};
DeviceSelection SelectDevice(std::span<const DeviceCandidate>,
                             std::string_view uuid);
struct SwapchainChoice {
  VkSurfaceFormatKHR format;
  VkPresentModeKHR present_mode;
  VkExtent2D extent;
  uint32_t count;
};
std::optional<SwapchainChoice>
ChooseSwapchain(const VkSurfaceCapabilitiesKHR &,
                std::span<const VkSurfaceFormatKHR>,
                std::span<const VkPresentModeKHR>, VkExtent2D, bool);
std::optional<uint32_t> ChooseMemoryType(uint32_t,
                                         std::span<const VkMemoryPropertyFlags>,
                                         VkMemoryPropertyFlags,
                                         VkMemoryPropertyFlags);
struct FlushRange {
  VkDeviceSize offset = 0, size = 0;
  bool valid = false;
};
FlushRange AlignFlushRange(VkDeviceSize, VkDeviceSize, VkDeviceSize,
                           VkDeviceSize);
} // namespace superman_returns::graphics::vulkan
