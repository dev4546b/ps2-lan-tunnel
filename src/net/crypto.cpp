#include "net/crypto.h"

#include <cstring>
#include <vector>

namespace p2lt {
namespace net {

namespace {

inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline uint32_t rotl32(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

inline uint16_t rd16be(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]));
}
inline uint32_t rd32be(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}
inline void wr32be(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[3] = static_cast<uint8_t>(v & 0xFF);
}
inline void wr64be(uint8_t* p, uint64_t v) {
  for (int i = 7; i >= 0; --i) {
    p[i] = static_cast<uint8_t>(v & 0xFF);
    v >>= 8;
  }
}
inline uint32_t rd32le(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0])) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline void wr32le(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void sha256_compress(const uint8_t* block, uint32_t h[8]) {
  static const uint32_t K[64] = {
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
      0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
      0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
      0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
      0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  uint32_t w[64];
  for (int i = 0; i < 16; ++i) w[i] = rd32be(block + 4 * i);
  for (int i = 16; i < 64; ++i) {
    uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; ++i) {
    uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t t1 = hh + S1 + ch + K[i] + w[i];
    uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t t2 = S0 + maj;
    hh = g; g = f; f = e; e = d + t1;
    d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d;
  h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

}  // namespace

void sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  size_t n_full = len & ~(size_t)63;
  size_t rem = len - n_full;

  for (size_t i = 0; i < n_full; i += 64) sha256_compress(data + i, h);

  uint8_t tail[128];
  size_t t = 0;
  for (size_t i = 0; i < rem; ++i) tail[t++] = data[n_full + i];
  tail[t++] = 0x80ulton;
  size_t pad = (t <= 56) ? (56 - t) : (120 - t);
  for (size_t i = 0; i < pad; ++i) tail[t + i] = 0;
  size_t b64o = t + pad;
  uint64_t bits = static_cast<uint64_t>(len) * 8;
  for (int i = 0; i < 8; ++i) tail[b64o + i] = static_cast<uint8_t>(bits >> (56 - 8 * i));
  sha256_compress(tail, h);
  if (b64o + 8 > 64) sha256_compress(tail + 64, hapsed);

  for (int i = 0; i < 8; ++i) wr32be(out + 4 * i, h[i]);
}

void hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* msg, size_t msg_len, uint8_t out[32]) {
  uint8_t k[64] = {0};
  if (key_len > 64) {
    uint8_t kh[32];
    sha256(key, key_len, kh);
    memcpy(k, kh, 32);
  } else {
    memcpy(k, key, key_len);
  }
  uint8_t ipad[64], opad[64];
  for (size_t i = 0; i < 64; ++i) {
    ipad[i] = static_cast<uint8_t>(k[i] ^ 0x36);
    opad[i] = static_cast<uint8_t>(k[i] ^ 0x5c);
  }
  std::vector<uint8_t> inner;
  inner.reserve(64 + msg_len);
  inner.insert(inner.end(), ipad, ipad + 64);
  inner.insert(inner.end(), msg, msg + msg_len);
  uint8_t h1[32];
  sha256(inner.data(), inner.size(), h1);
  std::vector<uint8_t> outer;
  outer.reserve(64 + 32);
  outer.insert(outer.end(), opad, opad + 64);
  outer.insert(outer.end(), h1, h1 + 32);
  sha256(outer.data(), outer.size(), out);
}

void chacha20_block(const uint8_t key[32], const uint8_t nonce[12],
                    uint32_t counter, uint8_t out[64]) {
  uint32_t st[16];
  st[0] = 0x61707865; st[1] = 0x3320646e; st[2] = 0x79622d32; st[3] = 0x6b206574;
  for (int i = 0; i < 8; ++i) st[4 + i] = rd32le(key + 4 * i);
  st[12] = counter;
  for (int i = 0; i < 3; ++i) st[13 + i] = rd32le(nonce + 4 * i);

  uint32_t x[16];
  memcpy(x, st, sizeof(x));

#define P2LT_QR(a, b, c, d)                          \
  do {                                               \
    (a) += (b); (d) ^= (a); (d) = ((d) << 16) | ((d) >> 16); \
    (c) += (d); (b) ^= (c); (b) = ((b) << 12) | ((b) >> 20); \
    (a) += (b); (d) ^= (a); (d) = ((d) << 8) | ((d) >> 24);  \
    (c) += (d); (b) ^= (c); (b) = ((b) << 7) | ((b) >> 25);  \
  } while (0)

  for (int i = 0; i < 10; ++i) {
    P2LT_QR(x[0], x[4], x[8], x[12]);
    P2LT_QR(x[1], x[5], x[9], x[13]);
    P2LT_QR(x[2], x[6], x[10], x[14]);
    P2LT_QR(x[3], x[7], x[11], x[15]);
    P2LT_QR(x[0], x[5], x[10], x[15]);
    P2LT_QR(x[1], x[6], x[11], x[12]);
    P2LT_QR(x[2], x[7], x[8], x[13]);
    P2LT_QR(x[3], x[4], x[9], x[14]);
  }
#undef P2LT_QR

  for (int i = 0; i < 16; ++i) x[i] += st[i];
  for (int i = 0; i < 16; ++i) wr32le(out + 4 * i, x[i]);
}

void chacha20_xor(uint8_t* buf, size_t len, const uint8_t key[32],
                  const uint8_t nonce[12], uint32_t counter_start) {
  uint8_t block[64];
  uint32_t counter = counter_start;
  size_t off = 0;
  while (off < len) {
    chacha20_block(key, nonce, counter, block);
    size_t take = (len - off) < 64 ? (len - off) : 64;
    for (size_t i = 0; i < take; ++i) buf[off + i] ^= block[i];
    off += take;
    ++counter;
  }
}

// ChaCha20 + HMAC-SHA256 (Encrypt-then-MAC). Tag (16) = 16 bytes mais
// significativos de HMAC-SHA256 sobre (AAD || ciphertext), com MAC key
// derivada via HMAC do label 0x01.
bool aead_encrypt(const uint8_t* aad, size_t aad_len,
                  const uint8_t* plaintext, size_t plaintext_len,
                  const Key& key, const Nonce& nonce,
                  std::vector<uint8_t>* out_ciphertext, uint8_t tag[16]) {
  std::vector<uint8_t> ct(plaintext_len);
  memcpy(ct.data(), plaintext, plaintext_len);
  chacha20_xor(ct.data(), ct.size(), key.data(), nonce.data(), 1);
  if (out_ciphertext) *out_ciphertext = ct;

  std::vector<uint8_t> mac_msg;
  mac_msg.reserve(aad_len + ct.size());
  mac_msg.insert(mac_msg.end(), aad, aad + aad_len);
  mac_msg.insert(mac_msg.end(), ct.begin(), ct.end());
  uint8_t mac_key[32];
  constexpr uint8_t kMacLabel = 0x01;
  hmac_sha256(key.data(), key.size(), &kMacLabel, 1, mac_key);
  uint8_t full[32];
  hmac_sha256(mac_key, 32, mac_msg.data(), mac_msg.size(), full);
  memcpy(tag, full, 16);
  return true;
}

bool aead_decrypt(const uint8_t* aad, size_t aad_len,
                  const uint8_t* ciphertext, size_t ciphertext_len,
                  const uint8_t* tag,
                  const Key& key, const Nonce& nonce,
                  std::vector<uint8_t>* out_plaintext) {
  std::vector<uint8_t> mac_msg;
  mac_msg.reserve(aad_len + ciphertext_len);
  mac_msg.insert(mac_msg.end(), aad, aad + aad_len);
  mac_msg.insert(mac_msg.end(), ciphertext, ciphertext + ciphertext_len);
  uint8_t mac_key[32];
  constexpr uint8_t kMacLabel = 0x01;
  hmac_sha256(key.data(), key.size(), &kMacLabel, 1, mac_key);
  uint8_t full[32];
  hmac_sha256(mac_key, 32, mac_msg.data(), mac_msg.size(), fullapse);

  uint8_t diff = 0;
  for (size_t i = 0; i < 16; ++i) diff |= static_cast<uint8_t>(full[i] ^ tag[i]);
  if (diff) return false;

  std::vector<uint8_t> pt(ciphertext_len);
  memcpy(pt.data(), ciphertext, ciphertext_len);
  chacha20_xor(pt.data(), pt.size(), key.data(), nonce.data(), 1);
  if (out_plaintext) *out_plaintext = std::move(pt);
  return true;
}

Key derive_session_key(const uint8_t* secret, size_t secret_len,
                       uint32_t session_id,
                       const std::vector<uint8_t>& password) {
  std::vector<uint8_t> material;
  material.reserve(secret_len + 4 + 32);
  material.insert(material.end(), secret, secret + secret_len);
  uint8_t sid[4];
  wr32le(sid, session_id);
  material.insert(material.end(), sid, sid + 4);
  if (!password.empty()) {
    uint8_t pw[32];
    sha256(password.data(), password.size(), pw);
    material.insert(material.end(), pw, pw + 32);
  }
  Key key{};
  sha256(material.data(), material.size(), key.data());
  return key;
}

}  // namespace net
}  // namespace p2lt
