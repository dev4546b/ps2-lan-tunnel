#include "net/wire.h"

#include <cstring>
#include <sstream>

#include "util/endian.h"

namespace p2lt {
namespace net {

namespace util_ns = util;

// ---- Helpers de endian big-endian ---------------------------------------------
inline void put_u16be(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[1] = static_cast<uint8_t>(v & 0xFF);
}

inline void put_u32be(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[3] = static_cast<uint8_t>(v & 0xFF);
}

inline uint16_t get_u16be(const uint8_t* p) {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                               static_cast<uint16_t>(p[1]));
}

inline uint32_t get_u32be(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

// ---- Serialização ---------------------------------------------------------------
void encode_frame(const wire_frame_t& frame, std::vector<uint8_t>* out) {
  if (!out) return;
  if (frame.payload.size() > kMaxPayloadSize) return;

  out->resize(kHeaderSize + frame.payload.size());
  uint8_t* buf = out->data();

  // magic (4 bytes)
  put_u32be(buf + 0, kWireMagic);
  // version (1 byte)
  buf[4] = kWireVersion;
  // type (1 byte)
  buf[5] = static_cast<uint8_t>(frame.type);
  // flags (1 byte)
  buf[6] = frame.flags;
  // reserved (1 byte)
  buf[7] = 0;
  // session_id (4 bytes, big-endian)
  put_u32be(buf + 8, frame.session_id);
  // seq (4 bytes, big-endian)
  put_u32be(buf + 12, frame.seq);
  // sender_id (8 bytes)
  std::memcpy(buf + 16, frame.sender_id.data(), 8);
  // payload_len (2 bytes, big-endian)
  put_u16be(buf + 24, static_cast<uint16_t>(frame.payload.size()));

  // payload
  if (!frame.payload.empty()) {
    std::memcpy(buf + kHeaderSize, frame.payload.data(), frame.payload.size());
  }
}

// ---- Desserialização -------------------------------------------------------------
bool decode_frame(const uint8_t* data, size_t len, wire_frame_t* frame) {
  if (!data || !frame || len < kHeaderSize) return false;

  // Verifica magic
  if (get_u32be(data) != kWireMagic) return false;

  // Verifica versão
  if (data[4] != kWireVersion) return false;

  // Lê tipo
  msg_type_t type = static_cast<msg_type_t>(data[5]);
  // Valida tipo conhecido
  bool valid_type = false;
  for (int i = 1; i <= 7; ++i) {
    if (type == static_cast<msg_type_t>(i)) { valid_type = true; break; }
  }
  if (type == msg_type_t::kData || type == msg_type_t::kAck || type == msg_type_t::kError) {
    valid_type = true;
  }
  if (!valid_type) return false;

  // Flags
  uint8_t flags = data[6];
  // Reserved byte (data[7]) ignorado

  // session_id
  uint32_t session_id = get_u32be(data + 8);
  // seq
  uint32_t seq = get_u32be(data + 12);
  // sender_id
  peer_id_t sender_id{};
  std::memcpy(sender_id.data(), data + 16, 8);

  // payload_len
  uint16_t payload_len = get_u16be(data + 24);

  // Verifica se o payload declarado cabe no buffer restante
  if (len < kHeaderSize + payload_len) return false;
  if (payload_len > kMaxPayloadSize) return false;

  // Preenche o frame
  frame->type = type;
  frame->flags = flags;
  frame->session_id = session_id;
  frame->seq = seq;
  frame->sender_id = sender_id;
  frame->payload.assign(data + kHeaderSize, data + kHeaderSize + payload_len);

  return true;
}

// ---- Convites --------------------------------------------------------------------
std::string make_invite(const invite_t& invite) {
  std::ostringstream oss;
  oss << "p2lt://" << invite.host << ":" << invite.port << "/" << invite.room;
  if (!invite.password.empty()) {
    oss << "?pw=" << invite.password;
  }
  return oss.str();
}

bool parse_invite(const std::string& text, invite_t* invite) {
  if (!invite) return false;

  constexpr const char* kPrefix = "p2lt://";
  if (text.size() < strlen(kPrefix) || text.compare(0, strlen(kPrefix), kPrefix) != 0) {
    return false;
  }

  size_t pos = strlen(kPrefix);
  size_t slash = text.find('/', pos);
  if (slash == std::string::npos) return false;

  std::string host_port = text.substr(pos, slash - pos);
  size_t colon = host_port.find(':');
  if (colon == std::string::npos) return false;

  invite->host = host_port.substr(0, colon);
  std::string port_str = host_port.substr(colon + 1);
  try {
    invite->port = static_cast<uint16_t>(std::stoi(port_str));
  } catch (...) {
    return false;
  }
  if (invite->port == 0) return false;

  size_t qmark = text.find('?', slash);
  std::string room_pw = (qmark == std::string::npos)
    ? text.substr(slash + 1)
    : text.substr(slash + 1, qmark - slash - 1);
  invite->room = room_pw;

  invite->password.clear();
  if (qmark != std::string::npos) {
    std::string query = text.substr(qmark + 1);
    if (query.rfind("pw=", 0) == 0) {
      invite->password = query.substr(3);
    }
  }

  return !invite->room.empty();
}

}  // namespace net
}  // namespace p2lt