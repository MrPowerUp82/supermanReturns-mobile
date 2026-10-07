#include "frame_loop.h"
#include <exception>
namespace superman_returns::graphics::vulkan {
FrameLoop::~FrameLoop() {
  if (context_) {
    Error e;
    Retire(*context_, e);
  }
  Destroy();
}
void FrameLoop::Destroy() {
  if (!context_)
    return;
  auto &c = *context_;
  for (auto &slot : slots_) {
    if (slot.pool)
      c.f.vkDestroyCommandPool(c.device, slot.pool, nullptr);
    if (slot.acquire)
      c.f.vkDestroySemaphore(c.device, slot.acquire, nullptr);
    if (slot.fence)
      c.f.vkDestroyFence(c.device, slot.fence, nullptr);
    slot = {};
  }
  for (auto h : finished_)
    if (h)
      c.f.vkDestroySemaphore(c.device, h, nullptr);
  finished_.clear();
  images_in_flight_.clear();
  context_ = nullptr;
}
bool FrameLoop::Retire(Context &c, Error &e) {
  auto ok = Check(c.f.vkDeviceWaitIdle(c.device), "Frame retirement", e);
  if(ok) for(auto& slot:slots_) if(slot.serial && slot.retire) {slot.retire(slot.serial);slot.serial=0;slot.retire={};}
  Destroy();
  return ok;
}
bool FrameLoop::Initialize(Context &c, Swapchain &s, Error &e) {
  if (context_ && !Retire(*context_, e))
    return false;
  context_ = &c;
  failed_ = false;
  cursor_ = 0;
  for (auto &slot : slots_) {
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.queueFamilyIndex = c.graphics_family;
    if (!Check(c.f.vkCreateCommandPool(c.device, &pi, nullptr, &slot.pool),
               "CreateCommandPool", e))
      return false;
    VkCommandBufferAllocateInfo bi{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    bi.commandPool = slot.pool;
    bi.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bi.commandBufferCount = 1;
    if (!Check(c.f.vkAllocateCommandBuffers(c.device, &bi, &slot.command),
               "AllocateCommandBuffers", e))
      return false;
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (!Check(c.f.vkCreateSemaphore(c.device, &si, nullptr, &slot.acquire),
               "Acquire semaphore", e))
      return false;
    VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (!Check(c.f.vkCreateFence(c.device, &fi, nullptr, &slot.fence),
               "CreateFence", e))
      return false;
  }
  finished_.resize(s.images.size(), VK_NULL_HANDLE);
  images_in_flight_.resize(s.images.size(), VK_NULL_HANDLE);
  for (auto &h : finished_) {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (!Check(c.f.vkCreateSemaphore(c.device, &si, nullptr, &h),
               "Present semaphore", e))
      return false;
  }
  return true;
}
FrameOutcome
FrameLoop::Draw(Context &c, Swapchain &s,
                const std::function<void(VkCommandBuffer, uint32_t)> &record,
                Error &e) {
  return DrawGame(c,s,FrameWork{{},record,{}},e);
}
FrameOutcome FrameLoop::DrawGame(Context& c,Swapchain& s,const FrameWork& work,Error& e) {
  if (failed_)
    return FrameOutcome::kFailed;
  if (!s.handle || !s.choice.extent.width || !s.choice.extent.height)
    return FrameOutcome::kSuspended;
  auto fail = [&](VkResult r, const char *op) {
    failed_ = true;
    Check(r, op, e);
    return FrameOutcome::kFailed;
  };
  auto &slot = slots_[cursor_];
  auto &f = c.f;
  // The queue mutex is shared with the guest frame thread. Waiting for a fence or for a swapchain
  // image (a vblank under FIFO) must not hold it, or that thread cannot submit meanwhile: it is
  // taken only for work that touches the queue or shared resources.
  std::unique_lock<std::mutex> queue_lock;
  if (work.queue_mutex) queue_lock = std::unique_lock<std::mutex>(*work.queue_mutex, std::defer_lock);
  auto lock_queue = [&] { if (queue_lock.mutex() && !queue_lock.owns_lock()) queue_lock.lock(); };
  VkResult r = f.vkWaitForFences(c.device, 1, &slot.fence, VK_TRUE, 100000000);
  if (r == VK_TIMEOUT)
    return FrameOutcome::kSuspended;
  if (r != VK_SUCCESS)
    return fail(r, "WaitForFences");
  if(slot.serial && slot.retire) {
    lock_queue();
    slot.retire(slot.serial);slot.serial=0;slot.retire={};
    if (queue_lock.owns_lock()) queue_lock.unlock();
  }
  uint32_t index = 0;
  r = f.vkAcquireNextImageKHR(c.device, s.handle, 100000000, slot.acquire,
                              VK_NULL_HANDLE, &index);
  if (r == VK_ERROR_OUT_OF_DATE_KHR)
    return FrameOutcome::kRecreate;
  if (r == VK_TIMEOUT || r == VK_NOT_READY)
    return FrameOutcome::kSuspended;
  bool suboptimal = r == VK_SUBOPTIMAL_KHR;
  if (r != VK_SUCCESS && !suboptimal)
    return fail(r, "AcquireNextImage");
  if (index >= finished_.size() || index >= s.framebuffers.size())
    return fail(VK_ERROR_INITIALIZATION_FAILED, "Acquired image index");
  if (images_in_flight_[index]) {
    r = f.vkWaitForFences(c.device, 1, &images_in_flight_[index], VK_TRUE,
                          UINT64_MAX);
    if (r != VK_SUCCESS)
      return fail(r, "Image fence");
  }
  if ((r = f.vkResetCommandPool(c.device, slot.pool, 0)) != VK_SUCCESS)
    return fail(r, "ResetCommandPool");
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if ((r = f.vkBeginCommandBuffer(slot.command, &bi)) != VK_SUCCESS)
    return fail(r, "BeginCommandBuffer");
  lock_queue();  // from here on: prepare, record, submit and present
  const uint64_t serial=++next_serial_;
  bool prepared=true;Error preparation_error;
  if(work.prepare) {
    try {prepared=work.prepare(slot.command,serial,preparation_error);}
    catch(const std::exception& ex) {prepared=false;preparation_error={"Game preparation",VK_ERROR_INITIALIZATION_FAILED,ex.what()};}
    catch(...) {prepared=false;preparation_error={"Game preparation",VK_ERROR_INITIALIZATION_FAILED,"Unknown frame callback exception"};}
    if(!prepared && preparation_error.message.empty()) preparation_error={"Game preparation",VK_ERROR_INITIALIZATION_FAILED,"Frame callback failed without a diagnostic"};
  }
  VkClearValue clear{};
  clear.color = {{0.015f, 0.025f, 0.07f, 1.0f}};
  VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
  pass.renderPass = s.render_pass;
  pass.framebuffer = s.framebuffers[index];
  pass.renderArea.extent = s.choice.extent;
  pass.clearValueCount = 1;
  pass.pClearValues = &clear;
  f.vkCmdBeginRenderPass(slot.command, &pass, VK_SUBPASS_CONTENTS_INLINE);
  if(prepared && work.paint) {
    try {work.paint(slot.command,index);}
    catch(const std::exception& ex) {prepared=false;preparation_error={"Game painting",VK_ERROR_INITIALIZATION_FAILED,ex.what()};}
    catch(...) {prepared=false;preparation_error={"Game painting",VK_ERROR_INITIALIZATION_FAILED,"Unknown frame callback exception"};}
  }
  f.vkCmdEndRenderPass(slot.command);
  if ((r = f.vkEndCommandBuffer(slot.command)) != VK_SUCCESS)
    return fail(r, "EndCommandBuffer");
  if ((r = f.vkResetFences(c.device, 1, &slot.fence)) != VK_SUCCESS)
    return fail(r, "ResetFences");
  VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit.waitSemaphoreCount = 1;
  submit.pWaitSemaphores = &slot.acquire;
  submit.pWaitDstStageMask = &stage;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &slot.command;
  submit.signalSemaphoreCount = 1;
  submit.pSignalSemaphores = &finished_[index];
  if ((r = f.vkQueueSubmit(c.graphics_queue, 1, &submit, slot.fence)) !=
      VK_SUCCESS)
    return fail(r, "QueueSubmit");
  images_in_flight_[index] = slot.fence;
  slot.serial=serial;slot.retire=work.retire;
  VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present.waitSemaphoreCount = 1;
  present.pWaitSemaphores = &finished_[index];
  present.swapchainCount = 1;
  present.pSwapchains = &s.handle;
  present.pImageIndices = &index;
  r = f.vkQueuePresentKHR(c.present_queue, &present);
  cursor_ = (cursor_ + 1) % slots_.size();
  if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR)
    ++presented;
  if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR && r != VK_ERROR_OUT_OF_DATE_KHR)
    return fail(r, "QueuePresent");
  if(!prepared) {failed_=true;e=std::move(preparation_error);return FrameOutcome::kFailed;}
  if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || suboptimal)
    return FrameOutcome::kRecreate;
  return FrameOutcome::kPresented;
}
} // namespace superman_returns::graphics::vulkan
