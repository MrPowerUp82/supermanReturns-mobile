#include "context.h"
#include "composition.h"
#include "image_state.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <vector>
using namespace superman_returns::graphics::vulkan;
static void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Vk(VkResult r){if(r!=VK_SUCCESS)throw std::runtime_error("VkResult="+std::to_string(r));}
struct Buffer {VkBuffer handle{};VkDeviceMemory memory{};void* mapped{};};
struct Gpu {
 Context c;std::vector<std::function<void()>> cleanup;VkCommandPool pool{};VkCommandBuffer command{};
 ~Gpu(){if(c.device)c.f.vkDeviceWaitIdle(c.device);for(auto i=cleanup.rbegin();i!=cleanup.rend();++i)(*i)();}
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
  Vk(c.f.vkCreateDevice(c.physical,&info,nullptr,&c.device));Loader loader;Require(loader.LoadDevice(c.f,c.device,e),e.message.c_str());c.f.vkGetDeviceQueue(c.device,c.graphics_family,0,&c.graphics_queue);
  VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=c.graphics_family;Vk(c.f.vkCreateCommandPool(c.device,&cp,nullptr,&pool));cleanup.push_back([this]{c.f.vkDestroyCommandPool(c.device,pool,nullptr);});
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;Vk(c.f.vkAllocateCommandBuffers(c.device,&ca,&command));
  std::printf("GPU %s; validation=%s\n",c.properties.deviceName,c.validation_active?"active":"unavailable");
 }
 uint32_t Memory(uint32_t bits,VkMemoryPropertyFlags flags){for(uint32_t i=0;i<c.memory.memoryTypeCount;++i)if((bits&(1u<<i))&&(c.memory.memoryTypes[i].propertyFlags&flags)==flags)return i;throw std::runtime_error("Memory type unavailable");}
 Buffer MakeBuffer(size_t bytes,VkBufferUsageFlags usage){
  Buffer b;VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=bytes;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;Vk(c.f.vkCreateBuffer(c.device,&info,nullptr,&b.handle));
  auto handle=b.handle;cleanup.push_back([this,handle]{c.f.vkDestroyBuffer(c.device,handle,nullptr);});
  VkMemoryRequirements req;c.f.vkGetBufferMemoryRequirements(c.device,b.handle,&req);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=Memory(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);Vk(c.f.vkAllocateMemory(c.device,&ai,nullptr,&b.memory));
  auto memory=b.memory;cleanup.insert(cleanup.end()-1,[this,memory]{c.f.vkFreeMemory(c.device,memory,nullptr);});Vk(c.f.vkBindBufferMemory(c.device,b.handle,b.memory,0));Vk(c.f.vkMapMemory(c.device,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped));cleanup.push_back([this,memory]{c.f.vkUnmapMemory(c.device,memory);});return b;
 }
 void Begin(){Vk(c.f.vkResetCommandPool(c.device,pool,0));VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};Vk(c.f.vkBeginCommandBuffer(command,&bi));}
 void Submit(){Vk(c.f.vkEndCommandBuffer(command));VkFence fence;VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};Vk(c.f.vkCreateFence(c.device,&fi,nullptr,&fence));VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};si.commandBufferCount=1;si.pCommandBuffers=&command;Vk(c.f.vkQueueSubmit(c.graphics_queue,1,&si,fence));auto r=c.f.vkWaitForFences(c.device,1,&fence,VK_TRUE,5000000000ull);c.f.vkDestroyFence(c.device,fence,nullptr);Vk(r);}
 VkShaderModule Shader(const char* path){std::ifstream f(path,std::ios::binary|std::ios::ate);auto size=f.tellg();Require(f && size>=20 && size%4==0,"Missing/invalid probe SPIR-V");std::vector<uint32_t> words(size_t(size)/4);f.seekg(0);f.read(reinterpret_cast<char*>(words.data()),size);VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};info.codeSize=size_t(size);info.pCode=words.data();VkShaderModule shader;Vk(c.f.vkCreateShaderModule(c.device,&info,nullptr,&shader));cleanup.push_back([this,shader]{c.f.vkDestroyShaderModule(c.device,shader,nullptr);});return shader;}
};

