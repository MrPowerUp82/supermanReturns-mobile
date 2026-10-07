#include "native_bootstrap.h"
#include "native_bridge.h"
#include "native_frontend.h"
#include "native_graphics_system_vulkan.h"
#include "shader_fixture.h"
#include <cstdio>
#include <source_location>
#include <stdexcept>
using namespace superman_returns;
static void Check(bool ok,std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("bootstrap assertion line "+std::to_string(at.line()));}
struct FakeFrontend: native::NativeFrontend {unsigned swaps=0;uint32_t front=0;void OnSwap(uint8_t*,uint32_t value,uint64_t) override{++swaps;front=value;}};
int main(){try{
 auto root=std::filesystem::temp_directory_path()/("sr-bootstrap-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(root/"shaders");
 bool rejected=false;try{android::CreateNativeVulkanGraphicsSystem(root);}catch(const std::exception&){rejected=true;}Check(rejected);Check(!native::RendererActive());
 shader_fixture::Write(root/"shaders/superman_returns_vulkan.srvk",shader_fixture::Library(std::vector<uint8_t>(24,1)));
 {auto system=android::CreateNativeVulkanGraphicsSystem(root);Check(bool(system));Check(dynamic_cast<native::VulkanNativeGraphicsSystem*>(system.get())!=nullptr);Check(!native::RendererActive());}
 FakeFrontend frontend;std::string error;Check(!native::ActivateNativeFrontend(frontend,error));
 Check(frontend.InstallPacketSink([](auto&&,std::string&){return true;},[]{}));Check(native::ActivateNativeFrontend(frontend,error));Check(native::RendererActive());native::OnFrameStatsSwap(nullptr,0x1234,0x5678);Check(frontend.swaps==1 && frontend.front==0x5678);native::DeactivateNativeFrontend();native::OnFrameStatsSwap(nullptr,0,99);Check(frontend.swaps==1);
 native::NativeFrontend failed;unsigned failure_reports=0;failed.SetFailureHandler([&](const std::string& reason){Check(reason=="test GPU failure");++failure_reports;});Check(failed.InstallPacketSink([](auto&&,std::string& reason){reason="test GPU failure";return false;},[]{}));
 auto batch=std::make_unique<graphics::guest::WorkBatch>();graphics::guest::WorkCmd command;command.op=graphics::guest::Op::kPassEnd;batch->cmds.push_back(command);Check(failed.SubmitCapturedBatch(std::move(batch),error));Check(!failed.Drain(error));failed.ShutdownWorker();Check(failure_reports==1);Check(!native::RendererActive());
 std::filesystem::remove_all(root);std::puts("PASS bootstrap: missing library blocks boot, native factory, activation prerequisites, exactly one swap, sink failure reports without fallback");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
