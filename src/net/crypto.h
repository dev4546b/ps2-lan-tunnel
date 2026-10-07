#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace p2lt {
namespace net {

constexpr size_t kKeySize = 32;
constexpr size_t kNonceSize = 12;
constexpr size_t kTagSize = 16;  // HMAC-SHA256 truncado para 16 bytes
constexpr size_t kSha256Size = 32;
constexpr size_t kHmacBlockSize = 64;

using Key = std::array<uint8_t, kKeySize>;
using Nonce = std::array<uint8_t, kNonceSize>;

void sha256(const uint8_t* data, size_t len, uint8_t out[32]);

void hmac_sha256(const uint8_t* key, size_t key_len,
                 const uint8_t* msg, size_t msg_len,
                 uint8_t out[32]);

void chacha20_block(const uint8_t key[32], const uint8_t nonce[12],
                    uint32_t counter, uint8_t out[64]);

void chacha20_xor(uint8_t* buf, size_t len, const uint8_t key[32],
                  const uint8_t nonce[12], uint32_t counter_start);

// AEAD ChaCha20 + HMAC-SHA256 (Encrypt-then-MAC). AAD autenticado mas nao
// cifrado. Tag (16 bytes) = primeiros 16 de HMAC-SHA256 sobre (AAD || CT).
bool aead_encrypt(const uint8_t* aad, size_t aad_len,
                  const uint8_t* plaintext, size_t plaintext_len,
                  const Key& key, const Nonce& nonce,
                  std::vector<uint8_t>* out_ciphertext, uint8_t tag[kTagSize]);

bool aead_decrypt(const uint8_t* aad, size_t aad_len,
                  const uint8_t* ciphertext, size_t ciphertext_len,
                  const uint8_t* tag,
                  const Key& key, const Nonce& nonce,
                  std::vector<uint8_t>* out_plaintext);

// Clave de sessao = SHA256(secret || session_id(4 LE) || [SHA256(password)]).
Key derive_session_key(const uint8_t* secret, size_t secret_len,
                       uint32_t session_id,
                       const std::vector<uint8_t>& password);

}  // namespace net
}  // namespace p2lt
