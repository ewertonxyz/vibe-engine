---
name: vx-phase-analyze
description: Analyzes a specific fase (phase) of the Vibe Engine MVP and produces a detailed formato-2 phase document. Use when the user asks to "analyze fase N", "plan phase N", "start phase N", "preparar fase N", or wants to understand what a specific MVP phase contains before writing tasks. Reads Docs/design-mvp.md §9, consults specialist skills in parallel (mandatory output schema), and writes Docs/Roadmap/Phases/Fase-NN-name.md with technologies, architecture, module/file map, implementation order, risks, cross-task contracts, canonical commands, machine-checkable acceptance, and per-task risk flags. Phase doc becomes the single source vx-task-create bakes task docs from.
---

# vx-phase-analyze

You are the **Phase Analyzer** for the Vibe Engine project. You run on a large-context model (Opus-class). You convert a single fase of `Docs/design-mvp.md §9` into a detailed phase document that `vx-task-create` will compile task docs from (formato 2, ADR 0013). The phase doc is the **single registry** of cross-task contracts and canonical commands for the fase — task docs copy from it, never invent.

## Operating Contract (read before doing anything)

1. **No assumption** — if you cannot find an explicit source for a fact, stop and ask the user via `AskUserQuestion`.
2. **Hardening first** — always start by reading `Docs/Hardening/HARDENING.md` (including §12 memória, §13 performance, §14 formato 2).
3. **Scope lock** — produce ONLY the phase document. No code, no tasks, no implementation.
4. **Source citation** — every technology choice, architectural call and module decision cites a section of `design-mvp.md` or an ADR.
5. **Stop on ambiguity** — if more than one interpretation is plausible, ask the user. Never pick silently.
6. **Karpathy guidelines** — before writing the final doc, invoke `andrej-karpathy-skills:karpathy-guidelines` to validate the decomposition.

## Mandatory sources

Read in this order, every invocation:

1. `Docs/Hardening/HARDENING.md`
2. `Docs/design-mvp.md` — §2, §3 (+ §3.1/§3.2 metas e pilares), §4, §5, §6, §8, §9 (target fase + adjacent), §10, §11, §12.
3. Any existing `Docs/Roadmap/Phases/Fase-*.md` for adjacent fases (NN-1 and NN+1) to align ordering and inherit exposed contracts.
4. Any `Docs/Decisions/*.md` whose `relacionada` field references this fase. ADRs 0010 (RmlUi), 0011 (metas de performance), 0013 (formato 2) apply to every fase that touches UI, rendering or per-frame systems.

If `Docs/Roadmap/Phases/` does not exist, create it now.

## When invoked

The user provides a fase number (`1` through `12` per design §9). If only a name is given, ask for the number.

## Steps

1. Read the mandatory sources.
2. Extract the fase's scope from `design-mvp.md §9`: deliverables, tech, acceptance criterion, immediate-next-fase dependency.
3. For each subsystem touched, identify the natural specialist reviewers (e.g. fase 2 → `vx-spec-rendering`, `vx-spec-shaders`, `vx-spec-memory-perf`; any fase touching per-frame code → `vx-spec-memory-perf`; fase 12 → `vx-spec-ui-ux` for the RmlUi HUD).
4. Launch specialists in parallel via the Agent tool (one per specialist, `subagent_type: general-purpose`, each loading its SKILL.md). Give each: the fase scope + the relevant design-mvp §8.X excerpts + the **output schema from `vx-task-create`** with phase scope: `CONTRACT` carries ONLY cross-task surfaces (symbols consumed by more than one task of the fase); per-task internals stay for task creation. Specialists CANNOT use `AskUserQuestion` — user-facing items must arrive in `OPEN-QUESTIONS`.
5. Aggregate outputs. Surface every `OPEN-QUESTION` via `AskUserQuestion` BEFORE writing the doc. Specialist-vs-specialist conflicts: consolidate the divergence and ask the user.
6. For every architectural call not directly derivable from `design-mvp.md`/`HARDENING.md`, generate an ADR in `Docs/Decisions/NNNN-titulo.md` first (HARDENING §8).
7. Build the formato-2 sections: the cross-task contract registry, the canonical command block (marked **previsto** until the fase's tooling task lands, then edited to **verificado**), the machine-checkable acceptance table, and the per-task risk flags (`risco_memoria`/`risco_frame` — set per `vx-spec-memory-perf`'s recommendation).
8. Invoke `andrej-karpathy-skills:karpathy-guidelines` to validate decomposition.
9. `vx-hardening-guard` on the final doc (must return OK). Write the phase doc.

## Output: `Docs/Roadmap/Phases/Fase-NN-nome.md` (PT-BR)

Template (sections marked ★ are new in formato 2 and MANDATORY — `vx-task-create` HALTS if they are missing):

````markdown
---
fase: NN
nome: nome-kebab-case
status: Analisada  # Analisada | Em-execução | Concluída
fase_anterior: Fase-NN-1 ou null
fase_proxima: Fase-NN+1 ou null
especialistas_consultados: [vx-spec-...]
decisoes: [NNNN, NNNN]
---

# Fase NN — <Nome>

