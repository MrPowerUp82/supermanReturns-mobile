#include "native_bootstrap.h"
#include "superman_returns_init.h"
#include <rex/rex_app.h>
#include <cstdlib>
#include <jni.h>
#include <stdexcept>
#include <fstream>

extern "C" void REX_AndroidSetTouchGamepadState(int,float,float,float,float,float,float);
extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_GameActivity_setTouchState(JNIEnv*,jclass,jint buttons,jfloat lx,jfloat ly,jfloat rx,jfloat ry,jfloat lt,jfloat rt) {
    REX_AndroidSetTouchGamepadState(buttons,lx,ly,rx,ry,lt,rt);
}

extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_GameActivity_setNativePaused(JNIEnv*,jclass,jboolean paused) {
    superman_returns::android::SetNativePaused(paused);
}

class SupermanAndroidApp : public rex::ReXApp {
public:
    using rex::ReXApp::ReXApp;
    static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
        return std::unique_ptr<SupermanAndroidApp>(new SupermanAndroidApp(ctx,"superman_returns",PPCImageConfig));
    }
    void OnPreSetup(rex::RuntimeConfig& config) override {
        const char* files=std::getenv("SR_ANDROID_FILES");
        if(!files || !*files) throw std::runtime_error("Android files directory unavailable");
        const auto error_path=std::filesystem::path(files)/"native-error.txt";
        std::error_code ignored;
        std::filesystem::remove(error_path,ignored);
        try {
            config.graphics = superman_returns::android::CreateNativeVulkanGraphicsSystem(files);
        } catch(const std::exception& error) {
            std::ofstream(error_path) << error.what() << '\n';
            throw;
        }
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

