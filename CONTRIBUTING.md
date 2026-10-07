# CONTRIBUTING — PS2 LAN Tunnel

Obrigado pelo interesse em contribuir. Este guia explica como contribuir de
forma útil e segura para o P2LT.

---

## Antes de tudo: a licença ainda não foi definida

> **O projeto ainda não possui licença.** Sem uma licença, não há termos
> claros de uso, e nenhum pull request pode ser aceito sob termos definidos.
>
> Se você é o proprietário do projeto: **defina a licença antes de aceitar
> contribuições.** Isso é uma tarefa aberta e está registrada em
> `docs/ROADMAP.md` (Etapa 9).

Contribuições feitas antes de a licença ser definida podem ter que ser
reatribuídas. Isso é inevitável, mas é preferível a deixar contribuidores sem
cláusulas claras.

---

## Como contribuir

### 1. Reporte problemas

Para bugs e pedidos de recurso, abra uma issue incluindo:

- Versão do P2LT usada (quando existir release) e do Windows.
- Passos para reproduzir.
- Resultado esperado e resultado obtido.
- Log relevante, **sem** chaves, senhas ou IPs pessoais.

### 2. Trabalhe em uma mudança pequena

- Crie um branch: `feature/...`, `fix/...` ou `docs/...`.
- **Não** faça commit direto em `master`.
- Mantenha o escopo da alteração pequeno e relacionado a um único objetivo.
- Evite reformatações em massa junto com mudanças funcionais.

### 3. Siga o padrão de código existente

Antes de escrever, leia os arquivos vizinhos. Convenções atuais:

| Aspecto | Padrão |
|---|---|
| Padrão da linguagem | C++17 |
| Guardas de header | `#pragma once` |
| Includes | Relativos ao projeto (`#include "net/udp_transport.h"`) |
| Namespaces | `p2lt`, com `p2lt::net` e `p2lt::util` |
| Nomes | `snake_case`; constantes `kPrefixo`; tipos `sufixo_t` |
| Erros | `enum class *_error_t`, valores negativos, `kNone = 0` |
| Estado | `enum class` |
| Classes de rede | RAII, destrutor `noexcept`, não copiável, move-only quando aplicável |
| Endereços | Sempre fornecidos pelo chamador — nunca hardcoded |
| Endianness | Rede em big-endian; usar `src/util/endian.h` |
| Comentários | Em português, explicando a intenção, não repetindo o código |

### 4. Segurança é responsabilidade de quem altera

Se sua mudança toca criptografia, rede ou sessões:

- Valide **todo** buffer recebido antes de ler.
- Nunca registre chaves, senhas, `room_code` ou payloads de sessão.
- Nunca use `rand()` para material criptográfico.
- Não introduza endereços de servidor fixos.
- Se alterar o formato de um cabeçalho de protocolo, atualize a versão do
  protocolo e documente em `docs/ARCHITECTURE.md`.

> **Aviso:** o módulo criptográfico atual **não passou por auditoria de
> segurança**. Não o use em produção nem para dados reais sem revisão.

### 5. Atualize a documentação

Mudanças de comportamento exigem atualização documental:

| Tipo de mudança | Documento a atualizar |
|---|---|
| Módulo ganhou/perdeu funcionalidade | `docs/STATUS.md` |
| Alteração de cabeçalho/protocolo | `docs/ARCHITECTURE.md` |
| Mudança de build ou ferramentas | `docs/DEVELOPMENT.md` |
| Nova etapa ou progresso | `docs/ROADMAP.md` |
| Qualquer alteração visível | `CHANGELOG.md`, seção `Não publicado` |

### 6. Reporte o que você testou

Ao abrir o pull request, descreva explicitamente:

- **O que você testou** (comandos, cenários).
- **O que você não testou**.
- **Riscos conhecidos** da alteração.

Não declare algo como "funcionando" sem evidência de execução. O projeto
distingue rigorosamente entre "implementado", "testado" e "comprovado em rede" —
veja `docs/STATUS.md`.

---

## Pull requests

- Título curto e descritivo, em português.
- Descrição com: contexto, o que muda, por quê, e como foi validado.
- Um assunto por pull request.
- Referencie a issue relacionada, se houver.

---

## O que **não** aceitamos

- Código que simula sucesso onde a funcionalidade não existe.
- Dependências externas novas sem justificativa (o projeto hoje não tem nenhuma
  biblioteca de C++).
- Credenciais, chaves, IPs privados ou dados pessoais no código ou na
  documentação.
- Binários, builds ou artefatos compilados commitados no repositório.
- Reformatar o repositório inteiro sem necessidade.
- Afirmações de compatibilidade sem teste.

---

## Commits

Mensagens curtas, em português, no formato:

```
<tipo>: <descrição curta>
```

Tipos sugeridos: `feat`, `fix`, `docs`, `refactor`, `test`, `build`, `chore`.

Exemplos:

```
docs: clarifies that P2P is not yet functional
fix: adds missing <array> include to udp_transport.h
feat: implements p2p hello handshake
```

---

## Código de conduta

Seja respeitoso e objetivo. Críticas técnicas devem ser sobre o **código**, não
sobre a pessoa. Assuma boa intenção, peça esclarecimento antes de interpretar
uma ofensa.

---

## Dúvidas

Abra uma issue. Não há fórum, lista de e-mail ou canal de suporte dedicado
neste momento.
