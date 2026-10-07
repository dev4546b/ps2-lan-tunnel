#pragma once

// relay.h - Contrato do componente Relay do P2LT (PS2 LAN Tunnel).
//
// FINALIDADE:
//   O Relay é a alternativa de transporte quando uma conexão P2P direta não
//   puder ser estabelecida (devido a NAT simétrico, firewall, etc.). Em vez de
//   trafegar entre peer e peer diretamente, os datagramas encapsulados pelo
//   protocolo P2LT são encaminhados por um servidor relay na Internet.
//
// RELAY x P2P:
//   - P2P:   comunicação direta entre peers (hole punching); infraestrutura
//            mínima, porém dependente da topologia de NAT.
//   - RELAY: todo o tráfego passa por um servidor intermediário. Funciona em
//            qualquer topologia, ao custo de latência e largura de banda do
//            servidor.
//
// ESTE ESTÁGIO:
//   - Apenas o contrato (API) é definido. Não há protocolo de transporte do
//     relay implementado ainda: envio/espera de respostas virão em relay.cpp.
//   - O endereço do servidor relay é SEMPRE fornecido pelo chamador. NENHUM
//     servidor é hardcoded aqui.
//   - Este módulo NÃO contém lógica de STUN (isso pertence ao nat_traversal),
//     NÃO contém hole punching (pertencerá ao módulo P2P), NÃO captura pacotes
//     PS2 (camadas superiores) e NÃO possui interface gráfica.
//   - A API é pequena e prevê operações futuras de inicialização, conexão,
//     desconexão, estado, envio (relay) e consulta de erros.

#include <array>
#include <cstddef>
#include <cstdint>

namespace p2lt {
namespace net {

// ---- Estado do relay ------------------------------------------------------------------
enum class relay_state_t : int {
  kDisconnected = 0,  // não conectado (estado inicial / após disconnect)
  kConnecting = 1,    // tentando estabelecer conexão com o relay
  kConnected = 2,     // conectado e pronto para encaminhar dados
  kFailed = 3,        // falha (timeout, erro de rede, protocolo)
};

// ---- Erros do relay -------------------------------------------------------------------
enum class relay_error_t : int {
  kNone = 0,             // sem erro
  kNotInitialized = -1,  // cliente não inicializado (initialize não chamado)
  kInvalidArgument = -2, // argumento inválido (ex.: payload nulo com size>0)
  kTimeout = -3,         // tempo limite excedido (connect ou send)
  kConnectionFailed = -4, // não foi possível conectar ao servidor relay
  kProtocolError = -5,   // resposta/experiência incoerente com o protocolo
  kNetworkError = -6,    // erro de socket/rede (ver native_error)
  kNotSupported = -7,    // operação não suportada neste estágio
  kUnknown = -8,         // erro não classificado
};

// ---- Configuração da conexão relay ------------------------------------------------------
// Parâmetros mínimos: TODO e reuso vêm de camadas acima. Sem opções prematuras.
struct relay_config_t {
  uint32_t connect_timeout_ms = 5000;          // timeout por tentativa de connect
  uint32_t connect_attempts = 3;               // nº de tentativas
  uint32_t connect_attempt_interval_ms = 500;  // intervalo entre tentativas
};

// ---- Endpoint/configuração do servidor relay --------------------------------------------
// Endereço fornecido sempre pelo chamador. NUNCA hardcoded neste módulo.
struct relay_server_t {
  std::array<uint8_t, 4> ip{};  // endereço IPv4 do relay
  uint16_t port = 0;            // porta do relay

  bool is_valid() const noexcept {
    bool ip_nonzero = false;
    for (uint8_t b : ip) {
      if (b != 0) {
        ip_nonzero = true;
        break;
      }
    }
    return port != 0 && ip_nonzero;
  }
};

// ---- Resultado de uma operação relay ------------------------------------------------------
struct relay_result_t {
  bool success = false;                    // operação concluída com sucesso
  relay_error_t error = relay_error_t::kNone;
  int native_error = 0;                    // código nativo (Winsock) do último erro
};

// ---- Cliente relay (RAII, move-only, não copiável) -----------------------------------------
class relay_client {
public:
  relay_client() noexcept = default;
  ~relay_client() noexcept;

  // Não copiável.
  relay_client(const relay_client&) = delete;
  relay_client& operator=(const relay_client&) = delete;

  // Move-only (definições em relay.cpp; transferem o estado e
  // devolvem a origem a um estado neutro desconectado).
  relay_client(relay_client&&) noexcept;
  relay_client& operator=(relay_client&&) noexcept;

  // ---- Inicialização ---------------------------------------------------------------
  // Torna o cliente utilizável. Retorna false em caso de falha irreversível.
  bool initialize() noexcept;

  // ---- Conexão ---------------------------------------------------------------------
  // Conecta ao servidor relay. O endereço é sempre fornecido pelo chamador.
  bool connect(const relay_server_t& server,
               const relay_config_t& config = relay_config_t{},
               relay_result_t* result = nullptr) noexcept;

  // ---- Desconexão ------------------------------------------------------------------
  void disconnect() noexcept;

  // ---- Consulta de estado ----------------------------------------------------------
  bool is_connected() const noexcept;
  relay_state_t state() const noexcept;
  relay_error_t last_error() const noexcept;
  int last_native_error() const noexcept;

  // ---- Envio de dados --------------------------------------------------------------
  // Encaminha um payload pelo relay. Representa o CONCEITO de envio; o protocolo
  // de encaminhamento de fato será implementado em relay.cpp. Não implementa uma
  // falsificação: sem transporte implementado, as operações retornam erro
  // (não suportado / não inicializado).
  bool send(const uint8_t* data, std::size_t size,
            relay_error_t* error = nullptr) noexcept;

private:
  relay_state_t state_ = relay_state_t::kDisconnected;
  relay_error_t error_ = relay_error_t::kNone;
  int native_error_ = 0;
  bool initialized_ = false;
  relay_server_t server_{};
  relay_config_t config_{};
};

}  // namespace net
}  // namespace p2lt
