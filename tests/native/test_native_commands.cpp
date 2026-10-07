#include "native_command_system.h"
#include "native_graphics_system_vulkan.h"
#include <rex/system/xmemory.h>
#include <cstdio>
#include <cstring>
#include <future>
#include <stdexcept>
#include <source_location>
using namespace superman_returns::native;
static void Check(bool ok,std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("assertion line "+std::to_string(at.line()));}
int main(){try{
 rex::memory::Memory memory;Check(memory.Initialize());uint32_t alias=0;
 Check(memory.LookupHeapByType(true,0x1000)->Alloc(0x1000,0x1000,rex::memory::kMemoryAllocationReserve|rex::memory::kMemoryAllocationCommit,rex::memory::kMemoryProtectRead|rex::memory::kMemoryProtectWrite,false,&alias));
 uint32_t physical=(alias&0x1FFFFFFF)+(alias>=0xE0000000?0x1000:0);auto bytes=memory.TranslatePhysical<uint8_t*>(physical);
 auto write=[&](unsigned index,uint32_t value){value=__builtin_bswap32(value);std::memcpy(bytes+index*4,&value,4);};
 auto read=[&](unsigned offset){uint32_t value;std::memcpy(&value,bytes+offset,4);return __builtin_bswap32(value);};
 VulkanNativeGraphicsSystem gpu({});gpu.InitializeCommandMemory(memory);gpu.InitializeRingBuffer(physical,0);gpu.EnableReadPointerWriteBack(physical+64,0);gpu.SetSystemCommandBufferGpuIdentifierAddress(alias+68);
 // An 8-byte ring consumes a Type-0 packet, then the same packet across wrap.
 write(0,0x100);write(1,7);Check(gpu.ConsumeRing(0)==true); // empty
 // Reconfigure a 32-byte ring (8 dwords); fill to six, then wrap header/payload.
 gpu.InitializeRingBuffer(physical,2);for(unsigned i=0;i<6;++i)write(i,0x80000000u);Check(gpu.ConsumeRing(6));
 write(6,0xC0013D00u);write(7,(physical+68)|2);write(0,0x12345678u);Check(gpu.ConsumeRing(1));Check(read(68)==0x12345678u);Check(read(64)==1);
 uint32_t cb=0,data=0,source=0,cpu=0;gpu.SetInterruptCallback(0x82001000,0x11223344);gpu.SetInterruptDispatcher([&](uint32_t c,uint32_t d,uint32_t s,uint32_t n){cb=c;data=d;source=s;cpu=n;});
 write(1,0xC0005400u);write(2,4);Check(gpu.ConsumeRing(3));Check(cb==0x82001000 && data==0x11223344 && source==1 && cpu==2);
 auto since=gpu.progress_generation();auto waiter=std::async(std::launch::async,[&]{gpu.WaitProgress(since,1000000);});gpu.SignalGpuProgress();Check(waiter.wait_for(std::chrono::milliseconds(200))==std::future_status::ready);
 gpu.SetPaused(true);auto frame=gpu.guest_frame_counter();gpu.TickVblank();Check(gpu.guest_frame_counter()==frame);gpu.SetPaused(false);gpu.TickVblank();Check(gpu.guest_frame_counter()==frame+1);
 // Malformed WAIT has fewer words than its documented five-word payload.
 write(3,0xC0003C00u);write(4,0);Check(!gpu.ConsumeRing(5));
 // A real WAIT_REG_MEM cannot satisfy its predicate and must stop on shutdown.
 write(3,0xC0043C00u);write(4,0x13);write(5,(physical+72)|2);write(6,99);write(7,0xFFFFFFFF);write(0,0x100);
 auto blocked=std::async(std::launch::async,[&]{gpu.ConsumeRing(1);});
 std::this_thread::sleep_for(std::chrono::milliseconds(10));
 since=gpu.progress_generation();auto cancelled=std::async(std::launch::async,[&]{gpu.WaitProgress(since,1000000);});gpu.Shutdown();gpu.Shutdown();Check(blocked.wait_for(std::chrono::milliseconds(200))==std::future_status::ready);Check(cancelled.wait_for(std::chrono::milliseconds(200))==std::future_status::ready);
 std::puts("PASS commands: wrapped big-endian ring, fence/identifier write, interrupt, progress, pause and shutdown");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
