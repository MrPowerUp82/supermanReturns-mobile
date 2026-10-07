#include "../../../superman_returns_recomp/port/src/native_renderer/native_graphics_system_vulkan.h"
#include "superman_returns_init.h"
#include <rex/rex_app.h>
#include <cstdlib>
#include <jni.h>

extern "C" void REX_AndroidSetTouchGamepadState(int,float,float,float,float,float,float);
extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_GameActivity_setTouchState(JNIEnv*,jclass,jint buttons,jfloat lx,jfloat ly,jfloat rx,jfloat ry,jfloat lt,jfloat rt) {
    REX_AndroidSetTouchGamepadState(buttons,lx,ly,rx,ry,lt,rt);
}

class SupermanAndroidApp : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;
    static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<SupermanAndroidApp>(new SupermanAndroidApp(ctx,"superman_returns",PPCImageConfig));
    }
    void OnPreSetup(rex::RuntimeConfig& config) override {
        config.graphics = std::make_unique<superman_returns::native::VulkanNativeGraphicsSystem>();
    }
    void OnConfigurePaths(rex::PathConfig& paths) override {
        if(const char* base=std::getenv("SR_ANDROID_FILES")) {
            std::filesystem::path root(base);
            paths.game_data_root=root/"game"; paths.user_data_root=root/"userdata";
            paths.cache_root=root/"cache"; paths.config_path=root/"superman_returns.toml";
        }
    }
};
REX_DEFINE_APP(superman_returns,SupermanAndroidApp::Create)

