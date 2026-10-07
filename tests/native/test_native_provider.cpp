#include "context.h"
#include "game_pipeline.h"
#include <fstream>
#include <cstring>
#include "platform/native_provider.h"
#include <cstdio>
#include <stdexcept>
using namespace superman_returns::graphics::vulkan;
static void Check(bool ok){if(!ok)throw std::runtime_error("provider assertion");}
static size_t initial_bytes=0;static unsigned cache_calls=0;static bool reject_initial=false;
static VkResult VKAPI_CALL Layout(VkDevice,const VkPipelineLayoutCreateInfo*,const VkAllocationCallbacks*,VkPipelineLayout* out){*out=(VkPipelineLayout)1;return VK_SUCCESS;}
static void VKAPI_CALL DestroyLayout(VkDevice,VkPipelineLayout,const VkAllocationCallbacks*){}
static VkResult VKAPI_CALL Cache(VkDevice,const VkPipelineCacheCreateInfo* info,const VkAllocationCallbacks*,VkPipelineCache* out){initial_bytes=info->initialDataSize;++cache_calls;if(reject_initial && initial_bytes)return VK_ERROR_UNKNOWN;*out=(VkPipelineCache)2;return VK_SUCCESS;}
static void VKAPI_CALL DestroyCache(VkDevice,VkPipelineCache,const VkAllocationCallbacks*){}
static VkResult VKAPI_CALL Idle(VkDevice){return VK_SUCCESS;}
static void CacheTests(){
 Dispatch dispatch{};dispatch.vkDeviceWaitIdle=Idle;dispatch.vkCreatePipelineLayout=Layout;dispatch.vkDestroyPipelineLayout=DestroyLayout;dispatch.vkCreatePipelineCache=Cache;dispatch.vkDestroyPipelineCache=DestroyCache;
 Context context(dispatch);context.device=(VkDevice)1;struct Reset{Context& c;~Reset(){c.device=VK_NULL_HANDLE;}} reset{context};context.properties.vendorID=9;context.properties.deviceID=7;context.properties.driverVersion=3;
 auto directory=std::filesystem::temp_directory_path()/("sr-provider-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(directory);auto file=directory/"cache.bin";
 std::vector<uint32_t> header={0x33435053,1,9,7,4,0,0,0,0,32,1,9,7,0,0,0,0};
 auto write=[&]{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(header.data()),header.size()*4);};write();
 std::array<VkDescriptorSetLayout,4> layouts{};Error error;
 {GamePipelineStore store(context);Check(store.Initialize(layouts,file,error));Check(initial_bytes==0);}
 header[4]=3;write();cache_calls=0;reject_initial=true;
 {GamePipelineStore store(context);Check(store.Initialize(layouts,file,error));Check(cache_calls==2 && initial_bytes==0);}
 context.device=VK_NULL_HANDLE;std::filesystem::remove_all(directory);
}
int main(){try{
 NativeProviderConfig config;config.shaders.precompiled_only=true;config.shaders.library="/data/local/tmp/no-such-native-library.srvk";Check(!CreateNativeVulkanProvider(config));
 VkPhysicalDeviceFeatures available{},requested{};requested.shaderClipDistance=VK_TRUE;Error error;Check(!ValidateRequiredDeviceFeatures(available,requested,error));Check(error.message.find("shaderClipDistance")!=std::string::npos);available.shaderClipDistance=VK_TRUE;Check(ValidateRequiredDeviceFeatures(available,requested,error));
 CacheTests();
 std::puts("PASS provider prerequisites: invalid library rejected and missing device feature named");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}


