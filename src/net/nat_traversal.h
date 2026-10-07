#pragma once

#include <cstddef>
#include <cstdint>

#include "net/udp_transport.h"

namespace p2lt {
namespace net {

// ---- Tipos de NAT ----------------------------------------------------------------
// Classificação informativa, NÃO é garantia absoluta.
// Fornece contexto heurístico para a camada P2P decidir a estratégia.
enum class nat_type_t : int {
  kUnknown = 0,
  kOpenInternet = 1,          // endpoint diretamente acessível (sem NAT)
  kFullCone = 2,              // qualquer origem pode enviar ao mapeamento
  kRestrictedCone = 3,        // só aceita de IPs já visitados
  kPortRestrictedCone = 4,    // só aceita do mesmo IP:porta já visitado
  kSymmetric = 5,             // novo mapeamento para cada destino
};

// ---- Códigos de erro do NAT traversal -------------------------------------------
enum class nat_error_t : int {
  kNone = 0,
  kNotInitialized = -1,       // initialize() ainda não foi chamado
  kInvalidArgument = -2,      // argumento inválido (ex.: stun_server vazio, transport nulo)
  kTimeout = -3,              // STUN não respondeu no prazo
  kStunFailed = -4,           // resposta STUN inválida/incoerente
  kNetworkError = -5,         // erro de rede/Winsock nativo
  kNotSupported = -6,         // recurso não suportado nesta versão
  kUnknown = -7,
};

// ---- Configuração do probe ------------------------------------------------------
struct nat_probe_config_t {
  uint32_t timeout_ms = 3000;        // prazo de espera por resposta STUN
  uint32_t attempt_count = 3;        // número de tentativas
  uint32_t attempt_interval_ms = 500; // intervalo entre tentativas
};

// ---- Resultado da descoberta ----------------------------------------------------
struct nat_result_t {
  bool success = false;              // descobriu o endpoint público?
  udp_endpoint_t public_endpoint{};  // endpoint público descoberto (se success)
  udp_endpoint_t local_endpoint{};   // endpoint local (bind efetivo)
  nat_type_t nat_type = nat_type_t::kUnknown;
  nat_error_t error = nat_error_t::kNone;
  int native_error = 0;              // WSAGetLastError() quando aplicável
};

// ---- NAT traversal / STUN -------------------------------------------------------
// Camada responsável por descoberta de endpoint público, informação de NAT e
// preparação para UDP hole punching. NÃO faz relay nem P2P real (camadas acima).
class nat_traversal {
public:
  nat_traversal() = default;
  ~nat_traversal() = default;

  nat_traversal(const nat_traversal&) = delete;
  nat_traversal& operator=(const nat_traversal&) = delete;
  nat_traversal(nat_traversal&&) noexcept = default;
  nat_traversal& operator=(nat_traversal&&) noexcept = default;

  // Associa o transporte UDP existente (não cria outro).
  // Deve ser chamado antes de discover_public_endpoint().
  void initialize(udp_transport* transport) noexcept;

  // Solicita descoberta do endpoint público via STUN.
  // O servidor STUN é fornecido pelo chamador (não fixado aqui).
  // Preenche *result quando não nulo. Retorna false em falha.
  bool discover_public_endpoint(const udp_endpoint_t& stun_server,
                                const nat_probe_config_t& config = {},
                                nat_result_t* result = nullptr) noexcept;

  // Estado após a descoberta.
  bool has_public_endpoint() const noexcept;
  const udp_endpoint_t& local_endpoint() const noexcept;
  const udp_endpoint_t& public_endpoint() const noexcept;
  nat_type_t nat_type() const noexcept;

private:
  udp_transport* transport_ = nullptr;
  bool initialized_ = false;
  bool has_public_ = false;
  udp_endpoint_t local_ep_{};
  udp_endpoint_t public_ep_{};
  nat_type_t nat_type_ = nat_type_t::kUnknown;
};

}  // namespace net
}  // namespace p2lt
