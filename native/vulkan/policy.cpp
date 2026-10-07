#include "policy.h"
#include <algorithm>
#include <limits>
namespace superman_returns::graphics::vulkan {
DeviceSelection SelectDevice(std::span<const DeviceCandidate> devices,
                             std::string_view uuid) {
  DeviceSelection result;
  int best = -1;
  result.error = uuid.empty()
                     ? "No Vulkan 1.1 GPU supports graphics and presentation"
                     : "Requested GPU unavailable or unsupported";
  for (size_t i = 0; i < devices.size(); ++i) {
    const auto &d = devices[i];
    if ((!uuid.empty() && d.uuid != uuid) ||
        d.api_version < VK_API_VERSION_1_1 || !d.swapchain)
      continue;
    std::optional<uint32_t> g, p;
    for (uint32_t j = 0; j < d.queues.size(); ++j) {
      if (d.queues[j].flags & VK_QUEUE_GRAPHICS_BIT)
        g = j;
      if (d.queues[j].present)
        p = j;
      if ((d.queues[j].flags & VK_QUEUE_GRAPHICS_BIT) && d.queues[j].present) {
        g = p = j;
        break;
      }
    }
    if (!g || !p)
      continue;
    int score = d.type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 3
                : d.type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2
                                                                   : 1;
    if (score > best) {
      result = {i, *g, *p, ""};
      best = score;
    }
  }
  return result;
}
std::optional<SwapchainChoice> ChooseSwapchain(
    const VkSurfaceCapabilitiesKHR &c, std::span<const VkSurfaceFormatKHR> f,
    std::span<const VkPresentModeKHR> m, VkExtent2D requested, bool vsync) {
  if (!requested.width || !requested.height || f.empty() || m.empty())
    return {};
  auto format = f[0];
  if (format.format == VK_FORMAT_UNDEFINED)
    format = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
  else
    for (auto preferred : {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM})
      for (auto v : f)
        if (v.format == preferred &&
            v.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
          format = v;
  VkExtent2D extent = c.currentExtent;
  if (extent.width == UINT32_MAX)
    extent = {std::clamp(requested.width, c.minImageExtent.width,
                         c.maxImageExtent.width),
              std::clamp(requested.height, c.minImageExtent.height,
                         c.maxImageExtent.height)};
  if (!extent.width || !extent.height)
    return {};
  auto mode = VK_PRESENT_MODE_FIFO_KHR;
  if (!vsync)
    for (auto preferred :
         {VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR})
      if (std::find(m.begin(), m.end(), preferred) != m.end())
        mode = preferred;
  uint32_t count = c.minImageCount + 1;
  if (c.maxImageCount)
    count = std::min(count, c.maxImageCount);
  return SwapchainChoice{format, mode, extent, count};
}
std::optional<uint32_t>
ChooseMemoryType(uint32_t mask, std::span<const VkMemoryPropertyFlags> flags,
                 VkMemoryPropertyFlags required,
                 VkMemoryPropertyFlags preferred) {
  std::optional<uint32_t> fallback;
  for (uint32_t i = 0; i < flags.size() && i < 32; ++i)
    if ((mask & (1u << i)) && (flags[i] & required) == required) {
      if ((flags[i] & preferred) == preferred)
        return i;
      if (!fallback)
        fallback = i;
    }
  return fallback;
}
FlushRange AlignFlushRange(VkDeviceSize offset, VkDeviceSize size,
                           VkDeviceSize allocation, VkDeviceSize atom) {
  if (!atom || !size || offset > allocation || size > allocation - offset)
    return {};
  auto start = offset - offset % atom;
  auto end = offset + size;
  auto extra = (atom - end % atom) % atom;
  end += std::min(extra, allocation - end);
  return {start, end - start, true};
}
} // namespace superman_returns::graphics::vulkan
