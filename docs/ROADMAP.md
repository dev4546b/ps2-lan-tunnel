# ROADMAP — PS2 LAN Tunnel

> Plano de desenvolvimento. Cada item traz um **critério de conclusão
> verificável** — não basta a alteração parecer pronta.
>
> **Nenhum item abaixo está concluído**, exceto onde explicitamente indicado.

---

## Estado atual

Fase: **fundação das camadas de rede** (etapa 0 concluída, etapa 1 em andamento).

Não existe executável funcional. As camadas de rede possuem contratos e parte
das implementações, mas não há caminho completo entre dois participantes.

---

## Etapa 0 — Fundação das camadas de rede ✅

**Objetivo:** criar os contratos e as implementações de base das camadas de rede.

- [x] Utilitários de `src/util/` (endian, tempo, random, hex, log)
- [x] Formato de mensagens (`wire`) com encode/decode e convites
- [x] Primitivas criptográficas (SHA-256, HMAC, ChaCha20, AEAD)
- [x] Transporte UDP sobre Winsock
- [x] Estruturas de sessão e roster
- [x] Contratos de NAT/STUN, P2P e relay
- [x] Codec do protocolo P2P com teste dedicado escrito

**Critério de conclusão:** headers e implementações existem e são coerentes entre
si. Atingido com ressalvas (ver pendências da Etapa 1).

---

## Etapa 1 — Build íntegro e testes automatizados 🔜 (próxima)

**Objetivo:** o projeto deve compilar e ter testes executáveis por comando.

### 1.1 Corrigir pendências de compilação

- [ ] Criar `src/main.cpp` (o `CMakeLists.txt` já o referencia)
- [ ] `udp_transport.h`: incluir `<array>`
- [ ] `session.h`: corrigir o uso de `crypto::Key` / `crypto::Nonce`
- [ ] `session.h`: remover a duplicação de `peer_id_t` (mantendo uma única
      declaração, de preferência em `wire.h`)
- [ ] `session.h`: parametrizar o servidor STUN (remover
      `stun.l.google.com` hardcoded)

**Critério de conclusão:** `cmake --build build --config Release` conclui sem
erro e produz `build\Release\ps2lan.exe`.

### 1.2 Integrar testes ao build

- [ ] Adicionar `enable_testing()` e `add_test()` ao `CMakeLists.txt`
- [ ] Adicionar o alvo `test_p2p_protocol`
- [ ] Confirmar que os testes existentes passam

**Critério de conclusão:** `ctest --test-dir build` executa e reporta sucesso.

### 1.3 Unificar o formato de quadro

- [ ] Decidir se o formato de produção é o `wire` (26 bytes) ou o
      `p2p_protocol` (20 bytes), ou se são complementares (um para controle, um
      para dados)
- [ ] Documentar a decisão em `docs/ARCHITECTURE.md`

**Critério de conclusão:** existe **um** formato de cabeçalho documentado, com
versão, e ambos os módulos o referenciam.

---

## Etapa 2 — Transporte P2P funcional

**Objetivo:** dois peers em máquinas distintas estabelecem conexão direta.

- [ ] Integrar `p2p_protocol` ao `p2p_client` (hoje independentes)
- [ ] Abrir socket e enviar/receber datagramas de handshake
- [ ] Implementar `kHello` / `kHelloAck` com timeout e retentativas
- [ ] Usar `nat_traversal` para descobrir o endpoint público
- [ ] Implementar hole punching (coordenação pela sala/convite)
- [ ] Keepalive (`kPing`/`kPong`) e detecção de queda
- [ ] Remover `kNotSupported` de `connect()` e `send()` **somente quando
      houver implementação real**

**Critério de conclusão:** dois processos em máquinas diferentes trocam
datagramas de dados pelo caminho direto, com medição de RTT.

---

## Etapa 3 — Relay como fallback

**Objetivo:** conexão funcional quando o P2P direto é impossível.

- [ ] Definir o protocolo do relay (envelope de encaminhamento, identificação de
      sessão/peers)
- [ ] Implementar `relay_client` (connect, send, receive)
- [ ] Servidor relay executável (pode ser um binário simples de linha de
      comando)
