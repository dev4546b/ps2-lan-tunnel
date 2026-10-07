#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace p2lt {
namespace util {

std::string hex_encode(const uint8_t* data, size_t len, bool upper = true);
bool hex_decode(const std::string& s, std::vector<uint8_t>& out);

}  // namespace util
}  // namespace p2lt