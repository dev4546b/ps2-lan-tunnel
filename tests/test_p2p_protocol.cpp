// tests/test_p2p_protocol.cpp - Testes unitarios do codec do protocolo P2P.
//
// ESCOPO:
//   Valida o comportamento PUBLICO de src/net/p2p_protocol.h e
//   src/net/p2p_protocol.cpp (serializacao, parsing e validacao). NAO testa
//   p2p_client, sockets, STUN, relay, NAT, crypto, PS2, PCSX2, Core ou UI.
//
// SISTEMA DE TESTES:
//   O projeto NAO possui framework de testes instalado (sem GoogleTest,
//   Catch2, doctest). Portanto este arquivo e AUTOCONTIDO: usa APENAS a
//   biblioteca padrao C++17 e um harness minimo (contador de falhas + main).
//   Nenhuma dependencia externa foi adicionada.
//   O CMakeLists.txt habilita CTest, compila este arquivo em um alvo proprio
//   (test_p2p_protocol) e o registra com add_test. O codigo de retorno do main
//   e o que o CTest interpreta como sucesso (0) ou falha (1).
//
// INTEGRACAO:
//   Este arquivo esta integrado ao CMake/CTest: o alvo test_p2p_protocol
//   declara explicitamente estas duas fontes
//     tests/test_p2p_protocol.cpp
//     src/net/p2p_protocol.cpp
//   recebe o diretorio `src` no include path (o codigo inclui
//   "net/p2p_protocol.h") e e registrado em CTest com add_test. O alvo e
//   independente do executavel principal ps2lan.
//
//   RESULTADOS JA OBTIDOS:
//   Em uma execucao anterior, o conjunto de fontes acima foi compilado
//   manualmente com MSVC e executado de forma direta, com 80 verificacoes e
//   zero falhas (RESULTADO: OK).
//
//     cl /std:c++17 /W3 /EHsc /utf-8 /I src tests\test_p2p_protocol.cpp
//          src\net\p2p_protocol.cpp /Fe:test_p2p_protocol.exe
//     test_p2p_protocol.exe
//
//   ATENCAO -- LIMITE DESSA EVIDENCIA:
//   Compilar e executar o binario DIRETO com o compilador NAO e o mesmo que
//   passar pelo CTest. O resultado acima comprova apenas que as fontes do codec
//   compilam sob C++17 e que as assertcoes passam. Ele NAO comprova, por si so,
//   a execucao via CMake/CTest, que depende de configuracao e geracao
//   corretas do build. Para afirmar isso, e necessario o ctest --test-dir build.
//
// INDEPENDENCIA DOS TESTES:
//   Os bytes do wire sao montados e inspecionados aqui com codigo PROPRIO
//   (big-endian explicito), deliberadamente NAO reaproveitando os helpers
//   internos da implementacao. Assim os testes verificam de fato a codificacao
//   real e nao apenas um round-trip auto-confirmatorio.

#include "net/p2p_protocol.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

// ---- Harness minimo ------------------------------------------------------------------
int g_checks = 0;
int g_failures = 0;
const char* g_current_test = "";

void report_failure(int line, const char* expression) {
  ++g_failures;
  std::printf("FAIL [%s] linha %d: %s\n", g_current_test, line, expression);
}

void check_true(bool condition, int line, const char* expression) {
  ++g_checks;
  if (!condition) {
    report_failure(line, expression);
  }
}

void check_equal(long long actual, long long expected, int line,
                 const char* expression) {
  ++g_checks;
  if (actual != expected) {
    ++g_failures;
    std::printf("FAIL [%s] linha %d: %s (obtido=%lld, esperado=%lld)\n",
                g_current_test, line, expression, actual, expected);
  }
}

#define CHECK(cond) check_true((cond), __LINE__, #cond)
#define CHECK_EQ(actual, expected) \
  check_equal(static_cast<long long>(actual),              \
              static_cast<long long>(expected), __LINE__, #actual " == " #expected)

// ---- Codificacao big-endian local (independente da implementacao) ---------------------
// Mesmo wire format documentado no header, reimplementado aqui para inspecionar
// os bytes reais produzidos pelas funcoes sob teste.

void put_u32(uint8_t* p, uint32_t value) {
  p[0] = static_cast<uint8_t>((value >> 24) & 0xFFu);
  p[1] = static_cast<uint8_t>((value >> 16) & 0xFFu);
  p[2] = static_cast<uint8_t>((value >> 8) & 0xFFu);
  p[3] = static_cast<uint8_t>(value & 0xFFu);
}

uint32_t get_u32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         (static_cast<uint32_t>(p[3]));
}

