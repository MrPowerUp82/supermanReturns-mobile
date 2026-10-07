#include "vulkan/frame_loop.h"
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <array>
#include <memory>
#include <mutex>
#include <sstream>

namespace vk = superman_returns::graphics::vulkan;
namespace {
struct Session {
    // Reverse destruction order: loop -> chain -> context -> window.
    struct WindowDelete { void operator()(ANativeWindow* w) const { if(w) ANativeWindow_release(w); } };
    std::unique_ptr<ANativeWindow, WindowDelete> window;
    vk::Context context;
    vk::Swapchain chain;
    vk::FrameLoop loop;
};
std::unique_ptr<Session> session;
uint64_t session_owner=0;
std::mutex render_mutex, input_mutex;
uint32_t buttons = 0;
std::array<float,6> axes{};
jstring str(JNIEnv* env, const std::string& s) { return env->NewStringUTF(s.c_str()); }
std::string failure(const vk::Error& e) { return "ERRO: " + e.operation + ": " + e.message; }
void androidLog(const std::string& s) { __android_log_write(ANDROID_LOG_INFO,"SupermanMobile",s.c_str()); }
bool rebuild(Session& s, vk::Error& e) {
    if(s.context.device && !s.loop.Retire(s.context,e)) return false;
    const auto w=ANativeWindow_getWidth(s.window.get()), h=ANativeWindow_getHeight(s.window.get());
    if(w<=0 || h<=0) { e={"Android surface",VK_ERROR_OUT_OF_DATE_KHR,"Surface has no extent"}; return false; }
    return s.chain.Recreate(s.context,{uint32_t(w),uint32_t(h)},true,e) &&
           s.chain.handle && s.loop.Initialize(s.context,s.chain,e);
}
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_supermanreturns_mobile_NativeBridge_probe(JNIEnv* env,jclass) {
    vk::Context c; vk::Error e; c.logger=androidLog;
    const char* extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    if(!c.CreateInstance(extensions,false,e)) return str(env,failure(e));
    std::vector<vk::DeviceCandidate> devices;
    if(!c.EnumerateCandidates(VK_NULL_HANDLE,devices,e)) return str(env,failure(e));
    std::ostringstream out;
    for(auto& d:devices) out << d.name << " | Vulkan " << VK_VERSION_MAJOR(d.api_version) << '.'
        << VK_VERSION_MINOR(d.api_version) << " | swapchain=" << (d.swapchain?"sim":"não") << '\n';
    if(devices.empty()) return str(env,"ERRO: nenhuma GPU Vulkan encontrada");
    return str(env,out.str());
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_supermanreturns_mobile_NativeBridge_openSurface(JNIEnv* env,jclass,jlong owner,jobject surface) {
    std::lock_guard lock(render_mutex);
    if(uint64_t(owner)<session_owner) return str(env,"ERRO: sessão Vulkan substituída");
    session.reset();session_owner=uint64_t(owner);
    auto s=std::make_unique<Session>(); vk::Error e; s->context.logger=androidLog;
    s->window.reset(ANativeWindow_fromSurface(env,surface));
    if(!s->window) return str(env,"ERRO: Surface Android indisponível");
    const char* extensions[]={VK_KHR_SURFACE_EXTENSION_NAME,VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
    if(!s->context.CreateInstance(extensions,false,e)) return str(env,failure(e));
    auto create=reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(s->context.Proc()(s->context.instance,"vkCreateAndroidSurfaceKHR"));
    if(!create) return str(env,"ERRO: VK_KHR_android_surface indisponível");
    VkAndroidSurfaceCreateInfoKHR info{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR}; info.window=s->window.get();
    VkSurfaceKHR handle=VK_NULL_HANDLE;
    if(!vk::Check(create(s->context.instance,&info,nullptr,&handle),"CreateAndroidSurface",e)) return str(env,failure(e));
    if(!s->context.OpenDevice(handle,"",e) || !rebuild(*s,e)) return str(env,failure(e));
    auto name=s->context.selected.name;
    session=std::move(s); return str(env,"Vulkan ativo · "+name);
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_supermanreturns_mobile_NativeBridge_draw(JNIEnv* env,jclass,jlong owner) {
    std::lock_guard lock(render_mutex);
    if(!session || uint64_t(owner)!=session_owner) return str(env,"ERRO: sessão Vulkan indisponível");
    auto& s=*session; vk::Error e;
    auto result=s.loop.Draw(s.context,s.chain,[&](VkCommandBuffer cmd,uint32_t) {
        uint32_t state; { std::lock_guard input_lock(input_mutex); state=buttons; }
        // A red panel confirms real Vulkan command submission and changes on button press.
        VkClearAttachment clear{}; clear.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        clear.clearValue.color={{state?0.85f:0.38f,0.08f,0.12f,1.f}};
        const auto extent=s.chain.choice.extent;
        VkClearRect rect{}; rect.rect={{int32_t(extent.width/3),int32_t(extent.height/3)},
                                     {extent.width/3,extent.height/3}}; rect.layerCount=1;
        s.context.f.vkCmdClearAttachments(cmd,1,&clear,1,&rect);
    },e);
    if(result==vk::FrameOutcome::kRecreate && !rebuild(s,e)) return str(env,failure(e));
    if(result==vk::FrameOutcome::kFailed) return str(env,failure(e));
    return str(env,"Vulkan · quadros apresentados: "+std::to_string(s.loop.presented));
}
extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_NativeBridge_closeSurface(JNIEnv*,jclass,jlong owner) {
    std::lock_guard lock(render_mutex); if(uint64_t(owner)==session_owner) session.reset();
}
extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_NativeBridge_setInput(JNIEnv*,jclass,jint b,jfloat lx,jfloat ly,jfloat rx,jfloat ry,jfloat lt,jfloat rt) {
    std::lock_guard lock(input_mutex); buttons=uint32_t(b); axes={lx,ly,rx,ry,lt,rt};
}
extern "C" JNIEXPORT jstring JNICALL
Java_org_supermanreturns_mobile_NativeBridge_inputState(JNIEnv* env,jclass) {
    std::lock_guard lock(input_mutex); std::ostringstream out;
    out << "XInput 0x" << std::hex << buttons << std::dec;
    out.precision(2); out << std::fixed << " | L " << axes[0] << ',' << axes[1]
        << " | R " << axes[2] << ',' << axes[3] << " | LT " << axes[4] << " RT " << axes[5];
    return str(env,out.str());
}
