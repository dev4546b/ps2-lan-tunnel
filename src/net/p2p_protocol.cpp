#include "net/p2p_protocol.h"

#include <cstddef>
#include <cstdint>

// p2p_protocol.cpp - Codec/validador do protocolo de transporte P2P do P2LT.
//
// ESCOPO:
//   Implementa EXCLUSIVAMENTE a serializacao/desserializacao/validacao dos
//   datagramas P2P (as quatro funcoes declaradas em p2p_protocol.h).
//
// FORA DO ESCOPO (NAO implementado aqui):
//   sockets, Winsock, STUN, NAT traversal, hole punching, relay, conexao P2P,
//   criptografia/HMAC/ChaCha20, retransmissao, threads, captura de trafego
//   PS2, PCSX2, Core e UI. O payload e tratado como BYTES OPACOS: nunca
//   interpretado, nunca decodificado.
//
// REPRESENTACAO NA REDE:
//   Ordem de bytes FIXA = big-endian (network order) para todos os inteiros.
//   Cada campo e lido/escrito byte a byte. NAO usamos memcpy() da struct
//   inteira, NAO usamos sizeof(struct) como protocolo e NAO fazemos
//   reinterpret_cast: o layout implicito de C++ (padding/alinhamento) nunca
//   toca o wire.
//
//   Layout logico do header (20 bytes, identico ao comentario do header):
//     [0..3]  magic          (uint32, big-endian)
//     [4]     version        (uint8)
//     [5]     type           (uint8)
//     [6]     flags          (uint8)
//     [7]     reserved       (uint8, sempre 0)
//     [8..11] session_id     (uint32, big-endian)
//     [12..15] sequence      (uint32, big-endian)
//     [16..19] payload_length(uint32, big-endian)
//
// ENDlAN NOTE:
//   A instrucao do estagio cita `src/util/endian.h`, porem o escopo desta
//   operacao proibe ler/tocar qualquer arquivo alem de p2p_protocol.h/.cpp.
//   Por isso os helpers abaixo sao PRIVADOS (detalhe interno deste .cpp,
//   invisivel no header) e apenas encodam/decodcam big-endian explicitamente.
//   Migrar para util/endian.h e uma troca interna, sem alterar o contrato.
//
// SEGURANCA:
//   * Ponteiro nulo e capacidade insuficiente sao rejeitados antes de
//     qualquer escrita/leitura.
//   * Nenhuma leitura ultrapassa `size`; nenhuma escrita ultrapassa
//     `capacity`.
//   * `payload_length` NUNCA e confiado antes das validacoes: e conferido
//     contra kP2pMaxPayloadSize, kP2pMaxPacketSize e contra o tamanho real
//     do buffer.
//   * Somente aritmetica AFTER validacao evita overflow de size_t.

