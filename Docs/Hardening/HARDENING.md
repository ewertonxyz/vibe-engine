# HARDENING — Vibe Engine

Documento mestre de regras inegociáveis. Toda skill `vx-*` DEVE ler este arquivo antes de tomar qualquer decisão ou gerar qualquer artefato. Qualquer ação que conflite com uma regra abaixo é interrompida e a skill consulta o usuário via `AskUserQuestion`.

Este documento existe para impedir que modelos de IA — por perda de contexto, suposição ou criatividade indesejada — desviem o projeto do contrato definido em [Docs/design-mvp.md](../design-mvp.md).

---

## 1. Escopo do MVP

Fonte de verdade: [design-mvp.md §2 "Escopo"](../design-mvp.md).

**Apenas o que está listado como "in scope" em §2 pode ser implementado.** Não há exceções. Qualquer ideia, sugestão, refactor ou feature fora dessa lista deve ser registrado em `Docs/Roadmap/Backlog.md` para avaliação pós-MVP.

Critérios de "MVP pronto" estão em [design-mvp.md §3](../design-mvp.md). Cobertura de "pronto para MVP" por sistema está em [design-mvp.md §11](../design-mvp.md).

---

## 2. Lista negativa (NUNCA propor durante o MVP)

Fonte: [design-mvp.md §10](../design-mvp.md).

- Backends Vulkan, PS5, Xbox
- Streaming de mundo, WorldPartition, HLOD, impostors, DirectStorage
- Cooker, `.vpkg`, virtual file system
- Mesh shaders, GPU-driven completo, work graphs
- Ray tracing, DDGI, path tracing
- DLSS, XeSS
- Editor C# / Avalonia
- Reflection generator (libclang)
- Motion matching, full-body IK, pose warping
- Behavior trees, utility AI, squad system
- Quest, dialogue, inventory completo, cinematic, lipsync
- Hot reload de C++
- Networking, multiplayer, cloud saves
- Localization, TRC/XR
- Crash reporting integrado (Crashpad/Sentry)
- Wwise/FMOD, NoesisGUI
- Save versioning robusto, múltiplos slots
- DLC/patch, plugin system

Se o usuário pedir explicitamente algo dessa lista, a skill DEVE confirmar antes de executar e registrar a decisão em `Docs/Decisions/`.

### Fronteira de UI (ADR 0010)

`RmlUi` saiu da lista negativa, mas com fronteira rígida:

- RmlUi é permitido **apenas** no módulo `EngineUI` (HUD e menus do jogo). Nenhum header RmlUi vaza em `Public/`; gameplay escreve em data models.
- **Editor fora de Dear ImGui = violação.** O editor é ImGui + ImGuizmo (design-mvp.md §5.1).
- **Dear ImGui na UI de produto do jogo = violação.** ImGui no runtime do jogo é exclusivo de overlays de debug (console, profiler).
- NoesisGUI permanece banido.

---

## 3. Nomenclatura C++

### Tipos, classes, structs, enums, funções, arquivos
- **PascalCase** sem underscore. Exemplos: `RenderGraph`, `CharacterMotor`, `AssetHandle`, `ParseGltf()`, `RenderGraph.h`.

### Variáveis membro de classes/structs
- **`m_PascalCase`**. Exemplos: `m_Device`, `m_FrameIndex`, `m_CurrentClip`.

### Variáveis locais e parâmetros
- **PascalCase**. Exemplos: `FrameIndex`, `Result`, `OutTexture`.

### Constantes / `constexpr`
- **PascalCase**. Exemplos: `MaxFrameLatency`, `DefaultPoolSize`.

### Macros
- **SCREAMING_SNAKE_CASE** prefixadas com `V`. Exemplos: `VLOG_INFO`, `VASSERT`, `VCHECK`.

### Tipos próprios da engine (referência: [design-mvp.md §8.1](../design-mvp.md))
- Prefixo **`V`** para escalares e tipos fundamentais: `Vint8`, `Vint16`, `Vint32`, `Vint64`, `Vuint8`–`Vuint64`, `Vfloat`, `Vdouble`, `Vbyte`, `Vspan`, `Vstring`, `VHandle<T>`, `VStringId`, `VResult<T,E>`.

### Namespaces
- **PascalCase**. Namespace raiz: `VibeEngine`. Sub-namespaces por subsistema: `VibeEngine::RHI`, `VibeEngine::Render`, `VibeEngine::Animation`, etc.

### Rename obrigatório
- **`WorldPartition` é proibido** — usar `WorldStream` ou `StreamingPartition`. Razão: trademark da Unreal Engine. Fonte: [design-mvp.md §5.11](../design-mvp.md).

