#include "texture_filter.h"
#include <cstdio>
#include <cstring>
#include <source_location>
#include <stdexcept>
using namespace superman_returns::graphics::vulkan;
static void Check(bool ok,std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("filter assertion line "+std::to_string(at.line()));}
static std::array<uint32_t,6> Fetch(uint32_t mag=1,uint32_t min=1,uint32_t mip=2,uint32_t aniso=0){return {2u|(2u<<10)|(1u<<13),0,0,(mag<<19)|(min<<21)|(mip<<23)|(aniso<<25),0,1u<<9};}
int main(){try{
 Error e;TextureSamplingPlan p{};auto fetch=Fetch();
 for(auto format:{VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32_SFLOAT,VK_FORMAT_R32G32B32A32_SFLOAT}){
  Check(PlanTextureSampling(fetch,format,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,TextureDimension::k2D,31,p,e));
  Check(p.manual);Check(p.sampler_word==767u);Check((p.sampler_word&31u)==31u);
  VkPhysicalDeviceFeatures features{};VkPhysicalDeviceLimits limits{};VkSamplerCreateInfo sampler{};
  Check(PlanSampler(p.sampler_fetch,features,limits,false,sampler,e));Check(sampler.magFilter==VK_FILTER_NEAREST && sampler.minFilter==VK_FILTER_NEAREST && sampler.mipmapMode==VK_SAMPLER_MIPMAP_MODE_NEAREST);Check(!sampler.anisotropyEnable);Check(sampler.addressModeU==VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE && sampler.addressModeV==VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT);
 }
 Check(PlanTextureSampling(fetch,VK_FORMAT_R32_SFLOAT,VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,TextureDimension::k2D,31,p,e));Check(!p.manual && p.sampler_word==31 && p.sampler_fetch==fetch);
 {VkPhysicalDeviceFeatures features{};VkPhysicalDeviceLimits limits{};VkSamplerCreateInfo sampler{};Check(PlanSampler(p.sampler_fetch,features,limits,false,sampler,e));Check(sampler.maxLod==0);}

 for(uint32_t slot=0;slot<32;++slot)for(uint32_t mag=0;mag<2;++mag)for(uint32_t min=0;min<2;++min)for(uint32_t mip=0;mip<3;++mip){
  fetch=Fetch(mag,min,mip);Check(PlanTextureSampling(fetch,VK_FORMAT_R32_SFLOAT,VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT,TextureDimension::k2D,slot,p,e));
  bool manual=mag||min||mip==1;Check(p.manual==manual);Check((p.sampler_word&31u)==slot);Check(p.sampler_word==(manual?(slot|32u|(mag<<6)|(min<<7)|(mip<<8)):slot));
 }
 fetch=Fetch();
 for(auto dim:{TextureDimension::k3D,TextureDimension::kCube}){p.sampler_word=12345;Check(!PlanTextureSampling(fetch,VK_FORMAT_R32_SFLOAT,0,dim,7,p,e));Check(p.sampler_word==12345);Check(e.message.find("slot=7")!=std::string::npos && e.message.find("format=100")!=std::string::npos);}
 for(auto bad:{Fetch(1,1,2,2),Fetch(3,1),Fetch(1,3),Fetch(1,1,3)}){p.sampler_word=12345;Check(!PlanTextureSampling(bad,VK_FORMAT_R32_SFLOAT,0,TextureDimension::k2D,0,p,e));Check(p.sampler_word==12345 && !e.message.empty());}
 Check(!PlanTextureSampling(fetch,VK_FORMAT_R16_SFLOAT,0,TextureDimension::k2D,0,p,e));
 Check(!PlanTextureSampling(fetch,VK_FORMAT_R32_SFLOAT,0,TextureDimension::k2D,32,p,e));
 for(uint32_t address:{3u,5u,7u}){auto bad=fetch;bad[0]=(bad[0]&~(7u<<10))|(address<<10);Check(!PlanTextureSampling(bad,VK_FORMAT_R32_SFLOAT,0,TextureDimension::k2D,0,p,e));}
 std::array<uint32_t,6> none{};Check(PlanTextureSampling(none,VK_FORMAT_R32_SFLOAT,0,TextureDimension::k2D,0,p,e));Check(!p.manual && p.sampler_word==0);
 std::puts("PASS texture sampling: float32 formats, original hardware path, all slots/filter combinations, unsupported states and unchanged output on failure");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
