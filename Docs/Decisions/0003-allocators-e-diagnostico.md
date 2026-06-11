---
id: 0003
titulo: Allocators da Fase 1 e diagnóstico de memória
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0001, Decisions/0002]
---

## Contexto

§9 Fase 1 entrega `LinearAllocator`, `FrameAllocator` e `MemoryTag`. §8.4 lista o conjunto maior (Stack, Pool, Tracking). Três decisões pendentes:

1. `FrameAllocator` é per-thread (TLS) ou global com mutex?
2. Override global de `operator new`/`operator delete` para forçar telemetria?
3. Em quais builds o `TrackingAllocator` fica ativo, e como ASAN entra na fase?

## Opções consideradas

- **FrameAllocator**: (A) per-thread; (B) global com mutex.
- **Override global new**: (A) não sobrescrever; (B) sobrescrever em Debug/Development.
- **Tracking + ASAN**: (A) Tracking em Debug+Development, ASAN preset à parte; (B) Tracking só em Debug, sem ASAN; (C) Tracking em todos os builds.

## Decisão

- **`FrameAllocator` é per-thread**. Cada `WorkerThread` cria sua instância; main thread também. Zero contenção, zero false-sharing entre workers. `BeginFrame()` é chamado pelo coordinator no início do frame e propagado aos workers via callback do `JobSystem`. Default size por instância = **1 MiB com grow + warning logado** (forçando dimensionamento intencional nas fases que crescerem demanda).
- **`operator new` global NÃO é sobrescrito**. Allocators da engine são opt-in. Código terceiro (spdlog, glm, Catch2) continua no heap padrão. `TrackingAllocator` cobre apenas alocações que passam pelos allocators da engine. Trade-off aceito: menos visibilidade total, muito menos atrito com libs externas e static-init order.
- **`TrackingAllocator` ativo em Debug e Development**; desligado em Shipping. Implementação: wrapper que delega ao allocator concreto, registra `{tag, size, thread_id, callsite}` em mapa protegido por mutex. Relatório de leaks no shutdown agrupado por `MemoryTag`.
- **ASAN entra como preset CMake `asan-debug`** opt-in (não automático). Habilita `/fsanitize=address` do MSVC. Roteado para uso pontual em CI e local quando há suspeita de UB; não obrigatório no fluxo diário.

## Consequências

- `JobSystem::Initialize(N)` aloca N+1 `FrameAllocator`s (workers + main). Shutdown libera na ordem inversa.
- `BeginFrame()` no coordinator faz broadcast: cada worker reseta seu allocator no início do próximo job que tomar. Detalhe de barreira fica para a task da implementação.
- `TrackingAllocator` adiciona ~mutex+map por alocação em Debug/Development — inaceitável em Shipping. CI roda smoke em Development por padrão; isso pega leaks.
- Preset `asan-debug` referenciado pelo `CMakePresets.json`. Existe em paralelo com `debug`/`development`/`shipping`.
- Tasks subsequentes que adicionam novo subsistema devem registrar tag em `MemoryTag` (enum) — ver ADR 0001.
