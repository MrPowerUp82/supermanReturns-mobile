// Integration test: the real NtYieldExecution entry point with the backoff on.
// A guest spin loop must stop burning a core, and it must still notice a flag
// quickly, because waiting workers rely on picking up jobs without delay.
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <vector>

namespace rex::kernel::xboxkrnl {
uint32_t NtYieldExecution_entry();
}

namespace {

int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

double ThreadCpuSeconds() {
  timespec ts{};
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  return double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9;
}

}  // namespace

int main() {
  // Read once by the runtime on the first yield.
  setenv("SR_YIELD_BACKOFF_US", "50", 1);

  // 1. The yield still reports success.
  CHECK(rex::kernel::xboxkrnl::NtYieldExecution_entry() == 0);

  // 2. A pure spin loop uses a small fraction of a core.
  double cpu_fraction = 1.0;
  uint64_t spins = 0;
  {
    std::atomic<bool> stop{false};
    double cpu = 0, wall = 0;
    std::thread spinner([&] {
      const double cpu_start = ThreadCpuSeconds();
      const auto wall_start = std::chrono::steady_clock::now();
      while (!stop.load(std::memory_order_relaxed)) {
        rex::kernel::xboxkrnl::NtYieldExecution_entry();
        ++spins;
      }
      cpu = ThreadCpuSeconds() - cpu_start;
      wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wall_start).count();
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    stop.store(true);
    spinner.join();
    cpu_fraction = cpu / wall;
  }
  std::printf("spin loop: %.1f%% of a core, %llu yields in 400 ms\n", cpu_fraction * 100.0,
              static_cast<unsigned long long>(spins));
  CHECK(cpu_fraction < 0.30);

  // 3. A waiting loop notices a flag within a bounded delay.
  std::vector<double> delays_us;
  for (int trial = 0; trial < 60; ++trial) {
    std::atomic<bool> flag{false};
    std::atomic<int64_t> seen_ns{0};
    std::thread waiter([&] {
      while (!flag.load(std::memory_order_acquire)) rex::kernel::xboxkrnl::NtYieldExecution_entry();
      seen_ns.store(std::chrono::steady_clock::now().time_since_epoch().count());
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    const int64_t set_ns = std::chrono::steady_clock::now().time_since_epoch().count();
    flag.store(true, std::memory_order_release);
    waiter.join();
    delays_us.push_back(double(seen_ns.load() - set_ns) / 1000.0);
  }
  std::sort(delays_us.begin(), delays_us.end());
  const double median = delays_us[delays_us.size() / 2];
  const double p95 = delays_us[delays_us.size() * 95 / 100];
  std::printf("wake latency: median %.0f us, p95 %.0f us\n", median, p95);
  CHECK(median < 500.0);
  CHECK(p95 < 2000.0);

  if (failures) {
    std::fprintf(stderr, "%d yield spin check(s) failed\n", failures);
    return 1;
  }
  std::puts("yield spin tests passed");
  return 0;
}
