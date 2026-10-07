#pragma once
#include <vector>
#include <filesystem>
#include <fstream>
#include <stdexcept>
namespace shader_fixture {
static void Check(bool ok){if(!ok)throw std::runtime_error("shader fixture write failed");}
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
}
