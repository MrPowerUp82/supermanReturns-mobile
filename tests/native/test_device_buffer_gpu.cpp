#include "context.h"
#include "descriptor_sets.h"
#include "resources.h"
#include <cstring>
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <vector>
using namespace superman_returns::graphics::vulkan;
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Vk(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("VkResult="+std::to_string(r));}
struct Gpu {
 Context c;
 Gpu(){
  Error e;const char* extensions[]{VK_KHR_SURFACE_EXTENSION_NAME};Require(c.CreateInstance(extensions,true,e),(e.operation+": "+e.message).c_str());
  uint32_t n=0;Vk(c.f.vkEnumeratePhysicalDevices(c.instance,&n,nullptr));Require(n>0,"No physical device");
  std::vector<VkPhysicalDevice> devices(n);Vk(c.f.vkEnumeratePhysicalDevices(c.instance,&n,devices.data()));c.physical=devices[0];
  VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};c.f.vkGetPhysicalDeviceProperties2(c.physical,&props);c.properties=props.properties;
  c.f.vkGetPhysicalDeviceMemoryProperties(c.physical,&c.memory);
  c.f.vkGetPhysicalDeviceQueueFamilyProperties(c.physical,&n,nullptr);std::vector<VkQueueFamilyProperties> queues(n);c.f.vkGetPhysicalDeviceQueueFamilyProperties(c.physical,&n,queues.data());
  auto it=std::find_if(queues.begin(),queues.end(),[](auto q){return q.queueCount && (q.queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT);});Require(it!=queues.end(),"Graphics/compute queue unavailable");c.graphics_family=uint32_t(it-queues.begin());
  float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=c.graphics_family;queue.queueCount=1;queue.pQueuePriorities=&priority;
  const char* extension="VK_KHR_swapchain";VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};info.queueCreateInfoCount=1;info.pQueueCreateInfos=&queue;info.enabledExtensionCount=1;info.ppEnabledExtensionNames=&extension;
  VkPhysicalDeviceFeatures features{};features.robustBufferAccess=features.independentBlend=features.shaderSampledImageArrayDynamicIndexing=features.shaderStorageBufferArrayDynamicIndexing=features.shaderClipDistance=features.shaderCullDistance=1;info.pEnabledFeatures=&features;Vk(c.f.vkCreateDevice(c.physical,&info,nullptr,&c.device));c.enabled_features=features;Loader loader;Require(loader.LoadDevice(c.f,c.device,e),e.message.c_str());c.f.vkGetDeviceQueue(c.device,c.graphics_family,0,&c.graphics_queue);
  std::printf("GPU %s; validation=%s\n",c.properties.deviceName,c.validation_active?"active":"unavailable");
 }

};


static PFN_vkAllocateMemory allocate_real;
static uint32_t allocation_calls;
static VkResult VKAPI_CALL LimitedAllocate(VkDevice d,const VkMemoryAllocateInfo* a,const VkAllocationCallbacks* cb,VkDeviceMemory* out) {
 if(allocation_calls>=32)return VK_ERROR_TOO_MANY_OBJECTS;
 auto result=allocate_real(d,a,cb,out);if(result==VK_SUCCESS)++allocation_calls;return result;
}
int main(){try{
 setbuf(stdout,nullptr);Gpu g;auto& f=g.c.f;Error e;
 allocate_real=f.vkAllocateMemory;f.vkAllocateMemory=LimitedAllocate;
 VkCommandPool pool{};VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=g.c.graphics_family;
 Vk(f.vkCreateCommandPool(g.c.device,&pi,nullptr,&pool));
 {
  ResourceStore store(g.c);VkCommandBuffer cmd{};VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
  ai.commandPool=pool;ai.commandBufferCount=1;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;Vk(f.vkAllocateCommandBuffers(g.c.device,&ai,&cmd));
  VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Vk(f.vkBeginCommandBuffer(cmd,&bi));Require(store.BeginSubmission(cmd,1,e),e.message.c_str());
  constexpr uint32_t count=5000;
  for(uint32_t id=1;id<=count;++id) Require(store.UploadBuffer(id,std::as_bytes(std::span(&id,1)),id,e),e.message.c_str());
  auto old=store.Buffer(1,e);uint32_t changed=123456;
  Require(store.UploadDynamicBuffer(1,std::as_bytes(std::span(&changed,1)),6000,e),e.message.c_str());
  auto readback=store.ReadbackBuffer((count+1)*4,e);Require(bool(readback),e.message.c_str());
  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  f.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&barrier,0,nullptr,0,nullptr);
  for(uint32_t id=1;id<=count;++id){auto b=id==1?old:store.Buffer(id,e);Require(bool(b),e.message.c_str());VkBufferCopy copy{b->offset,(id-1)*4u,4};f.vkCmdCopyBuffer(cmd,b->handle,readback->handle,1,&copy);}
  auto latest=store.Buffer(1,e);VkBufferCopy copy{latest->offset,count*4u,4};f.vkCmdCopyBuffer(cmd,latest->handle,readback->handle,1,&copy);
  barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;f.vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);
  Vk(f.vkEndCommandBuffer(cmd));VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&cmd;
  Vk(f.vkQueueSubmit(g.c.graphics_queue,1,&si,VK_NULL_HANDLE));Vk(f.vkDeviceWaitIdle(g.c.device));
  std::vector<uint8_t> data;Require(store.Readback(readback,data,e),e.message.c_str());
  for(uint32_t i=0;i<=count;++i){uint32_t actual;std::memcpy(&actual,data.data()+i*4,4);Require(actual==(i==count?changed:i+1),"GPU readback corrupted a buffer version");}
  Require(allocation_calls<=4,"Small buffers still use individual allocations");store.Retire(1);
 }
 f.vkDestroyCommandPool(g.c.device,pool,nullptr);
 Require(g.c.validation_errors.load()==0,"Vulkan validation reported errors");
 printf("5001 GPU buffer values and retained versions PASS; allocations=%u (limit=32)\n",allocation_calls);return 0;
 }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
