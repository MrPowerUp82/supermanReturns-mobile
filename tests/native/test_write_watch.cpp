// Characterization tests for the guest write-watch fault path (MMIOHandler).
// Every write to a watched physical page faults once; the handler must notify
// the invalidation callback, unprotect the page and resume the writer, also
// when several threads hit the same pages at the same time. The cost of one
// fault is reported because the handler is the render thread's main hotspot.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <thread>
#include <utility>
#include <vector>

#include <rex/memory/utils.h>
#include <rex/system/xmemory.h>

using rex::memory::BaseHeap;
using rex::memory::Memory;

namespace {

int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

constexpr uint32_t kPage = 0x1000;
constexpr uint32_t kPages = 256;
constexpr uint32_t kAlloc = rex::memory::kMemoryAllocationReserve | rex::memory::kMemoryAllocationCommit;
constexpr uint32_t kRw = rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite;

struct Counters {
  std::atomic<uint32_t> notifications{0};
  std::atomic<uint64_t> notified_bytes{0};
  std::vector<std::atomic<uint8_t>>* page_hits = nullptr;
  uint32_t physical_base = 0;
};

// Exact unwatch: one fault covers one page, so faults are comparable run to run.
std::pair<uint32_t, uint32_t> OnInvalidate(void* context, uint32_t start, uint32_t length, bool) {
  auto* counters = static_cast<Counters*>(context);
  counters->notifications.fetch_add(1, std::memory_order_relaxed);
  counters->notified_bytes.fetch_add(length, std::memory_order_relaxed);
  for (uint32_t offset = start - counters->physical_base; offset < start - counters->physical_base + length;
       offset += kPage) {
    (*counters->page_hits)[offset / kPage].fetch_add(1, std::memory_order_relaxed);
  }
  return {start, length};
}

}  // namespace

// The game process has thousands of mappings below the guest address space,
// which is what makes a maps scan expensive. Alternating protections keeps the
// kernel from merging neighbours; placing them just below `above` puts them
// ahead of the guest mapping in /proc/self/maps order.
void* FragmentAddressSpace(size_t count, void* above) {
  const size_t length = count * kPage;
  const uintptr_t wanted = (reinterpret_cast<uintptr_t>(above) & ~uintptr_t(0xFFFF)) - (64u << 20) - length;
  void* block = mmap(reinterpret_cast<void*>(wanted), length, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
  if (block == MAP_FAILED) return nullptr;
  for (size_t i = 0; i < count; i += 2) {
    mprotect(static_cast<char*>(block) + i * kPage, kPage, PROT_READ);
  }
  return block;
}

int main() {
  Memory memory;
  CHECK(memory.Initialize());
  constexpr size_t kFragments = 6000;
  void* fragments = FragmentAddressSpace(kFragments, memory.TranslateVirtual<uint8_t*>(0));
  CHECK(fragments != nullptr);
  BaseHeap* heap = memory.LookupHeapByType(true, kPage);
  if (!heap) return 1;
  uint32_t alias = 0;
  CHECK(heap->Alloc(kPages * kPage, kPage, kAlloc, kRw, false, &alias));
  if (!alias) return 1;
  const uint32_t physical = (alias & 0x1FFFFFFF) + (alias >= 0xE0000000 ? 0x1000 : 0);
  uint8_t* data = memory.TranslateVirtual<uint8_t*>(alias);

  std::vector<std::atomic<uint8_t>> page_hits(kPages);
  Counters counters;
  counters.page_hits = &page_hits;
  counters.physical_base = physical;
  void* handle = memory.RegisterPhysicalMemoryInvalidationCallback(OnInvalidate, &counters);

  auto rewatch = [&] {
    for (auto& hit : page_hits) hit.store(0);
    counters.notifications.store(0);
    counters.notified_bytes.store(0);
    memory.EnablePhysicalMemoryAccessCallbacks(physical, kPages * kPage, true, false);
  };

  // Single thread: each first write to a watched page notifies exactly once.
  constexpr int kSingleRounds = 20;
  double total_us = 0;
  uint64_t total_faults = 0;
  for (int round = 0; round < kSingleRounds; ++round) {
    rewatch();
    const auto start = std::chrono::steady_clock::now();
    for (uint32_t page = 0; page < kPages; ++page) data[page * kPage] = uint8_t(round + 1);
    total_us += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
    total_faults += counters.notifications.load();
    CHECK(counters.notifications.load() == kPages);
    CHECK(counters.notified_bytes.load() == uint64_t(kPages) * kPage);
    for (uint32_t page = 0; page < kPages; ++page) {
      CHECK(page_hits[page].load() == 1);
      CHECK(data[page * kPage] == uint8_t(round + 1));
    }
    // Unwatched pages are plain memory again: no further notifications.
    for (uint32_t page = 0; page < kPages; ++page) data[page * kPage + 1] = 0x5A;
    CHECK(counters.notifications.load() == kPages);
  }
  const double single_us = total_us / double(total_faults);
  std::printf("write-watch single thread: %.2f us per fault (%llu faults, %zu extra mappings)\n", single_us,
              static_cast<unsigned long long>(total_faults), kFragments);
  // Signal delivery, one mprotect and the callback cost a few microseconds; a
  // per-fault scan of the process maps costs far more with this many mappings.
  CHECK(single_us < 60.0);

  // Stale host protection: the page is read-only on the host although the guest
  // marks it writable and nothing watches it. The handler must still restore
  // write access instead of treating the fault as a real violation.
  {
    uint8_t* stale = data + 7 * kPage;
    CHECK(rex::memory::Protect(stale, kPage, rex::memory::PageAccess::kReadOnly));
    stale[0] = 0x77;
    CHECK(stale[0] == 0x77);
    stale[1] = 0x78;  // writable again, no further fault
    CHECK(stale[1] == 0x78);
  }

  // Several threads write the same pages at once (the benign race the handler's
  // recheck exists for). Nothing may crash or get lost, and a page notifies at
  // most once per watch.
  constexpr int kThreads = 4;
  constexpr int kRaceRounds = 60;
  for (int round = 0; round < kRaceRounds; ++round) {
    rewatch();
    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&, t] {
        ready.fetch_add(1);
        while (!go.load()) std::this_thread::yield();
        for (uint32_t i = 0; i < kPages; ++i) {
          const uint32_t page = (t & 1) ? (kPages - 1 - i) : i;  // opposite directions meet in the middle
          data[page * kPage + 16 + t] = uint8_t(0xA0 + t);
        }
      });
    }
    while (ready.load() < kThreads) std::this_thread::yield();
    go.store(true);
    for (auto& thread : threads) thread.join();
    CHECK(counters.notifications.load() <= kPages);
    for (uint32_t page = 0; page < kPages; ++page) {
      CHECK(page_hits[page].load() == 1);
      for (int t = 0; t < kThreads; ++t) CHECK(data[page * kPage + 16 + t] == uint8_t(0xA0 + t));
    }
  }

  memory.UnregisterPhysicalMemoryInvalidationCallback(handle);
  heap->Release(alias);
  if (fragments) munmap(fragments, kFragments * kPage);
  if (failures) {
    std::fprintf(stderr, "%d write-watch check(s) failed\n", failures);
    return 1;
  }
  std::puts("write-watch tests passed");
  return 0;
}
