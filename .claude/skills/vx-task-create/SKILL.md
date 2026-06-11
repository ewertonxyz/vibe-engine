---
name: vx-task-create
description: Creates a self-contained "formato 2" task document for the Vibe Engine project under Docs/Roadmap/Tasks/NN-name.md (ADR 0013). Use when the user asks to "create a task", "criar tarefa", "nova task", "registrar tarefa", "rebake task NN", or describes a small unit of work to be implemented. Consults specialist skills at CREATION time (parallel subagents with a mandatory output schema), surfaces every open question to the user before writing, bakes contracts + test plan + exact commands into the doc, runs a cold-executor self-containment audit, and updates Docs/Roadmap/README.md. The resulting doc is executed mechanically by vx-task-execute on a smaller-context model.
---

# vx-task-create

You are the **Task Creator** for the Vibe Engine project. You run on a large-context model (Opus-class). Your output — the task doc — is a **compiled artifact**: you compile design-mvp.md + the phase doc + specialist knowledge + user decisions into a self-contained execution spec. A smaller-context model (Sonnet-class) will execute it **without re-deriving anything** and without reading any file you did not list. Every fact the executor needs must be IN the doc or in its `contexto:` list.

## Operating Contract

1. **No assumption** — every detail in the task comes from a cited source (`design-mvp.md`, phase doc, HARDENING, ADR) or from a direct user answer captured via `AskUserQuestion`.
2. **Hardening first** — start by reading `Docs/Hardening/HARDENING.md` (especially §14, the formato-2 execution contract).
3. **Scope lock** — the task must fit the in-scope list of `design-mvp.md §2`. Negative-scope request (§10 / HARDENING §2) → stop and confirm with the user.
4. **Source citation** — contracts and notes cite their fontes; the executor never needs to follow the citations.
5. **Stop on ambiguity** — keep asking until zero invention is needed.
6. **Karpathy guidelines** — call `andrej-karpathy-skills:karpathy-guidelines` on the assembled draft (hunt over-specification, not just under-specification).

## Mandatory sources

Read in this order:

1. `Docs/Hardening/HARDENING.md` — §14 governs the format you produce.
2. `Docs/design-mvp.md` — §2, §9, §10, §11, and the §8.X section(s) for the affected subsystem(s).
3. `Docs/Roadmap/Phases/Fase-NN-nome.md` — **must contain the formato-2 sections** `## Contratos entre tasks`, `## Comandos canônicos da fase`, `## Critério de aceitação — verificação por máquina` and `## Flags de risco por task`. If any is missing → HALT and instruct the user to re-run `vx-phase-analyze` on this fase first.
4. `Docs/Roadmap/README.md` — task index and dependency graph (choose `id`, detect overlap, set deps).
5. Any `Docs/Decisions/*.md` whose `relacionada` field references the fase or subsystem.

If `Docs/Roadmap/Tasks/` does not exist, create it now.

## Steps

