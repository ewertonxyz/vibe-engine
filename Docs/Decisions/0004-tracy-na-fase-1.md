---
id: 0004
titulo: Integrar Tracy já na Fase 1 (client + macros)
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Phases/Fase-03-rendergraph-imgui]
---

## Contexto

§9 do design-mvp.md coloca Tracy como deliverable explícito apenas na Fase 3 ("Tracy + frame timing"). Porém:

- §4 lista Tracy como parte da stack baseline.
- §11 ("Job System pronto") exige "ParallelFor escala em throughput" — afirmação que só pode ser verificada empiricamente com profiler.
- A skill `vx-spec-memory-perf` orienta instrumentação Tracy em todo código per-frame desde a primeira fase para evitar refactor cross-cutting depois.

Adiar Tracy até a Fase 3 significa retrofitar `VPROFILE_ZONE`/`VPROFILE_FRAME` em todo `JobSystem` e `FileSystem`, e ainda assim ficar sem dados de baseline para comparar regressão.

## Opções consideradas

- **A**: Integrar Tracy já na Fase 1 (client + macros, sem overlay visual).
- **B**: Seguir o §9 literal — Tracy só na Fase 3.

## Decisão

**Opção A — Tracy integrado na Fase 1.** Escopo limitado:

- Adicionar `tracy` ao `vcpkg.json` (já listado em §4 stack).
- Macros `VPROFILE_ZONE("Name")`, `VPROFILE_ZONE_COLORED(...)`, `VPROFILE_FRAME()`, `VPROFILE_PLOT(...)` em `Core/Public/Profile.h`. Compiladas como no-op em Shipping (via `TRACY_ENABLE` desativado).
- Instrumentação inicial: zonas em `JobSystem::Schedule`, `JobSystem::ParallelFor`, `FileSystem::ReadAsync`, callbacks de `WorkerThread`.
- **Sem overlay visual** na Fase 1. Overlay depende de ImGui e fica para a Fase 3 (mantém o §9).

## Consequências

- Pequeno custo extra de implementação na Fase 1 — ~1 arquivo (`Profile.h`) + ~20 callsites espalhados.
- Habilita medição de "ParallelFor escala em throughput" (§11) já no smoke da Fase 1.
- Evita refactor cross-cutting na Fase 3.
- `VPROFILE_*` se torna macro obrigatória em todo código que entra em hot path nas fases seguintes — convenção registrada aqui.
- Em Shipping, zonas compilam para nada; zero impacto em perf no build final.

Esta é um desvio deliberado do §9 e fica explicitamente registrado como aceito.