### Arquivos
- Header: `.h`. Implementação: `.cpp`. Inline: `.inl`. Shader: `.hlsl`. Common shader: `.hlsli`.
- Um tipo público principal por arquivo. Nome do arquivo == nome do tipo.

### Skills (este repositório)
- Diretório: `.claude/skills/vx-<kebab-name>/SKILL.md`. Prefixo obrigatório `vx-`.

---

## 4. Layout Public/Private

Fonte: [design-mvp.md §6](../design-mvp.md).

Cada módulo tem separação rígida:

```
Engine/Source/Runtime/<Module>/
    Public/   # headers expostos para outros módulos
    Private/  # implementação + headers internos
```

**Regras:**
- Header em `Public/` NÃO pode incluir header de `Private/` de qualquer módulo.
- Header em `Public/` NÃO pode incluir header de `Private/` do próprio módulo.
- Apenas `Public/` é incluído via `#include <Module/Foo.h>` por outros módulos.
- **Layout de include (ADR 0014)**: headers públicos vivem em `Public/<Module>/Foo.h`; o include root de cada alvo é o seu `Public/`; a forma canônica — inclusive dentro do próprio módulo e nos testes — é `#include <Module/Foo.h>` (ex.: `#include <Core/Types.h>`). Header público fora de `Public/<Module>/` é violação.
- Implementações `.cpp` ficam sempre em `Private/`.
- Dependências entre módulos seguem o grafo definido nas tasks; ciclos são proibidos.

---

## 5. Doxygen — tags obrigatórias

Toda declaração pública (em `Public/`) tem comentário Doxygen com:

### Classes / structs / enums
```cpp
/**
 * @brief Resumo de uma linha.
 * @details Descrição extendida se necessária.
 */
```

### Funções públicas
```cpp
/**
 * @brief Resumo.
 * @param ParamName Significado.
 * @return Significado do retorno (omitir se void).
 * @throws Tipo de exceção (apenas se aplicável; preferir VResult<T,E>).
 */
```

### Variáveis membro públicas / globais
```cpp
/// @brief Resumo de uma linha.
Vint32 m_FrameIndex { 0 };
```

### Macros
```cpp
/**
 * @brief Resumo.
 * @param X Significado.
 */
#define VLOG_INFO(X) ...
```

Headers `Private/` podem ter Doxygen mais sucinto, mas declarações públicas internas (cross-arquivo) seguem o mesmo padrão.

Doxygen rodará periodicamente para gerar referência de API; quebrar tag = quebra build na CI quando houver.

---

## 6. Git, branch, commit, PR

### Branch
- Uma branch por tarefa: `task/NN-nome-kebab` (mesmo nome do arquivo `Docs/Roadmap/Tasks/NN-nome.md`).
- Criada a partir de `main` atualizado.
- Nunca commit direto em `main`.

### Commit
- Formato: `[task NN] resumo curto no imperativo`.
- Corpo opcional explicando o porquê (não o "o quê", que está no diff).
- Sem `--no-verify`. Hook falhou → investigar e corrigir, nunca pular.

### Push & PR
- Push apenas após todos os testes/smoke/validações verdes localmente.
- PR descreve **o que mudou, onde mudou, por que mudou, o que foi removido e por quê, links para Task/Fase/Decisões consultadas, checklist de validações executadas**. PR genérico é rejeitado.
- PR title: `[task NN] <objetivo da task>`.

---

## 7. Política de testes

Fonte: critérios verificáveis em [design-mvp.md §9](../design-mvp.md) e §11.

- **100% das funcionalidades implementadas devem ter teste.** Cobertura de linhas não é métrica suficiente — cada caminho funcional precisa ter pelo menos um teste explícito.
- **Smoke test obrigatório** em cada task: roda em <30s e exercita o caminho feliz da funcionalidade.
- **Determinístico**: testes não dependem de tempo wall-clock, RNG sem seed, threading sem barreira, ou rede.
- **Fixtures isolados** por teste; nenhum estado global compartilhado.
- **Catch2** é o framework. Tags por módulo (`[core]`, `[rhi]`, `[render]`, etc.) para execução seletiva.
- Test runner é parte da task: tarefa só é `Implementado` quando todos os testes (existentes + novos) passam.

---

## 8. Regra de decisão (ADR-style)

Toda decisão que não está diretamente derivável de:
- [design-mvp.md](../design-mvp.md),
- este HARDENING.md,
- arquivo da Fase em `Docs/Roadmap/Phases/`,
- arquivo da Task em `Docs/Roadmap/Tasks/`,

…é uma decisão **arquitetural** e deve gerar um documento em `Docs/Decisions/NNNN-titulo-kebab.md` com:

