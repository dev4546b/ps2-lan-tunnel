# DEVELOPMENT — Ambiente, build e validação

> Este documento descreve o fluxo de trabalho **como ele existe hoje**. Onde há
> limitação ou bloqueio, está marcado explicitamente.

---

## 1. Pré-requisitos

| Ferramenta | Versão / requisito | Obrigatória? |
|---|---|---|
| Windows | Windows 10/11 x64 | Sim |
| Visual Studio | 2022 ou superior, com workload **"Desktop development with C++"** | Sim |
| CMake | ≥ 3.22 | Sim |
| PowerShell | 5.1+ (usado por `scripts/build.ps1`) | Sim |
| Git | qualquer versão recente | Sim |
| Npcap | instalado no sistema | Não (apenas para captura) |
| Python / geradores | — | Não (build sem dependências externas) |

O projeto não usa geradores de código, frameworks de teste ou bibliotecas
externas de C++. O único componente opcional é o Npcap.

---

## 2. Obter o código

```powershell
git clone git@github.com:dev4546b/ps2-lan-tunnel.git
cd ps2-lan-tunnel
```

> **Nota:** o repositório ainda não possui nenhum commit publicado no momento da
> escrita deste documento. Verifique com o mantenedor antes de assumir que há
> histórico disponível.

---

## 3. Build

### 3.1 Script automatizado

```powershell
.\scripts\build.ps1
```

O script:

1. Localiza o Visual Studio via `vswhere` (workload `VC.Tools.x86.x64`).
2. Localiza e executa `vcvars64.bat` para configurar o ambiente do MSVC.
3. Executa `cmake -S . -B build` e `cmake --build build --config Release`.

Parâmetros opcionais:

```powershell
.\scripts\build.ps1 -Configuration Debug
.\scripts\build.ps1 -Generator "Visual Studio 17 2022"
```

### 3.2 Manual

A partir de um prompt com o ambiente do MSVC já carregado
("Developer PowerShell for VS"):

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Saída esperada: `build\Release\ps2lan.exe`.

### 3.3 Opção de captura

```powershell
cmake -S . -B build -DP2LT_BUILD_CAPTURE=ON    # padrão
cmake -S . -B build -DP2LT_BUILD_CAPTURE=OFF   # sem Npcap
```

O módulo `cmake/FindNpcap.cmake` é usado quando a opção está ativa. Enquanto não
existir `src/platform/`, o Npcap é apenas localizado, não utilizado.

### 3.4 Npcap

Para instalar/verificar o Npcap:

```powershell
.\scripts\setup_npcap.ps1
```

Npcap exige execução como administrador para instalação.

---

## 4. Estado atual do build (leitura obrigatória)

> **O build do executável atualmente falha.**
>
> O `CMakeLists.txt` referencia `src/main.cpp`, que **não existe** no
> repositório. Além disso, há inconsistências de compilação conhecidas em
> `src/net/udp_transport.h` e `src/net/session.h`.
>
> Portanto: **não há binário para executar**, e nenhum comando acima produzirá um
> executável funcional até que essas pendências sejam resolvidas.

Pendências detalhadas em [`STATUS.md`](STATUS.md).

---

## 5. Testes

### 5.1 Estado atual

- Existe **um** arquivo de teste: `tests/test_p2p_protocol.cpp`.
- Ele **não está integrado** ao CMake: não há `enable_testing()` nem
  `add_test()`.
- Ele **não foi executado** neste ambiente (sem MSVC).
- Não existe runner, framework ou CI.

### 5.2 Executar o teste manualmente (MSVC)

```powershell
cl /std:c++17 /W3 /I src tests\test_p2p_protocol.cpp src\net\p2p_protocol.cpp /Fe:test_p2p_protocol.exe
.\test_p2p_protocol.exe
```

O teste é auto-contido: usa um harness `main()` simples, sem dependências
externas. Ele cobre, entre outros: round-trip, payload vazio, ponteiros nulos,
buffer curto, magic/versão/tipo inválidos, tamanhos no limite, truncamento,
divergência entre tamanho declarado e payload, e capacidade insuficiente.

### 5.3 Integração futura com CTest

A integração pretendida (não aplicada) seria:

```cmake
enable_testing()
add_executable(test_p2p_protocol tests/test_p2p_protocol.cpp src/net/p2p_protocol.cpp)
target_include_directories(test_p2p_protocol PRIVATE src)
add_test(NAME p2p_protocol COMMAND test_p2p_protocol)
```

---

## 6. Convenções de código

Para manter consistência, siga o padrão já presente no repositório:

- **C++17**, sem dependências externas além de `bcrypt` e Winsock.
- **Headers** com `#pragma once` e guarda de include relativo
  (`#include "net/udp_transport.h"`), não caminho absoluto.
- **Namespace** `p2lt`, com sub-namespaces `p2lt::net` e `p2lt::util`.
- **Identificadores** em `snake_case`; constantes com prefixo `k`
  (`kMaxDatagramSize`); tipos com sufixo `_t` (`udp_endpoint_t`).
- **Erros** via enums `*_error_t` com valores negativos; `kNone = 0`.
- **Tipos de estado** via `enum class`.
- **Classes de rede** RAII: destrutor `noexcept`, não copiável, move-only quando
  fizer sentido.
- **Endereços de rede sempre fornecidos pelo chamador** — nada de IP/porta
  hardcoded em módulos de rede.
- **Endianness explícita**: rede em big-endian; usar os helpers de
  `src/util/endian.h` em vez de `reinterpret_cast`.
- **Comentários em português**, explicando a *intenção* e as decisões, não
  repetindo o código.

### 6.1 Sobre segurança

- Nunca use `rand()` para material criptográfico; prefira
  `src/util/random.h` e revise o uso de `BCryptGenRandom`.
- Nunca registre chaves, senhas, `room_code` ou payloads de sessão em logs.
- Endpoints recebidos da rede (STUN, relay) devem ser validados antes do uso.
- Todo buffer recebido da rede deve ser validado em tamanho antes de qualquer
  leitura.

---

## 7. Fluxo de trabalho de uma alteração

1. Abra uma issue descrevendo o problema e o resultado esperado.
2. Crie um branch dedicado (`feature/...` ou `fix/...`).
3. Faça a alteração **em um arquivo por vez**, mantendo o escopo pequeno.
4. Se houver alteração de rede ou de protocolo, documente o formato do
   cabeçalho afetado em `docs/ARCHITECTURE.md`.
5. Atualize `docs/STATUS.md` se o status de um módulo mudar.
6. Registre a alteração em `CHANGELOG.md` na seção `Não publicado`.
7. Abra o pull request descrevendo **o que foi testado** e **o que não foi**.

> Não faça commit direto em `master`.

---

## 8. O que ainda não existe

Não tente configurar estas etapas — elas ainda não foram criadas:

- Gerador de código (nenhum usado).
- Instalador / empacotamento.
- Integração contínua (CI).
- Suíte de testes de integração.
- Ferramenta de formatação automática (o padrão é seguir o estilo existente à
  mão).

---

## 9. Ambiente atual do autor

O ambiente em que esta documentação foi escrita **não possui MSVC, CMake ou
Npcap**. Nenhuma compilação e nenhum teste foram executados. Todas as afirmações
sobre o build e os testes neste repositório vêm de **leitura estática dos
arquivos**, não de execução.

> Compilação não realizada: ambiente sem MSVC/CMake/Npcap disponível. Validação
> apenas estática.