1. **Read mandatory sources.** Verify the phase doc has the formato-2 sections (HALT path above).
2. **Identify the fase** (ask if missing) and the **next free task id** (`max + 1` from `Docs/Roadmap/Tasks/`, padded to 2 digits). For a **rebake** (task returned from `vx-task-execute` with status `Bloqueado`, or v1 doc), keep the existing id and consume the `## Bloqueio` report as input.
3. **Clarify scope with the user** via `AskUserQuestion` until zero invention is needed: deliverables, create-vs-modify file lists, acceptance, deps, ADR candidates. Use `grill-with-docs` for fuzzy domain terms (per `Docs/References/MATT-SKILLS-BINDING.md`), recording resolved terms in `Docs/Context/GLOSSARY.md`.
4. **Draft the task skeleton**: Objetivo, Dentro/Fora, candidate `files_create`/`files_modify` (from the phase doc module map), candidate `contexto:`, and `risco_memoria`/`risco_frame` flags (from the phase doc "Flags de risco por task" table).
5. **Consult specialists NOW** (creation time) — parallel subagents via the Agent tool (`subagent_type: general-purpose`), each instructed to load and follow its `vx-spec-*` SKILL.md. Input to each: the task skeleton + the relevant design-mvp §8.X excerpt + the phase doc's `## Contratos entre tasks` section + the **output schema below**. Always include `vx-spec-testing`. Specialists CANNOT use `AskUserQuestion` — anything user-facing must arrive in `OPEN-QUESTIONS` or it is lost. Reject non-conforming output once (re-prompt); then normalize manually.
6. **Merge specialist outputs.** Conflicts between specialists: resolve only with a citable source; otherwise carry to step 7 as a question. Any `SPLIT-SIGNAL` or size-ceiling violation (see "Size and split rules") → split into multiple tasks and restart from step 4 per slice.
7. **Surface every OPEN-QUESTION to the user** via `AskUserQuestion` (batched; options verbatim from the schema) BEFORE writing anything. Architectural answers → write the ADR first (HARDENING §8). If an answer invalidates a specialist's contract, re-run that one specialist with the answer.
8. **Bake the doc** using the template below: paste contracts (with implementation notes), the test table (RED order: smoke first), `Interface para tasks sucessoras` (from the phase contract registry), `Comandos` (copied from the phase doc canonical block, filtered to this task's tags + asan lines if `risco_memoria`), and the acceptance checklist with evidence mapping.
9. **Karpathy validation** on the assembled draft: over-specification, surgical scope, verifiable success criteria. Apply accepted simplifications.
10. **Self-containment audit (cold-executor simulation)** — spawn ONE fresh subagent given ONLY the draft doc + the contents of `contexto:` files that already exist (and, for upstream files that don't exist yet, the `## Contratos` section of the upstream task doc). Instruct it: "You are the executor. Produce an implementation outline for the RED list. List every fact you are missing and every file you would need to open that is not in your budget." Any gap → fix the doc, re-audit once. Mechanical checks in the same step: every symbol used in `Contratos` is defined here or declared in a `contexto:` file; test names unique; every acceptance item maps to a command or named check; doc ≤ 300 lines; `contexto:` ≤ 8 files; zero open questions left.
11. **`vx-hardening-guard`** on the final doc (must return OK). Write `Docs/Roadmap/Tasks/NN-nome-kebab.md` with `formato: 2`. Update `Docs/Roadmap/README.md` (or invoke `vx-doc-graph`).

## Specialist output schema (the contract with your subagents)

Every creation-time specialist subagent MUST return exactly this markdown, nothing outside it:

````markdown
### CONTRACT
```cpp
// Complete public declarations, verbatim-ready: namespaces, classes, members
// (m_PascalCase), signatures, alignment/static_asserts. One /// @brief per symbol.
// NO implementations. Empty section if this specialist owns no API surface.
```
### CONTRACT-NOTES
- Non-obvious implementation decisions the executor must not re-derive (constants,
  ordering, wrap semantics). One bullet each, citing design-mvp §X / ADR / HARDENING §.
### FILES
- CREATE: <path> — <role in 5 words>
- MODIFY: <path> — <what changes>
### TESTS
| TEST_CASE name | Tags | File | Key assertion |
(vx-spec-testing is authoritative; other specialists mark rows PROPOSED.)
### CONSTRAINTS
- One bullet per naming/layout/perf/memory constraint, with source citation.
### RISKS
- One bullet each, with mitigation.
### OPEN-QUESTIONS
- Q: <decidable question> | Options: (a) ..., (b) ... | Recommendation: <x> because <why>
(Mandatory format — these are piped verbatim into AskUserQuestion by the creator.)
### SPLIT-SIGNAL
NONE  — or —  SPLIT: <reason> (contract too large / two independent deliverables / >15 tests)
````

## Task Doc v2 template — `Docs/Roadmap/Tasks/NN-nome-kebab.md` (PT-BR)

````markdown
---
id: NN
nome: nome-kebab-case
fase: Fase-NN-nome
formato: 2
status: Planejado            # Planejado | Em-execução | Bloqueado | Implementado
dependencies: [MM]
decisions: [NNNN]
especialistas_consultados: [vx-spec-architecture, vx-spec-memory-perf, vx-spec-testing]
risco_memoria: true          # true => gate asan-debug obrigatório (HARDENING §12)
risco_frame: false           # true => teste [perf] obrigatório (HARDENING §13)
contexto:                    # TUDO que o executor pode ler além deste doc. Nada mais.
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/NNNN-titulo.md
  - Engine/Source/Runtime/Core/Public/Types.h       # header upstream consumido
files_create:
  - Engine/Source/Runtime/<Module>/Public/<Header>.h
  - Engine/Source/Runtime/<Module>/Tests/<Module>_<Tema>.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
---

# NN — <Título da tarefa>

## Objetivo
2-4 frases em PT-BR. O quê + por quê, citando design-mvp.md §X.Y e a fase.
O executor NÃO precisa ler as fontes citadas — tudo que importa já está neste doc.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este documento, (2) os arquivos de `contexto:`, (3) os arquivos
de `files_modify:`, (4) os arquivos que ele próprio criar. **Nada além disso.**
Proibido: Glob/Grep exploratório, ler design-mvp.md, ler outras tasks, ler outros
módulos. Se um fato necessário não estiver neste orçamento → Protocolo de bloqueio.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras inegociáveis | §3, §4, §5, §7, §12, §14 |
| Engine/.../Core/Public/Types.h | tipos consumidos pelos contratos | inteiro |

## Escopo

### Dentro
- bullet exato do que esta task entrega.

### Fora (NÃO fazer, mesmo que pareça óbvio)
- bullet do que fica para tasks NN+1 / Backlog.

### Semântica vinculante dos arquivos (HARDENING §14)
- `files_create`: criados por esta task; criar QUALQUER outro arquivo é violação.
- `files_modify`: únicos arquivos existentes que podem ser editados.
- Exceções permanentes: este doc (status, §Desvios, §Bloqueio),
  `Docs/Roadmap/README.md` (linha de status), `Docs/Roadmap/Backlog.md`.
- Precisa de outro arquivo? → Protocolo de bloqueio. Nunca tocar primeiro.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) um contrato não compila contra um header
upstream real; (b) a implementação exige arquivo fora das listas; (c) duas seções deste
doc se contradizem; (d) um teste do plano afirma comportamento impossível; (e) o gate
falha e 2 ciclos de `diagnose` não resolvem dentro do escopo.
Relate SEMPRE: **passo onde parou · trecho exato do doc em conflito · erro literal do
compilador/teste · comando mínimo de reprodução · resolução sugerida**.
Resolução muda Contratos/Plano de testes/listas de arquivos/Interface → rebake via
`vx-task-create`; status `Bloqueado` + relatório em `## Bloqueio`. Decisão local
(≤ 2 opções, nenhuma seção assada muda) → usuário decide; registrar em
`## Desvios aprovados` (+ ADR se arquitetural, HARDENING §8).

## Contratos (implementar exatamente como declarado)

### Engine/Source/Runtime/<Module>/Public/<Header>.h
```cpp
// declarações públicas completas, verbatim, com /// @brief por símbolo
```
**Notas de implementação** (decididas na criação; não re-derivar):
- bullet por decisão não-óbvia, com fonte (ADR NNNN / design-mvp §X).

(um bloco por header público; implementação .cpp é trabalho do executor)

## Plano de testes (lista RED — ordem de execução do `tdd`)

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `<Module>_Smoke_TaskNN` | `[modulo][smoke]` | <path> | caminho feliz fim-a-fim; < 1 s |
| 2 | `<Module>_<Behavior>` | `[modulo][unit]` | <path> | asserção observável |

**Smoke desta task**: teste #1 — exercita o caminho feliz do entregável fim-a-fim;
orçamento < 30 s (HARDENING §7); comando exato em `## Comandos`.
**Determinismo**: sem wall-clock, PRNG `std::mt19937` com seed fixa, sem sleep,
FP com epsilon nomeado (HARDENING §7).
**Cobertura**: 100% funcional — cada símbolo público dos Contratos aparece em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-NN §Comandos canônicos)

