#include "context.h"
#include "descriptor_sets.h"
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
struct Query {float u,v,lod;uint32_t word;};static_assert(sizeof(Query)==16);
using Color=std::array<double,4>;
static const std::array<std::vector<float>,3> pixels{{
 {1.0001220703125f,1.000244140625f,-2.5f,4.25f,8.75f, -3,-2,-1,0,1, 2,3,4,5,6},
 {16.125f,32.5f},{64.25f}}};
static const uint32_t widths[]{5,2,1},heights[]{3,1,1};
// Independent integer-addressed reference; no production sampler planning/helpers.
static int Address(int i,int n,VkSamplerAddressMode mode){
 if(mode==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)return std::clamp(i,0,n-1);
 if(mode==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER)return i<0||i>=n?-1:i;
 if(mode==VK_SAMPLER_ADDRESS_MODE_REPEAT)return (i%n+n)%n;
 int t=(i%(2*n)+2*n)%(2*n);return t<n?t:2*n-1-t;
}
static uint32_t componentCount=1;
static Color Texel(uint32_t level,int x,int y,VkSamplerAddressMode mode){x=Address(x,int(widths[level]),mode);y=Address(y,int(heights[level]),mode);if(x<0||y<0)return Color{0,0,0,componentCount==4?0.:1.};double r=pixels[level][size_t(y)*widths[level]+size_t(x)];return Color{r,componentCount>=2?3*r+1:0,componentCount==4?-r*.5:0,componentCount==4?r/8+.25:1};}
static Color Level(uint32_t level,double u,double v,bool linear,VkSamplerAddressMode mode){
 if(!linear)return Texel(level,int(std::floor(u*widths[level])),int(std::floor(v*heights[level])),mode);
 double x=u*widths[level]-.5,y=v*heights[level]-.5;int ix=int(std::floor(x)),iy=int(std::floor(y));double fx=x-ix,fy=y-iy;Color out{};
 for(int j=0;j<2;++j)for(int i=0;i<2;++i){auto c=Texel(level,ix+i,iy+j,mode);double weight=(i?fx:1-fx)*(j?fy:1-fy);for(int k=0;k<4;++k)out[k]+=weight*c[k];}return out;
}
static Color Reference(Query q,VkSamplerAddressMode mode){
 bool linear=(q.word&(q.lod>0?128u:64u))!=0;uint32_t mip=(q.word>>8)&3;
 if(mip==2)return Level(0,q.u,q.v,linear,mode);
 double lod=std::clamp(double(q.lod),0.,2.);if(mip==0)return Level(uint32_t(std::floor(lod+.5)),q.u,q.v,linear,mode);
 uint32_t lo=uint32_t(std::floor(lod)),hi=std::min(lo+1,2u);auto a=Level(lo,q.u,q.v,linear,mode),b=Level(hi,q.u,q.v,linear,mode);for(int k=0;k<4;++k)a[k]+= (b[k]-a[k])*(lod-lo);return a;
}
static void FragmentNumeric(Gpu& g,VkPipelineLayout pipeline_layout,VkDescriptorSet set,const char* probe){
 auto& f=g.c.f;auto device=g.c.device;
 VkImage target;VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=VK_FORMAT_R32G32B32A32_SFLOAT;ii.extent={8,8,1};ii.mipLevels=1;ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;Vk(f.vkCreateImage(device,&ii,nullptr,&target));
 VkMemoryRequirements req;f.vkGetImageMemoryRequirements(device,target,&req);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=g.Memory(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);VkDeviceMemory memory;Vk(f.vkAllocateMemory(device,&ai,nullptr,&memory));g.cleanup.push_back([&g,memory]{g.c.f.vkFreeMemory(g.c.device,memory,nullptr);});g.cleanup.push_back([&g,target]{g.c.f.vkDestroyImage(g.c.device,target,nullptr);});Vk(f.vkBindImageMemory(device,target,memory,0));
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=target;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=ii.format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};VkImageView view;Vk(f.vkCreateImageView(device,&vi,nullptr,&view));g.cleanup.push_back([&g,view]{g.c.f.vkDestroyImageView(g.c.device,view,nullptr);});
 VkAttachmentDescription attachment{};attachment.format=ii.format;attachment.samples=VK_SAMPLE_COUNT_1_BIT;attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.storeOp=VK_ATTACHMENT_STORE_OP_STORE;attachment.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;attachment.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;attachment.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;attachment.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
 VkAttachmentReference color{0,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};VkSubpassDescription subpass{};subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;subpass.colorAttachmentCount=1;subpass.pColorAttachments=&color;
 VkSubpassDependency dependency{};dependency.srcSubpass=0;dependency.dstSubpass=VK_SUBPASS_EXTERNAL;dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;dependency.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;dependency.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
 VkRenderPassCreateInfo ri{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};ri.attachmentCount=1;ri.pAttachments=&attachment;ri.subpassCount=1;ri.pSubpasses=&subpass;ri.dependencyCount=1;ri.pDependencies=&dependency;VkRenderPass pass;Vk(f.vkCreateRenderPass(device,&ri,nullptr,&pass));g.cleanup.push_back([&g,pass]{g.c.f.vkDestroyRenderPass(g.c.device,pass,nullptr);});
 VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};fi.renderPass=pass;fi.attachmentCount=1;fi.pAttachments=&view;fi.width=fi.height=8;fi.layers=1;VkFramebuffer framebuffer;Vk(f.vkCreateFramebuffer(device,&fi,nullptr,&framebuffer));g.cleanup.push_back([&g,framebuffer]{g.c.f.vkDestroyFramebuffer(g.c.device,framebuffer,nullptr);});
 auto base=std::filesystem::path(probe).replace_extension().string();VkPipelineShaderStageCreateInfo stages[2]{};for(auto& stage:stages)stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
 stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;stages[0].module=g.Shader((base+".vs.spv").c_str());stages[0].pName="VSMain";stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;stages[1].module=g.Shader((base+".ps.spv").c_str());stages[1].pName="PSMain";
 VkPipelineVertexInputStateCreateInfo vertex{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
 VkViewport viewport{0,0,8,8,0,1};VkRect2D scissor{{0,0},{8,8}};VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};vp.viewportCount=vp.scissorCount=1;vp.pViewports=&viewport;vp.pScissors=&scissor;
 VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};raster.polygonMode=VK_POLYGON_MODE_FILL;raster.cullMode=VK_CULL_MODE_NONE;raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;raster.lineWidth=1;
 VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};ms.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;VkPipelineColorBlendAttachmentState blend{};blend.colorWriteMask=15;VkPipelineColorBlendStateCreateInfo bs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};bs.attachmentCount=1;bs.pAttachments=&blend;
 VkGraphicsPipelineCreateInfo pi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};pi.stageCount=2;pi.pStages=stages;pi.pVertexInputState=&vertex;pi.pInputAssemblyState=&assembly;pi.pViewportState=&vp;pi.pRasterizationState=&raster;pi.pMultisampleState=&ms;pi.pColorBlendState=&bs;pi.layout=pipeline_layout;pi.renderPass=pass;VkPipeline pipeline;Vk(f.vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&pipeline));g.cleanup.push_back([&g,pipeline]{g.c.f.vkDestroyPipeline(g.c.device,pipeline,nullptr);});
 auto readback=g.MakeBuffer(8*8*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT);unsigned checked=0;
 for(float scale:{.2f,2.f,8.f})for(uint32_t mip:{0u,1u,2u})for(uint32_t filter:{64u,128u}){
  uint32_t word=32u|filter|(mip<<8);
  struct Params{float scale;uint32_t word;float pad[2];} params{scale,word,{0,0}};
  auto render=[&](){
  g.Begin();VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};begin.renderPass=pass;begin.framebuffer=framebuffer;begin.renderArea=scissor;f.vkCmdBeginRenderPass(g.command,&begin,VK_SUBPASS_CONTENTS_INLINE);f.vkCmdBindPipeline(g.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);f.vkCmdBindDescriptorSets(g.command,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&set,0,nullptr);f.vkCmdPushConstants(g.command,pipeline_layout,VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(params),&params);f.vkCmdDraw(g.command,3,1,0,0);f.vkCmdEndRenderPass(g.command);
  VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={8,8,1};f.vkCmdCopyImageToBuffer(g.command,target,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.handle,1,&copy);VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);g.Submit();
  };render();
  std::array<float,256> filtered;std::memcpy(filtered.data(),readback.mapped,sizeof(filtered));
  params.pad[0]=1;render();
  auto lodValues=static_cast<float*>(readback.mapped);

  auto values=filtered.data();
  for(uint32_t y=0;y<8;++y)for(uint32_t x=0;x<8;++x){float lod=lodValues[(y*8+x)*4];
   Require(std::isfinite(lod) && std::abs(lod-std::log2(scale*5.f/8.f))<=std::ldexp(1.f,-int(g.c.properties.limits.mipmapPrecisionBits)),"Implicit LOD exceeds device mip precision");
   Query q{(float(x)+.5f)/8*scale,(float(y)+.5f)/8*scale,lod,word};auto want=Reference(q,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);for(uint32_t k=0;k<4;++k){float got=values[(y*8+x)*4+k];if(!std::isfinite(got)||std::abs(double(got)-want[k])>2e-6*std::max(1.,std::abs(want[k]))){std::fprintf(stderr,"fragment scale=%g word=%u xy=%u,%u component=%u got=%.10g expected=%.10g\n",scale,word,x,y,k,got,want[k]);throw std::runtime_error("fragment LOD/readback mismatch");}++checked;}}
 }
 std::printf("PASS fragment implicit LOD and mixed min/mag: %u component checks\n",checked);
}
static void Numeric(Gpu& g,const char* shader_path,VkFormat format,uint32_t components){
 componentCount=components;std::printf("Testing float32 format=%d components=%u\n",int(format),components);
 auto& f=g.c.f;auto device=g.c.device;
 VkImage image;VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};ii.imageType=VK_IMAGE_TYPE_2D;ii.format=format;ii.extent={5,3,1};ii.mipLevels=3;ii.arrayLayers=1;ii.samples=VK_SAMPLE_COUNT_1_BIT;ii.tiling=VK_IMAGE_TILING_OPTIMAL;ii.usage=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;ii.sharingMode=VK_SHARING_MODE_EXCLUSIVE;Vk(f.vkCreateImage(device,&ii,nullptr,&image));
 VkMemoryRequirements req;f.vkGetImageMemoryRequirements(device,image,&req);VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};ai.allocationSize=req.size;ai.memoryTypeIndex=g.Memory(req.memoryTypeBits,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);VkDeviceMemory memory;Vk(f.vkAllocateMemory(device,&ai,nullptr,&memory));g.cleanup.push_back([&g,memory]{g.c.f.vkFreeMemory(g.c.device,memory,nullptr);});g.cleanup.push_back([&g,image]{g.c.f.vkDestroyImage(g.c.device,image,nullptr);});Vk(f.vkBindImageMemory(device,image,memory,0));
 VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};vi.image=image;vi.viewType=VK_IMAGE_VIEW_TYPE_2D;vi.format=format;vi.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,3,0,1};VkImageView view;Vk(f.vkCreateImageView(device,&vi,nullptr,&view));g.cleanup.push_back([&g,view]{g.c.f.vkDestroyImageView(g.c.device,view,nullptr);});
 auto staging=g.MakeBuffer(18*sizeof(float)*components,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);size_t at=0;std::array<VkBufferImageCopy,3> copies{};
 for(uint32_t level=0;level<3;++level){auto* dst=reinterpret_cast<float*>(static_cast<char*>(staging.mapped)+at);for(uint32_t y=0;y<heights[level];++y)for(uint32_t x=0;x<widths[level];++x){auto color=Texel(level,int(x),int(y),VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);for(uint32_t k=0;k<components;++k)*dst++=float(color[k]);}copies[level].bufferOffset=at;copies[level].imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,level,0,1};copies[level].imageExtent={widths[level],heights[level],1};at+=pixels[level].size()*4*components;}
 g.Begin();VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;barrier.image=image;barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,3,0,1};barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);f.vkCmdCopyBufferToImage(g.command,staging.handle,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,3,copies.data());barrier.oldLayout=barrier.newLayout;barrier.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,0,nullptr,0,nullptr,1,&barrier);g.Submit();
 std::vector<Query> queries;
 for(uint32_t mip=0;mip<3;++mip)for(uint32_t filters=0;filters<4;++filters)for(float lod:{-1.f,0.f,.25f,.75f,1.25f,4.f})for(auto uv:{std::array{.1f,1.f/6},std::array{.3f,1.f/6},std::array{.2f,1.f/6},std::array{.15f,.25f},std::array{-.1f,-.25f},std::array{1.1f,1.25f}})queries.push_back({uv[0],uv[1],lod,32u|((filters&1)?64u:0u)|((filters&2)?128u:0u)|(mip<<8)});
 auto inputs=g.MakeBuffer(queries.size()*sizeof(Query),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT),output=g.MakeBuffer(queries.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);std::memcpy(inputs.mapped,queries.data(),queries.size()*sizeof(Query));
 std::array<VkDescriptorSetLayoutBinding,4> bindings{{{0,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{1,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},{3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}}};
 bindings[0].stageFlags|=VK_SHADER_STAGE_FRAGMENT_BIT;bindings[1].stageFlags|=VK_SHADER_STAGE_FRAGMENT_BIT;
 VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=4;li.pBindings=bindings.data();VkDescriptorSetLayout layout;Vk(f.vkCreateDescriptorSetLayout(device,&li,nullptr,&layout));g.cleanup.push_back([&g,layout]{g.c.f.vkDestroyDescriptorSetLayout(g.c.device,layout,nullptr);});
 VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&layout;VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT,0,16};pli.pushConstantRangeCount=1;pli.pPushConstantRanges=&push;VkPipelineLayout pipeline_layout;Vk(f.vkCreatePipelineLayout(device,&pli,nullptr,&pipeline_layout));g.cleanup.push_back([&g,pipeline_layout]{g.c.f.vkDestroyPipelineLayout(g.c.device,pipeline_layout,nullptr);});
 VkComputePipelineCreateInfo pi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pi.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};pi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pi.stage.module=g.Shader(shader_path);pi.stage.pName="CSMain";pi.layout=pipeline_layout;VkPipeline pipeline;Vk(f.vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&pi,nullptr,&pipeline));g.cleanup.push_back([&g,pipeline]{g.c.f.vkDestroyPipeline(g.c.device,pipeline,nullptr);});
 VkDescriptorPoolSize sizes[]{{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1},{VK_DESCRIPTOR_TYPE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dpi.maxSets=1;dpi.poolSizeCount=3;dpi.pPoolSizes=sizes;VkDescriptorPool descriptor_pool;Vk(f.vkCreateDescriptorPool(device,&dpi,nullptr,&descriptor_pool));g.cleanup.push_back([&g,descriptor_pool]{g.c.f.vkDestroyDescriptorPool(g.c.device,descriptor_pool,nullptr);});
 VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};dai.descriptorPool=descriptor_pool;dai.descriptorSetCount=1;dai.pSetLayouts=&layout;VkDescriptorSet set;Vk(f.vkAllocateDescriptorSets(device,&dai,&set));
 auto* results=static_cast<float*>(output.mapped);unsigned checked=0;
 for(auto mode:{VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,VK_SAMPLER_ADDRESS_MODE_REPEAT,VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER}){
  VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};si.magFilter=si.minFilter=VK_FILTER_NEAREST;si.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;si.addressModeU=si.addressModeV=si.addressModeW=mode;si.maxLod=VK_LOD_CLAMP_NONE;si.borderColor=VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;VkSampler sampler;Vk(f.vkCreateSampler(device,&si,nullptr,&sampler));g.cleanup.push_back([&g,sampler]{g.c.f.vkDestroySampler(g.c.device,sampler,nullptr);});
  VkDescriptorImageInfo images[]{{VK_NULL_HANDLE,view,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},{sampler,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_UNDEFINED}};VkDescriptorBufferInfo buffers[]{{inputs.handle,0,queries.size()*sizeof(Query)},{output.handle,0,queries.size()*16}};std::array<VkWriteDescriptorSet,4> writes{};
 for(uint32_t i=0;i<4;++i){writes[i]={VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};writes[i].dstSet=set;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=bindings[i].descriptorType;if(i<2)writes[i].pImageInfo=&images[i];else writes[i].pBufferInfo=&buffers[i-2];}f.vkUpdateDescriptorSets(device,4,writes.data(),0,nullptr);
  std::fill(results,results+queries.size()*4,-999.f);g.Begin();f.vkCmdBindPipeline(g.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);f.vkCmdBindDescriptorSets(g.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,nullptr);f.vkCmdDispatch(g.command,uint32_t(queries.size()),1,1);VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER};mb.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;mb.dstAccessMask=VK_ACCESS_HOST_READ_BIT;f.vkCmdPipelineBarrier(g.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&mb,0,nullptr,0,nullptr);g.Submit();
  for(size_t i=0;i<queries.size();++i){auto expected=Reference(queries[i],mode);for(size_t k=0;k<4;++k){double want=expected[k],got=results[i*4+k];if(!std::isfinite(got)||std::abs(got-want)>2e-6*std::max(1.,std::abs(want))){std::fprintf(stderr,"mode=%d query=%zu uv=%g,%g lod=%g word=%u component=%zu got=%.10g expected=%.10g\n",int(mode),i,queries[i].u,queries[i].v,queries[i].lod,queries[i].word,k,got,want);throw std::runtime_error("float32 filter/readback mismatch");}++checked;}}
  Require(results[0]!=results[4],"Distinct float32 texels collapsed");
  if(mode==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)FragmentNumeric(g,pipeline_layout,set,shader_path);
 }
 Require(g.c.validation_errors==0,"Vulkan validation errors");std::printf("PASS float32 precision, centers, fractional taps, edges, odd mips and mixed min/mag: %u component checks\n",checked);
}