// Monta um cabecalho valido no wire. Devolve sempre kP2pHeaderSize bytes.
void build_valid_header(uint8_t* out, uint8_t type, uint8_t flags,
                        uint32_t session_id, uint32_t sequence,
                        uint32_t payload_length) {
  put_u32(out + 0, p2lt::net::kP2pMagic);
  out[4] = p2lt::net::kP2pProtocolVersion;
  out[5] = type;
  out[6] = flags;
  out[7] = 0;  // reserved
  put_u32(out + 8, session_id);
  put_u32(out + 12, sequence);
  put_u32(out + 16, payload_length);
}

// Cabecalho em memoria equivalente a build_valid_header.
p2lt::net::p2p_packet_header_t make_header(uint8_t type, uint8_t flags,
                                          uint32_t session_id,
                                          uint32_t sequence,
                                          uint32_t payload_length) {
  p2lt::net::p2p_packet_header_t header;
  header.magic = p2lt::net::kP2pMagic;
  header.version = p2lt::net::kP2pProtocolVersion;
  header.type = static_cast<p2lt::net::p2p_message_type_t>(type);
  header.flags = flags;
  header.reserved = 0;
  header.session_id = session_id;
  header.sequence = sequence;
  header.payload_length = payload_length;
  return header;
}

using p2lt::net::kP2pHeaderSize;
using p2lt::net::kP2pMaxPacketSize;
using p2lt::net::kP2pMaxPayloadSize;
using p2lt::net::kP2pMinPacketSize;
using p2lt::net::p2p_message_type_t;
using p2lt::net::p2p_packet_header_t;
using p2lt::net::p2p_parse_error_t;

// ---- Casos 1 e 2: round-trip de header e de pacote com payload ----------------------
void test_roundtrip_header() {
  g_current_test = "roundtrip_header";
  const uint8_t type = static_cast<uint8_t>(p2p_message_type_t::kData);
  const p2p_packet_header_t header = make_header(type, 0x00, 0xAABBCCDDu, 42u, 0u);

  uint8_t buffer[kP2pHeaderSize] = {0};
  CHECK_EQ(p2lt::net::serialize_p2p_header(header, buffer, sizeof(buffer)),
           p2p_parse_error_t::kOk);

  // Verificacao byte-a-byte da codificacao big-endian (magic "P2LT").
  CHECK_EQ(buffer[0], 0x50);
  CHECK_EQ(buffer[1], 0x32);
  CHECK_EQ(buffer[2], 0x4C);
  CHECK_EQ(buffer[3], 0x54);
  CHECK_EQ(buffer[4], p2lt::net::kP2pProtocolVersion);
  CHECK_EQ(buffer[5], type);
  CHECK_EQ(buffer[6], 0x00);          // flags
  CHECK_EQ(buffer[7], 0x00);          // reserved
  CHECK_EQ(get_u32(buffer + 8), 0xAABBCCDDu);   // session_id em network order
  CHECK_EQ(get_u32(buffer + 12), 42u);          // sequence em network order
  CHECK_EQ(get_u32(buffer + 16), 0u);           // payload_length

  p2p_packet_header_t parsed;
  parsed.magic = 0;
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kOk);
  CHECK_EQ(parsed.magic, p2lt::net::kP2pMagic);
  CHECK_EQ(parsed.version, p2lt::net::kP2pProtocolVersion);
  CHECK_EQ(parsed.type, p2p_message_type_t::kData);
  CHECK_EQ(parsed.session_id, 0xAABBCCDDu);
  CHECK_EQ(parsed.sequence, 42u);
  CHECK_EQ(parsed.payload_length, 0u);
}

void test_roundtrip_packet_with_payload() {
  g_current_test = "roundtrip_packet_with_payload";
  const std::size_t payload_len = 16;
  uint8_t payload[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                         0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C};

  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x01, 0x11223344u, 7u,
      static_cast<uint32_t>(payload_len));

  uint8_t buffer[kP2pHeaderSize + 16] = {0};
  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, payload, payload_len, buffer,
                                           sizeof(buffer)),
           p2p_parse_error_t::kOk);

  // O payload deve ter sido copiado byte a byte, sem alteracao.
  for (std::size_t i = 0; i < payload_len; ++i) {
    CHECK_EQ(buffer[kP2pHeaderSize + i], payload[i]);
  }

  p2p_packet_header_t parsed;
  CHECK_EQ(p2lt::net::parse_p2p_packet(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kOk);
  CHECK_EQ(parsed.type, p2p_message_type_t::kData);
  CHECK_EQ(parsed.flags, 0x01);
  CHECK_EQ(parsed.session_id, 0x11223344u);
  CHECK_EQ(parsed.sequence, 7u);
  CHECK_EQ(parsed.payload_length, payload_len);
}

