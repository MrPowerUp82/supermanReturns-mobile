// Tests for the guest SwitchToThread (NtYieldExecution) backoff policy.
// Guest spin-wait loops call yield millions of times per second. On the phone
// those idle loops kept four cores permanently runnable and pushed the render,
// replay and recording threads onto the little cores. The policy turns a long
// run of rapid yields into a short sleep, and must leave every other pattern
// (an occasional yield, a loop that does real work between yields) untouched.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>

#include "core/yield_backoff.h"

using rex::thread::detail::YieldBackoff;
using rex::thread::detail::YieldBackoffConfig;

namespace {

int failures = 0;

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                          \
    }                                                                      \
  } while (0)

constexpr uint64_t kUs = 1000;

}  // namespace

int main() {
  const YieldBackoffConfig on{/*sleep_us=*/50, /*streak=*/8, /*gap_us=*/200};
  const YieldBackoffConfig off{};

  // Disabled by default: never sleeps, whatever the pattern.
  {
    YieldBackoff backoff;
    uint64_t now = 1000 * kUs;
    for (int i = 0; i < 1000; ++i, now += kUs) CHECK(backoff.OnYield(now, off) == 0);
  }

  // A tight run of yields starts sleeping once the streak is reached, not before.
  {
    YieldBackoff backoff;
    uint64_t now = 1000 * kUs;
    for (int i = 0; i < 8; ++i, now += kUs) CHECK(backoff.OnYield(now, on) == 0);
    CHECK(backoff.OnYield(now, on) == 50);
    now += 60 * kUs;  // the sleep itself must not break the streak
    CHECK(backoff.OnYield(now, on) == 50);
    now += 60 * kUs;
    CHECK(backoff.OnYield(now, on) == 50);
  }

  // A pause longer than the gap ends the streak: spinning resumes only after
  // a fresh run, so a thread that yields now and then keeps the plain yield.
  {
    YieldBackoff backoff;
    uint64_t now = 1000 * kUs;
    for (int i = 0; i < 20; ++i, now += kUs) backoff.OnYield(now, on);
    CHECK(backoff.OnYield(now, on) == 50);
    now += 5000 * kUs;
    CHECK(backoff.OnYield(now, on) == 0);
    for (int i = 0; i < 7; ++i) {
      now += kUs;
      CHECK(backoff.OnYield(now, on) == 0);
    }
    now += kUs;
    CHECK(backoff.OnYield(now, on) == 50);
  }

  // Yielding rarely never accumulates a streak.
  {
    YieldBackoff backoff;
    uint64_t now = 1000 * kUs;
    for (int i = 0; i < 100; ++i, now += 10000 * kUs) CHECK(backoff.OnYield(now, on) == 0);
  }

  // A loop that does a few hundred microseconds of work per iteration is not a
  // pure spin and keeps yielding normally.
  {
    YieldBackoff backoff;
    uint64_t now = 1000 * kUs;
    for (int i = 0; i < 100; ++i, now += 400 * kUs) CHECK(backoff.OnYield(now, on) == 0);
  }

  // Time never moves backwards in practice; if it does, treat it as a new run.
  {
    YieldBackoff backoff;
    uint64_t now = 5000 * kUs;
    for (int i = 0; i < 20; ++i, now += kUs) backoff.OnYield(now, on);
    CHECK(backoff.OnYield(1000 * kUs, on) == 0);
  }

  // Environment parsing: absent or invalid values keep the backoff disabled.
  CHECK(YieldBackoffConfig::Parse(nullptr, nullptr, nullptr).sleep_us == 0);
  CHECK(YieldBackoffConfig::Parse("", nullptr, nullptr).sleep_us == 0);
  CHECK(YieldBackoffConfig::Parse("abc", nullptr, nullptr).sleep_us == 0);
  CHECK(YieldBackoffConfig::Parse("-5", nullptr, nullptr).sleep_us == 0);
  CHECK(YieldBackoffConfig::Parse("0", nullptr, nullptr).sleep_us == 0);
  const auto parsed = YieldBackoffConfig::Parse("25", "16", "300");
  CHECK(parsed.sleep_us == 25 && parsed.streak == 16 && parsed.gap_us == 300);
  const auto defaults = YieldBackoffConfig::Parse("25", nullptr, nullptr);
  CHECK(defaults.streak == 32 && defaults.gap_us == 200);
  CHECK(YieldBackoffConfig::Parse("100000", nullptr, nullptr).sleep_us == 1000);  // capped

  if (failures) {
    std::fprintf(stderr, "%d yield backoff check(s) failed\n", failures);
    return 1;
  }
  std::puts("yield backoff tests passed");
  return 0;
}
