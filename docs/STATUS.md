# STATUS — Inventário técnico do P2LT

> Documento de referência interna. Descreve **o que existe nos arquivos hoje**,
> distinguindo com rigor entre API declarada, implementação escrita, código
> compilado, código testado e funcionalidade comprovada em rede.
>
> **Nada aqui deve ser lido como garantia de funcionamento.**

---

## Como ler as categorias

Estas categorias **não são equivalentes** e não devem ser confundidas:

| Categoria | Significado |
|---|---|
| **Arquivo existe** | O arquivo está presente no repositório. |
| **API declarada** | O header expõe funções/tipos. Não diz nada sobre o corpo. |
| **Implementação escrita** | Existe corpo de função com lógica. |
| **Compilado** | Foi compilado sem erro no ambiente do autor. |
| **Testado** | Executado por testes automatizados que passaram. |
| **Comprovado em rede** | Observado funcionando contra outro participante real. |

Nesta etapa do projeto, **nenhuma funcionalidade foi comprovada em rede**.

---

## Situação global

| Item | Estado |
|---|---|
| Commits no repositório | **Nenhum** (branch `master` sem histórico) |
| Build do executável `ps2lan` | **Quebrado** — falta `src/main.cpp` |
| Execução de testes | **Não realizada** — sem MSVC no ambiente |
| Releases / tags | **Nenhuma** |
| Licença | **Não definida** |

---

## Inventário por módulo

| Módulo | Arquivos | O que está implementado | O que é esqueleto | Pendente | Validação |
|---|---|---|---|---|---|
| **Utilitários** | `src/util/endian.h` | Conversão big-endian e little-endian (`put16/32/64`, `get*`, `*le`), `copy_n` | — | Não tem teste próprio | Somente leitura |
| | `src/util/hex.h/.cpp` | Conversão hexadecimal | — | Sem teste | Somente leitura |
| | `src/util/log.h/.cpp` | Utilitário de log | — | Sem teste | Somente leitura |
| | `src/util/random.h/.cpp` | Aleatoriedade | — | Sem teste; revisão de uso criptográfico pendente | Somente leitura |
| | `src/util/time.h` | Obtenção de tempo em ms | — | Sem teste | Somente leitura |
| **Formato de mensagens (wire)** | `src/net/wire.h/.cpp` | Quadro de 26 bytes big-endian; tipos, flags, `encode_frame`, `decode_frame`, convites `make_invite`/`parse_invite` | — | Sem teste; formato não versionado contra o P2P | Leitura |
| **Criptografia** | `src/net/crypto.h/.cpp` | SHA-256, HMAC-SHA256, ChaCha20 (`chacha20_block`/`xor`), AEAD encrypt-then-MAC, `derive_session_key` | — | **Sem auditoria**; sem troca de chaves; sem verificação de identidade do par; sem testes | Leitura |
| **Sessões** | `src/net/session.h/.cpp` | Estruturas (`session_state_t`, `remote_peer_t`), roster serializado, `next_seq`, timeouts | Operações de rede de sessão | **Erro de namespace** `crypto::Key`; `peer_id_t` duplicado; `stun.l.google.com` **hardcoded** | Leitura |
| **Transporte UDP** | `src/net/udp_transport.h/.cpp` | Socket Winsock não bloqueante, `send_to`, `receive`, erros | — | **Falta `#include <array>`**; teste inexistente | Leitura |
| **Descoberta de NAT** | `src/net/nat_traversal.h/.cpp` | Contrato de STUN e classificação de NAT | Corpo retorna `kNotSupported`/erro; **sem STUN real** | Implementar STUN; teste | Leitura |
| **P2P (cliente)** | `src/net/p2p.h/.cpp` | Ciclo de vida, estados, move, validação | **Sem conexão**: `connect()`/`send()` retornam `kNotSupported` | Hole punching, handshake, usar `p2p_protocol` | Leitura |
| **P2P (protocolo)** | `src/net/p2p_protocol.h/.cpp` | Codec: `serialize/parse` de header e pacote, validações e limites | — | Sem integração com `p2p_client` | **Teste escrito**, não executado |
| **Relay** | `src/net/relay.h/.cpp` | Contrato e esqueletos | **Sem transporte**: `send()` retorna `kNotSupported` | Protocolo de relay; servidor | Leitura |
| **Core** | — | — | — | Não existe | — |
| **Captura / plataforma** | `cmake/FindNpcap.cmake` | Módulo CMake para localizar Npcap | Fluxo de captura não existe | `src/platform/` | — |
| **Interface** | `interface/*.png` | Mockups visuais | Sem código | `src/ui/` | — |

---

## Pendências técnicas encontradas (auditoria)

Estas são **observações da leitura dos arquivos**, não alterações feitas:

1. **`CMakeLists.txt` referencia `src/main.cpp`, que não existe.**
   O build do alvo `ps2lan` falha na configuração/ligação. Bloqueia qualquer
   execução.

2. **`udp_transport.h` usa `std::array` sem incluir `<array>`**
   (linha 13). Erro de compilação previsível.

3. **`session.h` usa `crypto::Key` e `crypto::Nonce`**
   (linhas 40, 71), mas esses tipos estão em `p2lt::net` (`crypto.h` não tem
   sub-namespace `crypto`). Não compila como está.

4. **`peer_id_t` está declarado duas vezes**: em `wire.h` (linha 33) e em
   `session.h` (linha 22), no mesmo namespace `p2lt::net`. Conflito de
   redefinição. Ambos include `wire.h`.

5. **`session_config_t` traz `stun_host = "stun.l.google.com"`** hardcoded
   (linha 53). Um nome de servidor de terceiro fixo no código — deve ser
   parametrizado.

6. **Nenhum diretório `tests/` está integrado ao build.** Não há
   `enable_testing()`/`add_test()`. O teste do protocolo P2P existe como fonte,
   mas não roda por comando padrão.

7. **Divergência entre `wire` e `p2p_protocol`.** Existem **dois formatos de
   cabeçalho** independentes: o `wire` de 26 bytes e o `p2p_protocol` de 20
   bytes. Ainda não foi decidido qual é o formato de produção, nem como
   interoperam.

---

## Estado da validação

- **Compilação:** não realizada neste ambiente (sem MSVC/CMake). As pendências
  acima vêm de leitura estática.
- **Testes:** `tests/test_p2p_protocol.cpp` cobre 13 cenários (round-trip,
  payload vazio, ponteiros nulos, buffer curto, magic/versão/tipo inválidos,
  payload/pacote no limite, truncamento, divergência de tamanho, capacidade
  insuficiente). **Não foi executado** — nenhum resultado é afirmável.
- **Rede:** nada foi testado contra outro participante.

> Nenhuma funcionalidade deve ser declarada "funcionando" com base neste
> documento. Para tal, é necessária evidência de execução.

---

## Recomendações (não aplicadas nesta etapa)

- Corrigir as pendências 1–5 em uma etapa de código dedicada.
- Integrar `tests/` ao CMake com CTest.
- Decidir o formato de quadro único (unificar `wire` e `p2p_protocol`).
- Parametrizar o servidor STUN.
- Mover código-fonte para pastas futureiras (`src/core/`, `src/platform/`,
  `src/ui/`) apenas quando existirem.
