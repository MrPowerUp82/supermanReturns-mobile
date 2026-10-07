#include "superman_returns_init.h"
#include <rex/logging.h>
#include <atomic>
#include <chrono>
#include <android/log.h>
#include "native_bridge.h"

// Keep the original 1280x720 engine layout. Changing only the present parameters
// corrupts the game's resolve pitches (confirmed in the desktop project).
void SrScaleEngineRenderSize(PPCRegister&) {}

// Observe the genuine guest Swap routine without replacing its GPU work.
REX_EXTERN(__imp__sub_82112050);
extern "C" REX_FUNC(sub_82112050) {
    const uint32_t device=ctx.r3.u32, front_buffer=ctx.r4.u32;
    static std::atomic<uint64_t> frames{0};
    const auto frame=++frames;
    if(frame==1 || frame%120==0) __android_log_print(ANDROID_LOG_INFO,"SupermanGuest","Guest Swap frame %llu",static_cast<unsigned long long>(frame));
    __imp__sub_82112050(ctx,base);
    try {
        superman_returns::native::OnFrameStatsSwap(base,device,front_buffer);
    } catch(const std::exception& error) {
        superman_returns::native::ReportNativeFailure(error.what());
    }
}