namespace p2lt {
namespace net {
namespace {

// ---- Helpers endian privados (big-endian / network order) ---------------------------

// Escreve 4 bytes big-endian de `value` em `out`.
void store_u32_be(uint8_t* out, uint32_t value) noexcept {
  out[0] = static_cast<uint8_t>((value >> 24) & 0xFFu);
  out[1] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  out[2] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  out[3] = static_cast<uint8_t>(value & 0xFFu);
}

// Le 4 bytes big-endian de `data` (4 bytes validos garantidos pelo chamador).
uint32_t load_u32_be(const uint8_t* data) noexcept {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) |
         (static_cast<uint32_t>(data[3]));
}

// Verifica se `raw` corresponde a um tipo de mensagem conhecido (nao kInvalid).
bool is_known_message_type(uint8_t raw) noexcept {
  switch (raw) {
    case static_cast<uint8_t>(p2p_message_type_t::kHandshake):
    case static_cast<uint8_t>(p2p_message_type_t::kHandshakeAck):
    case static_cast<uint8_t>(p2p_message_type_t::kPing):
    case static_cast<uint8_t>(p2p_message_type_t::kPong):
    case static_cast<uint8_t>(p2p_message_type_t::kData):
    case static_cast<uint8_t>(p2p_message_type_t::kClose):
      return true;
    default:
      return false;
  }
}

}  // namespace

// ---- serialize_p2p_header ------------------------------------------------------------
// Serializa SOMENTE o header. Escreve exatamente kP2pHeaderSize bytes.
p2p_parse_error_t serialize_p2p_header(const p2p_packet_header_t& header,
                                        uint8_t* out,
                                        std::size_t capacity) noexcept {
  if (out == nullptr) {
    return p2p_parse_error_t::kInvalidPacket;
  }
  if (capacity < kP2pHeaderSize) {
    return p2p_parse_error_t::kInvalidSize;
  }

  // Rejeita valores impossiveis de representar em um unico datagrama.
  if (static_cast<std::size_t>(header.payload_length) > kP2pMaxPayloadSize) {
    return p2p_parse_error_t::kPayloadTooLarge;
  }

  // Byte a byte, big-endian, sem tocar no padding da struct.
  store_u32_be(out + 0, header.magic);
  out[4] = header.version;
  out[5] = static_cast<uint8_t>(header.type);
  out[6] = header.flags;
  out[7] = header.reserved;
  store_u32_be(out + 8, header.session_id);
  store_u32_be(out + 12, header.sequence);
  store_u32_be(out + 16, header.payload_length);

  return p2p_parse_error_t::kOk;
}

// ---- parse_p2p_header ----------------------------------------------------------------
// Desserializa e valida o header. NUNCA le alem de `size`.
p2p_parse_error_t parse_p2p_header(const uint8_t* data,
                                   std::size_t size,
                                   p2p_packet_header_t& header) noexcept {
  if (data == nullptr) {
    return p2p_parse_error_t::kInvalidPacket;
  }
  // Exige pelo menos o header inteiro antes de qualquer leitura.
  if (size < kP2pHeaderSize) {
    return p2p_parse_error_t::kInvalidSize;
  }

  const uint32_t magic = load_u32_be(data + 0);
  const uint8_t version = data[4];
  const uint8_t type_raw = data[5];
  const uint8_t flags = data[6];
  const uint8_t reserved = data[7];
  const uint32_t session_id = load_u32_be(data + 8);
  const uint32_t sequence = load_u32_be(data + 12);
  const uint32_t payload_length = load_u32_be(data + 16);

  if (magic != kP2pMagic) {
    return p2p_parse_error_t::kInvalidMagic;
  }
  if (version != kP2pProtocolVersion) {
    return p2p_parse_error_t::kUnsupportedVersion;
  }
  if (!is_known_message_type(type_raw)) {
    return p2p_parse_error_t::kUnknownType;
  }
  // Nao confiamos no payload_length antes de verificar o limite superior.
  if (static_cast<std::size_t>(payload_length) > kP2pMaxPayloadSize) {
    return p2p_parse_error_t::kPayloadTooLarge;
  }

  header.magic = magic;
  header.version = version;
  header.type = static_cast<p2p_message_type_t>(type_raw);
  header.flags = flags;
  header.reserved = reserved;
  header.session_id = session_id;
  header.sequence = sequence;
  header.payload_length = payload_length;

  return p2p_parse_error_t::kOk;
}

// ---- serialize_p2p_packet ------------------------------------------------------------
// Serializa header + payload opaco. Valida TUDO antes de escrever qualquer byte.
p2p_parse_error_t serialize_p2p_packet(const p2p_packet_header_t& header,
                                        const uint8_t* payload,
                                        std::size_t payload_length,
                                        uint8_t* out,
                                        std::size_t capacity) noexcept {
  if (out == nullptr) {
    return p2p_parse_error_t::kInvalidPacket;
  }

  // Valida o payload ANTES de qualquer calculo de tamanho/escrita.
  if (payload_length > kP2pMaxPayloadSize) {
    return p2p_parse_error_t::kPayloadTooLarge;
  }
  if (payload == nullptr && payload_length > 0) {
    return p2p_parse_error_t::kInvalidPacket;
  }
  // O header serializado deve refletir o payload real fornecido.
  if (static_cast<std::size_t>(header.payload_length) != payload_length) {
    return p2p_parse_error_t::kInvalidSize;
  }

  // Overflow-safe: payload_length ja esta <= kP2pMaxPayloadSize, logo a soma
  // abaixo nao pode transbordar size_t.
  const std::size_t total = kP2pHeaderSize + payload_length;
  if (total > kP2pMaxPacketSize) {
    return p2p_parse_error_t::kInvalidSize;
  }
  if (capacity < total) {
    return p2p_parse_error_t::kInvalidSize;
  }

  // Serializa o header primeiro; so copia o payload apos TODAS as validacoes.
  const p2p_parse_error_t header_result = serialize_p2p_header(header, out, capacity);
  if (header_result != p2p_parse_error_t::kOk) {
    return header_result;
  }

  if (payload_length > 0) {
    for (std::size_t i = 0; i < payload_length; ++i) {
      out[kP2pHeaderSize + i] = payload[i];
    }
  }

  return p2p_parse_error_t::kOk;
}

// ---- parse_p2p_packet ----------------------------------------------------------------
// Valida o pacote completo e extrai SOMENTE o header. Payload fica opaco.
p2p_parse_error_t parse_p2p_packet(const uint8_t* data,
                                   std::size_t size,
                                   p2p_packet_header_t& header) noexcept {
  if (data == nullptr) {
    return p2p_parse_error_t::kInvalidPacket;
  }
  // Limite inferior: pacote precisa conter ao menos o header completo.
  if (size < kP2pMinPacketSize) {
    return p2p_parse_error_t::kInvalidSize;
  }
  // Limite superior: rejeita pacotes gigantes antes de qualquer processamento.
  if (size > kP2pMaxPacketSize) {
    return p2p_parse_error_t::kInvalidSize;
  }

  // Reaproveita toda a validacao do header (magic, versao, tipo, payload).
  const p2p_parse_error_t header_result = parse_p2p_header(data, size, header);
  if (header_result != p2p_parse_error_t::kOk) {
    return header_result;
  }

  // AGORA (e so agora) o payload_length declarado e usado em aritmetica.
  const std::size_t declared_payload = static_cast<std::size_t>(header.payload_length);
  if (declared_payload > kP2pMaxPayloadSize) {
    return p2p_parse_error_t::kPayloadTooLarge;
  }

  // O payload declarado precisa caber EXATAMENTE no buffer recebido.
  const std::size_t expected_total = kP2pHeaderSize + declared_payload;
  if (size != expected_total) {
    return p2p_parse_error_t::kInvalidSize;
  }

  return p2p_parse_error_t::kOk;
}

}  // namespace net
}  // namespace p2lt
