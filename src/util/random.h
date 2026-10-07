#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace p2lt {
namespace util {

bool random_bytes(uint8_t* out, size_t n);
uint64_t random_u64();

void sha256_simple(const uint8_t* data, size_t len, uint8_t out[32]);

}  // namespace util
}  // namespace p2lt