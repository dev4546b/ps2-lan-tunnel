#include "net/p2p.h"

#include <cstddef>
#include <cstdint>

// p2p.cpp - Implementação do cliente P2P do P2LT (PS2 LAN Tunnel).
//
// FINALIDADE:
//   Estabelecer comunicação DIRETA entre dois participantes por UDP.
//   Se a conexão direta não puder ser estabelecida, a camada superior poderá
//   optar pelo Relay como fallback:
//
//     P2P conseguiu?   SIM -> conexão direta
//                      NAO -> tenta Relay
//
// NAT TRAVERSAL:
//   A descoberta do endpoint público e a classificação de NAT pertencem a
//   OUTRA camada (nat_traversal). Este arquivo NAO duplica STUN nem classifica
//   NAT.
//
// RELAY:
//   Este arquivo NAO implementa relay. O encaminhamento via servidor relay
//   pertence ao componente relay, usado pela camada superior como alternativa.
//
// ESTE ESTAGIO:
//   Implementa SOMENTE o ciclo de vida (RAII), move semantics, validação de
//   argumentos, estado interno e tratamento de erros do p2p_client.
//   NAO ha protocolo P2P real nem hole punching nem handshake implementado.
//   Por esse motivo, nenhuma conexao e declarada estabelecida: connect() e
//   send() retornam erro honesto (kNotSupported) quando a infraestrutura
//   atual nao permite implementar P2P real. Nunca se falsifica sucesso.
//
//   NAO contem: servidor P2P, relay, STUN, captura de pacotes PS2, PCSX2,
//   UI, sala/servidor, IP hardcoded, porta hardcoded, peer ID hardcoded,
//   chave/credential fixa.
//
// MAXIME:
//   * O transporte externo e NAO-owned: nunca e deletado por esta classe.
//   * Sem threads, sem loops infinitos, sem espera bloqueante artificial.
//   * C++17, noexcept, sem memoria manual desnecessaria.

