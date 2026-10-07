# ARCHITECTURE — PS2 LAN Tunnel

> Este documento separa, de forma explícita, **a arquitetura pretendida** da
> **arquitetura efetivamente implementada**. As camadas que ainda não existem
> estão marcadas como planejadas.

---

## 1. Visão geral

O P2LT é uma ponte que transporta tráfego de rede local de PlayStation 2 pela
Internet. O computador do usuário atua como **intermediário** (bridge): captura
quadros do PS2 na rede local, encapsula-os em datagramas UDP e os entrega aos
participantes remotos.

O caminho completo, do ponto de vista do usuário, tem seis etapas:

```
[PS2 / PCSX2]
      │  (1) tráfego Ethernet na rede local
      ▼
[captura de quadros] ──► (2) [encapsulamento em datagramas UDP]
                                   │
                                   ├─► (3) descoberta de NAT (STUN)
                                   ├─► (4) conexão direta P2P  (hole punching)
                                   └─► (5) Relay, se o P2P direto falhar
                                            │
                                            ▼
                              (6) decapsulação e entrega na rede local
```

---

## 2. Camadas e responsabilidades

O projeto organiza-se em camadas com responsabilidades bem separadas:

### 2.1 `src/util/` — Utilitários

Sem dependências de rede. Responsáveis pelos detalhes que várias camadas
compartilham:

| Arquivo | Responsabilidade |
|---|---|
| `endian.h` | Conversão big-endian / little-endian. Helper-only (header-only). |
| `time.h` | Tempo monotônico em milissegundos. |
| `random.h/.cpp` | Geração de aleatoriedade. |
| `hex.h/.cpp` | Conversão hexadecimal (depuração / logs). |
| `log.h/.cpp` | Registro de mensagens. |

**Estado:** existem e foram lidos. Não possuem testes próprios. *Validação:
somente leitura.*

### 2.2 `src/net/wire.*` — Formato de mensagens (wire)

Define o formato do **quadro de aplicação** que trafega entre participantes.

- Cabeçalho fixo de **26 bytes**, big-endian (network order).
- `kWireMagic = 0x50324C54`; versão de protocolo `1`.
- Payload máximo: `65535` bytes.
- Contém: magic, versão, tipo de mensagem, flags, tamanho, sequência, id de
  sessão e id de peer (8 bytes).
- Tipos: `kHello`, `kHelloAck`, `kInvite`, `kJoin`, `kLeave`, `kPing`, `kPong`,
  `kData`, `kAck`, `kError`.
- Funções: `encode_frame`, `decode_frame`, `make_invite`, `parse_invite`.

**Estado:** implementado, sem teste. *Validação: somente leitura.*

### 2.3 `src/net/p2p_protocol.*` — Codec do protocolo P2P

Define um **segundo** formato de cabeçalho, específico do transporte P2P.

- Cabeçalho de **20 bytes**, big-endian.
- Contém: magic, versão, tipo, flags, tamanho, sequência e id de sessão.
- API: `serialize_p2p_header`, `parse_p2p_header`, `serialize_p2p_packet`,
  `parse_p2p_packet`.
- O parser valida magic, versão, tipo, tamanho e coerência entre header e
  pacote; rejeita buffers curtos e ponteiros nulos.
- O pacote é `header || payload`.

> **Nota de arquitetura:** hoje existem **dois formatos de cabeçalho
> independentes** (`wire` de 26 bytes e `p2p_protocol` de 20 bytes). Qual será o
> formato de produção ainda **não foi decidido**. Ver `STATUS.md`.

**Estado:** implementado, com teste escrito (não executado). *Validação: teste
existe, sem resultado.*

### 2.4 `src/net/crypto.*` — Primitivas criptográficas

- `kKeySize = 32`, `kNonceSize = 12`, `kTagSize = 16`, `kSha256Size = 32`.
- SHA-256, HMAC-SHA256, bloco ChaCha20, `chacha20_xor`.
- `aead_encrypt` / `aead_decrypt`: AEAD **ChaCha20 + HMAC-SHA256**, com
  Encrypt-then-MAC; o cabeçalho é usado como AAD.
- `derive_session_key`.

> **Sem auditoria de segurança.** A existência das primitivas não implica que o
> sistema seja seguro. Não há troca de chaves, não há verificação de identidade
> do par e não há testes. Não use para dados reais sem revisão.

**Estado:** implementado, não auditado. *Validação: somente leitura.*

### 2.5 `src/net/udp_transport.*` — Transporte UDP

- Abstração de socket UDP sobre Winsock (`ws2_32`), não bloqueante.
- `udp_endpoint_t` (IPv4 + porta), `udp_datagram_t` (dados + remetente).
- `kMaxDatagramSize = 65535`.
- API: `open`, `close`, `send_to`, `receive`, `local_endpoint`, códigos de erro.

> **Pendência:** o header usa `std::array` sem incluir `<array>`.

**Estado:** implementado com pendência de compilação. *Validação: somente
leitura.*

### 2.6 `src/net/nat_traversal.*` — Descoberta de NAT / STUN

- Recebe um `udp_transport*` já aberto (não cria outro).
- `discover_public_endpoint(stun_server, config, result)` → endpoint público,
  endpoint local e `nat_type_t` heurístico.
- `nat_type_t`: `kOpenInternet`, `kFullCone`, `kRestrictedCone`,
  `kPortRestrictedCone`, `kSymmetric`, `kUnknown`.

> **Sem STUN real:** o corpo retorna `kNotSupported`/erro. O endereço do servidor
> STUN é fornecido pelo chamador (mas ver a pendência de hardcode em
> `session.h`).

