#include "guest_reads_android.h"
#include <rex/system/xmemory.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
using namespace superman_returns::native;
static void Check(bool value) {if(!value) throw std::runtime_error("guest read assertion failed");}
int main() {
  try {
    rex::memory::Memory memory;Check(memory.Initialize());
    std::string error;std::vector<uint8_t> bytes;
    Check(!ReadGuestBytes(memory,0xFFFFFFFE,8,bytes,error) && bytes.empty());
    Check(!ReadGuestBytes(memory,0x7F001000,4,bytes,error) && bytes.empty());
    auto heap=memory.LookupHeap(0x10000000);
    Check(heap->AllocFixed(0x10000000,0x1000,0x1000,
      rex::memory::kMemoryAllocationReserve|rex::memory::kMemoryAllocationCommit,
      rex::memory::kMemoryProtectRead|rex::memory::kMemoryProtectWrite));
    std::memcpy(memory.TranslateVirtual<uint8_t*>(0x10000000),"abcd",4);
    Check(ReadGuestBytes(memory,0x10000000,4,bytes,error));
    Check(bytes==std::vector<uint8_t>({'a','b','c','d'}));
    Check(!ReadGuestBytes(memory,0x10000FFE,4,bytes,error) && bytes.empty());
    uint32_t alias=0;
    Check(memory.LookupHeapByType(true,0x1000)->Alloc(0x1000,0x1000,
      rex::memory::kMemoryAllocationReserve|rex::memory::kMemoryAllocationCommit,
      rex::memory::kMemoryProtectRead|rex::memory::kMemoryProtectWrite,false,&alias));
    std::memcpy(memory.TranslateVirtual<uint8_t*>(alias),"WXYZ",4);
    const uint32_t physical=(alias&0x1FFFFFFF)+(alias>=0xE0000000?0x1000:0);
    Check(ReadGuestBytes(memory,0xA0000000+physical,4,bytes,error));
    Check(bytes==std::vector<uint8_t>({'W','X','Y','Z'}));
    Check(ReadGuestBytes(memory,alias,4,bytes,error));
    Check(bytes==std::vector<uint8_t>({'W','X','Y','Z'}));
    std::puts("PASS guest reads: overflow, unmapped, committed limits and physical aliases");
    return 0;
  } catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
