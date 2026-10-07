#pragma once

#include <cstdint>
#include <cstdlib>

namespace p2lt {
namespace util {

inline int64_t now_ms() {
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

inline int64_t now_us() {
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace util
}  // namespace p2lt