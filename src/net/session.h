#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "net/wire.h"
#include "net/crypto.h"

namespace p2lt {
namespace net {

// ---- Constantes de sessão -------------------------------------------------------
constexpr uint16_t kDefaultPort = 25020;
constexpr size_t kSessionIdBytes = 4;
constexpr uint32_t kStunTimeoutMs = 3000;

// ---- Identificadores ------------------------------------------------------------
using session_id_t = uint32_t;
using peer_id_t = std::array<uint8_t, 8>;

// ---- Estado de um peer remoto ---------------------------------------------------
struct remote_peer_t {
  peer_id_t id{};
  std::string host;
  uint16_t port = 0;
  uint32_t session_id = 0;
  uint32_t last_seq = 0;
  uint64_t last_seen_ms = 0;
  bool reachable = false;
  bool is_relay = false;
};

// ---- Estado da sessão -----------------------------------------------------------
struct session_state_t {
  session_id_t session_id = 0;
  peer_id_t local_id{};
  crypto::Key session_key{};
  std::vector<remote_peer_t> peers;
  uint32_t local_seq = 0;
  uint64_t created_ms = 0;
  uint64_t last_activity_ms = 0;
  bool is_host = false;
  std::string room_code;
  std::string password;
};

// ---- Configuração da sessão -----------------------------------------------------
struct session_config_t {
  uint16_t listen_port = kDefaultPort;
  std::string stun_host = "stun.l.google.com";
  uint16_t stun_port = 19302;
  uint32_t keepalive_interval_ms = 10000;
  uint32_t peer_timeout_ms = 30000;
  uint32_t nat_punch_retries = 3;
  uint32_t nat_punch_interval_ms = 500;
};

// ---- Interface da camada de sessão ---------------------------------------------
class session_t {
public:
  explicit session_t(const session_config_t& config = session_config_t());
  ~session_t();

  // Cria uma nova sessão como host (gera session_id e session_key)
  bool create_session(const std::string& room_code, const std::string& password);

  // Entra em uma sessão existente (usa session_id e session_key do convite)
  bool join_session(uint32_t session_id, const crypto::Key& session_key,
                    const std::string& host, uint16_t port);

  // Adiciona um peer remoto conhecido (via roster ou invite)
  bool add_peer(const remote_peer_t& peer);

  // Remove um peer
  bool remove_peer(const peer_id_t& peer_id);

  // Obtém a chave de sessão para criptografia
  const crypto::Key& session_key() const { return state_.session_key; }

  // Obtém o session_id
  uint32_t session_id() const { return state_.session_id; }

  // Obtém o ID local
  const peer_id_t& local_id() const { return state_.local_id; }

  // Obtém a lista de peers
  const std::vector<remote_peer_t>& peers() const { return state_.peers; }

  // Obtém um peer por ID
  remote_peer_t* find_peer(const peer_id_t& peer_id);

  // Incrementa e retorna o próximo número de sequência
  uint32_t next_seq() { return ++state_.local_seq; }

  // Marca atividade (para timeouts)
  void mark_activity() { state_.last_activity_ms = now_ms(); }

  // Verifica timeouts de peers
  void check_timeouts();

  // Verifica se a sessão é host
  bool is_host() const { return state_.is_host; }

  // Obtém o código da sala
  const std::string& room_code() const { return state_.room_code; }

  // Serializa roster para envio
  std::vector<uint8_t> serialize_roster() const;

  // Desserializa roster recebido
  bool parse_roster(const uint8_t* data, size_t len);

private:
  session_config_t config_;
  session_state_t state_;

  // Gera ID aleatório para peer
  static peer_id_t generate_peer_id();

  // Timestamp atual em ms
  static uint64_t now_ms();
};

}  // namespace net
}  // namespace p2lt