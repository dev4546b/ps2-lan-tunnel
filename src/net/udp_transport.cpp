#include "net/udp_transport.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace p2lt {
namespace net {

namespace {

// ---- Winsock ref-count process-wide ---------------------------------------------
// Mantém o Winsock inicializado enquanto houver pelo menos um transporte vivo.
long g_wsa_ref_count = 0;

bool wsa_startup() {
  if (g_wsa_ref_count == 0) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
      return false;
    }
  }
  ++g_wsa_ref_count;
  return true;
}

void wsa_cleanup() {
  if (g_wsa_ref_count > 0) {
    --g_wsa_ref_count;
    if (g_wsa_ref_count == 0) {
      WSACleanup();
    }
  }
}

// ---- Conversões endpoint <-> sockaddr_in -----------------------------------------
sockaddr_in endpoint_to_sockaddr(const udp_endpoint_t& endpoint) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(endpoint.port);
  addr.sin_addr.s_addr =
      htonl((static_cast<uint32_t>(endpoint.ip[0]) << 24) |
            (static_cast<uint32_t>(endpoint.ip[1]) << 16) |
            (static_cast<uint32_t>(endpoint.ip[2]) << 8) |
            static_cast<uint32_t>(endpoint.ip[3]));
  return addr;
}

udp_endpoint_t sockaddr_to_endpoint(const sockaddr_in& addr) {
  udp_endpoint_t endpoint{};
  uint32_t host = ntohl(addr.sin_addr.s_addr);
  endpoint.ip[0] = static_cast<uint8_t>((host >> 24) & 0xFF);
  endpoint.ip[1] = static_cast<uint8_t>((host >> 16) & 0xFF);
  endpoint.ip[2] = static_cast<uint8_t>((host >> 8) & 0xFF);
  endpoint.ip[3] = static_cast<uint8_t>(host & 0xFF);
  endpoint.port = ntohs(addr.sin_port);
  return endpoint;
}

// ---- Mapeamento de erros Winsock -> udp_error_t ----------------------------------
udp_error_t map_wsa_error(int wsa_error) {
  switch (wsa_error) {
    case WSAEWOULDBLOCK:
      return udp_error_t::kWouldBlock;
    case WSAEINVAL:
      return udp_error_t::kInvalidArgument;
    case WSAENOTSOCK:
      return udp_error_t::kNotOpen;
    case WSAENETDOWN:
      return udp_error_t::kSocketError;
    case WSAEADDRINUSE:
      return udp_error_t::kPortInUse;
    case WSAEADDRNOTAVAIL:
      return udp_error_t::kInvalidAddress;
    case WSAEMSGSIZE:
      return udp_error_t::kDatagramTooLarge;
    case WSAECONNRESET:
      return udp_error_t::kReceiveFailed;
    case WSAENETUNREACH:
      return udp_error_t::kSendFailed;
    default:
      return udp_error_t::kSocketError;
  }
}

}  // namespace

// ---- udp_endpoint_t --------------------------------------------------------------
std::string udp_endpoint_t::to_string() const {
  char buf[24] = {};
  std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u",
                ip[0], ip[1], ip[2], ip[3], port);
  return std::string(buf);
}

// ---- udp_transport ---------------------------------------------------------------
udp_transport::udp_transport(udp_transport&& other) noexcept
    : socket_handle_(other.socket_handle_),
      nonblocking_(other.nonblocking_) {
  other.socket_handle_ = nullptr;
}

udp_transport& udp_transport::operator=(udp_transport&& other) noexcept {
  if (this != &other) {
    close();
    socket_handle_ = other.socket_handle_;
    nonblocking_ = other.nonblocking_;
    other.socket_handle_ = nullptr;
  }
  return *this;
}

udp_transport::~udp_transport() {
  close();
}