namespace p2lt {
namespace net {

// ---- Ciclo de vida (RAII) ------------------------------------------------------------
// Construtor padrão já definido como = default no header (p2p.h).
// Não redefinir aqui (C2084).

p2p_client::~p2p_client() noexcept {
  // Nunca deleta o transporte: ele e externo (non-owned).
  disconnect();
}

// ---- Move constructor ----------------------------------------------------------------
// Transfere o estado e deixa a origem em estado seguro. A origem jamais
// continua reportando kConnected.
p2p_client::p2p_client(p2p_client&& other) noexcept
    : state_(other.state_),
      error_(other.error_),
      native_error_(other.native_error_),
      initialized_(other.initialized_),
      transport_(other.transport_),
      remote_(other.remote_),
      config_(other.config_) {
  // Estado seguro para a origem: desconectada e sem transporte.
  other.state_ = p2p_state_t::kDisconnected;
  other.error_ = p2p_error_t::kNone;
  other.native_error_ = 0;
  other.initialized_ = false;
  other.transport_ = nullptr;
}

// ---- Move assignment ----------------------------------------------------------------
p2p_client& p2p_client::operator=(p2p_client&& other) noexcept {
  if (this != &other) {
    disconnect();  // limpa o estado atual (idempotente)

    state_ = other.state_;
    error_ = other.error_;
    native_error_ = other.native_error_;
    initialized_ = other.initialized_;
    transport_ = other.transport_;
    remote_ = other.remote_;
    config_ = other.config_;

    // Estado seguro para a origem.
    other.state_ = p2p_state_t::kDisconnected;
    other.error_ = p2p_error_t::kNone;
    other.native_error_ = 0;
    other.initialized_ = false;
    other.transport_ = nullptr;
  }
  return *this;
}

// ---- Inicialização ------------------------------------------------------------------
// Recebe o transporte externo (NAO-owned). Reconfigurável: pode ser chamado
// novamente com outro transporte.
bool p2p_client::initialize(class udp_transport* transport) noexcept {
  disconnect();  // limpa estado anterior (idempotente)
  initialized_ = false;
  transport_ = nullptr , error_ = p2p_error_t::kNone, native_error_ = 0;

  if (transport == nullptr) {
    error_ = p2p_error_t::kNotInitialized;
    state_ = p2p_state_t::kFailed;
    return false;
  }

  transport_ = transport;  // non-owned; nunca sera deletado aqui
  initialized_ = true;
  error_ = p2p_error_t::kNone;
  native_error_ = 0;
  state_ = p2p_state_t::kDisconnected;
  return true;
}

// ---- Conexão ------------------------------------------------------------------------
// Valida inicialização, transporte e peer. NAO declarara conexao estabelecida
// sem protocolo/transporte P2P real: retorna kNotSupported, estado kFailed.
bool p2p_client::connect(const peer_t& remote,
                         const p2p_config_t& config,
                         p2p_result_t* result) noexcept {
  if (result) {
    result->success = false;
    result->error = p2p_error_t::kNone;
    result->native_error = 0;
  }

  if (!initialized_ || transport_ == nullptr) {
    error_ = p2p_error_t::kNotInitialized;
    state_ = p2p_state_t::kFailed;
    if (result) {
      result->error = error_;
      result->native_error = native_error_;
    }
    return false;
  }

  const bool ip_zero = (remote.ip[0] == 0 && remote.ip[1] == 0 &&
                        remote.ip[2] == 0 && remote.ip[3] == 0);
  if (ip_zero || remote.port == 0) {
    error_ = p2p_error_t::kInvalidArgument;
    state_ = p2p_state_t::kFailed;
    if (result) {
      result->error = error_;
      result->native_error = native_error_;
    }
    return false;
  }

  // Informacoes do peer solicitado ficam disponiveis para consulta.
  remote_ = remote;
  config_ = config;

  // NAO ha hole punching / handshake P2P implementado neste estagio.
  // Preferimos um erro honesto a uma conexao falsificada.
  error_ = p2p_error_t::kNotSupported;
  state_ = p2p_state_t::kFailed;
  if (result) {
    result->error = error_;
    result->native_error = native_error_;
    result->remote_peer = remote_;
  }
  return false;
}

// ---- Desconexão ---------------------------------------------------------------------
// Idempotente e seguro em qualquer estado.
void p2p_client::disconnect() noexcept {
  state_ = p2p_state_t::kDisconnected;
  error_ = p2p_error_t::kNone;
  native_error_ = 0;
  // O transporte externo nao e tocado aqui (removido apenas no destino real).
  // O transporte NAO-owned nunca e destruido por esta classe.
  // A configuração e o peer consultados sao mantidos (apenas consulta).
}

// ---- Consultas de estado -------------------------------------------------------------
bool p2p_client::is_connected() const noexcept {
  return state_ == p2p_state_t::kConnected;
}

p2p_state_t p2p_client::state() const noexcept {
  return state_;
}

p2p_error_t p2p_client::last_error() const noexcept {
  return error_;
}

int p2p_client::last_native_error() const noexcept {
  return native_error_;
}

// ---- Peer remoto ---------------------------------------------------------------------
const peer_t& p2p_client::remote_peer() const noexcept {
  return remote_;
}

// ---- Envio de dados ------------------------------------------------------------------
// Valida inicialização, estado e argumentos. Sem transporte P2P real, nunca
// retorna sucesso falso: reporta kNotSupported.
bool p2p_client::send(const uint8_t* data, std::size_t size,
                      p2p_error_t* error) noexcept {
  if (error) {
    *error = p2p_error_t::kNone;
  }

  if (!initialized_ || transport_ == nullptr) {
    error_ = p2p_error_t::kNotInitialized;
    if (error) {
      *error = error_;
    }
    return false;
  }

  if (state_ != p2p_state_t::kConnected) {
    error_ = p2p_error_t::kNotInitialized;
    if (error) {
      *error = error_;
    }
    return false;
  }

  if (data == nullptr && size > 0) {
    error_ = p2p_error_t::kInvalidArgument;
    if (error) {
      *error = error_;
    }
    return false;
  }

  // Sem transporte P2P real implementado: nunca fingir transmissao.
  error_ = p2p_error_t::kNotSupported;
  if (error) {
    *error = error_;
  }
  return false;
}

}  // namespace net
}  // namespace p2lt