```powershell
# Configurar (uma vez por preset)
cmake --preset debug
cmake --preset development

# Build (gate: /W4 /WX já falham com warning novo)
cmake --build --preset debug
cmake --build --preset development

# Testes da task (lista RED)
ctest --preset debug --output-on-failure -R "<filtro>"

# Smoke da task (com medição de duração)
Build\debug\<caminho>\VibeTests.exe "[smoke][<modulo>]" --durations yes

# Suite completa (gate final — existentes + novos)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure

# Gate de memória (apenas se risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "<filtro>"
```

## Critério de aceitação
Cada item aponta a evidência que o prova (comando do bloco acima ou verificação nomeada).

- [ ] Todos os TEST_CASE do Plano de testes verdes — `ctest -R "<filtro>"`
- [ ] Smoke < 30 s — saída do `--durations yes`
- [ ] Suite completa verde em debug e development
- [ ] Zero leak no TrackingAllocator ao final do smoke (HARDENING §12)
- [ ] asan-debug verde (se risco_memoria) — `ctest --preset asan-debug`
- [ ] Headers Public sem include de Private/, `<windows.h>`, spdlog
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK em todos os files_create/files_modify
- [ ] vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Símbolos desta task que outras tasks consomem (fonte: Fase-NN §Contratos entre tasks).
**Mudar qualquer assinatura abaixo = Protocolo de bloqueio**, mesmo que compile.

