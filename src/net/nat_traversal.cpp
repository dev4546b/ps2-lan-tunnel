#include "net/nat_traversal.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <thread>

#include "util/random.h"

namespace p2lt {
namespace net {

namespace {

// ---- Constantes STUN (RFC 5389) ------------------------------------------------------
constexpr uint16_t kStunBindingRequest = 0x0001;
constexpr uint16_t kStunBindingSuccess = 0x0101;
constexpr uint16_t kStunBindingError = 0x0111;
constexpr uint32_t kStunMagicCookie = 0x2112A442u;
constexpr size_t kStunHeaderSize = 20;       // cabeçalho STUN (bytes)
constexpr size_t kStunTransactionSize = 12;  // transaction ID (bytes)
constexpr uint16_t kAttrMappedAddress = 0x0001;
constexpr uint16_t kAttrXorMappedAddress = 0x0020;

// ---- Leitura big-endian (network order) ---------------------------------------------
uint16_t read_be16(const uint8_t* p) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) |
                               static_cast<uint16_t>(p[1]));
}

uint32_t read_be32(const uint8_t* p) noexcept {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

// ---- Parser defensivo do endpoint mapeado --------------------------------------------
// Procura XOR-MAPPED-ADDRESS (0x0020) preferencialmente e MAPPED-ADDRESS (0x0001)
// como fallback. Nunca lê além de (data + size). Padding respeitado (múltiplo de 4).
// Preenche *out quando encontra um endpoint IPv4 válido.
bool parse_mapped_address(const uint8_t* data, size_t size, udp_endpoint_t* out) noexcept {
  if (data == nullptr || size < kStunHeaderSize || out == nullptr) {
    return false;
  }

  size_t pos = kStunHeaderSize;
  while (pos + 4 <= size) {
    uint16_t attr_type = read_be16(data + pos);
    uint16_t attr_len = read_be16(data + pos + 2);
    size_t attr_value = pos + 4;
    if (attr_len > size - attr_value) {
      break;  // atributo declara comprimento além do buffer -> resposta incoerente
    }

    // XOR-MAPPED-ADDRESS (0x0020) - preferido
    if (attr_type == kAttrXorMappedAddress && attr_len >= 8) {
      uint8_t family = data[attr_value + 1];
      if (family == 0x01) {  // IPv4
        uint16_t xor_port = read_be16(data + attr_value + 2);
        uint32_t xor_ip = read_be32(data + attr_value + 4);
        // XOR com o magic cookie, conforme RFC 5389 §15.2.
        uint16_t mapped_port = static_cast<uint16_t>(xor_port ^ (kStunMagicCookie >> 16));
        uint32_t mapped_ip = xor_ip ^ kStunMagicCookie;
        out->port = mapped_port;
        out->ip[0] = static_cast<uint8_t>((mapped_ip >> 24) & 0xFF);
        out->ip[1] = static_cast<uint8_t>((mapped_ip >> 16) & 0xFF);
        out->ip[2] = static_cast<uint8_t>((mapped_ip >> 8) & 0xFF);
        out->ip[3] = static_cast<uint8_t>(mapped_ip & 0xFF);
        return true;
      }
    }

    // MAPPED-ADDRESS (0x0001) - fallback
    if (attr_type == kAttrMappedAddress && attr_len >= 8) {
      uint8_t family = data[attr_value + 1];
      if (family == 0x01) {  // IPv4
        uint16_t mapped_port = read_be16(data + attr_value + 2);
        uint32_t mapped_ip = read_be32(data + attr_value + 4);
        out->port = mapped_port;
        out->ip[0] = static_cast<uint8_t>((mapped_ip >> 24) & 0xFF);
        out->ip[1] = static_cast<uint8_t>((mapped_ip >> 16) & 0xFF);
        out->ip[2] = static_cast<uint8_t>((mapped_ip >> 8) & 0xFF);
        out->ip[3] = static_cast<uint8_t>(mapped_ip & 0xFF);
        return true;
      }
    }

    // Avança para o próximo atributo, respeitando padding (múltiplo de 4).
    size_t step = 4 + attr_len;
    step = (step + 3) & ~size_t{3};
    if (step <= 4) {
      break;  // evita loop infinito com atributo de comprimento zero
    }
    pos += step;
  }

  return false;
}

}  // namespace

void nat_traversal::initialize(udp_transport* transport) noexcept {
  transport_ = transport;
  initialized_ = (transport != nullptr);
  has_public_ = false;
  local_ep_ = {};
  public_ep_ = {};
  nat_type_ = nat_type_t::kUnknown;
}

