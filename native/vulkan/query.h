#pragma once
#include "loader.h"
#include <vector>
namespace superman_returns::graphics::vulkan {
template <class T, class F>
bool Query(F fn, std::vector<T> &out, const char *name, Error &e) {
  for (int tries = 0; tries < 4; ++tries) {
    uint32_t count = 0;
    auto r = fn(&count, static_cast<T *>(nullptr));
    if (r != VK_SUCCESS && r != VK_INCOMPLETE)
      return Check(r, name, e);
    out.resize(count);
    r = fn(&count, out.empty() ? nullptr : out.data());
    if (r == VK_SUCCESS) {
      out.resize(count);
      return true;
    }
    if (r != VK_INCOMPLETE)
      return Check(r, name, e);
  }
  e = {name, VK_INCOMPLETE, "Enumeration changed repeatedly"};
  return false;
}
} // namespace superman_returns::graphics::vulkan