- [ ] Política de decisão no Core: tentar P2P, cair para relay após N falhas
- [ ] Tratar metastabilidade e reconexão

**Critério de conclusão:** com P2P bloqueado por NAT simétrico, a conexão é
estabelecida via relay e os dados chegam integralmente.

---

## Etapa 4 — Core de sessões e salas

**Objetivo:** colar as camadas e fechar o fluxo completo do usuário.

- [ ] Criar `src/core/`
- [ ] Gerenciar ciclo de vida da sessão (criar, entrar, sair, desconectar)
- [ ] Convites por código de sala (usar `make_invite`/`parse_invite`)
- [ ] Roster de peers e propagação de entrada/saída
- [ ] Negociação de chaves de sessão (`derive_session_key`) com o modelo
      escolhido
- [ ] Timeouts, heartbeats e recuperação de falhas

**Critério de conclusão:** o fluxo "criar sala → outro participante entra pelo
código → ambos trocam dados" funciona ponta a ponta.

---

## Etapa 5 — Captura e injeção de tráfego

**Objetivo:** transportar o tráfego real do PS2.

- [ ] Criar `src/platform/`
- [ ] Captura de quadros via Npcap (usando `cmake/FindNpcap.cmake`)
- [ ] Filtro de captura para o tráfego do PS2 (redução de volume)
- [ ] Encapsulamento dos quadros capturados nos datagramas P2P
- [ ] Decapsulação e injeção na rede local do destinatário
- [ ] Tratamento de MTU, fragmentação e reordenação

**Critério de conclusão:** o tráfego do PS2 é capturado no host A e entregue no
host B, chegando ao console.

---

## Etapa 6 — Integração com PCSX2

**Objetivo:** suportar o emulador como ponta.

- [ ] Identificar como o PCSX2 expõe seu tráfego de rede
- [ ] Integrar sem captura de sistema (plugin, socket local ou equivalente)
- [ ] Validar a operação PS2 ↔ PCSX2

**Critério de conclusão:** sessão entre PS2 físico e PCSX2 com tráfego
verificado.

---

## Etapa 7 — Interface gráfica

**Objetivo:** tornar o projeto utilizável por quem não é da área.

- [ ] Criar `src/ui/` conforme os mockups em `interface/`
- [ ] Tela principal: ativar conexão, estado
- [ ] Criar sala / entrar com código
- [ ] Lista de participantes e estado da conexão
- [ ] Log acessível e legível para leigos
- [ ] Mensagens de erro claras (inclusive "não foi possível conectar" com causa)

**Critério de conclusão:** um usuário consegue completar o fluxo do início ao fim
apenas pela interface, sem usar terminal.

---

## Etapa 8 — Qualidade, testes e auditoria

**Objetivo:** elevar a confiança no código.

- [ ] Suíte de testes de integração (dois processos reais)
- [ ] Testes de longo prazo (estabilidade, vazamento de handles/buffers)
- [ ] Auditoria de segurança do módulo criptográfico e da negociação de chaves
- [ ] Sanitizadores (ASan/UBSan) em modo de teste
- [ ] CI com build e testes automáticos
- [ ] Teste com jogos reais e registro de compatibilidade

**Critério de conclusão:** suíte de testes roda automaticamente e a auditoria de
segurança não aponta riscos altos abertos.

---

## Etapa 9 — Empacotamento e distribuição

**Objetivo:** entregar um programa instalável.

- [ ] Installer para Windows
- [ ] Detecção de Npcap na instalação
- [ ] Assinatura de código (opcional)
- [ ] Decisão e publicação da licença
- [ ] Primeiro release estável

**Critério de conclusão:** um usuário instala e executa sem intervenção manual
além de instalar o Npcap.

---

## Cronograma

**Não há cronograma definido.** A ordem das etapas reflete dependências técnicas,
não prazos. Etapas 1–4 são pré-requisitos de qualquer demonstração funcional.

---

## Fora do escopo (por enquanto)

- Suporte a plataformas não-Windows.
- NAT traversal avançado além de STUN + hole punching (por exemplo TURN/ICE).
- Certificação ou garantia de compatibilidade com jogos específicos.
- Modo servidor dedicado com descoberta automática de salas públicas.