```markdown
---
id: NNNN
titulo: ...
data: YYYY-MM-DD
status: Aceita  # Proposta | Aceita | Substituída | Rejeitada
relacionada: [Tasks/NN, Phases/Fase-NN, Decisions/NNNN]
---

## Contexto
Por que essa decisão precisa ser tomada.

## Opções consideradas
- A: ...
- B: ...

## Decisão
Opção escolhida e justificativa.

## Consequências
O que muda no projeto.
```

**Toda skill que pergunta ao usuário via `AskUserQuestion` e recebe resposta que define uma decisão arquitetural DEVE gerar a ADR antes de prosseguir.**

Citation obrigatória: toda task/fase/skill que aplica uma decisão referencia o `id` da ADR.

---

## 9. Status de Task

Cada task em `Docs/Roadmap/Tasks/NN-nome.md` tem status no frontmatter:

| Status | Significado | Permite execução de dependentes? |
|---|---|---|
| `Planejado` | Doc criado, ainda não iniciado | Não |
| `Em-execução` | Branch criado, implementação em andamento | Não |
| `Bloqueado` | Aguardando decisão do usuário ou dependência | Não |
| `Implementado` | PR mergeado, todos os testes verdes | Sim |

`vx-dependency-check` consulta este campo. Se qualquer dependência != `Implementado`, a execução é interrompida.

---

## 10. Critério de "tarefa pronta"

Uma task só vira `Implementado` quando TODOS os itens abaixo são verdadeiros:

```
[ ] Código compila em Debug e Development sem warnings novos
[ ] Doxygen tags presentes em todas as declarações públicas afetadas
[ ] Naming validado por vx-naming-style (PascalCase, m_, V prefix)
[ ] Layout Public/Private respeitado
[ ] Testes Catch2 cobrindo 100% das funcionalidades novas
[ ] Smoke test passando
[ ] Suite completa de testes verde (não só os novos)
[ ] vx-hardening-guard retornou OK
[ ] Nenhuma feature fora da lista in-scope do MVP foi tocada
[ ] Branch task/NN-nome existe e está atualizada com main
[ ] Commit segue formato [task NN]
[ ] PR aberto com descrição completa (o quê, onde, por quê, removidos, links)
[ ] Decisões arquiteturais registradas em Docs/Decisions/
[ ] Docs/Roadmap/README.md atualizado (status + grafo se aplicável)
[ ] TrackingAllocator reporta zero leaks ao final do smoke (Debug/Development) — §12
[ ] Preset asan-debug verde, se a task tem risco_memoria: true — §12
[ ] Teste [perf] presente e verde, se a task tem risco_frame: true — §13
[ ] Orçamento de contexto respeitado: executor leu apenas task + contexto: + files_modify: — §14
```

Referência adicional: [design-mvp.md §11](../design-mvp.md).

---

## 11. Princípios operacionais para skills `vx-*`

Estes princípios são repetidos em cada SKILL.md por redundância intencional:

1. **No assumption** — se faltar fonte explícita, perguntar via `AskUserQuestion`.
2. **Hardening first** — ler este arquivo antes de agir.
3. **Scope lock** — executar APENAS o que a task/fase/decision pede.
4. **Source citation** — toda decisão registrada cita a fonte (arquivo + seção).
5. **Stop on ambiguity** — múltiplas interpretações = parar e perguntar.
6. **Karpathy guidelines** — orchestrators chamam `andrej-karpathy-skills:karpathy-guidelines` antes de fechar planejamento.

Violação desses princípios = parar a execução e relatar ao usuário.

---

## 12. Segurança de memória

Fonte: design-mvp.md §3.2, ADR 0003, ADR 0011. O gerenciamento de memória desta engine é exemplar por contrato, não por aspiração.

- **RAII obrigatório.** Todo recurso (memória, handle de SO, recurso GPU) tem dono com lifetime determinístico.
- **`new`/`delete`/`malloc`/`free` crus são proibidos** fora do módulo `Memory/` e de wrappers RAII explicitamente aprovados. Alocação dinâmica passa pelos allocators da engine (`LinearAllocator`, `FrameAllocator`, `PoolAllocator`...) com `MemoryTag`, ou por `std::unique_ptr`/containers padrão em código frio.
- **Ownership único e explícito**: `std::unique_ptr` ou `VHandle<T>` (generation counter detecta use-after-free). Raw pointer e referência significam observação não-dona, nunca posse. `std::shared_ptr` exige justificativa em ADR.
- **Sem C-style casts.** Usar `static_cast`/`reinterpret_cast`/`const_cast` — visíveis e grepáveis.
- **Interfaces recebem `Vspan`**, nunca par ponteiro+tamanho.
- **Gates por task** (checklist §10):
  - `TrackingAllocator` reporta **zero leaks** ao final de todo smoke em Debug e Development.
  - Task com `risco_memoria: true` no frontmatter (§14) só fica `Implementado` com o preset `asan-debug` verde (`/fsanitize=address`, ADR 0008).