// ---- Caso 3: payload vazio -----------------------------------------------------------
void test_empty_payload_allowed() {
  g_current_test = "empty_payload_allowed";
  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kPing), 0x00, 99u, 1u, 0u);

  uint8_t buffer[kP2pHeaderSize] = {0};
  // payload == nullptr e payload_length == 0 e aceito pelo contrato.
  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, nullptr, 0, buffer,
                                           sizeof(buffer)),
           p2p_parse_error_t::kOk);

  p2p_packet_header_t parsed;
  CHECK_EQ(p2lt::net::parse_p2p_packet(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kOk);
  CHECK_EQ(parsed.payload_length, 0u);
  CHECK_EQ(parsed.type, p2p_message_type_t::kPing);
}

// ---- Caso 4: ponteiros nulos ----------------------------------------------------------
void test_null_pointers() {
  g_current_test = "null_pointers";
  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kPong), 0x00, 1u, 1u, 0u);
  uint8_t buffer[kP2pHeaderSize] = {0};
  p2p_packet_header_t parsed;

  CHECK_EQ(p2lt::net::serialize_p2p_header(header, nullptr, kP2pHeaderSize),
           p2p_parse_error_t::kInvalidPacket);
  CHECK_EQ(p2lt::net::parse_p2p_header(nullptr, kP2pHeaderSize, parsed),
           p2p_parse_error_t::kInvalidPacket);
  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, nullptr, 0, nullptr,
                                           kP2pHeaderSize),
           p2p_parse_error_t::kInvalidPacket);
  CHECK_EQ(p2lt::net::parse_p2p_packet(nullptr, kP2pHeaderSize, parsed),
           p2p_parse_error_t::kInvalidPacket);
}

// ---- Caso 5: buffer menor que o cabecalho ---------------------------------------------
void test_buffer_smaller_than_header() {
  g_current_test = "buffer_smaller_than_header";
  uint8_t buffer[kP2pHeaderSize] = {0};
  build_valid_header(buffer, static_cast<uint8_t>(p2p_message_type_t::kData), 0x00,
                     1u, 1u, 0u);
  p2p_packet_header_t parsed;

  // Um byte a menos que o cabecalho completo.
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, kP2pHeaderSize - 1, parsed),
           p2p_parse_error_t::kInvalidSize);
  // Buffer vazio tambem deve ser rejeitado.
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, 0, parsed),
           p2p_parse_error_t::kInvalidSize);
  // Idem para o parser de pacote completo.
  CHECK_EQ(p2lt::net::parse_p2p_packet(buffer, kP2pHeaderSize - 1, parsed),
           p2p_parse_error_t::kInvalidSize);
}

// ---- Caso 6: magic invalido -----------------------------------------------------------
void test_invalid_magic() {
  g_current_test = "invalid_magic";
  uint8_t buffer[kP2pHeaderSize] = {0};
  build_valid_header(buffer, static_cast<uint8_t>(p2p_message_type_t::kData), 0x00,
                     1u, 1u, 0u);
  p2p_packet_header_t parsed;

  put_u32(buffer + 0, 0xDEADBEEFu);  // magic corrompido
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kInvalidMagic);
  CHECK_EQ(p2lt::net::parse_p2p_packet(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kInvalidMagic);
}

// ---- Caso 7: versao incompativel -------------------------------------------------------
void test_unsupported_version() {
  g_current_test = "unsupported_version";
  uint8_t buffer[kP2pHeaderSize] = {0};
  build_valid_header(buffer, static_cast<uint8_t>(p2p_message_type_t::kData), 0x00,
                     1u, 1u, 0u);
  p2p_packet_header_t parsed;

  buffer[4] = static_cast<uint8_t>(p2lt::net::kP2pProtocolVersion + 1u);
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kUnsupportedVersion);
  CHECK_EQ(p2lt::net::parse_p2p_packet(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kUnsupportedVersion);
}

