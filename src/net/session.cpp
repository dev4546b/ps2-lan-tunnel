#include "net/session.h"

#include <algorithm>
#include <random>

#include "net/crypto.h"
#include "net/wire.h"
#include "util/endian.h"
#include "util/log.h"
#include "util/random.h"
#include "util/time.h"

namespace p2lt {
namespace net {

namespace util_ns = util;

session_t::session_t(const session_config_t& config)
    : config_(config) {
  state_.local_id = generate_peer_id();
}

session_t::~session_t() = default;

bool session_t::create_session(const std::string& room_code, const std::string& password) {
  if (room_code.empty()) return false;

  // Gera session_id aleatório (não-zero)
  state_.session_id = util_ns::random_u64() & 0xFFFFFFFFu;
  if (state_.session_id == 0) state_.session_id = 1;

  // Gera segredo aleatório para a sessão
  std::array<uint8_t, 32> secret{};
  if (!util::random_bytes(secret.data(), secret.size())) return false;

  // Deriva a chave de sessão
  state_.session_key = crypto::derive_session_key(
      secret.data(), secret.size(), state_.session_id, 
      std::vector<uint8_t>(password.begin(), password.end()));

  state_.local_id = generate_peer_id();
  state_.local_seq = 0;
  state_.created_ms = now_ms();
  state_.last_activity_ms = state_.created_ms;
  state_.is_host = true;
  state_.room_code = room_code;
  state_.password = password;
  state_.peers.clear();

  P2LT_LOGI("Sessão criada: room=%s session_id=0x%08X local_id=%s",
            room_code.c_str(), state_.session_id,
            util::hex_encode(state_.local_id.data(), state_.local_id.size()).c_str());
  return true;
}

bool session_t::join_session(uint32_t session_id, const crypto::Key& session_key,
                             const std::string& host, uint16_t port) {
  if (session_id == 0) return false;
  if (host.empty() || port == 0) return false;

  state_.session_id = session_id;
  state_.session_key = session_key;
  state_.local_id = generate_peer_id();
  state_.local_seq = 0;
  state_.created_ms = now_ms();
  state_.last_activity_ms = state_.created_ms;
  state_.is_host = false;
  state_.room_code.clear();
  state_.password.clear();
  state_.peers.clear();

  // Adiciona o host como peer conhecido
  remote_peer_t host_peer{};
  host_peer.id.fill(0); // será preenchido quando receber hello_ack
  host_peer.host = host;
  host_peer.port = port;
  host_peer.session_id = session_id;
  host_peer.reachable = true;
  state_.peers.push_back(host_peer);

  P2LT_LOGI("Entrou na sessão: session_id=0x%08X host=%s:%u",
            session_id, host.c_str(), port);
  return true;
}

bool session_t::add_peer(const remote_peer_t& peer) {
  if (peer.id == state_.local_id) return false;
  if (peer.session_id != state_.session_id) return false;

  auto it = std::find_if(state_.peers.begin(), state_.peers.end(),
                         [&](const remote_peer_t& p) { return p.id == peer.id; });
  if (it != state_.peers.end()) {
    *it = peer;
    return true;
  }
  state_.peers.push_back(peer);
  return true;
}

bool session_t::remove_peer(const peer_id_t& peer_id) {
  auto it = std::find_if(state_.peers.begin(), state_.peers.end(),
                         [&](const remote_peer_t& p) { return p.id == peer_id; });
  if (it != state_.peers.end()) {
    state_.peers.erase(it);
    return true;
  }
  return false;
}

remote_peer_t* session_t::find_peer(const peer_id_t& peer_id) {
  auto it = std::find_if(state_.peers.begin(), state_.peers.end(),
                         [&](const remote_peer_t& p) { return p.id == peer_id; });
  return (it != state_.peers.end()) ? &(*it) : nullptr;
}

void session_t::check_timeouts() {
  uint64_t now = now_ms();
  auto it = state_.peers.begin();
  while (it != state_.peers.end()) {
    if (now - it->last_seen_ms > config_.peer_timeout_ms) {
      P2LT_LOGW("Peer timeout removido: %s",
                util::hex_encode(it->id.data(), it->id.size()).c_str());
      it = state_.peers.erase(it);
    } else {
      ++it;
    }
  }
}

peer_id_t session_t::generate_peer_id() {
  peer_id_t id{};
  if (!util::random_bytes(id.data(), id.size())) return id;
  // Garante que não seja zero
  bool all_zero = std::all_of(id.begin(), id.end(), [](uint8_t b) { return b == 0; });
  if (all_zero) id[0] = 1;
  return id;
}

uint64_t session_t::now_ms() {
  return util_ns::now_ms();
}

std::vector<uint8_t> session_t::serialize_roster() const {
  std::vector<uint8_t> out;
  // Formato simples: [count:u16] [peer_id:8][session_id:u32][host_len:u8][host][port:u16][flags:u8]...
  out.reserve(2 + state_.peers.size() * (8 + 4 + 1 + 256 + 2 + 1));
  util_ns::put16(out.data(), static_cast<uint16_t>(state_.peers.size()));
  out.resize(2);
  
  for (const auto& p : state_.peers) {
    out.insert(out.end(), p.id.begin(), p.id.end());
    uint32_t sid = p.session_id;
    uint8_t sid_bytes[4];
    util_ns::put32(sid_bytes, sid);
    out.insert(out.end(), sid_bytes, sid_bytes + 4);
    uint8_t host_len = static_cast<uint8_t>(std::min<size_t>(p.host.size(), 255));
    out.push_back(host_len);
    out.insert(out.end(), p.host.begin(), p.host.begin() + host_len);
    uint16_t port = p.port;
    uint8_t port_bytes[2];
    util_ns::put16(port_bytes, port);
    out.insert(out.end(), port_bytes, port_bytes + 2);
    uint8_t flags = (p.reachable ? 1 : 0) | (p.is_relay ? 2 : 0);
    out.push_back(flags);
  }
  return out;
}

bool session_t::parse_roster(const uint8_t* data, size_t len) {
  if (!data || len < 2) return false;
  size_t offset = 0;
  uint16_t count = util_ns::get16(data + offset);
  offset += 2;
  
  std::vector<remote_peer_t> new_peers;
  new_peers.reserve(count);
  
  for (uint16_t i = 0; i < count; ++i) {
    if (offset + 8 + 4 > len) return false;
    remote_peer_t p{};
    std::memcpy(p.id.data(), data + offset, 8);
    offset += 8;
    p.session_id = get_u32be(data + offset);
    offset += 4;
    if (offset >= len) return false;
    uint8_t host_len = data[offset++];
    if (offset + host_len > len) return false;
    p.host.assign(reinterpret_cast<const char*>(data + offset), host_len);
    offset += host_len;
    if (offset + 2 > len) return false;
    p.port = get_u16be(data + offset);
    offset += 2;
    if (offset >= len) return false;
    uint8_t flags = data[offset++];
    p.reachable = (flags & 1) != 0;
    p.is_relay = (flags & 2) != 0;
    new_peers.push_back(p);
  }
  state_.peers = std::move(new_peers);
  return true;
}

}  // namespace net
}  // namespace p2lt