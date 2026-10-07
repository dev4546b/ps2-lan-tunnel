#include "util/random.h"

#include <windows.h>
#include <bcrypt.h>

namespace p2lt {
namespace util {

bool random_bytes(uint8_t* out, size_t n) {
  if (n == 0) return true;
  NTSTATUS st = BCryptGenRandom(nullptr, out, static_cast<ULONG>(n), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  return st == 0;
}

uint64_t random_u64() {
  uint64_t v = 0;
  if (!random_bytes(reinterpret_cast<uint8_t*>(&v), sizeof(v))) v = 0;
  return v;
}

}  // namespace util
}  // namespace p2lt