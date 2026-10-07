#pragma once

// p2p.h - Contrato do componente P2P do P2LT (PS2 LAN Tunnel).
//
// FINALIDADE:
//   O P2P tenta estabelecer comunicação DIRETA entre dois participantes por UDP.
//   Se a conexão direta não puder ser estabelecida (NAT simétrico, firewall,
//   etc.), a camada superior poderá optar pelo Relay como fallback:
//
//     P2P conseguiu?
//       SIM -> conexão direta
//       NÃO -> tenta Relay
//
// NAT TRAVERSAL:
//   A descoberta do endpoint público e a classificação de NAT pertencem a
//   OUTRA camada (nat_traversal). O P2P PODE consumir o resultado produzido por
//   ela, mas NÃO duplica STUN nem classifica NAT aqui.
//
// RELAY:
//   O P2P NÃO implementa relay. O encaminhamento via servidor relay pertence ao
//   componente relay, usado pela camada superior como alternativa.
//
// ESTE ESTÁGIO:
//   - Define SOMENTE o contrato (API). A implementação virá em p2p.cpp.
//   - NÃO contém hole punching real, NÃO contém STUN, NÃO contém servidor relay,
//     NÃO captura pacotes PS2, NÃO tem lógica PCSX2, NÃO tem interface gráfica e
//     NÃO cria sala/servidor.
//   - NÃO há servidor hardcoded, IP hardcoded, chave/crypto fixa ou endereço
//     fixo. Os endpoints são sempre fornecidos pelo chamador.
//   - O transporte é fornecido externamente (não-owned pelo p2p_client).

#include <array>
#include <cstddef>
#include <cstdint>

namespace p2lt {
namespace net {

// ---- Estados da conexão P2P -------------------------------------------------------------
enum class p2p_state_t : int {
  kDisconnected = 0,  // desconectado (estado inicial / após disconnect)
  kProbing = 1,       // sondando a possibilidade de conexão direta (hole-punching)
  kConnecting = 2,    // tentando estabelecer a conexão direta
  kConnected = 3,     // conexão direta estabelecida e utilizável
  kFailed = 4,        // falha irrecuperável (ver last_error)
};

// ---- Erros específicos do P2P ------------------------------------------------------------
enum class p2p_error_t : int {
  kNone = 0,               // sem erro
  kNotInitialized = -1,    // cliente não inicializado (initialize não chamado)
  kInvalidArgument = -2,   // argumento inválido (ex.: peer inválido)
  kTimeout = -3,           // tempo limite excedido
  kNatTraversalFailed = -4, // descoberta de NAT/hole-punching falhou
  kConnectionFailed = -5,  // não foi possível estabelecer a conexão direta
  kProtocolError = -6,     // resposta/experiência incoerente com o protocolo
  kNetworkError = -7,      // erro de socket/rede (ver native_error)
  kNotSupported = -8,      // operação não suportada neste estágio
  kUnknown = -9,           // erro não classificado
};

// ---- Configuração mínima do P2P -----------------------------------------------------------
// Apenas parâmetros realmente necessários para a futura conexão direta.
// Nada de dezenas de opções prematuras.
struct p2p_config_t {
  uint32_t timeout_ms = 3000;          // timeout por tentativa
  uint32_t attempt_count = 3;          // nº de tentativas
  uint32_t attempt_interval_ms = 500;  // intervalo entre tentativas
};

// ---- Participante remoto ------------------------------------------------------------------
// Informações mínimas necessárias para a camada P2P. NÃO duplica a estrutura de
// sessão completa.
struct peer_t {
  std::array<uint8_t, 4> ip{};    // endereço IPv4 do peer remoto
  uint16_t port = 0;              // porta UDP do peer remoto
  std::array<uint8_t, 16> id{};   // identificador do peer (opcional)

  // Um peer é considerado válido se a porta é não-nula e o IP não é tudo-zero.
  bool is_valid() const noexcept {
    const bool ip_nonzero =
        (ip[0] | ip[1] | ip[2] | ip[3]) != 0;
    return port != 0 && ip_nonzero;
  }
};

// ---- Resultado de uma operação P2P ----------------------------------------------------------
struct p2p_result_t {
  bool success = false;            // operação concluída com sucesso
  p2p_error_t error = p2p_error_t::kNone;
  int native_error = 0;            // código nativo (Winsock) do último erro
  peer_t remote_peer{};            // peer remoto resolvido, quando apropriado
};

// ---- Cliente P2P (RAII, move-only, não copiável) ---------------------------------------------
// RAII: o ciclo de vida é gerenciado internamente. O transporte é fornecido
// externamente (não-owned) e NUNCA é destruído por esta classe.
class p2p_client {
public:
  p2p_client() noexcept = default;
  ~p2p_client() noexcept;

  // Não copiável.
  p2p_client(const p2p_client&) = delete;
  p2p_client& operator=(const p2p_client&) = delete;

  // Move-only.
  p2p_client(p2p_client&&) noexcept;
  p2p_client& operator=(p2p_client&&) noexcept;

  // ---- Inicialização ---------------------------------------------------------------------
  // Recebe um transporte externo (não-owned). Reconfigurável: pode ser chamado
  // novamente.
  bool initialize(class udp_transport* transport) noexcept;

  // ---- Conexão ---------------------------------------------------------------------------
  // Tenta estabelecer conexão direta com o peer remoto. O endpoint é sempre
  // fornecido pelo chamador.
  bool connect(const peer_t& remote,
               const p2p_config_t& config = p2p_config_t{},
               p2p_result_t* result = nullptr) noexcept;

  // ---- Desconexão ------------------------------------------------------------------------
  // Idempotente: seguro em qualquer estado.
  void disconnect() noexcept;

  // ---- Consulta de estado ----------------------------------------------------------------
  bool is_connected() const noexcept;
  p2p_state_t state() const noexcept;
  p2p_error_t last_error() const noexcept;
  int last_native_error() const noexcept;

  // ---- Envio de dados --------------------------------------------------------------------
  // Encaminha um payload pela conexão P2P. Sem transporte P2P implementado
  // nesta etapa, NUNCA retorna sucesso falso: sem transporte, a operação
  // retorna erro apropriado (kNotSupported, kNotInitialized, ...).
  bool send(const uint8_t* data, std::size_t size,
            p2p_error_t* error = nullptr) noexcept;

  // ---- Consulta do peer remoto -----------------------------------------------------------
  // Último peer definido/estabelecido pela conexão.
  const peer_t& remote_peer() const noexcept;

private:
  p2p_state_t state_ = p2p_state_t::kDisconnected;
  p2p_error_t error_ = p2p_error_t::kNone;
  int native_error_ = 0;
  bool initialized_ = false;
  class udp_transport* transport_ = nullptr;
  peer_t remote_{};
  p2p_config_t config_{};
};

}  // namespace net
}  // namespace p2lt
