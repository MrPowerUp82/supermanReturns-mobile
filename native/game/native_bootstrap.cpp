#include "native_bootstrap.h"
#include "native_graphics_system_vulkan.h"
#include "native_frontend.h"
#include "native_bridge.h"
#include "platform/native_provider.h"
#include "../shaders/binding_contract.h"
#include <rex/crypto/sha256.h>
#include <rex/logging.h>
#include <fstream>
#include <stdexcept>
#ifndef SR_NATIVE_SOURCE_REVISION
#define SR_NATIVE_SOURCE_REVISION "test"
#endif
#ifndef SR_NATIVE_SOURCE_DIGEST
#define SR_NATIVE_SOURCE_DIGEST "test"
#endif
namespace superman_returns::android {
std::unique_ptr<rex::system::IGraphicsSystem> CreateNativeVulkanGraphicsSystem(const std::filesystem::path& files_root){
 graphics::vulkan::NativeProviderConfig config;
 config.shaders.precompiled_only=true;config.shaders.library=files_root/"shaders/superman_returns_vulkan.srvk";config.shaders.missing_dump_dir=files_root/"native-shader-misses";
 graphics::shaders::VulkanShaderService library(config.shaders,{});
 if(!library.PrecompiledCount() || !library.LibraryDiagnostic().empty())throw std::runtime_error("Native Vulkan shader library unavailable: "+library.LibraryDiagnostic());
 const auto hash=rex::crypto::sha256_file(config.shaders.library);
 config.driver_cache=files_root/"cache/pc-native-vulkan"/("bindings-v"+std::to_string(graphics::shaders::BindingContractVersion))/SR_NATIVE_SOURCE_DIGEST/hash/"driver.cache";
 std::filesystem::create_directories(config.driver_cache.parent_path());
 native::SetNativeFailureHandler([files_root](const std::string& reason){
   std::ofstream error(files_root/"native-error.txt",std::ios::trunc);error<<reason<<'\n';error.flush();REXLOG_ERROR("renderer=pc-native-vulkan stopped: {}",reason);
 });
 native::NativeFrontend::Get().SetFailureHandler([](const std::string& reason){native::ReportNativeFailure(reason);});
 REXLOG_INFO("renderer=pc-native-vulkan source={} digest={} shader_sha256={} precompiled={}",SR_NATIVE_SOURCE_REVISION,SR_NATIVE_SOURCE_DIGEST,hash,library.PrecompiledCount());
 return std::make_unique<native::VulkanNativeGraphicsSystem>([config]{return graphics::vulkan::CreateNativeVulkanProvider(config);});
}
void SetNativePaused(bool paused){if(auto* system=dynamic_cast<native::NativeCommandSystem*>(native::ActiveNativeGraphicsSystem()))system->SetPaused(paused);}
}

namespace superman_returns::native {bool GuestGammaRamp256(uint32_t* entries){auto* system=ActiveNativeGraphicsSystem();return system && system->GetGammaRamp256(entries);}}