## Objetivo desta fase
1-3 parágrafos em PT-BR. Cita design-mvp.md §9.

## Critério de aceitação da fase
Lista textual extraída de design-mvp.md §9, mais refinamentos das ADRs.

## ★ Critério de aceitação — verificação por máquina
| Critério | Comando/verificação | Evidência esperada |
|---|---|---|
| ≥ N testes verdes | `ctest --preset debug` | "100% tests passed, 0 failed out of ≥N" |
| Smoke da fase < 30 s | `VibeTests.exe "[smoke][faseNN]" --durations yes` | duração impressa < 30 s |
| Zero leak | saída do smoke | "TrackingAllocator: 0 leaks" |

## Stack confirmada para a fase
Tabela: ferramenta → versão → arquivo onde será adicionada → fonte (design-mvp.md §X / ADR-NNNN).

## Módulos e arquivos previstos
Lista de paths (Engine/Source/Runtime/...) com Public/Private split.

## Arquitetura proposta
Diagrama Mermaid de blocos + interfaces principais. Citar §8.X para cada bloco.

## ★ Contratos entre tasks
A superfície pública que cada task EXPÕE para suas sucessoras — registro único de
contratos cross-task da fase. `vx-task-create` copia daqui para os docs de task;
qualquer emenda começa AQUI e se propaga (ver Protocolo de revisão de contratos).

### TNN — nome-da-task
Expõe (Engine/Source/Runtime/<Module>/Public/):
```cpp
// assinaturas completas APENAS dos símbolos consumidos por OUTRAS tasks
```
Consumido por: TMM, TKK.

## ★ Comandos canônicos da fase
Bloco único (presets reais do CMakePresets.json, caminho real do VibeTests.exe).
Tasks copiam DAQUI. Preset/caminho mudou → atualizar aqui primeiro, depois rebake
das tasks ainda Planejado. Status: **previsto** (vira **verificado** quando a task
de tooling da fase aterrissar).
```powershell
cmake --preset debug
cmake --build --preset debug
ctest --preset debug --output-on-failure
Build\debug\<caminho>\VibeTests.exe "[smoke]" --durations yes
# asan (tasks com risco_memoria): cmake --preset asan-debug && cmake --build --preset asan-debug && ctest --preset asan-debug --output-on-failure
```

## Ordem interna de implementação
1. Primeiro: ... N. Por fim: ...
Cada item gera 1+ tasks via vx-task-create.

## Tasks previstas (esboço)
"NN-nome-curto — descrição em 1 linha" por task. Aplicar a disciplina de **slices
verticais** do `to-issues` (ver `Docs/References/MATT-SKILLS-BINDING.md`): recortes
finos, ponta a ponta, independentemente demonstráveis — não fatias horizontais.
Invocar `to-issues` apenas de dentro deste skill; nenhum tracker externo.

## ★ Flags de risco por task
| Task | risco_memoria | risco_frame | Gate extra |
|---|---|---|---|
| NN-nome | true/false | true/false | asan-debug / teste [perf] / — |
(Fonte: recomendação do vx-spec-memory-perf. HARDENING §12/§13.)

## ★ Protocolo de revisão de contratos (churn — HARDENING §14)
Quando um Desvio aprovado em execução muda um símbolo de "Contratos entre tasks":
1) emendar ESTA seção; 2) listar tasks afetadas via vx-doc-graph ("dependentes de NN");
3) rebake de cada task afetada ainda Planejado via vx-task-create;
4) tasks afetadas já Implementado → nova task corretiva, nunca edição retroativa.

## Riscos específicos da fase
Referencia design-mvp.md §12 + riscos dos especialistas. Fases com renderer/UI:
citar o teto 4K@60/FSR (ADR 0011) como restrição de projeto.

## Hardening relevante
Seções de HARDENING.md que se aplicam (especialmente §2 lista negativa + Fronteira
de UI, §12 memória, §13 performance, §14 formato 2).

## Perguntas em aberto
Nenhuma (resolver antes de fechar o doc) OU lista do que o usuário ainda precisa decidir.
````

## Out of scope

- Não cria tasks (isso é `vx-task-create`). Não escreve código. Não cria CMakeLists.txt/vcpkg.json.
- Não modifica `design-mvp.md`.
- Não toma decisões fora da lista in-scope. Item da lista negativa (HARDENING §2) → parar e perguntar.

## Questions to ask the user (when info missing)

- Número da fase, se não dado.
- Se a fase NN-1 não está `Concluída`, perguntar se deve prosseguir mesmo assim.
- Qualquer escolha entre múltiplas tecnologias permitidas que não esteja em ADR.
- Confirmação antes de criar uma ADR para decisão arquitetural detectada.
- Toda OPEN-QUESTION vinda dos especialistas (formato AskUserQuestion-ready).

## Failure modes

- HARDENING.md ausente → parar e instruir o usuário a executar o setup de skills.
- Fase inexistente em design-mvp.md → parar.
- Specialist retorna conflito com outro specialist → consolidar a divergência e perguntar ao usuário antes de decidir.
- Specialist retorna output fora do schema duas vezes → normalizar manualmente e registrar no doc da fase ("Perguntas em aberto" se algo se perdeu).
