#pragma once

// p2p_protocol.h - Contrato do protocolo de transporte P2P do P2LT (PS2 LAN Tunnel).
//
// FUNCAO:
//   Descreve o formato logico dos datagramas trocados entre dois participantes
//   via UDP para uma futura conexao P2P direta (hole punching tratado em OUTRA
//   camada). Este header NAO implementa: sockets, STUN, NAT traversal,
//   hole punching, relay, criptografia, servidor P2P, captura de pacotes PS2,
//   PCSX2, UI ou Core.
//
//   Sera consumido TARDIAMENTE pelo p2p_client (src/net/p2p.cpp) para
//   construir/validar datagramas reais.
//
// LAYOUT DE REDE (IMPORTANTE):
//   As estruturas abaixo sao CONTRATOS DE MEMORIA. Nao se deve confiar no
//   layout binario implicito de C++ para transmitir na rede: os campos serao
//   serializados/deserializados EXPLICITAMENTE (byte a byte, endianness
//   definido pela camada de serializacao). Por isso usamos somente tipos de
//   largura fixa (<cstdint>) e verificamos o tamanho do header com
//   static_assert.
//
// SEGURANCA:
//   O MAIOR sobre o wire CAI na validacao: nao ha IP, porta, peer ID, sessao
//   ou chave hardcoded aqui. Sessao/sequencia/payload sao sempre fornecidos
//   pelo chamador em runtime. A integracao com criptografia (crypto.h) e
//   reservada para um estagio futuro e NAO e feita neste arquivo.

#include <array>
#include <cstddef>
#include <cstdint>

