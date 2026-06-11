---
id: 0012
titulo: EngineCore como módulo próprio no layout §6
data: 2026-06-11
status: Aceita
relacionada: [Decisions/0006, Phases/Fase-01-foundation]
---

## Contexto

A ADR 0006 criou `EngineCore::Initialize/Shutdown` como orquestrador central de bootstrap, e a Fase 01 prevê o módulo `Engine/Source/Runtime/EngineCore/`. Porém o layout de pastas do design-mvp.md §6 não enumera `EngineCore`, o que gerou um veredito `AMBIGUOUS` do `vx-hardening-guard`, registrado como pendência de governança em `Docs/Roadmap/README.md`. A pendência precisa ser resolvida antes da Task 13 (engine-core-bootstrap).

## Opções consideradas

- **A**: Fundir `EngineCore::Initialize` dentro do módulo `Core`.
- **B**: Manter `EngineCore` como módulo separado e emendar o §6 do design.
- **C**: Declarar o §6 "indicativo" e aceitar módulos não listados sem emenda.

## Decisão

**Opção B — `EngineCore` é módulo próprio e o §6 é emendado.**

Razão técnica decisiva: `Core` é **folha** do grafo de dependências (todos dependem dele; ele não depende de ninguém além de STL e spdlog — §8.1). `EngineCore` é o **inverso**: depende de `Core`, `Memory`, `Platform`, `JobSystem` e `FileSystem` para orquestrá-los (ADR 0006). Fundir os dois (opção A) criaria ciclo ou forçaria `Core` a conhecer todos os módulos que o consomem. A opção C destruiria o valor do §6 como contrato verificável — exatamente o tipo de erosão que o HARDENING existe para impedir.

Layout resultante:

```
Engine/Source/Runtime/EngineCore/
    Public/
        EngineCore.h        (Initialize, Shutdown, EngineCoreConfig — ADR 0006)
    Private/
        EngineCoreImpl.cpp
```

Regra geral derivada: **módulo novo não listado no §6 exige ADR + emenda do §6 no mesmo PR.** O §6 permanece normativo.

## Consequências

- design-mvp.md §6 ganha `EngineCore/` na árvore de Runtime.
- Pendência de governança do `Docs/Roadmap/README.md` é encerrada citando esta ADR.
- Fase-01 e Task 13 citam esta ADR; `vx-hardening-guard` passa a validar §6 incluindo `EngineCore`.
- Precedente registrado para fases futuras (ex.: se `ShaderSystem` precisar de submódulo novo, o caminho é ADR + emenda, nunca pasta silenciosa).