// ---- Caso 8: tipo de mensagem desconhecido ---------------------------------------------
void test_unknown_message_type() {
  g_current_test = "unknown_message_type";
  uint8_t buffer[kP2pHeaderSize] = {0};
  p2p_packet_header_t parsed;

  // Valor acima de todos os tipos definidos.
  build_valid_header(buffer, 0xFF, 0x00, 1u, 1u, 0u);
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kUnknownType);

  // kInvalid (0) tambem nao e um tipo valido para o wire.
  build_valid_header(buffer,
                     static_cast<uint8_t>(p2p_message_type_t::kInvalid), 0x00, 1u,
                     1u, 0u);
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kUnknownType);

  // Valor 7 tambem esta fora do conjunto definido (1..6).
  build_valid_header(buffer, 0x07, 0x00, 1u, 1u, 0u);
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kUnknownType);
}

// ---- Caso 9: payload acima do limite --------------------------------------------------
void test_payload_above_limit() {
  g_current_test = "payload_above_limit";
  uint8_t buffer[kP2pHeaderSize] = {0};
  uint8_t payload[kP2pMaxPayloadSize + 1] = {0};

  // Serializacao com payload_length acima de kP2pMaxPayloadSize.
  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 1u, 1u,
      static_cast<uint32_t>(kP2pMaxPayloadSize + 1));
  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, payload,
                                           kP2pMaxPayloadSize + 1, buffer,
                                           sizeof(buffer)),
           p2p_parse_error_t::kPayloadTooLarge);

  // Serializacao do cabecalho isolado tambem rejeita o valor.
  CHECK_EQ(p2lt::net::serialize_p2p_header(header, buffer, sizeof(buffer)),
           p2p_parse_error_t::kPayloadTooLarge);

  // Parsing de cabecalho que declara payload excessivo.
  build_valid_header(buffer, static_cast<uint8_t>(p2p_message_type_t::kData), 0x00,
                     1u, 1u, static_cast<uint32_t>(kP2pMaxPayloadSize + 1));
  p2p_packet_header_t parsed;
  CHECK_EQ(p2lt::net::parse_p2p_header(buffer, sizeof(buffer), parsed),
           p2p_parse_error_t::kPayloadTooLarge);
}

// ---- Caso 10: pacote truncado ---------------------------------------------------------
void test_truncated_packet() {
  g_current_test = "truncated_packet";
  const std::size_t declared = 16;
  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 1u, 1u,
      static_cast<uint32_t>(declared));

  // Serializa um pacote completo e valido (16 bytes de payload)...
  uint8_t complete_payload[16] = {0};
  uint8_t full[kP2pHeaderSize + 16] = {0};
  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, complete_payload, declared, full,
                                           sizeof(full)),
           p2p_parse_error_t::kOk);

  // ... confirma que o pacote completo e aceito...
  p2p_packet_header_t parsed;
  CHECK_EQ(p2lt::net::parse_p2p_packet(full, sizeof(full), parsed),
           p2p_parse_error_t::kOk);

  // ... e que o MESMO pacote truncado (8 de 16 bytes de payload) e rejeitado.
  uint8_t truncated[kP2pHeaderSize + 8] = {0};
  build_valid_header(truncated, static_cast<uint8_t>(p2p_message_type_t::kData),
                     0x00, 1u, 1u, static_cast<uint32_t>(declared));
  for (std::size_t i = 0; i < 8; ++i) {
    truncated[kP2pHeaderSize + i] = full[kP2pHeaderSize + i];
  }
  CHECK_EQ(p2lt::net::parse_p2p_packet(truncated, sizeof(truncated), parsed),
           p2p_parse_error_t::kInvalidSize);
}

