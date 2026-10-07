#include "util/hex.h"

#include <cctype>
#include <vector>

namespace p2lt {
namespace util {

std::string hex_encode(const uint8_t* data, size_t len, bool upper) {
  static const char* lc = "0123456789abcdef";
  static const char* uc = "0123456789ABCDEF";
  const char* tbl = upper ? uc : lc;
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out.push_back(tbl[data[i] >> 4]);
    out.push_back(tbl[data[i] & 0x0F]);
  }
  return out;
}

bool hex_decode(const std::string& s, std::vector<uint8_t>& out) {
  std::string t;
  t.reserve(s.size());
  for (char c : s) {
    if (c == ' ' || c == '-' || c == ':') continue;
    if (!isxdigit(static_cast<unsigned char>(c))) return false;
    t.push_back(static_cast<char>(tolower(static_cast<unsigned char>(c))));
  }
  if (t.size() % 2 != 0) return false;
  out.clear();
  out.reserve(t.size() / 2);
  auto val = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    return c - 'a' + 10;
  };
  for (size_t i = 0; i < t.size(); i += 2) {
    out.push_back(static_cast<uint8_t>((val(t[i]) << 4) | val(t[i + 1])));
  }
  return true;
}

}  // namespace util
}  // namespace p2lt