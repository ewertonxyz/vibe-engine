# Binding das skills do Matt Pocock ao fluxo vx-*

> Fonte única de verdade para **como** as skills auxiliares do Matt Pocock se conectam ao fluxo
> `vx-*` do projeto. Toda referência a essas skills dentro de um `vx-*` deve apontar para cá em vez
> de repetir o mapeamento.
>
> Relacionado: [VIBE-CODING-WORKFLOW.md](VIBE-CODING-WORKFLOW.md) · `CLAUDE.md` · `Docs/Hardening/HARDENING.md`

---

## 1. Política de invocação — **somente via vx-***

As skills do Matt **nunca rodam soltas**. Só são acionadas como sub-passo *de dentro* de um
orquestrador `vx-*`, no ponto de costura definido na tabela abaixo. Isso preserva a regra do
`CLAUDE.md` ("nenhuma ação fora de uma skill `vx-*`").

Enforcement técnico: cada uma das 4 skills integradas tem `disable-model-invocation: true` no
frontmatter — ou seja, não disparam por match de descrição. Continuam invocáveis **explicitamente**
de dentro de um `vx-*` por uma de duas vias (ambas já previstas pelos orquestradores):

1. **Skill tool** pelo nome exato (`tdd`, `diagnose`, `grill-with-docs`, `to-issues`); ou
2. **Agent** carregando e seguindo o `SKILL.md` da auxiliar (`subagent_type: general-purpose`).

Toda invocação termina, como qualquer ação do projeto, no **`vx-hardening-guard`**.

---

## 2. Mapa de integração (onde cada skill entra)

| Skill Matt | Acionada por | Ponto de costura | Papel no projeto |
|---|---|---|---|
| `tdd` | `vx-task-execute` | passo 9 "Implement" | Dirige o loop red-green-refactor usando o **Test plan** do `vx-spec-testing` como lista RED. Cada ciclo: 1 teste Catch2 falho → implementação mínima → refactor. |
| `diagnose` | `vx-task-execute` | passo 10, falha do gate de validação | Loop disciplinado (reproduzir → minimizar → hipótese → instrumentar → corrigir → teste de regressão) acionado quando build/teste/smoke fica vermelho, **antes** de "STOP/ask". |
| `grill-with-docs` | `vx-task-create` e `vx-spec-workflow` | clarificação de escopo / desenho de workflow | Questionário de domínio para afiar termos ambíguos e manter `Docs/Context/GLOSSARY.md` atualizado inline. |
| `to-issues` | `vx-phase-analyze` | "Ordem interna de implementação" / "Tasks previstas" | Disciplina de **slices verticais** para transformar as sugestões de ordenação dos especialistas em stubs de task independentes e demonstráveis. **Não** cria issues em tracker externo. |

---

## 3. Tradução de convenções (Matt → projeto)

As skills do Matt assumem convenções que **não** são as do projeto. Ao invocá-las, aplique sempre
esta tradução. Se uma skill insistir numa convenção da coluna esquerda, prevalece a direita.

| O que a skill do Matt assume | O que o projeto usa |
|---|---|
| Issue tracker (GitHub / Linear / `.scratch/`) | `Docs/Roadmap/Tasks/NN-nome.md` — o "tracker" é o Roadmap. `to-issues` produz **esboço de Tasks**, que viram tasks reais via `vx-task-create`. Nada é postado em serviço externo. |
| ADRs em `docs/adr/` (minúsculo, EN) | `Docs/Decisions/NNNN-titulo.md` — PT-BR, frontmatter `id/titulo/data/status/relacionada`, seções `## Contexto / ## Opções consideradas / ## Decisão / ## Consequências`. |
| Test runner genérico, mocks de colaboradores internos | **Catch2**, executável único **`VibeTests.exe`** (ADR-0005), tags por subsistema (`[core]`, `[render]`, ...) e tier (`[smoke]`, `[unit]`, `[integration]`, `[perf]`). Determinismo: sem wall-clock, RNG semeado, `FileWatcher::Poll()`. Mock só em fronteira externa, nunca em colaborador interno. |
| Glossário em `CONTEXT.md` na raiz | **`Docs/Context/GLOSSARY.md`** (PT-BR). É só glossário de domínio — sem specs nem detalhes de implementação. |
| Saída em inglês | Artefatos gerados em **PT-BR** (tasks, ADRs, docs). `SKILL.md` permanece em EN para triggering. |
| Refatoração oportunista / exploração ampla | **Scope-lock**: nada além do `## Critério de aceitação` da task corrente (HARDENING §1, §3). Achados fora de escopo vão para `Docs/Roadmap/Backlog.md`. |
| Decisão autônoma do agente | `vx-hardening-guard` como **gate final** de toda invocação; ambiguidade real → `AskUserQuestion`. |

---

## 4. Não integradas agora (adiadas / proibidas)

As demais skills do Matt **não** estão conectadas ao fluxo. Motivo registrado para cada uma:

| Skill | Status | Por quê |
|---|---|---|
| `triage` | Adiada | Pressupõe um issue tracker com máquina de estados de labels; o projeto usa o Roadmap. Reavaliar pós-MVP se adotar GitHub Issues. |
| `to-prd` | Adiada | Sobrepõe a coleta de requisitos do `vx-task-create`/`vx-phase-analyze`. Pode entrar no nível de fase pós-MVP. |
| `prototype` | Adiada | Gera código descartável fora do fluxo de task; conflita com o scope-lock. Só via um conceito explícito de "spike task" no futuro. |
| `improve-codebase-architecture` | Adiada | Procura refatorações — e o projeto **proíbe** refatoração oportunista durante o MVP. Só faria sentido como *propositor* que emite tasks via `vx-task-create`, com guardas estritas. |
| `zoom-out` | Adiada | Auxiliar conversacional leve; não justifica integração estrutural. |
| `setup-matt-pocock-skills` | **NÃO RODAR** | Criaria `CONTEXT.md` na raiz, `docs/agents/` e config de tracker **paralelos**, forkando as convenções já estabelecidas (`Docs/Decisions`, Roadmap, `Docs/Context/GLOSSARY.md`). O binding deste documento substitui o que ela faria. |

---

## 5. Suíte caveman (Julius Brussee) — fora deste binding

A suíte caveman (`cavecrew`, `caveman-commit`, `caveman-compress`, `caveman-help`, `caveman-review`,
`caveman-stats`) é **tooling de comunicação** (compressão de tokens), não faz parte do pipeline de
engenharia e não está sujeita a este binding. Uso é avulso, a critério do operador.
