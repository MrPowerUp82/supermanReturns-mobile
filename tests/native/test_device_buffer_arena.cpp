#include "resources.h"
#include "game_pipeline.h"
#include "test_main.h"
#include <cstring>
using namespace superman_returns::graphics::vulkan;
namespace {
uint64_t next=10;
struct Buffer {VkDeviceSize size;VkDeviceMemory memory{};};
std::map<VkBuffer,Buffer> buffers;
std::map<VkDeviceMemory,std::vector<std::byte>> memory;
size_t peak=0;
Dispatch Fake() {
  Dispatch f;
  f.vkCreateBuffer=[](VkDevice,const VkBufferCreateInfo* ci,const VkAllocationCallbacks*,VkBuffer* out) { *out=reinterpret_cast<VkBuffer>(++next);buffers[*out]={ci->size};return VK_SUCCESS;};
  f.vkGetBufferMemoryRequirements=[](VkDevice,VkBuffer b,VkMemoryRequirements* out) {*out={buffers.at(b).size,64,3};};
  f.vkAllocateMemory=[](VkDevice,const VkMemoryAllocateInfo* a,const VkAllocationCallbacks*,VkDeviceMemory* out) {
    if(memory.size()>=32) return VK_ERROR_TOO_MANY_OBJECTS;
    *out=reinterpret_cast<VkDeviceMemory>(++next);memory[*out].resize(a->allocationSize);peak=std::max(peak,memory.size());return VK_SUCCESS;
  };
  f.vkBindBufferMemory=[](VkDevice,VkBuffer b,VkDeviceMemory m,VkDeviceSize offset) {SR_CHECK_EQ(offset,0u);buffers.at(b).memory=m;return VK_SUCCESS;};
  f.vkMapMemory=[](VkDevice,VkDeviceMemory m,VkDeviceSize,VkDeviceSize,VkMemoryMapFlags,void** out) {*out=memory.at(m).data();return VK_SUCCESS;};
  f.vkFreeMemory=[](VkDevice,VkDeviceMemory m,const VkAllocationCallbacks*) {memory.erase(m);};
  f.vkDestroyBuffer=[](VkDevice,VkBuffer b,const VkAllocationCallbacks*) {buffers.erase(b);};
  f.vkDestroyDevice=[](VkDevice,const VkAllocationCallbacks*) {};
  f.vkDeviceWaitIdle=[](VkDevice) {return VK_SUCCESS;};
  f.vkCmdCopyBuffer=[](VkCommandBuffer,VkBuffer src,VkBuffer dst,uint32_t n,const VkBufferCopy* copies) {
    auto& s=memory.at(buffers.at(src).memory);auto& d=memory.at(buffers.at(dst).memory);
    for(uint32_t i=0;i<n;++i) {auto& c=copies[i];SR_CHECK(c.srcOffset+c.size<=s.size());SR_CHECK(c.dstOffset+c.size<=d.size());std::memcpy(d.data()+c.dstOffset,s.data()+c.srcOffset,c.size);}
  };
  f.vkCmdPipelineBarrier=[](VkCommandBuffer,VkPipelineStageFlags,VkPipelineStageFlags,VkDependencyFlags,uint32_t,const VkMemoryBarrier*,uint32_t,const VkBufferMemoryBarrier*,uint32_t,const VkImageMemoryBarrier*) {};
  return f;
}
uint32_t Value(const std::shared_ptr<BufferResource>& b) {uint32_t out;std::memcpy(&out,memory.at(b->memory).data()+b->offset,4);return out;}
}
SR_TEST(device_buffer_arena_preserves_thousands_of_live_versions_under_allocation_limit) {
  peak=0;Context c(Fake());c.device=reinterpret_cast<VkDevice>(1);
  c.memory.memoryTypeCount=2;c.memory.memoryTypes[0].propertyFlags=VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  c.memory.memoryTypes[1].propertyFlags=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  c.properties.limits.maxStorageBufferRange=1024*1024;c.properties.limits.minStorageBufferOffsetAlignment=256;
  {
    ResourceStore store(c);Error e;
    SR_CHECK(store.BeginSubmission(reinterpret_cast<VkCommandBuffer>(1),1,e));
    uint32_t uploaded=0;
    for(uint32_t id=1;id<=5000;++id) {if(!store.UploadBuffer(id,std::as_bytes(std::span(&id,1)),id,e)) break;++uploaded;}
    SR_CHECK_EQ(uploaded,5000u);
    if(uploaded==5000) {
      auto old=store.Buffer(1,e);SR_CHECK_EQ(Value(old),1u);
      // Append in a new submission without resetting a chunk containing live data.
      SR_CHECK(store.BeginSubmission(reinterpret_cast<VkCommandBuffer>(1),2,e));
      uint32_t changed=123456;SR_CHECK(store.UploadDynamicBuffer(1,std::as_bytes(std::span(&changed,1)),6000,e));
      SR_CHECK_EQ(Value(old),1u);SR_CHECK_EQ(Value(store.Buffer(1,e)),changed);
      for(uint32_t id=2;id<=5000;++id) {auto b=store.Buffer(id,e);SR_CHECK_EQ(Value(b),id);SR_CHECK_EQ(b->offset%256,0u);}
      store.Retire(1);SR_CHECK_EQ(Value(old),1u);old.reset();
      SR_CHECK(peak<=4);
    }
  }
  SR_CHECK(memory.empty());SR_CHECK(buffers.empty());
}
SR_TEST(normal_float32_input_is_bound_as_raw_uint_bits_for_shader_asfloat) {
  namespace guest=superman_returns::graphics::guest;
  namespace shaders=superman_returns::graphics::shaders;
  guest::DrawPacket draw;draw.inline_vertices=true;draw.inline_stride=32;
  draw.attributes={{0,0,0x1a23a6,0,0},{0,16,0x1a23a6,3,1}};
  shaders::CompiledShader vs;vs.inputs={{0,"float32x4"},{6,"uint32x4"}};
  TargetPass pass;GamePipelinePlan plan;Error e;
  SR_CHECK(PlanGamePipeline(draw,pass,vs,plan,e));
  SR_CHECK_EQ(plan.attributes[0].format,VK_FORMAT_R32G32B32A32_SFLOAT);
  SR_CHECK_EQ(plan.attributes[1].format,VK_FORMAT_R32G32B32A32_UINT);
  SR_CHECK_EQ(plan.attributes[1].offset,16u);
  auto float_key=plan.key;
  vs.inputs[1].type="float32x4";SR_CHECK(PlanGamePipeline(draw,pass,vs,plan,e));
  SR_CHECK_EQ(plan.attributes[1].format,VK_FORMAT_R32G32B32A32_SFLOAT);SR_CHECK(plan.key!=float_key);
  draw.attributes[1].type=0x2a2187;vs.inputs[1].type="uint32x4";
  SR_CHECK(PlanGamePipeline(draw,pass,vs,plan,e));SR_CHECK_EQ(plan.attributes[1].format,VK_FORMAT_R32_UINT);
}
