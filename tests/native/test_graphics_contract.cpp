#include <rex/system/interfaces/graphics.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdio>

namespace {
using rex::X_STATUS;
void Check(bool condition) { if (!condition) throw std::runtime_error("graphics contract assertion failed"); }
struct InjectedGraphics final : rex::system::IGraphicsSystem {
  std::vector<std::string> events;
  bool presentation=false;
  uint32_t callback=0, user=0, ring=0, ring_size=0, writeback=0, block_size=0, identifier=0;
  uint32_t title=0;
  std::filesystem::path cache;
  bool blocking=false;
  rex::X_STATUS fail_setup=X_STATUS_SUCCESS;
  rex::ui::GraphicsProvider* provider() const override { return nullptr; }
  rex::ui::Presenter* presenter() const override { return nullptr; }
  rex::X_STATUS SetupPresentation(rex::ui::WindowedAppContext*) override {
    events.push_back("presentation"); presentation=true; return fail_setup;
  }
  rex::X_STATUS SetupGuestGpu(rex::runtime::FunctionDispatcher*,rex::system::KernelState*) override {
    events.push_back("guest-gpu"); return X_STATUS_SUCCESS;
  }
  bool has_presentation() const override { return presentation; }
  void Shutdown() override { events.push_back("shutdown"); presentation=false; }
  void InitializeShaderStorage(const std::filesystem::path& path,uint32_t id,bool block) override {
    events.push_back("shaders"); cache=path; title=id; blocking=block;
  }
  void SetInterruptCallback(uint32_t cb,uint32_t data) override {callback=cb;user=data;}
  void InitializeRingBuffer(uint32_t ptr,uint32_t size) override {ring=ptr;ring_size=size;}
  void EnableReadPointerWriteBack(uint32_t ptr,uint32_t size) override {writeback=ptr;block_size=size;}
  void SetSystemCommandBufferGpuIdentifierAddress(uint32_t ptr) override {identifier=ptr;}
};

void routes_provider_and_shader_storage_without_xenos() {
  InjectedGraphics injected;
  rex::system::IGraphicsSystem& graphics=injected;
  Check(graphics.Setup(nullptr,nullptr,nullptr,true)==X_STATUS_SUCCESS);
  graphics.InitializeShaderStorage("cache",0x454107ED,true);
  Check(injected.events==std::vector<std::string>{"presentation","guest-gpu","shaders"});
  Check(injected.cache=="cache" && injected.title==0x454107ED && injected.blocking);
  Check(graphics.provider()==nullptr && graphics.presenter()==nullptr);
  injected.fail_setup=X_STATUS_UNSUCCESSFUL;
  injected.presentation=false; injected.events.clear();
  Check(XFAILED(graphics.Setup(nullptr,nullptr,nullptr,true)));
  Check(injected.events==std::vector<std::string>{"presentation"});
}
void routes_video_ring_interrupt_and_identifier() {
  InjectedGraphics injected;
  rex::system::IGraphicsSystem& graphics=injected;
  graphics.SetInterruptCallback(0x82001234,0x12345678);
  graphics.InitializeRingBuffer(0x10002000,16);
  graphics.EnableReadPointerWriteBack(0x10003000,6);
  graphics.SetSystemCommandBufferGpuIdentifierAddress(0x10004000);
  Check(injected.callback==0x82001234 && injected.user==0x12345678);
  Check(injected.ring==0x10002000 && injected.ring_size==16);
  Check(injected.writeback==0x10003000 && injected.block_size==6);
  Check(injected.identifier==0x10004000);
}
void shutdown_without_presentation() {
  InjectedGraphics injected;
  rex::system::IGraphicsSystem& graphics=injected;
  Check(graphics.Setup(nullptr,nullptr,nullptr,false)==X_STATUS_SUCCESS);
  graphics.Shutdown();
  Check(injected.events==std::vector<std::string>{"guest-gpu","shutdown"});
}
}
int main() {
  try {
    routes_provider_and_shader_storage_without_xenos();
    routes_video_ring_interrupt_and_identifier();
    shutdown_without_presentation();
    std::puts("PASS graphics contract: startup order, failure, injected video callbacks, headless shutdown");
    return 0;
  } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
