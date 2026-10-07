#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace p2lt {
namespace net {

// ---- Endpoint IPv4 -------------------------------------------------------------
struct udp_endpoint_t {
  std::array<uint8_t, 4> ip{};
  uint16_t port = 0;

  // Converte para string "a.b.c.d:port" para diagnóstico
  std::string to_string() const;
};

// ---- Datagrama recebido --------------------------------------------------------
struct udp_datagram_t {
  udp_endpoint_t remote;
  std::vector<uint8_t> payload;
};

// ---- Limites -------------------------------------------------------------------
constexpr size_t kMaxDatagramSize = 65535;  // limite teórico UDP, prático ~1472 MTU

// ---- Códigos de erro -----------------------------------------------------------
enum class udp_error_t : int {
  kOk = 0,
  kSocketError = -1,
  kBindFailed = -2,
  kPortInUse = -3,
  kInvalidAddress = -4,
  kSendFailed = -5,
  kReceiveFailed = -6,
  kNotOpen = -6,
  kWouldBlock = -7,      // nenhum pacote disponível (non-blocking)
  kDatagramTooLarge = -8,
  kInvalidArgument = -9,
};

// ---- Transporte UDP ------------------------------------------------------------
class udp_transport {
public:
  udp_transport() = default;
  ~udp_transport();

  udp_transport(const udp_transport&) = delete;
  udp_transport& operator=(const udp_transport&) = delete;
  udp_transport(udp_transport&&) noexcept;
  udp_transport& operator=(udp_transport&&) noexcept;

  // Abre o socket e faz bind na porta local.
  // Se local_port == 0, o SO escolhe uma porta efêmera.
  // Retorna true em sucesso, false em falha (erro em *error se não nulo).
  bool open(uint16_t local_port = 0, udp_error_t* error = nullptr);

  // Fecha o socket. Pode ser chamado múltiplas vezes.
  void close();

  // Verifica se o transporte está aberto.
  bool is_open() const noexcept;

  // Retorna a porta local do bind (0 se fechado).
  uint16_t local_port() const noexcept;

  // Define modo não bloqueante (padrão: true).
  void set_nonblocking(bool enable);

  // Envia um datagrama para o endpoint remoto.
  // Retorna true em sucesso, false em falha (erro em *error se não nulo).
  bool send_to(const udp_endpoint_t& endpoint,
               const uint8_t* data,
               size_t size,
               udp_error_t* error = nullptr);

  // Tenta receber um datagrama.
  // Retorna:
  //   true  -> datagrama recebido (preenche *datagram)
  //   false -> nenhum dado disponível (non-blocking) ou erro (erro em *error)
  // Se error == kWouldBlock, nenhum dado estava disponível no momento.
  bool receive(udp_datagram_t* datagram, udp_error_t* error = nullptr);

  // Retorna o último erro de Winsock capturado (WSAGetLastError).
  int last_native_error() const noexcept;

private:
  // Handle do socket (INVALID_SOCKET quando fechado)
  void* socket_handle_{nullptr};
  bool nonblocking_{true};
};

}  // namespace net
}  // namespace p2lt