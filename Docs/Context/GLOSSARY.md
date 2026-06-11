# Glossário de domínio — Vibe Engine

> Glossário **puramente de domínio**: define os termos que o projeto usa, para que skills e pessoas
> falem a mesma língua. **Não** contém specs, decisões nem detalhes de implementação — isso vive em
> `Docs/design-mvp.md`, `Docs/Decisions/` e nas tasks.
>
> Mantido inline por `grill-with-docs` (ao afiar requisitos), e consultado por `tdd`/`diagnose` para
> nomear conceitos corretamente. Ver [MATT-SKILLS-BINDING.md](../References/MATT-SKILLS-BINDING.md).

---

## Convenções

- Um termo = uma definição. Evite sinônimos ambíguos; se dois termos parecem iguais mas diferem,
  defina ambos e explique a diferença.
- Termos de código usam o nome exato do símbolo (ex.: `VResult`, `VHandle<T>`).
- Ordem alfabética dentro de cada seção.

---

## Processo / workflow

- **Fase** — uma das 12 etapas sequenciais do MVP (`design-mvp.md §9`). Cada fase termina num build
  demonstrável. Documentada em `Docs/Roadmap/Phases/Fase-NN-nome.md`.
- **Task** — menor unidade de trabalho executável, em `Docs/Roadmap/Tasks/NN-nome.md`. Tem `## Critério
  de aceitação` observável. É o "issue" do projeto (não há tracker externo).
- **Formato 2** — formato autossuficiente de doc de task (ADR-0013, HARDENING §14): especialistas
  consultados na criação, contratos/testes/comandos assados no doc; `formato: 2` no frontmatter é
  gate de execução. Tasks sem ele são "v1" e exigem rebake.
- **Bake / assar** — compilar os outputs dos especialistas (contratos, plano de testes, comandos)
  para dentro do doc da task no momento da criação.
- **Rebake** — recriar um doc de task via `vx-task-create` (após bloqueio, drift de contrato ou
  migração v1 → formato 2), mantendo o mesmo id.
- **Orçamento de leitura (contexto)** — lista exaustiva `contexto:` + `files_modify:` que o executor
  pode ler. Fato fora do orçamento = bloqueio, não busca (HARDENING §14).
- **Protocolo de bloqueio** — formato fixo de parada do executor: passo · trecho do doc · erro
  literal · repro mínimo · resolução sugerida. Resultado: desvio aprovado OU status `Bloqueado` + rebake.
- **Desvio aprovado** — decisão local tomada durante a execução (≤ 2 opções, nenhuma seção assada
  muda), registrada na seção `## Desvios aprovados` da task.
- **risco_memoria / risco_frame** — flags de frontmatter (definidas pelo `vx-spec-memory-perf` na
  criação) que disparam o gate `asan-debug` (HARDENING §12) e o teste `[perf]` (HARDENING §13).
- **ADR** (Architecture Decision Record) — registro de uma decisão arquitetural em
  `Docs/Decisions/NNNN-titulo.md`, em PT-BR, com status `Aceita`/`Proposta`/`Supersedida`/`Deprecada`.
- **Slice vertical** — recorte fino ponta a ponta de uma funcionalidade (atravessa todas as camadas),
  independentemente demonstrável. Unidade preferida de decomposição de fase em tasks.
- **Scope-lock** — disciplina de implementar exatamente o `## Critério de aceitação` da task; o resto
  vai para `Docs/Roadmap/Backlog.md`.

## Testes

- **Smoke test** — teste `[smoke]` < 30 s que exercita o caminho feliz ponta a ponta do entregável da
  task. Sempre presente.
- **Lista RED** — conjunto de testes falhos que o `tdd` usa para dirigir o ciclo red-green-refactor;
  produzida pelo `vx-spec-testing` como "Test plan".
- **Tag de subsistema / tier** — rótulos Catch2 que classificam um teste por módulo (`[core]`,
  `[render]`, ...) e por nível (`[smoke]`, `[unit]`, `[integration]`, `[perf]`).

## Tipos fundamentais (Core)

- **`VResult<T,E>`** — tipo de retorno de erro explícito (sucesso ou erro), sem exceções no caminho
  comum. Ver ADR-0001.
- **`VHandle<T>`** — handle opaco e estável para um recurso, em vez de ponteiro cru.
- **`VStringId`** — identificador de string internado, com hash determinístico (ADR-0009).
- **`MemoryTag`** — categoria de alocação para rastrear memória por subsistema (ADR-0003).

## UI / Renderer

- **EngineUI** — módulo (`Engine/Source/Runtime/UI/`) que isola o backend de UI do jogo. Backend do
  MVP: RmlUi (ADR-0010). Gameplay escreve em data models; nunca toca elementos.
- **RmlUi** — biblioteca de UI (documentos `.rml` + folhas `.rcss`, data bindings) usada para a HUD
  e menus do jogo. Proibida no editor; o editor é Dear ImGui (HARDENING §2, Fronteira de UI).
- **Teto de design 4K@60** — meta arquitetural (ADR-0011): sustentar 4K@60 via FSR em GPU classe
  RTX 4070+ sem mudanças estruturais. Não é gate do MVP; veta arquitetura que o impossibilite.

---

*Termos novos são adicionados aqui assim que surgem numa sessão de `grill-with-docs` ou quando uma
task introduz vocabulário de domínio inédito.*
