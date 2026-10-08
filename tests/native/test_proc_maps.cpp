// Regression tests for the /proc/self/maps scanner used by QueryProtect.
// QueryProtect runs inside the guest write-watch fault handler, so its cost is
// paid on every watched-page fault. These tests pin the parsing semantics of
// the former ifstream/sscanf implementation and the properties the fast path
// relies on (chunk independence, early exit, over-long lines).
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "core/proc_maps.h"

using rex::memory::detail::FindProcMapsEntry;
using rex::memory::detail::IsProcMapsRangeMapped;
using rex::memory::detail::ParseProcMapsLine;
using rex::memory::detail::ProcMapsEntry;

namespace {

int failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

// Feeds a text blob to the scanner in fixed-size chunks and counts bytes served.
struct ChunkedSource {
  const std::string& text;
  size_t chunk;
  size_t offset = 0;
  std::ptrdiff_t operator()(char* buffer, size_t capacity) {
    const size_t n = std::min({chunk, capacity, text.size() - offset});
    std::memcpy(buffer, text.data() + offset, n);
    offset += n;
    return static_cast<std::ptrdiff_t>(n);
  }
};

const char kMaps[] =
    "00400000-00452000 r-xp 00000000 08:02 173521  /usr/bin/dbus-daemon\n"
    "00651000-00652000 r--p 00051000 08:02 173521  /usr/bin/dbus-daemon\n"
    "00652000-00655000 rw-p 00052000 08:02 173521  /usr/bin/dbus-daemon\n"
    "00e03000-00e24000 rw-p 00000000 00:00 0       [heap]\n"
    "7f0000000000-7f0000021000 ---p 00000000 00:00 0\n"
    "7f0000021000-7f0000022000 rwxp 00000000 00:00 0\n"
    "ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0 [vsyscall]";  // no trailing newline

void TestParseLine() {
  ProcMapsEntry e;
  const char line[] = "00400000-00452000 r-xp 00000000 08:02 173521  /usr/bin/dbus-daemon";
  CHECK(ParseProcMapsLine(line, sizeof(line) - 1, e));
  CHECK(e.start == 0x400000 && e.end == 0x452000);
  CHECK(std::strcmp(e.perms, "r-xp") == 0);

  const char high[] = "7ffd1c3d5000-7ffd1c3f6000 rw-p 00000000 00:00 0 [stack]";
  CHECK(ParseProcMapsLine(high, sizeof(high) - 1, e));
  CHECK(e.start == 0x7ffd1c3d5000ULL && e.end == 0x7ffd1c3f6000ULL);

  const char bare[] = "1000-2000 rw-p";
  CHECK(ParseProcMapsLine(bare, sizeof(bare) - 1, e));
  CHECK(e.start == 0x1000 && e.end == 0x2000);

  // Rejections: the old sscanf("%llx-%llx %4s") needed all three fields and start < end.
  const char* bad[] = {"", "garbage", "1000-2000", "1000-2000 ", "1000 2000 rw-p",
                       "-2000 rw-p", "1000- rw-p", "2000-1000 rw-p", "1000-1000 rw-p",
                       "zzzz-2000 rw-p"};
  for (const char* text : bad) {
    CHECK(!ParseProcMapsLine(text, std::strlen(text), e));
  }
  // Hex overflow (17 digits) must be rejected rather than wrap.
  const char wide[] = "10000000000000000-10000000000000001 rw-p";
  CHECK(!ParseProcMapsLine(wide, sizeof(wide) - 1, e));
}

void TestFindAcrossChunkSizes() {
  const std::string text(kMaps);
  for (size_t chunk : {size_t{1}, size_t{2}, size_t{7}, size_t{64}, size_t{4096}, size_t{1} << 20}) {
    ProcMapsEntry e;
    ChunkedSource inside{text, chunk};
    CHECK(FindProcMapsEntry(inside, 0x651800, e));
    CHECK(e.start == 0x651000 && e.end == 0x652000 && std::strcmp(e.perms, "r--p") == 0);

    ChunkedSource first_byte{text, chunk};
    CHECK(FindProcMapsEntry(first_byte, 0x400000, e) && e.end == 0x452000);
    ChunkedSource last_byte{text, chunk};
    CHECK(FindProcMapsEntry(last_byte, 0x451fff, e) && e.end == 0x452000);
    ChunkedSource at_end{text, chunk};  // end is exclusive: belongs to the next entry
    CHECK(FindProcMapsEntry(at_end, 0x652000, e) && e.start == 0x652000);
    ChunkedSource rwx{text, chunk};
    CHECK(FindProcMapsEntry(rwx, 0x7f0000021800ULL, e) && std::strcmp(e.perms, "rwxp") == 0);
    ChunkedSource unterminated{text, chunk};  // last line has no newline
    CHECK(FindProcMapsEntry(unterminated, 0xffffffffff600800ULL, e) && e.start == 0xffffffffff600000ULL);

    ChunkedSource gap{text, chunk};
    CHECK(!FindProcMapsEntry(gap, 0x500000, e));
    ChunkedSource below{text, chunk};
    CHECK(!FindProcMapsEntry(below, 0x1000, e));
    ChunkedSource above{text, chunk};
    CHECK(!FindProcMapsEntry(above, 0x7fffffffffffULL, e));
  }
}

void TestEarlyExit() {
  // 20000 sorted mappings; the target is in the first one. The scanner must stop
  // reading long before consuming the whole file.
  std::string text;
  char line[96];
  for (uintptr_t i = 0; i < 20000; ++i) {
    std::snprintf(line, sizeof(line), "%08llx-%08llx rw-p 00000000 00:00 0\n",
                  static_cast<unsigned long long>(0x10000 + i * 0x2000),
                  static_cast<unsigned long long>(0x10000 + i * 0x2000 + 0x1000));
    text += line;
  }
  ProcMapsEntry e;
  ChunkedSource hit{text, 4096};
  CHECK(FindProcMapsEntry(hit, 0x10800, e));
  CHECK(hit.offset <= 8192);

  // An address in a gap before a later mapping must stop at the first entry
  // that starts above it, not scan to EOF.
  ChunkedSource gap{text, 4096};
  CHECK(!FindProcMapsEntry(gap, 0x11800, e));
  CHECK(gap.offset <= 8192);

  // A miss beyond the last mapping legitimately reads everything.
  ChunkedSource miss{text, 4096};
  CHECK(!FindProcMapsEntry(miss, 0x7000000000ULL, e));
  CHECK(miss.offset == text.size());
}

void TestLongLines() {
  // A path longer than the scanner buffer must not hide the next mapping.
  for (size_t chunk : {size_t{1}, size_t{13}, size_t{4096}, size_t{1} << 20}) {
    std::string text = "1000-2000 r-xp 00000000 08:02 1  /" + std::string(20000, 'x') + "\n";
    text += "3000-4000 rw-p 00000000 00:00 0\n";
    text += "5000-6000 r--p 00000000 08:02 1  /" + std::string(4095, 'y');  // long, unterminated
    ProcMapsEntry e;
    ChunkedSource a{text, chunk};
    CHECK(FindProcMapsEntry(a, 0x1800, e) && e.end == 0x2000);  // prefix of the long line is parsed
    ChunkedSource b{text, chunk};
    CHECK(FindProcMapsEntry(b, 0x3800, e) && std::strcmp(e.perms, "rw-p") == 0);
    ChunkedSource c{text, chunk};
    CHECK(FindProcMapsEntry(c, 0x5800, e) && e.end == 0x6000);
    ChunkedSource d{text, chunk};
    CHECK(!FindProcMapsEntry(d, 0x2800, e));
  }
}

void TestRangeFullyMapped() {
  const std::string text(
      "1000-2000 rw-p 0 0:0 0\n2000-3000 r--p 0 0:0 0\n3000-4000 rw-p 0 0:0 0\n"
      "6000-7000 rw-p 0 0:0 0\n");
  for (size_t chunk : {size_t{1}, size_t{5}, size_t{4096}}) {
    auto run = [&](uintptr_t b, uintptr_t e) {
      ChunkedSource s{text, chunk};
      return IsProcMapsRangeMapped(s, b, e);
    };
    CHECK(run(0x1000, 0x4000));   // spans three adjacent mappings
    CHECK(run(0x1800, 0x2800));   // spans a boundary
    CHECK(run(0x3000, 0x4000));   // exact single mapping
    CHECK(!run(0x1000, 0x4001));  // one byte past the last adjacent mapping
    CHECK(!run(0x3800, 0x6800));  // gap 0x4000-0x6000
    CHECK(!run(0x500, 0x1800));   // starts before the first mapping
    CHECK(!run(0x5000, 0x5800));  // entirely in a gap
    CHECK(!run(0x7000, 0x7800));  // past the last mapping
    CHECK(run(0x6000, 0x7000));
  }
  ChunkedSource s{text, 64};
  CHECK(!IsProcMapsRangeMapped(s, 0x2000, 0x2000));  // empty range is not "mapped"
  ChunkedSource wrap{text, 64};
  CHECK(!IsProcMapsRangeMapped(wrap, 0x2000, 0x1000));
}

// Reference: the previous implementation, verbatim in behaviour.
bool ReferenceFind(uintptr_t addr, ProcMapsEntry& out) {
  std::ifstream maps("/proc/self/maps");
  if (!maps.is_open()) return false;
  std::string line;
  while (std::getline(maps, line)) {
    unsigned long long start = 0, end = 0;
    char perms[5] = {};
    if (std::sscanf(line.c_str(), "%llx-%llx %4s", &start, &end, perms) < 3) continue;
    if (start >= end) continue;
    if (addr >= start && addr < end) {
      out.start = start;
      out.end = end;
      std::memcpy(out.perms, perms, sizeof(out.perms));
      return true;
    }
  }
  return false;
}

int Anchor() { return 7; }

void TestAgainstLiveProcess() {
  static int global_value = 1;
  int stack_value = 2;
  std::vector<char>* heap = new std::vector<char>(1 << 20);
  const uintptr_t probes[] = {
      reinterpret_cast<uintptr_t>(&stack_value), reinterpret_cast<uintptr_t>(&global_value),
      reinterpret_cast<uintptr_t>(heap->data()), reinterpret_cast<uintptr_t>(&Anchor),
      reinterpret_cast<uintptr_t>(&std::printf), 0x1000, 0xfffffffffffff000ULL,
  };
  for (uintptr_t probe : probes) {
    ProcMapsEntry expected, actual;
    const bool want = ReferenceFind(probe, expected);
    const bool got = FindProcMapsEntry(
        [fp = std::fopen("/proc/self/maps", "r")](char* buffer, size_t capacity) mutable {
          if (!fp) return std::ptrdiff_t{-1};
          const size_t n = std::fread(buffer, 1, capacity, fp);
          if (n == 0) { std::fclose(fp); fp = nullptr; }
          return static_cast<std::ptrdiff_t>(n);
        },
        probe, actual);
    CHECK(want == got);
    if (want && got) {
      CHECK(expected.start == actual.start && expected.end == actual.end);
      CHECK(std::strcmp(expected.perms, actual.perms) == 0);
    }
  }
  delete heap;
}

}  // namespace

int main() {
  TestParseLine();
  TestFindAcrossChunkSizes();
  TestEarlyExit();
  TestLongLines();
  TestRangeFullyMapped();
  TestAgainstLiveProcess();
  if (failures) {
    std::fprintf(stderr, "%d proc maps check(s) failed\n", failures);
    return 1;
  }
  std::puts("proc maps scanner tests passed");
  return 0;
}
