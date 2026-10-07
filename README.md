# PS2 LAN Tunnel (P2LT)

![prototype](https://img.shields.io/badge/estado-prot%C3%B3tipo-0ea5e9) ![vers%C3%A3o](https://img.shields.io/badge/vers%C3%A3o-0.1.0--prot%C3%B3tipo-6c4fd6) ![licen%C3%A7a](https://img.shields.io/badge/licen%C3%A7a-a%20definir-95a5a6)

> **Aviso importante**
> Este repositório está em **estado de protótipo inicial**. Nenhuma conexão
> entre jogadores foi comprovada até o momento. As combinações de plataforma
> descritas abaixo são **objetivos de desenvolvimento**, e não
> compatibilidades já testadas. Não há binaries, releases ou versões publicadas.

O **PS2 LAN Tunnel** é um projeto em desenvolvimento para permitir que
participantes em **redes físicas diferentes** joguem juntos, pela Internet,
títulos de PlayStation 2 com multiplayer em rede local — usando um computador
como ponte.

---

## Problema

Jogos de PS2 com multiplayer em rede local (LAN) normalmente exigem que todos os
participantes estejam na **mesma rede física** (o mesmo roteador, o mesmo
cabo). Quando amigos estão em cidades ou provedores diferentes, a única forma de
se conectar é pela **LAN over WAN** — um túnel que faz o tráfego da rede local
do PS2 atravessar a Internet.

O P2LT quer resolver isso de forma transparente: o usuário conecta o PS2 à rede
local, abre o programa no computador, ativa a conexão, cria ou entra em uma sala
e joga. O objetivo é **esconder os detalhes técnicos de rede** de quem não é da
área, sem abrir mão de segurança nem de transparência sobre o estado real da
conexão.

---

## Objetivo do projeto (combinações pretendidas)

Estas combinações são **metas**, ainda **não comprovadas**:

| Combinação pretendida | Estado |
|---|---|
| PS2 físico ↔ PS2 físico | Planejado |
| PS2 físico ↔ PCSX2 | Planejado |
| PCSX2 ↔ PS2 físico | Planejado |
| PCSX2 ↔ PCSX2 | Planejado |

---

## Como a solução deverá funcionar (visão geral)

Fluxo desejado para o usuário:

1. Conectar o PS2 à rede local (quando aplicável).
2. Abrir o programa no computador.
3. Ativar a conexão.
4. Criar uma sala ou entrar em uma sala usando um código.
5. Jogar com os demais participantes, **quando o jogo e a implementação forem
   compatíveis**.

Visão de alto nível do caminho dos dados:

```
Console/Emulador (PS2 / PCSX2)
        │  tráfego Ethernet
        ▼
  Computador puente
        │
        ├─ captura dos quadros  (planejado)
        ├─ encapsulamento em datagramas  (parcial)
        ├─ conexão direta P2P (UDP + hole punching)  (esqueleto)
        └─ Relay, quando o P2P direto não é possível  (esqueleto)
                ▼
          Participantes remotos
```

Cada etapa é detalhada em [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md), com a
distinção explícita entre o que **existe hoje** e o que é **pretendido**.

---

## Público-alvo

- Usuários que querem jogar títulos de PS2 com amigos em regiões diferentes.
- Pessoas familiarizadas com desenvolvimento técnico, que podem ajudar a testar e reportar problemas.

---

## Estado atual do projeto

O projeto está na **fase de fundação das camadas de rede**. Ainda **não existe um
programa executável funcional** que conecte jogadores.

### Funcionalidades implementadas

- **Utilitários de rede** (`src/util/`): helpers de conversão de endianness
  (big-endian e little-endian), tempo, aleatoriedade, log e hexadecimal.
- **Formato de mensagens** (`src/net/wire.h`/`.cpp`): cabeçalho de quadro de 26
  bytes em big-endian, com tipos de mensagem, flags, convites (código de sala) e
  codificação/decodificação com validação.
- **Criptografia** (`src/net/crypto.h`/`.cpp`): SHA-256, HMAC-SHA256,
  ChaCha20 e um AEAD (Encrypt-then-MAC). **Não passou por auditoria de
  segurança** — ver `docs/STATUS.md`.
- **Transporte UDP** (`src/net/udp_transport.h`/`.cpp`): socket Winsock não
  bloqueante, envio/recepção de datagramas e códigos de erro.
- **Sessões** (`src/net/session.h`/`.cpp`): estrutura de estado de sessão
  (sessão, peers, chave, sala, senhas e serialização de roster).
- **Descoberta de NAT** (`src/net/nat_traversal.h`/`.cpp`): contrato de STUN e
  classificação de NAT.
- **Protocolo P2P** (`src/net/p2p_protocol.h`/`.cpp`): serialização e validação
  de datagramas P2P com teste unitário dedicado.
- **P2P e Relay** (`src/net/p2p.*`, `src/net/relay.*`): contratos e esqueletos
  honestos (ver abaixo).

### Funcionalidades planejadas (não concluídas)

- Captura de quadros Ethernet via Npcap.
- Transcaminhamento do tráfego capturado entre participantes.
- Conexão P2P direta real (hole punching).
- Servidor Relay real.
- Core de gerenciamento de sessões e salas.
- Interface gráfica.
- Empacotamento e distribuição.

### Esqueletos honestos (contrato sem funcionamento real)

`p2p_client` e `relay_client` já possuem ciclo de vida (RAII, estados, move),
mas **não estabelecem conexão**: retornam `kNotSupported` de forma deliberada,
em vez de simular sucesso. Isso é intencional — nenhum handshake está inventado.

---

## Arquitetura resumida

| Camada | Pasta | Papel |
|---|---|---|
| Utilitários | `src/util/` | endianness, log, tempo, random, hex |
| Rede | `src/net/` | wire, crypto, sessão, UDP, NAT, P2P, relay |
| Núcleo (futuro) | `src/core/` | gerenciamento de salas e sessões — **ainda não existe** |
| Plataforma (futuro) | `src/platform/` | captura Npcap — **ainda não existe** |
| Interface (futuro) | `src/ui/` | interface gráfica — **ainda não existe** |
| Testes | `tests/` | testes unitários do protocolo P2P |
| Documentação | `docs/` | arquitetura, roadmap, status, desenvolvimento |

Detalhes em [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

---

## Tecnologias utilizadas

- **C++17** (padrão definido no `CMakeLists.txt`).
- **CMake** ≥ 3.22 para build.
- **Winsock** (`ws2_32`) para o transporte UDP.
- **Npcap** (opcional) para captura de quadros — ainda não integrado ao fluxo.
- **Windows / MSVC** como plataforma de referência atual.

Protocolo próprio de datagramas UDP (cabeçalhos em big-endian / network order).

---

## Estrutura de diretórios

```
.
├── CMakeLists.txt          # build do executável (referencia src/main.cpp)
├── cmake/
│   └── FindNpcap.cmake     # localização do Npcap
├── docs/                   # documentação do projeto
│   ├── ARCHITECTURE.md
│   ├── DEVELOPMENT.md
│   ├── ROADMAP.md
│   └── STATUS.md
├── interface/              # mockups de interface (PNG, referência visual)
├── scripts/
│   ├── build.ps1           # script de build (PowerShell)
│   └── setup_npcap.ps1     # instalação/verificação do Npcap
├── src/
│   ├── version.h           # macros de versão do app/protocolo
│   ├── net/                # protocolo, crypto, sessão, UDP, NAT, P2P, relay
│   │   ├── crypto.{h,cpp}
│   │   ├── nat_traversal.{h,cpp}
│   │   ├── p2p.{h,cpp}
│   │   ├── p2p_protocol.{h,cpp}
│   │   ├── relay.{h,cpp}
│   │   ├── session.{h,cpp}
│   │   ├── udp_transport.{h,cpp}
│   │   └── wire.{h,cpp}
│   └── util/               # utilitários gerais
│       ├── endian.h
│       ├── hex.{h,cpp}
│       ├── log.{h,cpp}
│       ├── random.{h,cpp}
│       └── time.h
└── tests/
    └── test_p2p_protocol.cpp  # testes do codec P2P (CTest ainda não integrado)
```

---

## Requisitos de desenvolvimento

- **Windows** com Visual Studio 2022 (ou superior) e o workload
  **"Desktop development with C++"** (MSVC v143, ferramentas C++ x64/x86).
- **CMake** ≥ 3.22.
- **PowerShell** (usado pelo `scripts/build.ps1`).
- **Npcap** (opcional): apenas para a futura captura de quadros. Habilitada por
  padrão (`P2LT_BUILD_CAPTURE=ON`); pode ser desabilitada com
  `-DP2LT_BUILD_CAPTURE=OFF`.
- **Git** para versionamento.

---

## Como compilar

O projeto possui o script `scripts/build.ps1`, que localiza o MSVC via
`vswhere`, configura o ambiente e chama o CMake:

```powershell
.\scripts\build.ps1
```

Ou, manualmente, a partir de um prompt com o ambiente do MSVC carregado:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

> **Pendência conhecida:** o `CMakeLists.txt` referencia `src/main.cpp`, que
> **ainda não existe**. Portanto, o build do executável `ps2lan` **não conclui**
> neste momento. Isso está registrado como pendência em
> [`docs/STATUS.md`](docs/STATUS.md) e será resolvido em etapa posterior.
> Em consequência, **não há binário para executar hoje**.

Para compilar e rodar apenas os testes do protocolo P2P (sem CMake, com MSVC):

```powershell
cl /std:c++17 /W3 /I src tests\test_p2p_protocol.cpp src\net\p2p_protocol.cpp /Fe:test_p2p_protocol.exe
.\test_p2p_protocol.exe
```

Os testes ainda **não** foram executados neste ambiente (sem MSVC disponível).
Veja [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md).

---

## Limitações conhecidas

- **Não há executável funcional**: falta `src/main.cpp` e falta o Core que
  amarra as camadas.
- **Não há captura de tráfego PS2** implementada.
- **P2P e Relay são esqueletos**: não há handshake, hole punching nem protocolo
  de encaminhamento.
- **A criptografia não foi auditada**: a existência das funções (SHA-256, HMAC,
  ChaCha20, AEAD) **não** significa que o sistema como um todo seja seguro.
  Não há troca de chaves, não há verificação de identidade do par e não há
  análise formal.
- **Nenhum teste de rede real** foi executado; a classificação de NAT é
  heurística.
- **Inconsistências de compilação pendentes** em `udp_transport.h` e
  `session.h` (detalhadas em `docs/STATUS.md`).

---

## Próximas etapas

Consulte [`docs/ROADMAP.md`](docs/ROADMAP.md) para o plano completo. Resumo:

1. Resolver pendências de compilação e criar o ponto de entrada (`main.cpp`).
2. Integrar testes ao CTest.
3. Completar o transporte P2P e o Relay.
4. Implementar o Core de sessões e salas.
5. Integrar captura Npcap e encaminhamento.
6. Integrar PCSX2 e interface gráfica.
7. Testar com jogos reais e documentar compatibilidade.
8. Empacotar e distribuir.

---

## Como contribuir

Leia primeiro [`CONTRIBUTING.md`](CONTRIBUTING.md). Em resumo: alterações
pequenas, seguindo o padrão existente, sem dependerências novas sem
justificativa, e sempre informando quais testes foram executados.

> **Antes de contribuir, o proprietário do projeto precisa definir a licença.**
> Nenhuma licença foi escolhida até o momento, e nenhuma contribuição pode ser
> aceita sob termos definidos até que isso seja resolvido.

---

## Avisos finais

- **Compatibilidade por jogo precisa ser testada.** Nenhuma compatibilidade
  específica é garantida. Alguns títulos podem ter requisitos de rede que o
  P2LT não consegue reproduzir.
- **O projeto não substitui os requisitos de rede de cada jogo.** Se um jogo
  exige uma topologia, um modo de operação ou um mecanismo de detecção
  específico, a solução precisa respeitar esse requisito — não o contrário.
- **Nenhuma versão foi publicada.** Não há releases, tags ou binários oficiais.

---

## Documentação

| Documento | Conteúdo |
|---|---|
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | Arquitetura pretendida e efetivamente implementada |
| [`docs/ROADMAP.md`](docs/ROADMAP.md) | Etapas de desenvolvimento verificáveis |
| [`docs/STATUS.md`](docs/STATUS.md) | Inventário técnico por módulo |
| [`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md) | Ferramentas, build e validação |
| [`CHANGELOG.md`](CHANGELOG.md) | Histórico de alterações |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Orientações de contribuição |