int main(){try{
 setbuf(stdout,nullptr);Gpu g;auto& f=g.c.f;Error e;
 ImageState state(f);ResourceStore resources(g.c);FrontbufferCompositor compositor(g.c);
 Require(compositor.Initialize(e),e.message.c_str());g.Begin();Require(resources.BeginSubmission(g.command,1,e),e.message.c_str());Require(resources.CreateDummies(e),e.message.c_str());
 auto source=resources.ResolveTexture(100,VK_FORMAT_R8G8B8A8_UNORM,{256,1,1},1,1,false,state,e);Require(bool(source),e.message.c_str());
 auto staging=g.MakeBuffer(1024,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);auto* input=static_cast<uint8_t*>(staging.mapped);
 for(uint32_t i=0;i<256;++i){input[i*4]=input[i*4+1]=input[i*4+2]=uint8_t(i);input[i*4+3]=255;}
 VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};Require(state.Transition(g.command,source->handle,range,ImageUsage::TransferDestination(),e),e.message.c_str());
 VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={256,1,1};f.vkCmdCopyBufferToImage(g.command,staging.handle,source->handle,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
 VkImage image{};VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=VK_FORMAT_R8G8B8A8_UNORM;ii.extent={256,1,1};ii.mipLevels=ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;Vk(f.vkCreateImage(g.c.device,&ii,nullptr,&image));
 VkMemoryRequirements mr;f.vkGetImageMemoryRequirements(g.c.device,image,&mr);VkMemoryAllocateInfo mi{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mi.allocationSize=mr.size;mi.memoryTypeIndex=g.Memory(mr.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);VkDeviceMemory memory;Vk(f.vkAllocateMemory(g.c.device,&mi,nullptr,&memory));Vk(f.vkBindImageMemory(g.c.device,image,memory,0));g.cleanup.push_back([&g,image,memory]{g.c.f.vkDestroyImage(g.c.device,image,nullptr);g.c.f.vkFreeMemory(g.c.device,memory,nullptr);});
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ii.format;vi.subresourceRange=range;VkImageView view;Vk(f.vkCreateImageView(g.c.device,&vi,nullptr,&view));g.cleanup.push_back([&g,view]{g.c.f.vkDestroyImageView(g.c.device,view,nullptr);});
 TargetPass pass;pass.context=&g.c;pass.extent={256,1};pass.color_count=1;pass.formats[0]=ii.format;
 VkAttachmentDescription attachment{};attachment.format=ii.format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
 VkAttachmentReference ref{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&ref;
 VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=1;ri.pAttachments=&attachment;ri.subpassCount=1;ri.pSubpasses=&subpass;Vk(f.vkCreateRenderPass(g.c.device,&ri,nullptr,&pass.render_pass));
 VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass.render_pass;fi.attachmentCount=1;fi.pAttachments=&view;fi.width=256;fi.height=fi.layers=1;Vk(f.vkCreateFramebuffer(g.c.device,&fi,nullptr,&pass.framebuffer));
 std::array<uint32_t,256> gamma{};for(uint32_t i=0;i<256;++i)gamma[i]=((i*1023/255)<<20)|(((255-i)*1023/255)<<10)|((i/2)*1023/255);
 auto draw=compositor.Prepare(g.command,pass,source,resources,state,gamma,true,256,1,e);Require(bool(draw),e.message.c_str());
 VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=pass.render_pass;begin.framebuffer=pass.framebuffer;begin.renderArea.extent=pass.extent;f.vkCmdBeginRenderPass(g.command,&begin,VK_SUBPASS_CONTENTS_INLINE);compositor.Record(g.command,*draw);f.vkCmdEndRenderPass(g.command);
 VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.oldLayout=barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=image;barrier.subresourceRange=range;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
 auto output=g.MakeBuffer(1024,VK_BUFFER_USAGE_TRANSFER_DST_BIT);f.vkCmdCopyImageToBuffer(g.command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,output.handle,1,&copy);
 VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);g.Submit();
 auto* pixels=static_cast<uint8_t*>(output.mapped);unsigned mismatches=0;
 for(uint32_t i=0;i<256;++i){int expected[]{int(i),255-int(i),int(i/2),255};for(int k=0;k<4;++k)if(std::abs(int(pixels[i*4+k])-expected[k])>1){if(mismatches++<4)printf("gamma index=%u channel=%d actual=%u expected=%d\n",i,k,pixels[i*4+k],expected[k]);}}
 Require(mismatches==0,"Compositor gamma LUT was corrupted by descriptor metadata");Require(g.c.validation_errors.load()==0,"Vulkan validation errors");
 compositor.Retire(1);resources.Retire(1);puts("256 gamma entries and 1024 compositor GPU components PASS");return 0;
 }catch(const std::exception& e){fprintf(stderr,"FAIL composition: %s\n",e.what());return 1;}}