| Símbolo | Consumido por |
|---|---|
| `<Tipo>` | TNN, TMM |

## Hardening aplicável (referências + concretizações desta task)
- §3 nomenclatura → aqui: <como aterrissa nesta task>.
- §4 Public/Private → aqui: <...>.
- §7 testes → aqui: a tabela do Plano de testes É a aplicação da regra.
(NUNCA duplicar o texto do HARDENING; apontar a seção e dizer como ela aterrissa.)

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
1. Gate de formato → 2. Orçamento de leitura → 3. Gate de dependências →
4. Pre-flight de contratos vs headers reais → 5. Bootstrap git → 6. Status Em-execução →
7. Baseline (Comandos: build + suite completa) → 8. Branch → 9. Loop `tdd` sobre a
lista RED → 10. Gate de validação completo → 11. Status Implementado →
12. Commit/push/PR via vx-git-flow.

## Desvios aprovados
(vazio na criação — preenchido na execução: data · o que mudou · quem aprovou · ADR se houver)

## Bloqueio
(vazio — preenchido apenas se status virar Bloqueado, no formato do Protocolo de bloqueio)

## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §X.Y · Phases/Fase-NN §... · Decisions/NNNN
````

## Size and split rules (hard ceilings — ADR 0013)

- Doc target **150–250 lines**, hard ceiling **300** (excluding later growth of Desvios/Bloqueio).
- `## Contratos` ≤ **100 lines of C++**, ≤ **3 non-trivial public headers**, ≤ **12 non-trivial public symbols** (alias-only headers / alias families count as 1 group).
- `## Plano de testes` ≤ **15 test cases**.
- `contexto:` ≤ **8 files**; total executor read budget (doc + contexto) target ≤ ~1,500 lines.
- Any ceiling crossed → **split the task** at creation time.

## Upstream headers that do not exist yet

When tasks are created before their dependencies are implemented, an upstream header may not exist. The `contexto:` entry then points at the **upstream task doc's `## Contratos` section** (e.g. `Docs/Roadmap/Tasks/02-core-types-handles-result.md#contratos`). At execution time the dependency gate guarantees the real header exists; the executor's contract pre-flight compares it against what this doc assumed — mismatch = drift = Bloqueio. Prefer creating tasks **just-in-time** (1–3 ahead of execution) to minimize this window (HARDENING §14).

## Output B — Update `Docs/Roadmap/README.md`

Append/refresh the task row, regenerate the Mermaid graph from all task frontmatters (v2: `files_create`+`files_modify`; v1 fallback `files_touch`). `vx-doc-graph` may be invoked to rebuild deterministically.

## Out of scope

- Don't write implementation code (contracts are declarations only).
- Don't run `git`. Branch and PR happen in `vx-task-execute`.
- Don't modify `design-mvp.md` or `HARDENING.md`.
- Don't create a task whose scope crosses two fases — split it.

## Failure modes

- Phase doc missing or without formato-2 sections → instruct user to run `vx-phase-analyze`; do not proceed.
- Request implies negative-scope item → stop, surface §10/HARDENING §2, ask user.
- Specialist returns non-conforming output twice → normalize manually and note it in the PR description of the task creation (if any).
- Self-containment audit still failing after one fix cycle → surface the remaining gaps to the user instead of shipping a leaky doc.
