#include "net/relay.h"

// relay.cpp - Implementação do cliente relay do P2LT (PS2 LAN Tunnel).
//
// ESTE ESTÁGIO:
//   - Implementa apenas o CONTRATO de ciclo de vida e estado do relay_client.
//   - AINDA NÃO existe protocolo de transporte do relay implementado (nenhum
//     handshake, nenhum encaminhamento real). Por isso:
//       * connect() NÃO falsifica kConnected;
//       * send()   NUNCA retorna sucesso falso;
//   - O endereço do servidor relay é SEMPRE fornecido pelo chamador. Nenhum
//     servidor é hardcoded aqui.
//   - NÃO contém STUN, NÃO contém hole punching, NÃO captura pacotes PS2,
//     NÃO implementa lógica PCSX2, NÃO tem interface gráfica e NÃO cria
//     servidor relay.
//   - Este arquivo não cria threads nem loops bloqueantes.

#include <cstddef>
#include <cstdint>

namespace p2lt {
namespace net {

// ---- Destrutor -----------------------------------------------------------------------
// O relay_client não detém nenhum recurso externo neste estágio.
// O destrutor apenas garante um estado coerente de desconexão.
relay_client::~relay_client() noexcept {
  disconnect();
}

// ---- Move constructor ----------------------------------------------------------------
// Move-only: transfere o estado e devolve a origem a um estado neutro, de forma
// que o objeto movido NÃO continue aparentando estar conectado.
relay_client::relay_client(relay_client&& other) noexcept
    : state_(other.state_),
      error_(other.error_),
      native_error_(other.native_error_),
      initialized_(other.initialized_),
      server_(other.server_),
      config_(other.config_) {
  // Devolve a origem a um estado neutro desconectado.
  other.state_ = relay_state_t::kDisconnected;
  other.error_ = relay_error_t::kNone;
  other.native_error_ = 0;
  other.initialized_ = false;
  other.server_ = {};
  other.config_ = {};
}

// ---- Move assignment -----------------------------------------------------------------
relay_client& relay_client::operator=(relay_client&& other) noexcept {
  if (this != &other) {
    disconnect();  // libera/neutraliza o estado atual antes de assumir o novo
    state_ = other.state_;
    error_ = other.error_;
    native_error_ = other.native_error_;
    initialized_ = other.initialized_;
    server_ = other.server_;
    config_ = other.config_;
    // Devolve a origem a um estado neutro desconectado.
    other.state_ = relay_state_t::kDisconnected;
    other.error_ = relay_error_t::kNone;
    other.native_error_ = 0;
    other.initialized_ = false;
    other.server_ = {};
    other.config_ = {};
  }
  return *this;
}

// ---- Inicialização --------------------------------------------------------------------
// Operação de preparação do stub (sem transporte neste estágio).
// Corresponde à API do header: bool initialize() noexcept.
// Não há modo de falha irreversível aqui; retorna sempre true.
bool relay_client::initialize() noexcept {
  disconnect();
  initialized_ = true;
  state_ = relay_state_t::kDisconnected;
  error_ = relay_error_t::kNone;
  native_error_ = 0;
  return true;
}

// ---- Conexão -------------------------------------------------------------------------
// Valida argumentos e inicialização. Como NENHUM protocolo de transporte relay
// está implementado nesta etapa, esta operação NÃO estabelece conexão real e
// nunca falsifica kConnected: retorna erro honesto (kNotSupported) quando os
// argumentos são coerentes, mas não há como encaminhar dados.
bool relay_client::connect(const relay_server_t& server,
                           const relay_config_t& config,
                           relay_result_t* result) noexcept {
  if (result) {
    result->success = false;
    result->error = relay_error_t::kNone;
    result->native_error = 0;
  }

  if (!initialized_) {
    state_ = relay_state_t::kFailed;
    error_ = relay_error_t::kNotInitialized;
    if (result) {
      result->error = error_;
    }
    return false;
  }

  // Valida endpoint do servidor (fornecido pelo chamador; nunca hardcoded).
  const bool ip_nonzero =
      (server.ip[0] | server.ip[1] | server.ip[2] | server.ip[3]) != 0;
  if (server.port == 0 || !ip_nonzero) {
    state_ = relay_state_t::kFailed;
    error_ = relay_error_t::kInvalidArgument;
    if (result) {
      result->error = error_;
    }
    return false;
  }

  // Armazena a configuração solicitada para consulta futura.
  server_ = server;
  config_ = config;

  // Sem transporte relay implementado, não há conexão real a estabelecer.
  // IMPORTANTE: não inventar um handshake nem falsificar kConnected.
  state_ = relay_state_t::kFailed;
  error_ = relay_error_t::kNotSupported;
  if (result) {
    result->error = error_;
  }
  return false;
}

// ---- Desconexão -----------------------------------------------------------------------
// Idempotente: seguro em qualquer estado, inclusive já desconectado. Não destrói
// nenhum recurso externo.
void relay_client::disconnect() noexcept {
  state_ = relay_state_t::kDisconnected;
  error_ = relay_error_t::kNone;
  native_error_ = 0;
  server_ = {};
  config_ = {};
}

// ---- Consulta de estado ---------------------------------------------------------------
bool relay_client::is_connected() const noexcept {
  return state_ == relay_state_t::kConnected;
}

relay_state_t relay_client::state() const noexcept {
  return state_;
}

// ---- Envio de dados -------------------------------------------------------------------
// Encaminha um payload pelo relay. Sem transporte relay implementado, esta
// operação NUNCA retorna sucesso falso: valida argumentos e, não havendo como
// transmitir de fato, retorna erro apropriado.
bool relay_client::send(const uint8_t* data, std::size_t size,
                        relay_error_t* error) noexcept {
  if (error) {
    *error = relay_error_t::kNone;
  }

  // Argumentos inválidos.
  if (data == nullptr && size > 0) {
    if (error) {
      *error = relay_error_t::kInvalidArgument;
    }
    return false;
  }

  if (!initialized_) {
    if (error) {
      *error = relay_error_t::kNotInitialized;
    }
    return false;
  }

  // Sem transporte implementado, nunca chega ao estado conectado; portanto o
  // envio não é suportado neste estágio.
  if (error) {
    *error = relay_error_t::kNotSupported;
  }
  return false;
}

// ---- Erros ----------------------------------------------------------------------------
relay_error_t relay_client::last_error() const noexcept {
  return error_;
}

int relay_client::last_native_error() const noexcept {
  return native_error_;
}

}  // namespace net
}  // namespace p2lt