namespace p2lt {
namespace net {

// ---- Constantes globais do protocolo --------------------------------------------------
inline constexpr uint32_t kP2pMagic = 0x50324C54u;  // "P2LT" (magic do datagrama P2P)
inline constexpr uint8_t kP2pProtocolVersion = 1;    // versao do protocolo P2P

// ---- Limites (UDP amigavel, sem depender de fragmentacao IP) --------------------------
// Datagrama unico escalado para caber folgadamente em UDP sobre IPv4/IPv6 sem
// fragmentacao obrigatoria. Valores estaveis e conservadores.
inline constexpr std::size_t kP2pMaxPayloadSize = 1400;  // payload maximo (bytes)
inline constexpr std::size_t kP2pHeaderSize = 20;        // tamanho do header fixo (bytes)
inline constexpr std::size_t kP2pMaxPacketSize =
    kP2pHeaderSize + kP2pMaxPayloadSize;                 // pacote maximo (bytes)
inline constexpr std::size_t kP2pMinPacketSize = kP2pHeaderSize;  // pacote minimo valido

// ---- Tipos de mensagem P2P -------------------------------------------------------------
// Valores explicitos e ESTAVEIS: jamais devem ser reordenados/renumerados.
enum class p2p_message_type_t : uint8_t {
  kInvalid = 0,        // tipo vazio/invalido
  kHandshake = 1,      // handshake inicial (inicio de sessao)
  kHandshakeAck = 2,   // confirmacao/ack do handshake
  kPing = 3,           // keepalive / ping
  kPong = 4,           // resposta de ping / pong
  kData = 5,           // dados de aplicacao
  kClose = 6,          // encerramento da sessao
};

// ---- Flags do pacote (value = bit) ----------------------------------------------------
// Pequeno conjunto estavel de sinalizacoes. Bits restantes reservados para uso
// futuro.
inline constexpr uint8_t kP2pFlagNone = 0x00;
inline constexpr uint8_t kP2pFlagAck = 0x01;          // pacote confirma o anterior
inline constexpr uint8_t kP2pFlagRetransmit = 0x02;   // retransmissao de dados

// ---- Cabecalho de pacote P2P (contrato de memoria; serializacao explicita) ------------
// Layout logico (order fixa para a futura serializacao explicita):
//   [0..3]  magic
//   [4]     version
//   [5]     type
//   [6]     flags
//   [7]     reserved
//   [8..11] session_id
//   [12..15] sequence
//   [16..19] payload_length
struct p2p_packet_header_t {
  uint32_t magic = kP2pMagic;                          // magic de validacao
  uint8_t version = kP2pProtocolVersion;               // versao do protocolo
  p2p_message_type_t type = p2p_message_type_t::kInvalid;  // tipo da mensagem
  uint8_t flags = kP2pFlagNone;                        // flags (bitmask)
  uint8_t reserved = 0;                                // reservado (deve ser 0)
  uint32_t session_id = 0;                             // identificador de sessao
  uint32_t sequence = 0;                               // numero de sequencia
  uint32_t payload_length = 0;                         // tamanho do payload (bytes)
};

// O header nao depende do layout implicito de C++ na rede, mas o tamanho fixo
// e garantido estaticamente para a validacao local e a futura serializacao.
static_assert(sizeof(p2p_packet_header_t) == kP2pHeaderSize,
              "p2p_packet_header_t deve ter tamanho igual a kP2pHeaderSize");
static_assert(sizeof(kP2pMagic) >= 4 && sizeof(kP2pMagic) <= sizeof(uint32_t),
              "magic deve caber em uint32_t");

// ---- Resultado de parsing/validacao ----------------------------------------------------
// Enum de status para o futurado parser do p2p_client. Sem implementacao
// nesta etapa (apenas contrato).
enum class p2p_parse_error_t : int {
  kOk = 0,                 // pacote valido
  kInvalidPacket = -1,     // pacote estruturalmente invalido
  kInvalidMagic = -2,      // magic nao confere
  kUnsupportedVersion = -3,// versao incompativel
  kUnknownType = -4,       // tipo de mensagem desconhecido
  kInvalidSize = -5,       // tamanho inconsistente (pacote/header/payload)
  kPayloadTooLarge = -6,   // payload excede kP2pMaxPayloadSize
  kInvalidSequence = -7,   // sequencia fora de ordem/invalida
  kInvalidSession = -8,    // sessao desconhecida/invalida
};

// ---- Estruturas de mensagem (contratos de memoria apenas) ------------------------------
// Carregam SOMENTE meta-dados coerentes com o tipo. O payload de dados e
// fornecido externamente pelo chamador; estas estruturas nao armazenam o
// conteudo do datagrama UDP.

// Handshake inicial: abre uma nova sessao.
struct p2p_handshake_t {
  uint16_t source_port = 0;  // porta UDP de origem sugerida (nao hardcoded)
  uint16_t flags = 0;        // flags de handshake (reservado)
};

// Ack de handshake: confirma a sessao iniciada pelo peer.
struct p2p_handshake_ack_t {
  uint32_t session_id = 0;   // sessao confirmada (eco)
  uint32_t sequence = 0;     // sequencia do handshake original (eco)
};

// Ping (keepalive): mantem a sessao viva.
struct p2p_ping_t {
  uint32_t timestamp = 0;    // carimbo do emissor (nao hardcoded)
};

// Pong: resposta ao ping.
struct p2p_pong_t {
  uint32_t timestamp = 0;    // mesmo carimbo do ping correspondente
};

// Dados: transporte de payload de aplicacao (conteudo externo).
struct p2p_data_t {
  uint32_t payload_length = 0;  // numero de bytes do payload (externo)
};

// Close: encerramento da sessao.
struct p2p_close_t {
  uint8_t reason = 0;   // razao do encerramento (0 = normal; demais reservados)
  uint8_t reserved = 0;
  uint16_t reserved2 = 0;
};

// ---- Funcoes publicas de serializacao/parsing (DECLARACOES; definicoes em
//      p2p_protocol.cpp em estagio posterior) ---------------------------------------------
// Todas seguem o protocolo de memoria explicito: nao dependem do layout
// binario implicito de C++ na rede. A serializacao usara endianness fixo
// (network order / big-endian) definido pela camada de serializacao; o
// chamador fornece os buffers em runtime (sem IP/porta/sessao/chave/peer
// hardcoded aqui). Nenhuma destas funcoes abre sockets, nao usa Winsock,
// STUN, relay, NAT, hole punching, criptografia, captura PS2, PCSX2, UI ou
// Core.

// Serializa SOMENTE o cabecalho em `out` (capacity bytes minimos =
// kP2pHeaderSize). Retorna kOk em sucesso; erro especifico caso contrario.
p2p_parse_error_t serialize_p2p_header(
    const p2p_packet_header_t& header,
    std::uint8_t* out,
    std::size_t capacity) noexcept;

// Desserializa e valida o cabecalho a partir de `data` (size bytes).
// Verifica magic, versao, tipo, tamanho e limites. Nunca le alem de `size`.
p2p_parse_error_t parse_p2p_header(
    const std::uint8_t* data,
    std::size_t size,
    p2p_packet_header_t& header) noexcept;

// Serializa um pacote completo (header + payload opaco) em `out`
// (capacity bytes minimos = kP2pHeaderSize + payload_length). O payload e
// tratado como bytes opacos; NAO e interpretado aqui. Valida payload_length
// contra kP2pMaxPayloadSize e kP2pMaxPacketSize antes de escrever.
p2p_parse_error_t serialize_p2p_packet(
    const p2p_packet_header_t& header,
    const std::uint8_t* payload,
    std::size_t payload_length,
    std::uint8_t* out,
    std::size_t capacity) noexcept;

// Desserializa e valida um pacote completo a partir de `data` (size bytes).
// Extrai o cabecalho (com todas as validacoes de parse_p2p_header) e confere
// a coerencia entre o tamanho total do buffer, o tamanho do header e o
// payload declarado, respeitando kP2pMaxPayloadSize e kP2pMaxPacketSize.
// Nunca le alem de `size`; nunca confia em payload_length antes de validar.
p2p_parse_error_t parse_p2p_packet(
    const std::uint8_t* data,
    std::size_t size,
    p2p_packet_header_t& header) noexcept;

}  // namespace net
}  // namespace p2lt
