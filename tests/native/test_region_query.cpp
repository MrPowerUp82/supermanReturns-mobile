// Regression tests for the bounded BaseHeap::QueryRegionInfo scan.
// ValidatedGuestPointer asks for a few bytes per draw, but QueryRegionInfo used
// to walk the whole region page by page. A caller-supplied limit must stop the
// walk early without changing what the caller sees when the limit is not hit.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <rex/system/xmemory.h>

using rex::memory::BaseHeap;
using rex::memory::HeapAllocationInfo;
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

constexpr uint32_t kRegion = 64u << 20;
constexpr uint32_t kPage = 0x1000;
constexpr uint32_t kAlloc = rex::memory::kMemoryAllocationReserve | rex::memory::kMemoryAllocationCommit;
constexpr uint32_t kRw = rex::memory::kMemoryProtectRead | rex::memory::kMemoryProtectWrite;

uint32_t RegionSize(BaseHeap* heap, uint32_t address, uint32_t limit, bool* ok = nullptr) {
  HeapAllocationInfo info{};
  const bool result = limit ? heap->QueryRegionInfo(address, &info, limit)
                            : heap->QueryRegionInfo(address, &info);
  if (ok) *ok = result;
  return static_cast<uint32_t>(info.region_size);
}

}  // namespace

int main() {
  Memory memory;
  CHECK(memory.Initialize());
  BaseHeap* heap = memory.LookupHeapByType(true, kPage);
  CHECK(heap != nullptr);
  if (!heap) return 1;
  CHECK(heap->page_size() == kPage);

  uint32_t base = 0;
  CHECK(heap->Alloc(kRegion, kPage, kAlloc, kRw, false, &base));
  if (!base) return 1;

  // Unbounded behaviour is unchanged: the whole committed region is reported.
  CHECK(RegionSize(heap, base, 0) == kRegion);
  CHECK(RegionSize(heap, base + 0x10000, 0) == kRegion - 0x10000);

  // A limit stops the scan at the first page boundary at or above it.
  CHECK(RegionSize(heap, base, kPage) == kPage);
  CHECK(RegionSize(heap, base, 1) == kPage);
  CHECK(RegionSize(heap, base, kPage + 1) == 2 * kPage);
  CHECK(RegionSize(heap, base, 0x10000) == 0x10000);
  CHECK(RegionSize(heap, base + 2 * kPage, 0x100) == kPage);

  // A limit beyond the region reports the real extent.
  CHECK(RegionSize(heap, base, kRegion) == kRegion);
  CHECK(RegionSize(heap, base, 0xFFFFFFFFu) == kRegion);
  CHECK(RegionSize(heap, base + kRegion - kPage, 0x10000) == kPage);

  // A protection change inside the region still ends the extent when the limit
  // reaches past it, exactly like the unbounded query.
  const uint32_t split = base + 0x40000;
  CHECK(heap->Protect(split, 0x4000, rex::memory::kMemoryProtectRead));
  CHECK(RegionSize(heap, base, 0) == 0x40000);
  CHECK(RegionSize(heap, base, 0x100000) == 0x40000);
  CHECK(RegionSize(heap, base, 0x20000) == 0x20000);
  CHECK(RegionSize(heap, split, 0) == 0x4000);
  CHECK(RegionSize(heap, split, 0x100000) == 0x4000);
  CHECK(RegionSize(heap, split + 0x4000, 0) == kRegion - 0x44000);
  CHECK(RegionSize(heap, split + 0x4000, 0x100000) == 0x100000);  // limit reached before the end

  // The bounded query reports the same attributes as the unbounded one.
  HeapAllocationInfo full{}, bounded{};
  CHECK(heap->QueryRegionInfo(base, &full));
  CHECK(heap->QueryRegionInfo(base, &bounded, 0x1000));
  CHECK(full.state == bounded.state && full.protect == bounded.protect);
  CHECK(full.allocation_base == bounded.allocation_base);
  CHECK(full.allocation_protect == bounded.allocation_protect);

  // Cost: a 64 MB region is 16384 pages; the bounded scan must not pay for them.
  // The expected gap is over 1000x, so a 20x margin is not sensitive to noise.
  CHECK(heap->Protect(split, 0x4000, kRw));
  constexpr int kCalls = 2000;
  auto time = [&](uint32_t limit) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kCalls; ++i) (void)RegionSize(heap, base, limit);
    return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / kCalls;
  };
  const double unbounded_us = time(0);
  const double bounded_us = time(kPage);
  std::printf("QueryRegionInfo 64MB region: unbounded %.2f us, bounded %.2f us per call\n", unbounded_us,
              bounded_us);
  CHECK(unbounded_us > 20.0 * bounded_us);

  heap->Release(base);
  if (failures) {
    std::fprintf(stderr, "%d region query check(s) failed\n", failures);
    return 1;
  }
  std::puts("region query tests passed");
  return 0;
}
