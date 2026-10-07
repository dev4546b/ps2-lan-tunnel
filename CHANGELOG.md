# CHANGELOG

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/).
Este projeto segue [SemVer](https://semver.org/lang/pt-BR/) para versões
publicadas.

---

## [Não publicado]

Estado atual do repositório. **Ainda não há histórico de commits** — esta seção
registra o que existe hoje na árvore de trabalho.

### Adicionado

#### Documentação

- `README.md` com visão geral do projeto, problema, objetivo, estado atual,
  estrutura de diretórios, requisitos, build, limitações conhecidas e próximos
  passos.
- `docs/ARCHITECTURE.md` separando arquitetura pretendida de arquitetura
  implementada, com descrição por camada e diagramas de sequência planejados.
- `docs/STATUS.md` com inventário técnico por módulo e distinção explícita
  entre API declarada, implementação, compilação, teste e comprovação em rede.
- `docs/ROADMAP.md` com etapas de desenvolvimento e critérios verificáveis de
  conclusão.
- `docs/DEVELOPMENT.md` com pré-requisitos, fluxo de build, execução de testes,
  convenções de código e fluxo de trabalho de alterações.
- `CONTRIBUTING.md` com diretrizes de contribuição, convenções de código,
  exigências de segurança e política de pull request.
- `CHANGELOG.md` (este arquivo).
- Mockups de interface em `interface/` (referência visual).

#### Infraestrutura

- `CMakeLists.txt` com C++17, executável `ps2lan`, Npcap opcional
  (`P2LT_BUILD_CAPTURE`) e find module em `cmake/FindNpcap.cmake`.
- `scripts/build.ps1` para localizar o MSVC via `vswhere` e executar o build.
- `scripts/setup_npcap.ps1` para instalação/verificação do Npcap.
- `.gitignore` cobrindo artefatos de build, IDE e arquivos locais.

#### Utilitários (`src/util/`)

- Conversão de endianness big-endian e little-endian (`endian.h`).
- Tempo em milissegundos (`time.h`), aleatoriedade (`random.h/.cpp`),
  hexadecimal (`hex.h/.cpp`) e log (`log.h/.cpp`).

#### Rede (`src/net/`)

- `wire.h/.cpp`: formato de quadro de aplicação com cabeçalho de 26 bytes
  big-endian, tipos de mensagem, flags, `encode_frame`, `decode_frame`,
  `make_invite` e `parse_invite`.
- `crypto.h/.cpp`: SHA-256, HMAC-SHA256, ChaCha20 (`chacha20_block`,
  `chacha20_xor`), AEAD ChaCha20+HMAC com Encrypt-then-MAC e
  `derive_session_key`.
- `udp_transport.h/.cpp`: socket UDP não bloqueante sobre Winsock, envio e
  recepção de datagramas, endpoints e códigos de erro.
- `session.h/.cpp`: estruturas de sessão, roster de peers, serialização,
  contadores de sequência, timeouts e configuração de sessão.
- `nat_traversal.h/.cpp`: contrato de descoberta de endpoint público via STUN e
  classificação heurística de NAT.
- `p2p.h/.cpp`: cliente P2P com ciclo de vida RAII, estados e move semantics.
- `p2p_protocol.h/.cpp`: codec do protocolo P2P com cabeçalho de 20 bytes
  big-endian, `serialize_p2p_header`, `parse_p2p_header`,
  `serialize_p2p_packet` e `parse_p2p_packet`.
- `relay.h/.cpp`: cliente relay com contrato de servidor, configuração de
  tentativas e estados.

#### Testes

- `tests/test_p2p_protocol.cpp`: harness auto-contido (sem dependências
  externas) cobrindo round-trip, payload vazio, ponteiros nulos, buffer curto,
  magic/versão/tipo inválidos, tamanhos no limite, truncamento, divergência de
  tamanho declarado e capacidade insuficiente.

### Pendente

Não resolvido neste momento, registrado em `docs/STATUS.md`:

- `src/main.cpp` é referenciado pelo `CMakeLists.txt` mas não existe.
- Build do executável não conclui; não há binário disponível.
- `udp_transport.h` usa `std::array` sem incluir `<array>`.
- `session.h` referencia `crypto::Key`/`crypto::Nonce` em namespace incorreto.
- `peer_id_t` declarado em `wire.h` e `session.h` no mesmo namespace.
- `session_config_t` fixa `stun.l.google.com` no código.
- Testes não integrados ao CMake (sem `enable_testing()`/`add_test()`).
- Dois formatos de cabeçalho coexistem (`wire` e `p2p_protocol`) sem decisão
  sobre o formato de produção.
- Licença não definida.
- Nenhuma compilação ou execução de testes foi realizada neste ambiente.

---

## Versões

Nenhuma versão foi publicada. O projeto está em **protótipo inicial**
(`0.1.0-prototype`, conforme `src/version.h`), sem release, sem tag e sem
distribuição.

Quando a primeira versão for publicada, adicione aqui uma seção no formato:

```markdown
## [0.1.0] - AAAA-MM-DD

### Adicionado
- ...
```

O primeiro release deve ocorrer apenas após: build limpo, testes automatizados
integrados ao CMake, definição da licença e verificação do fluxo de sala com
pelo menos dois participantes.
