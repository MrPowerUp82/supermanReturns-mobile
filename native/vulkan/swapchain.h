#pragma once
#include "context.h"
namespace superman_returns::graphics::vulkan {
class Swapchain {
public:
  Swapchain() = default;
  ~Swapchain();
  Swapchain(const Swapchain &) = delete;
  Swapchain &operator=(const Swapchain &) = delete;
  bool Recreate(Context &, VkExtent2D, bool, Error &);
  void Destroy();
  VkSwapchainKHR handle = VK_NULL_HANDLE;
  SwapchainChoice choice{};
  VkRenderPass render_pass = VK_NULL_HANDLE;
  std::vector<VkImage> images;
  std::vector<VkImageView> views;
  std::vector<VkFramebuffer> framebuffers;
  uint64_t generation = 0;

private:
  Context *context_ = nullptr;
};
} // namespace superman_returns::graphics::vulkan
