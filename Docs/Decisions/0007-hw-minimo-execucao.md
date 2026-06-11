---
id: 0007
titulo: Hardware mínimo de execução — 8 núcleos físicos
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0002, Decisions/0006]
---

## Contexto

§9 Fase 1 estabelece como critério "executável de teste roda 8 workers". §8.5 define worker pool com N = núcleos físicos − 1. §3 alvo de hardware é "RTX 3060 / RX 6600 em 1080p @ 60 fps" — implica CPU desktop moderna (tipicamente ≥6 físicos), mas o design não obriga isso explicitamente.

Decisão pendente: o que fazer quando a máquina tem menos de 8 núcleos físicos?

## Opções consideradas

- **A**: Mínimo 8 físicos estrito — abaixo disso, `Initialize` falha com mensagem clara.
- **B**: Mínimo 4 físicos com degradação para `N = cores − 1` e warning.
- **C**: Sem mínimo — sempre `N = max(1, cores − 1)`.

## Decisão

**Opção A — Mínimo 8 núcleos físicos estrito.**

`PlatformThread::GetPhysicalCoreCount()` (via `GetLogicalProcessorInformationEx(RelationProcessorCore)`) é chamada por `EngineCore::Initialize`. Se < 8, retorna `VResult` com erro `EngineError::InsufficientHardware` e mensagem:

```
Vibe Engine exige no mínimo 8 núcleos físicos.
Detectado: <N> físicos / <M> lógicos.
Veja Docs/Decisions/0007-hw-minimo-execucao.md.
```

`EngineCoreConfig::m_WorkerCount == 0` (auto) usa N = 8 ou `cores − 1`, o maior dos dois. Override manual aceita qualquer valor ≥ 1 (responsabilidade do chamador).

## Consequências

- Aderência literal ao critério §9 Fase 1 ("roda 8 workers").
- Dev em laptop modesto requer override explícito (`EngineCoreConfig::m_WorkerCount = 4`, por exemplo) — fricção intencional para garantir que medições de perf vivem em HW representativo.
- README do projeto (entrega de Fase 12) cita requisito de 8 núcleos físicos.
- Política revisitada se RTX 3060 / RX 6600 saírem do mercado e o alvo de HW mudar.
