#include "swapchain.h"
#include "query.h"
namespace superman_returns::graphics::vulkan {
Swapchain::~Swapchain() { Destroy(); }
void Swapchain::Destroy() {
  if (!context_)
    return;
  auto &c = *context_;
  auto &f = c.f;
  for (auto h : framebuffers)
    if (h)
      f.vkDestroyFramebuffer(c.device, h, nullptr);
  framebuffers.clear();
  if (render_pass)
    f.vkDestroyRenderPass(c.device, render_pass, nullptr);
  render_pass = VK_NULL_HANDLE;
  for (auto h : views)
    if (h)
      f.vkDestroyImageView(c.device, h, nullptr);
  views.clear();
  images.clear();
  if (handle)
    f.vkDestroySwapchainKHR(c.device, handle, nullptr);
  handle = VK_NULL_HANDLE;
  choice = {};
}
bool Swapchain::Recreate(Context &c, VkExtent2D requested, bool vsync,
                         Error &e) {
  if (context_ && context_ != &c) {
    e = {"Swapchain", VK_ERROR_INITIALIZATION_FAILED,
         "Cannot change owning context"};
    return false;
  }
  context_ = &c;
  auto &f = c.f;
  if (!Check(f.vkDeviceWaitIdle(c.device), "Swapchain retirement", e))
    return false;
  Destroy();
  if (!requested.width || !requested.height) {
    c.Log("swapchain suspended (zero extent)");
    return true;
  }
  VkSurfaceCapabilitiesKHR caps{};
  if (!Check(f.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(c.physical, c.surface,
                                                         &caps),
             "Surface capabilities", e))
    return false;
  if (caps.currentExtent.width != UINT32_MAX &&
      (!caps.currentExtent.width || !caps.currentExtent.height)) {
    c.Log("swapchain suspended (surface zero extent)");
    return true;
  }
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> modes;
  if (!Query<VkSurfaceFormatKHR>(
          [&](auto *n, auto *p) {
            return f.vkGetPhysicalDeviceSurfaceFormatsKHR(c.physical, c.surface,
                                                          n, p);
          },
          formats, "Surface formats", e) ||
      !Query<VkPresentModeKHR>(
          [&](auto *n, auto *p) {
            return f.vkGetPhysicalDeviceSurfacePresentModesKHR(c.physical,
                                                               c.surface, n, p);
          },
          modes, "Present modes", e))
    return false;
  auto selected = ChooseSwapchain(caps, formats, modes, requested, vsync);
  if (!selected) {
    e = {"Swapchain policy", VK_ERROR_INITIALIZATION_FAILED,
         "Surface has no usable format/mode/extent"};
    return false;
  }
  choice = *selected;
  if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)) {
    e = {"Surface usage", VK_ERROR_FORMAT_NOT_SUPPORTED,
         "Surface lacks color attachment support"};
    return false;
  }
  VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  bool alpha_found = false;
  for (auto v : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                 VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                 VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                 VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
    if (caps.supportedCompositeAlpha & v) {
      alpha = v;
      alpha_found = true;
      break;
    }
  if (!alpha_found) {
    e = {"Surface alpha", VK_ERROR_INITIALIZATION_FAILED,
         "No composite alpha mode"};
    return false;
  }
  uint32_t families[] = {c.graphics_family, c.present_family};
  VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  ci.surface = c.surface;
  ci.minImageCount = choice.count;
  ci.imageFormat = choice.format.format;
  ci.imageColorSpace = choice.format.colorSpace;
  ci.imageExtent = choice.extent;
  ci.imageArrayLayers = 1;
  ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (families[0] != families[1]) {
    ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    ci.queueFamilyIndexCount = 2;
    ci.pQueueFamilyIndices = families;
  }
  ci.preTransform = caps.currentTransform;
  ci.compositeAlpha = alpha;
  ci.presentMode = choice.present_mode;
  ci.clipped = VK_TRUE;
  if (!Check(f.vkCreateSwapchainKHR(c.device, &ci, nullptr, &handle),
             "CreateSwapchain", e))
    return false;
  if (!Query<VkImage>(
          [&](auto *n, auto *p) {
            return f.vkGetSwapchainImagesKHR(c.device, handle, n, p);
          },
          images, "Swapchain images", e))
    return false;
  for (auto image : images) {
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = choice.format.format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageView view = VK_NULL_HANDLE;
    if (!Check(f.vkCreateImageView(c.device, &vi, nullptr, &view),
               "CreateImageView", e))
      return false;
    views.push_back(view);
  }
  VkAttachmentDescription attachment{};
  attachment.format = choice.format.format;
  attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
  VkSubpassDescription sub{};
  sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  sub.colorAttachmentCount = 1;
  sub.pColorAttachments = &color;
  VkSubpassDependency dep{};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = dep.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
  ri.attachmentCount = 1;
  ri.pAttachments = &attachment;
  ri.subpassCount = 1;
  ri.pSubpasses = &sub;
  ri.dependencyCount = 1;
  ri.pDependencies = &dep;
  if (!Check(f.vkCreateRenderPass(c.device, &ri, nullptr, &render_pass),
             "CreateRenderPass", e))
    return false;
  for (auto view : views) {
    VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fi.renderPass = render_pass;
    fi.attachmentCount = 1;
    fi.pAttachments = &view;
    fi.width = choice.extent.width;
    fi.height = choice.extent.height;
    fi.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    if (!Check(f.vkCreateFramebuffer(c.device, &fi, nullptr, &framebuffer),
               "CreateFramebuffer", e))
      return false;
    framebuffers.push_back(framebuffer);
  }
  ++generation;
  c.Log("swapchain generation=" + std::to_string(generation) +
        " extent=" + std::to_string(choice.extent.width) + "x" +
        std::to_string(choice.extent.height) +
        " images=" + std::to_string(images.size()) +
        " format=" + std::to_string(choice.format.format));
  return true;
}
} // namespace superman_returns::graphics::vulkan