static void DescriptorCache(Gpu& g){
 Error e;ResourceStore resources(g.c);DescriptorStore descriptors(g.c);
 auto ok=[&](bool value){Require(value,(e.operation+": "+e.message).c_str());};
 ok(descriptors.Initialize(e));g.Begin();ok(resources.BeginSubmission(g.command,1,e));ok(resources.CreateDummies(e));
 namespace guest=superman_returns::graphics::guest;
 guest::LinearTexture texture;texture.width=texture.height=1;texture.format=guest::LinearFormat::kR32Float;texture.levels.push_back({1,1,4,1,0});texture.data.resize(4);float value=1.0001220703125f;std::memcpy(texture.data.data(),&value,4);ok(resources.UploadTexture(123,texture,1,e));
 TransientSlice constants;ok(resources.MapTransient(sizeof(guest::ConstantSnapshot),constants,e));guest::DrawPacket packet{};DrawBindings bindings;ok(BuildBindings(packet,constants.data,bindings,e));bindings.textures[0][0]=123;bindings.texture_indices[0]=0;
 std::array<std::array<uint32_t,6>,32> fetch{};fetch[0]={2u|(2u<<10)|(2u<<13),0,0,(1u<<19)|(1u<<21)|(2u<<23),0,1u<<9};
 auto prepare=[&](){DescriptorDraw draw;ok(descriptors.Prepare(bindings,fetch,resources,1,constants,draw,e));uint32_t word;std::memcpy(&word,constants.data+kSharedConstantsOffset+128,4);Require(word==bindings.sampler_indices[0],"Cache metadata differs from constants");return draw;};
 auto a=prepare();Require(bindings.sampler_indices[0]==736,"Manual metadata missing");auto aHit=prepare();Require(a.sets[1]==aHit.sets[1],"Identical binding missed cache");
 fetch[0][3]=(1u<<21)|(2u<<23);auto b=prepare();Require(bindings.sampler_indices[0]==672 && b.sets[1]!=a.sets[1],"Filter change reused old descriptor entry");auto bHit=prepare();Require(b.sets[1]==bHit.sets[1],"Filtered cache hit mismatch");
 texture.format=guest::LinearFormat::kRGBA8Unorm;ok(resources.UploadTexture(123,texture,2,e));auto replaced=prepare();Require(bindings.sampler_indices[0]==0 && replaced.sets[1]!=b.sets[1],"Replacement format was not reevaluated");
 bindings.sampler_indices[0]=1024;DescriptorDraw invalid;Require(!descriptors.Prepare(bindings,fetch,resources,1,constants,invalid,e),"Reserved metadata accepted");bindings.sampler_indices[0]=0;
 auto stats=descriptors.TakeCacheStats();Require(stats.hits==2 && stats.misses==3,"Unexpected descriptor cache statistics");
 texture.format=guest::LinearFormat::kR32Float;ok(resources.UploadTexture(123,texture,3,e));
 bindings.texture_indices[0]=0x10000000u;Require(!descriptors.Prepare(bindings,fetch,resources,1,constants,invalid,e),"Unproved bicubic flags accepted");bindings.texture_indices[0]=0;
 fetch[31]=fetch[0];bindings.texture_indices[31]=0;auto last=prepare();Require(bindings.sampler_indices[31]==703u,"Last descriptor slot metadata wrong");uint32_t lastWord;std::memcpy(&lastWord,constants.data+kSharedConstantsOffset+128+31*4,4);Require(lastWord==703u,"Last descriptor slot constants wrong");auto lastHit=prepare();Require(last.sets[1]==lastHit.sets[1],"Last slot cache hit mismatch");
 resources.FinishUploads();g.Submit();descriptors.Retire(1);resources.Retire(1);
 std::puts("PASS descriptor filter metadata, cache hits/misses, format replacement and reserved bits");
}

int main(int argc,char** argv){try{Gpu gpu;VkFormatProperties props;gpu.c.f.vkGetPhysicalDeviceFormatProperties(gpu.c.physical,VK_FORMAT_R32_SFLOAT,&props);std::printf("R32_SFLOAT linear filter supported=%s features=%u\n",props.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT?"yes":"no",props.optimalTilingFeatures);if(argc==2 && std::strcmp(argv[1],"--caps")==0)return 0;Require(argc==2,"usage: test_float_filter_gpu probe.spv | --caps");DescriptorCache(gpu);Numeric(gpu,argv[1],VK_FORMAT_R32_SFLOAT,1);Numeric(gpu,argv[1],VK_FORMAT_R32G32_SFLOAT,2);Numeric(gpu,argv[1],VK_FORMAT_R32G32B32A32_SFLOAT,4);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL GPU filter: %s\n",e.what());return 1;}}
