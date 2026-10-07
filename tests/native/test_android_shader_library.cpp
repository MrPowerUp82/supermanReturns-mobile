#include "vulkan_shader_service.h"
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <source_location>
using namespace superman_returns::graphics::shaders;
static void Check(bool ok,std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("assertion line "+std::to_string(at.line()));}
static void Word(std::vector<uint8_t>& out,uint32_t n){for(int i=0;i<4;++i)out.push_back(uint8_t(n>>(i*8)));}
static void Write(const std::filesystem::path& path,const std::vector<uint8_t>& data){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),data.size());Check(bool(file));}
static uint64_t Hash(const std::vector<uint8_t>& bytes){uint64_t h=14695981039346656037ull;for(auto b:bytes){h^=b;h*=1099511628211ull;}return h;}
static std::vector<uint8_t> Library(const std::vector<uint8_t>& container,uint32_t result_stage=0,uint32_t abi=1){
  std::vector<uint8_t> wire;for(uint32_t n:{0x33525653u,abi,1u,result_stage})Word(wire,n);
  for(int i=0;i<12;++i)Word(wire,0);Word(wire,0);Word(wire,0);Word(wire,20);
  for(uint32_t n:{0x07230203u,0x10300u,0u,1u,0u})Word(wire,n);
  std::vector<uint8_t> body;Word(body,0);Word(body,uint32_t(container.size()));Word(body,uint32_t(wire.size()));Word(body,0);body.insert(body.end(),container.begin(),container.end());body.insert(body.end(),wire.begin(),wire.end());
  std::vector<uint8_t> out={'S','R','V','K','L','I','B',0};Word(out,1);Word(out,1);auto hash=Hash(body);Word(out,uint32_t(hash));Word(out,uint32_t(hash>>32));out.insert(out.end(),body.begin(),body.end());return out;
}
int main(){try{
  auto directory=std::filesystem::temp_directory_path()/("sr-shader-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(directory);
  VulkanShaderConfig config;config.precompiled_only=true;config.library=directory/"test.srvk";config.missing_dump_dir=directory/"missing";
  unsigned calls=0;ShaderProcess runner=[&](auto,auto,auto,std::string&){++calls;return false;};
  std::vector<uint8_t> container(24,1);auto library=Library(container);Write(config.library,library);
  {VulkanShaderService service(config,runner);Check(service.LibraryDiagnostic().empty());Check(service.PrecompiledCount()==1);auto key=service.Request(container,ShaderStage::kVertex);Check(service.Poll(key).status==ShaderPoll::ready);
   auto missing=container;missing[0]=2;auto miss=service.Request(missing,ShaderStage::kPixel);auto result=service.Poll(miss);Check(result.status==ShaderPoll::failed);Check(result.diagnostic.find("precompiled")!=std::string::npos);Check(calls==0);
   auto binary=config.missing_dump_dir/(std::to_string(miss)+".ps.bin");std::ifstream file(binary,std::ios::binary);std::vector<uint8_t> dumped((std::istreambuf_iterator<char>(file)),{});Check(dumped==missing);Check(std::filesystem::is_regular_file(config.missing_dump_dir/(std::to_string(miss)+".json")));}
  for(auto invalid:{std::vector<uint8_t>{},std::vector<uint8_t>(library.begin(),library.end()-1),Library(container,1),Library(container,0,2)}){Write(config.library,invalid);VulkanShaderService service(config,runner);Check(service.PrecompiledCount()==0);Check(!service.LibraryDiagnostic().empty());}
  Check(calls==0);std::filesystem::remove_all(directory);std::puts("PASS offline shader library: ready, recoverable miss, truncation, stage and ABI rejection; zero processes");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
