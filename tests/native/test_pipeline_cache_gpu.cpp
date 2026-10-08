#include "context.h"
#include "descriptor_sets.h"
#include "game_pipeline.h"
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

// Inputs are private, game-derived SPIR-V supplied by the caller, never checked in.
static superman_returns::graphics::shaders::CompiledShader Read(const char* path,bool pixel){
 using namespace superman_returns::graphics::shaders;
 CompiledShader shader;shader.stage=pixel?ShaderStage::kPixel:ShaderStage::kVertex;
 std::ifstream file(path,std::ios::binary|std::ios::ate);auto size=file.tellg();
 Require(file && size>=20 && size%4==0,"Invalid shader input");shader.words.resize(size_t(size)/4);
 file.seekg(0);file.read(reinterpret_cast<char*>(shader.words.data()),size);Require(bool(file),"Shader read failed");return shader;
}
int main(int argc,char**argv){try{
 Require(argc==3,"Usage: test_pipeline_cache_gpu vertex.spv pixel.spv");setbuf(stdout,nullptr);Gpu g;
 auto& f=g.c.f;auto device=g.c.device;DescriptorStore descriptors(g.c);Error e;
 Require(descriptors.Initialize(e),e.message.c_str());GamePipelineStore pipelines(g.c);
 Require(pipelines.Initialize(descriptors.Layouts(),{},e),e.message.c_str());
 VkAttachmentDescription attachments[2]{};attachments[0].format=VK_FORMAT_R16G16B16A16_SFLOAT;
 attachments[0].samples=VK_SAMPLE_COUNT_1_BIT;attachments[0].loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
 attachments[0].storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachments[0].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
 attachments[0].finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;attachments[1]=attachments[0];
 attachments[1].format=VK_FORMAT_D24_UNORM_S8_UINT;attachments[1].finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
 VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},depth{1,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
 VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
 subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;subpass.pDepthStencilAttachment=&depth;
 VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=2;ri.pAttachments=attachments;
 ri.subpassCount=1;ri.pSubpasses=&subpass;TargetPass pass;pass.context=&g.c;pass.color_count=1;
 pass.formats[0]=attachments[0].format;pass.depth_format=attachments[1].format;
 Vk(f.vkCreateRenderPass(device,&ri,nullptr,&pass.render_pass));
 auto vs=Read(argv[1],false),ps=Read(argv[2],true);
 superman_returns::graphics::guest::DrawPacket draw;draw.registers[0x201]=1|(1u<<16);draw.registers[0x205]=2;
 for(unsigned variant=0;variant<64;++variant){
  draw.registers[0x104]=variant%16;draw.registers[0x200]=(6u<<4)|(((variant>>4)&1)<<1)|(((variant>>5)&1)<<2);
  std::printf("Compile variant %u\n",variant);
  auto pipeline=pipelines.Acquire(draw,pass,vs,&ps,1,e);Require(bool(pipeline),e.message.c_str());
  Require(pipelines.Acquire(draw,pass,vs,&ps,1,e)==pipeline,"Pipeline object cache missed identical state");
 }
 Require(pipelines.stats().created==64,"Missing pipeline variants");
 Require(g.c.validation_errors.load()==0,"Vulkan validation reported errors");
 puts("64 pipeline variants and object cache hits PASS");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