bool udp_transport::open(uint16_t local_port, udp_error_t* error) {
  if (is_open()) {
    close();
  }

  if (!wsa_startup()) {
    if (error) *error = udp_error_t::kSocketError;
    return false;
  }

  SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock == INVALID_SOCKET) {
    wsa_cleanup();
    if (error) *error = udp_error_t::kSocketError;
    return false;
  }

  sockaddr_in local_addr{};
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(local_port);
  local_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, reinterpret_cast<sockaddr*>(&local_addr),
           sizeof(local_addr)) == SOCKET_ERROR) {
    int wsa_error = WSAGetLastError();
    closesocket(sock);
    wsa_cleanup();
    if (error) {
      *error = (wsa_error == WSAEADDRINUSE) ? udp_error_t::kPortInUse
                                            : udp_error_t::kBindFailed;
    }
    return false;
  }

  u_long mode = nonblocking_ ? 1 : 0;
  if (ioctlsocket(sock, FIONBIO, &mode) == SOCKET_ERROR) {
    int wsa_error = WSAGetLastError();
    closesocket(sock);
    wsa_cleanup();
    if (error) {
      *error = (wsa_error == WSAEWOULDBLOCK) ? udp_error_t::kWouldBlock
                                             : udp_error_t::kSocketError;
    }
    return false;
  }

  socket_handle_ = reinterpret_cast<void*>(sock bonito);
  if (error) *error = udp_error_t::kOk;
  return true;
}

void udp_transport::close() {
  if (socket_handle_ != nullptr) {
    closesocket(reinterpret_cast<SOCKET>(socket_handle_));
    socket_handle_ = nullptr;
    wsa_cleanup();
  }
}

bool udp_transport::is_open() const noexcept {
  return socket_handle_ != nullptr;
}

uint16_t udp_transport::local_port() const noexcept {
  if (!is_open()) {
    return 0;
  }
  sockaddr_in addr{};
  int len = static_cast<int>(sizeof(addr));
  if (getsockname(reinterpret_cast<SOCKET>(socket_handle_),
                  reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
    return ntohs(addr.sin_port);
  }
  return 0;
}

void udp_transport::set_nonblocking(bool enable) {
  if (!is_open()) {
    return;
  }
  u_long mode = enable ? 1 : 0;
  if (ioctlsocket(reinterpret_cast<SOCKET>(socket_handle_),
                  FIONBIO, &mode) == 0) {
    nonblocking_ = enable;
  }
}

bool udp_transport::send_to(const udp_endpoint_t& endpoint,
                            const uint8_t* data,
                            size_t size,
                            udp_error_t* error) {
  if (!is_open()) {
    if (error) *error = udp_error_t::kNotOpen;
    return false;
  }
  if (data == nullptr && size > 0) {
    if (error) *error = udp_error_t::kInvalidArgument;
    return false;
  }
  if (size > kMaxDatagramSize) {
    if (error) *error = udp_error_t::kDatagramTooLarge;
    return false;
  }

  sockaddr_in dest_addr = endpoint_to_sockaddr(endpoint);
  int sent = sendto(reinterpret_cast<SOCKET>(socket_handle_),
                    reinterpret_cast<const char*>(data),
                    static_cast<int>(size),
                    0,
                    reinterpret_cast<sockaddr*>(&dest_addr),
                    sizeof(dest_addr));
  if (sent == SOCKET_ERROR) {
    int wsa_error = WSAGetLastError();
    if (error) *error = map_wsa_error(wsa_error);
    return false;
  }
  if (error) *error = udp_error_t::kOk;
  return true;
}

bool udp_transport::receive(udp_datagram_t* datagram, udp_error_t* error) {
  if (!is_open()) {
    if (error) *error = udp_error_t::kNotOpen;
    return false;
  }
  if (datagram == nullptr) {
    if (error) *error = udp_error_t::kInvalidArgument;
    return false;
  }

  std::vector<uint8_t> buffer(kMaxDatagramSize);
  sockaddr_in src_addr{};
  int addr_len = static_cast<int>(sizeof(src_addr));

  int received = recvfrom(reinterpret_cast<SOCKET>(socket_handle_),
                          reinterpret_cast<char*>(buffer.data()),
                          static_cast<int>(buffer.size()),
                          0,
                          reinterpret_cast<sockaddr*>(&src_addr),
                          &addr_len);
  if (received == SOCKET_ERROR) {
    int wsa_error = WSAGetLastError();
    if (error) *error = map_wsa_error(wsa_error);
    return false;
  }

  datagram->remote = sockaddr_to_endpoint(src_addr);
  datagram->payload.assign(buffer.begin(),
                           buffer.begin() + received);
  if (error) *error = udp_error_t::kOk;
  return true;
}

int udp_transport::last_native_error() const noexcept {
  return WSAGetLastError();
}

}  // namespace net
}  // namespace p2lt
