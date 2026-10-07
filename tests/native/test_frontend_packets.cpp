#include "native_frontend.h"
#include "../graphics/guest/render_packet.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <source_location>
#include <cstring>

using namespace superman_returns;
static void Check(bool value,std::source_location location=std::source_location::current()) {if(!value) throw std::runtime_error("frontend assertion failed at line "+std::to_string(location.line()));}

int main() {
  try {
    native::NativeFrontend frontend;
    std::vector<int> passes;
    std::atomic<unsigned> cancellations{0};
    Check(frontend.InstallPacketSink([&](graphics::guest::RenderPacket&& packet,std::string&) {
      passes.push_back(std::get<graphics::guest::PassPacket>(packet).pass); return true;
    },[&]{++cancellations;}));
    std::string error;
    auto batch=std::make_unique<graphics::guest::WorkBatch>();
    for(int pass:{2,7,3}) {
      graphics::guest::WorkCmd cmd;cmd.op=graphics::guest::Op::kPassEnd;cmd.u[0]=pass;
      batch->cmds.push_back(cmd);
    }
    Check(frontend.SubmitCapturedBatch(std::move(batch),error));
    Check(frontend.Drain(error));
    Check(passes==std::vector<int>{2,7,3});
    frontend.ShutdownWorker();frontend.ShutdownWorker();
    Check(cancellations==1);
    Check(!frontend.SubmitCapturedBatch(std::make_unique<graphics::guest::WorkBatch>(),error));

    graphics::guest::WorkBatch captured;
    captured.bytes={1,2,3,4};captured.ranges.push_back({0x10000,4,0});
    graphics::guest::WorkCmd cmd;cmd.range_count=1;
    graphics::guest::CapturedMemory frozen;
    Check(graphics::guest::CapturedMemory::Capture(captured,cmd,frozen,error));
    captured.bytes[0]=9;captured.Clear();
    auto bytes=frozen.Read(0x10000,4);
    Check(bytes.size()==4 && bytes[0]==1 && bytes[3]==4);
    for(uint32_t address:{0xFFFFu,0xFFFFFFFFu}) {
      bool rejected=false;try{frozen.Read(address,4);}catch(const std::out_of_range&){rejected=true;}Check(rejected);
    }
    native::NativeFrontend sequence;
    std::vector<graphics::guest::RenderPacket> packets;
    Check(sequence.InstallPacketSink([&](graphics::guest::RenderPacket&& packet,std::string&) {packets.push_back(std::move(packet));return true;},[]{}));
    auto fixture=std::make_unique<graphics::guest::WorkBatch>();
    graphics::guest::WorkCmd clear;clear.op=graphics::guest::Op::kClear;clear.u[2]=3;clear.f=0.625f;
    const std::array<float,4> color={0.125f,0.25f,0.5f,0.75f};std::memcpy(clear.u+3,color.data(),16);
    fixture->cmds.push_back(clear);
    graphics::guest::WorkCmd draw;draw.op=graphics::guest::Op::kDraw;draw.u[0]=4;fixture->cmds.push_back(draw);
    fixture->bytes.resize(64);fixture->ranges.push_back({0x1000,64,0});
    const std::array<uint32_t,6> fetch={0x600002,0x11223344,0x55667788,9,10,11};
    for(unsigned i=0;i<6;++i)for(unsigned b=0;b<4;++b)fixture->bytes[0x1c+i*4+b]=uint8_t(fetch[i]>>(24-b*8));
    graphics::guest::WorkCmd resolve;resolve.op=graphics::guest::Op::kResolve;resolve.u[2]=0x1000;resolve.u[6]=2;resolve.u[7]=5;resolve.range_count=1;fixture->cmds.push_back(resolve);
    graphics::guest::WorkCmd swap;swap.op=graphics::guest::Op::kSwap;swap.u64=42;fixture->cmds.push_back(swap);
    Check(sequence.SubmitCapturedBatch(std::move(fixture),error));Check(sequence.Drain(error));sequence.ShutdownWorker();
    Check(packets.size()==4);
    Check(std::get<graphics::guest::ClearPacket>(packets[0]).color==color);
    Check(std::get<graphics::guest::ClearPacket>(packets[0]).depth==0.625f);
    Check(std::holds_alternative<graphics::guest::DrawPacket>(packets[1]));
    const auto& resolved=std::get<graphics::guest::ResolvePacket>(packets[2]);
    Check(resolved.destination_fetch==fetch && resolved.level==2 && resolved.slice==5);
    Check(std::get<graphics::guest::SwapPacket>(packets[3]).guest_swap==42);
    native::NativeFrontend failure;
    Check(failure.InstallPacketSink([](graphics::guest::RenderPacket&&,std::string& error){error="expected rejection";return false;},[]{}));
    auto bad=std::make_unique<graphics::guest::WorkBatch>();bad->cmds.push_back(swap);
    Check(failure.SubmitCapturedBatch(std::move(bad),error));Check(!failure.Drain(error));Check(error=="expected rejection");failure.ShutdownWorker();
    std::puts("PASS frontend: packet ordering, frozen resources, cancellation and stopped rejection");
    return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}