// ---- Caso 11: tamanho declarado diferente do tamanho recebido --------------------------
void test_declared_size_mismatch() {
  g_current_test = "declared_size_mismatch";
  p2p_packet_header_t parsed;

  // Cabe declares 16 bytes, mas o buffer traz 12.
  uint8_t short_buf[kP2pHeaderSize + 12] = {0};
  build_valid_header(short_buf, static_cast<uint8_t>(p2p_message_type_t::kData),
                     0x00, 1u, 1u, 16u);
  CHECK_EQ(p2lt::net::parse_p2p_packet(short_buf, sizeof(short_buf), parsed),
           p2p_parse_error_t::kInvalidSize);

  // Cabe declares 12 bytes, mas o buffer traz 16 (bytes extras nao permitidos).
  uint8_t long_buf[kP2pHeaderSize + 16] = {0};
  build_valid_header(long_buf, static_cast<uint8_t>(p2p_message_type_t::kData),
                     0x00, 1u, 1u, 12u);
  CHECK_EQ(p2lt::net::parse_p2p_packet(long_buf, sizeof(long_buf), parsed),
           p2p_parse_error_t::kInvalidSize);

  // Payload declarado, porem nenhum byte de payload enviado.
  uint8_t header_only[kP2pHeaderSize] = {0};
  build_valid_header(header_only, static_cast<uint8_t>(p2p_message_type_t::kData),
                     0x00, 1u, 1u, 8u);
  CHECK_EQ(p2lt::net::parse_p2p_packet(header_only, sizeof(header_only), parsed),
           p2p_parse_error_t::kInvalidSize);

  // Serializacao tambem exige coerencia entre header.payload_length e o payload.
  const p2p_packet_header_t mismatched = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 1u, 1u, 8u);
  uint8_t out[kP2pMaxPacketSize] = {0};
  const uint8_t small_payload[4] = {0x01, 0x02, 0x03, 0x04};
  CHECK_EQ(p2lt::net::serialize_p2p_packet(mismatched, small_payload, 4, out,
                                           sizeof(out)),
           p2p_parse_error_t::kInvalidSize);
}

// ---- Caso 12: capacidade de saida insuficiente -----------------------------------------
void test_insufficient_output_capacity() {
  g_current_test = "insufficient_output_capacity";
  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 1u, 1u, 0u);
  uint8_t buffer[kP2pHeaderSize] = {0};

  // Um byte a menos que o cabecalho exigido.
  CHECK_EQ(p2lt::net::serialize_p2p_header(header, buffer, kP2pHeaderSize - 1),
           p2p_parse_error_t::kInvalidSize);
  CHECK_EQ(p2lt::net::serialize_p2p_header(header, buffer, 0),
           p2p_parse_error_t::kInvalidSize);

  // Pacote com payload: capacidade insuficiente para o total.
  const p2p_packet_header_t with_payload = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 1u, 1u, 16u);
  const uint8_t payload[16] = {0};
  CHECK_EQ(p2lt::net::serialize_p2p_packet(with_payload, payload, 16, buffer,
                                           kP2pHeaderSize + 15),
           p2p_parse_error_t::kInvalidSize);
}

// ---- Caso 13: pacote no limite maximo permitido -----------------------------------------
void test_maximum_packet() {
  g_current_test = "maximum_packet";
  static uint8_t payload[kP2pMaxPayloadSize];
  static uint8_t packet[kP2pMaxPacketSize];
  for (std::size_t i = 0; i < kP2pMaxPayloadSize; ++i) {
    payload[i] = static_cast<uint8_t>(i & 0xFFu);
  }

  const p2p_packet_header_t header = make_header(
      static_cast<uint8_t>(p2p_message_type_t::kData), 0x00, 0xFFFFFFFFu, 0xFFFFFFFEu,
      static_cast<uint32_t>(kP2pMaxPayloadSize));

  CHECK_EQ(p2lt::net::serialize_p2p_packet(header, payload, kP2pMaxPayloadSize,
                                           packet, sizeof(packet)),
           p2p_parse_error_t::kOk);
  CHECK_EQ(sizeof(packet), kP2pMaxPacketSize);

  p2p_packet_header_t parsed;
  CHECK_EQ(p2lt::net::parse_p2p_packet(packet, sizeof(packet), parsed),
           p2p_parse_error_t::kOk);
  CHECK_EQ(parsed.payload_length, kP2pMaxPayloadSize);
  CHECK_EQ(parsed.session_id, 0xFFFFFFFFu);
  CHECK_EQ(parsed.sequence, 0xFFFFFFFEu);

  // Um byte acima do maximo deve ser rejeitado.
  CHECK_EQ(p2lt::net::parse_p2p_packet(packet, kP2pMaxPacketSize + 1, parsed),
           p2p_parse_error_t::kInvalidSize);
}

}  // namespace

int main() {
  test_roundtrip_header();
  test_roundtrip_packet_with_payload();
  test_empty_payload_allowed();
  test_null_pointers();
  test_buffer_smaller_than_header();
  test_invalid_magic();
  test_unsupported_version();
  test_unknown_message_type();
  test_payload_above_limit();
  test_truncated_packet();
  test_declared_size_mismatch();
  test_insufficient_output_capacity();
  test_maximum_packet();

  std::printf("verificacoes: %d, falhas: %d\n", g_checks, g_failures);
  if (g_failures != 0) {
    std::printf("RESULTADO: FALHOU\n");
    return 1;
  }
  std::printf("RESULTADO: OK\n");
  return 0;
}
