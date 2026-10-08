#include "native_bootstrap.h"
#include "superman_returns_init.h"
#include <rex/rex_app.h>
#include <cstdlib>
#include <jni.h>
#include <stdexcept>
#include <fstream>
#include <string>

extern "C" void REX_AndroidSetTouchGamepadState(int,float,float,float,float,float,float);
extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_GameActivity_setTouchState(JNIEnv*,jclass,jint buttons,jfloat lx,jfloat ly,jfloat rx,jfloat ry,jfloat lt,jfloat rt) {
    REX_AndroidSetTouchGamepadState(buttons,lx,ly,rx,ry,lt,rt);
}

extern "C" JNIEXPORT void JNICALL
Java_org_supermanreturns_mobile_GameActivity_setNativePaused(JNIEnv*,jclass,jboolean paused) {
    superman_returns::android::SetNativePaused(paused);
}

namespace {
// Diagnostics such as SR_VULKAN_PROFILE=1 are read from the renderer with getenv.
// An app cannot receive environment variables from adb, so a debug file in the
// app-private files directory may set SR_* variables, one KEY=VALUE per line.
void LoadDebugEnvironment(const std::filesystem::path& files) {
    std::ifstream input(files/"sr-debug-env.txt");
    std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()==0x0D) line.pop_back();
        const auto equals=line.find('=');
        if(equals==std::string::npos || equals==0 || line.compare(0,3,"SR_")!=0) continue;
        setenv(line.substr(0,equals).c_str(),line.substr(equals+1).c_str(),1);
    }
}
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
        LoadDebugEnvironment(files);
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