**Estado:** contrato definido, implementação esqueleto. *Validação: somente
leitura.*

### 2.7 `src/net/p2p.*` — Cliente P2P

- Cliente RAII, move-only, não copiável.
- Ciclo de vida: `initialize`, `connect`, `disconnect`, `is_connected`, `state`,
  `last_error`, `last_native_error`, `send`.
- Estados: `kDisconnected`, `kConnecting`, `kConnected`, `kFailed`.

> **Esqueleto honesto:** `connect()` e `send()` retornam `kNotSupported`. Não há
> handshake, não há hole punching, não há socket real neste módulo. Não simula
> sucesso.

**Estado:** contrato + esqueleto. *Validação: somente leitura.*

### 2.8 `src/net/relay.*` — Relay

- Cliente RAII, move-only.
- `relay_server_t` (IPv4 + porta) é **sempre fornecido pelo chamador** — nenhum
> servidor é hardcoded.
- Estados: `kDisconnected`, `kConnecting`, `kConnected`, `kFailed`.
- `relay_config_t`: timeout, tentativas e intervalo entre tentativas.

> **Esqueleto honesto:** `send()` retorna `kNotSupported`. Não há protocolo de
> encaminhamento.

**Estado:** contrato + esqueleto. *Validação: somente leitura.*

### 2.9 `src/net/session.*` — Estado de sessão

- `session_id_t` (4 bytes), `remote_peer_t` (peer + endpoint + RTT).
- `session_state_t`: id, peers, chave, código de sala, senha, contadores de
  sequência e timeouts.
- Roster serializado; `next_seq`; cálculo de timeout.
- `session_config_t`: porta padrão, keepalive, timeouts, servidor STUN.

> **Pendências:** erro de namespace (`crypto::Key`), `peer_id_t` duplicado, e
> `stun.l.google.com` hardcoded.

**Estado:** parcialmente implementado. *Validação: somente leitura.*

### 2.10 Camadas planejadas (não existem)

| Camada | Pasta planejada | Responsabilidade |
|---|---|---|
| Núcleo | `src/core/` | Gerenciar salas, convites, roster e ciclo de vida da sessão; decidir quando usar P2P e quando usar relay. |
| Plataforma | `src/platform/` | Captura de quadros via Npcap; injeção de pacotes na rede local. |
| Interface | `src/ui/` | Interface gráfica para o usuário (ver mockups em `interface/`). |
| Entry point | `src/main.cpp` | Ponto de entrada do executável `ps2lan`. |

---

## 3. Decisões de projeto

### 3.1 Big-endian em toda a rede

Todos os cabeçalhos em `wire` e `p2p_protocol` usam **network byte order**.
Motivo: coerência e interoperabilidade com protocols de rede convencionais.

### 3.2 P2P direto com fallback para relay

A preferência é sempre a conexão direta (menor latência, menos infraestrutura).
Quando a topologia de NAT impede, o relay funciona como alternativa de menor
exigência, ao custo de latência e banda no servidor.

### 3.3 Endereços sempre fornecidos pelo chamador

Os módulos de rede **não fixam** endereços de relay nem de STUN. A configuração
vem de camadas superiores. (Ver a pendência de hardcode do STUN em
`session_config_t`.)

### 3.4 Esqueletos honestos

Módulos sem funcionamento retornam erros explícitos (`kNotSupported`) em vez de
simular sucesso. Isso mantém o estado do projeto verificável e evita que uma
funcionalidade aparente seja tomada como real.

---

## 4. Modelo de threading (planejado)

Ainda não implementado. A intenção de projeto é:

- Uma thread de recepção UDP, responsável por `receive` e despachar callbacks.
- A thread principal / UI, sem bloquear em I/O de rede.
- O Core coordena as transições de estado.

> Nenhuma thread é criada hoje: o `p2p_client` e o `relay_client` ainda não têm
> transporte.

---

## 5. Diagramas de sequência (planejados)

### 5.1 Conexão direta P2P (pretendido)

```
Participante A          Servidor STUN         Participante B
      |                        |                     |
      |--- discover ---------->|                     |
      |<-- endpoint público ----|                     |
      |                        |                     |
      |--- endpoint público para B (sala) ----------->|
      |                        |                     |
      |--- hole punch / keepalive ------------------->|
      |<================ fluxo P2P ==================>|
```

### 5.2 Fallback para relay (pretendido)

```
Participante A        Servidor Relay         Participante B
      |                        |                     |
      |--- connect (falha P2P) |                     |
      |--- connect relay ------>|                     |
      |                        |<---- connect --------|
      |<=========== tráfego encapsulado via relay ===>|
```

> Ambos os diagramas descrevem **intenção**, não implementação.

---

## 6. Referência rápida de arquivos

| Arquivo | Papel |
|---|---|
| `src/version.h` | `P2LT_VERSION_*`, `P2LT_PROTOCOL_VERSION`. |
| `src/net/wire.{h,cpp}` | Formato de quadro de aplicação. |
| `src/net/p2p_protocol.{h,cpp}` | Codec do protocolo P2P. |
| `src/net/crypto.{h,cpp}` | Primitivas criptográficas. |
| `src/net/udp_transport.{h,cpp}` | Socket UDP. |
| `src/net/nat_traversal.{h,cpp}` | STUN / classificação de NAT. |
| `src/net/p2p.{h,cpp}` | Cliente P2P (esqueleto). |
| `src/net/relay.{h,cpp}` | Cliente relay (esqueleto). |
| `src/net/session.{h,cpp}` | Estado de sessão. |
| `src/util/endian.h` | Conversão de endianness. |