bool nat_traversal::discover_public_endpoint(const udp_endpoint_t& stun_server,
                                             const nat_probe_config_t& config,
                                             nat_result_t* result) noexcept {
  // Limpa estado anterior.
  if (result) {
    *result = {};
  }
  has_public_ = false;
  public_ep_ = {};
  nat_type_ = nat_type_t::kUnknown;

  if (!initialized_ || transport_ == nullptr) {
    if (result) {
      result->error = nat_error_t::kNotInitialized;
    }
    return false;
  }

  if (!transport_->is_open()) {
    if (result) {
      result->error = nat_error_t::kNetworkError;
    }
    return false;
  }

  // Endpoint local (bind efetivo) para diagnóstico.
  local_ep_.port = transport_->local_port();

  const uint32_t timeout_ms = (config.timeout_ms > 0) ? config.timeout_ms : 3000;
  const uint32_t attempt_count =
      (config.attempt_count > 0) ? config.attempt_count : 3;
  const uint32_t interval_ms =
      (config.attempt_interval_ms > 0) ? config.attempt_interval_ms : 500;

  for (uint32_t attempt = 0; attempt < attempt_count; ++attempt) {
    // ---- Transaction ID (12 bytes aleatórios) -------------------------------------
    uint8_t transaction_id[kStunTransactionSize]{};
    if (!util::random_bytes(transaction_id, kStunTransactionSize)) {
      if (result) {
        result->error = nat_error_t::kUnknown;
      }
      return false;
    }

    // ---- Monta Binding Request (RFC 5389 §6) -------------------------------------
    std::array<uint8_t, kStunHeaderSize> request{};
    request[0] = static_cast<uint8_t>(kStunBindingRequest >> 8);
    request[1] = static_cast<uint8_t>(kStunBindingRequest & 0xFF);
    request[2] = 0;  // length = 0 (sem atributos)
    request[3] = 0;
    request[4] = static_cast<uint8_t>(kStunMagicCookie >> 24);
    request[5] = static_cast<uint8_t>((kStunMagicCookie >> 16) & 0xFF);
    request[6] = static_cast<uint8_t>((kStunMagicCookie >> 8) & 0xFF);
    request[7] = static_cast<uint8_t>(kStunMagicCookie & 0xFF);
    std::memcpy(request.data() + 8, transaction_id, kStunTransactionSize);

    udp_error_t send_error = udp_error_t::kOk;
    if (!transport_->send_to(stun_server, request.data(), request.size(),
                             &send_error)) {
      if (result) {
        result->error = nat_error_t::kNetworkError;
        result->native_error = transport_->last_native_error();
      }
      return false;
    }

    // ---- Espera por resposta (non-blocking) ---------------------------------------
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
      udp_datagram_t datagram{};
      udp_error_t recv_error = udp_error_t::kOk;
      if (transport_->receive(&datagram, &recv_error)) {
        // Valida resposta STUN: tamanho, magic cookie, msg type e transaction ID.
        if (datagram.payload.size() >= kStunHeaderSize) {
          const uint8_t* msg = datagram.payload.data();
          uint32_t cookie = read_be32(msg + 4);
          uint16_t msg_type = read_be16(msg);
          if (cookie == kStunMagicCookie &&
              (msg_type == kStunBindingSuccess || msg_type == kStunBindingError) &&
              std::memcmp(msg + 8, transaction_id, kStunTransactionSize) == 0) {
            if (msg_type == kStunBindingError) {
              if (result) {
                result->error = nat_error_t::kStunFailed;
              }
              return false;
            }
            // Extrai endpoint mapeado (XOR preferido, MAPPED fallback).
            udp_endpoint_t mapped{};
            if (parse_mapped_address(msg, datagram.payload.size(), &mapped)) {
              has_public_ = true;
              public_ep_ = mapped;
              nat_type_ = nat_type_t::kUnknown;  // classificação fica para camadas acima
              if (result) {
                result->success = true;
                result->public_endpoint = mapped;
                result->local_endpoint = local_ep_;
                result->nat_type = nat_type_t::kUnknown;
                result->error = nat_error_t::kNone;
              }
              return true;
            }
          }
        }
      } else if (recv_error != udp_error_t::kWouldBlock) {
        // Erro real de rede (kWouldBlock é tolerado: ainda não chegou resposta).
        if (result) {
          result->error = nat_error_t::kNetworkError;
          result->native_error = transport_->last_native_error();
        }
        return false;
      }
    }

    // Intervalo entre tentativas.
    if (attempt + 1 < attempt_count) {
      std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
  }

  // Nenhuma resposta válida chegou no prazo.
  if (result) {
    result->error = nat_error_t::kTimeout;
  }
  return false;
}

bool nat_traversal::has_public_endpoint() const noexcept {
  return has_public_;
}

const udp_endpoint_t& nat_traversal::public_endpoint() const noexcept {
  return public_ep_;
}

const udp_endpoint_t& nat_traversal::local_endpoint() const noexcept {
  return local_ep_;
}

nat_type_t nat_traversal::nat_type() const noexcept {
  return nat_type_;
}

}  // namespace net
}  // namespace p2lt
