#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace p2lt {
namespace net {

// ---- Protocolo de fio (wire) ---------------------------------------------------
// Quadro p2lt: cabeçalho fixo de 26 bytes (big-endian) + payload opcional.
//
//   offset  tamanho  campo
//   0       4        magic "p2lt" (0x50324C54)
//   4       1        versão do protocolo (1)
//   5       1        tipo (msg_type_t)
//   6       1        flags (wire_flags_t)
//   7       1        reservado (0)
//   8       4        session_id (u32 be)
//   12      4        seq (u32 be)
//   16      8        sender_id (peer_id_t)
//   24      2        payload_len (u16 be)
//
// O payload (payload_len bytes) segue após o cabeçalho.

constexpr uint32_t kWireMagic = 0x50324C54;  // "p2lt"
constexpr uint8_t kWireVersion = 1;
constexpr size_t kHeaderSize = 26;
constexpr size_t kMaxPayloadSize = 65535;

using peer_id_t = std::array<uint8_t, 8>;

// ---- Tipos de mensagem ----------------------------------------------------------
enum class msg_type_t : uint8_t {
  kHello = 1,
  kHelloAck = 2,
  kInvite = 3,
  kJoin = 4,
  kLeave = 5,
  kPing = 6,
  kPong = 7,
  kData = 0x20,
  kAck = 0x21,
  kError = 0x7F,
};

// ---- Flags ----------------------------------------------------------------------
enum wire_flags_t : uint8_t {
  kFlagEncrypted = 0x01,
  kFlagRelayed = 0x02,
  kFlagAckRequested = 0x04,
  kFlagAck = 0x08,
  kFlagEndOfStream = 0x10,
};

// ---- Quadro decodificado ----------------------------------------------------------
struct wire_frame_t {
  msg_type_t type = msg_type_t::kHello;
  uint8_t flags = 0;
  uint32_t session_id = 0;
  uint32_t seq = 0;
  peer_id_t sender_id{};
  std::vector<uint8_t> payload;  // payload bruto (cifrado se kFlagEncrypted)
};

// ---- Serialização ------------------------------------------------------------------
// Codifica o quadro para bytes (sem cifragem — a cifragem é responsabilidade da
// camada de sessão em net/session, usando net/crypto).
void encode_frame(const wire_frame_t& frame, std::vector<uint8_t>* out);
// Decodifica de bytes brutos com validação robusta (tamanhos, magic, versão).
bool decode_frame(const uint8_t* data, size_t len, wire_frame_t* frame);

// ---- Convites (código de sala) ------------------------------------------------------
// Um convite é uma string curta que codifica um convite de sala:
//   p2lt://<host>:<port>/<room>?pw=<password>
struct invite_t {
  std::string room;
  std::string host;
  uint16_t port = 0;
  std::string password;  // opcional
};

std::string make_invite(const invite_t& invite);
bool parse_invite(const std::string& text, invite_t* invite);

}  // namespace net
}  // namespace p2lt