- Violações detectadas em código existente fora do escopo da task → registrar em `Docs/Roadmap/Backlog.md`, nunca "consertar de passagem" (§11 scope lock).

---

## 13. Orçamento de performance

Fonte: design-mvp.md §3 #2, §3.1, §3.2, ADR 0011.

- **Gate**: 1080p @ 60 fps estáveis em RTX 3060 / RX 6600 com a cena do MVP. É o único critério que fecha o MVP.
- **Teto de design**: 4K @ 60 fps via FSR Quality/Balanced em GPU classe RTX 4070+. Nunca atrasa aceitação; **veta arquitetura** que o impossibilite (custo proporcional à resolução de saída além do necessário, buffers per-frame subdimensionados, motion vectors incorretos). `vx-spec-rendering` e `vx-spec-memory-perf` aplicam o veto.
- **Orçamento de frame de partida** (16.6 ms; refinar por medição, nunca citar como fixo sem captura Tracy/PIX):

| Fatia | Orçamento |
|---|---|
| CPU game (sim, gameplay, physics 1/60 fixo) | ≤ 4 ms |
| CPU render-prep (visibility, RenderGraph, submit) | ≤ 2 ms |
| GPU (GBuffer + shadows + lighting + post + UI) | ≤ 10 ms |
| Slack / OS / present | ~0.5 ms |

- **Tracy zones obrigatórias** em todo sistema per-frame desde a criação (ADR 0004). `VPROFILE_ZONE` ausente em hot path = violação.
- Task com `risco_frame: true` (§14) exige teste `[perf]` com teto de tempo explícito, coordenado entre `vx-spec-testing` e `vx-spec-memory-perf`.
- Proibido em hot path (lista de bloqueio imediato): alocação por frame, `std::function`/lambda heap-capturing, mutex em loop per-entity, atomic não-padded em contador quente, virtual call em inner loop sem justificativa medida, recriação de recurso GPU por frame.

---

## 14. Contrato de execução de tasks (formato 2)

Fonte: ADR 0013. O fluxo opera com **modelos diferentes por etapa**: criação de fases/tasks com modelo grande (classe Opus); execução com modelo de contexto menor (classe Sonnet). O doc de task é uma **spec autossuficiente**; a execução é quase-mecânica.

### Gate de formato
Task sem `formato: 2` no frontmatter é v1 e **não pode ser executada** — exige rebake via `vx-task-create`.

### Orçamento de leitura do executor
O executor lê **somente**: (1) o doc da task, (2) os arquivos listados em `contexto:`, (3) os arquivos de `files_modify:`, (4) os arquivos que ele próprio criar. Proibido: Glob/Grep exploratório, ler design-mvp.md, ler outras tasks, ler outros módulos. Fato necessário fora do orçamento = **protocolo de bloqueio**, não busca. (Exceção: este HARDENING.md e docs de infraestrutura citados pelo próprio SKILL.)

### Semântica vinculante de arquivos
- `files_create:` — arquivos que a task cria; criar qualquer outro arquivo é violação.
- `files_modify:` — únicos arquivos existentes editáveis.
- Exceções permanentes (não precisam constar): o próprio doc da task (status, Desvios, Bloqueio), `Docs/Roadmap/README.md` (linha de status), `Docs/Roadmap/Backlog.md` (achados fora de escopo).

### Protocolo de bloqueio
O executor PARA e pergunta quando: (a) um contrato não compila contra um header upstream real; (b) a implementação exige arquivo fora das listas; (c) duas seções do doc se contradizem; (d) um teste do plano afirma comportamento impossível; (e) o gate falha e 2 ciclos de `diagnose` não resolvem dentro do escopo.

Formato fixo do relatório: **passo onde parou · trecho exato do doc em conflito · erro literal de compilador/teste · comando mínimo de reprodução · resolução sugerida.**

- Resolução muda seção assada (Contratos, Plano de testes, listas de arquivos, Interface) → task volta para `vx-task-create` (rebake); status `Bloqueado` com relatório em `## Bloqueio`.
- Decisão local (≤ 2 opções, nenhuma seção assada muda) → usuário decide; registrar em `## Desvios aprovados` (+ ADR se arquitetural, §8).

### Churn de contratos
Desvio que muda símbolo listado em "Interface para tasks sucessoras": emendar primeiro a seção "Contratos entre tasks" da fase → listar dependentes via `vx-doc-graph` → rebake das tasks `Planejado` afetadas. Tasks `Implementado` afetadas ganham task corretiva — nunca edição retroativa.

### Norma operacional
Criar tasks **just-in-time** (1–3 à frente da execução), não a fase inteira de uma vez — minimiza rebake por drift.
