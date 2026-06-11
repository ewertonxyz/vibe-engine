---
id: 0006
titulo: EngineCore::Initialize centralizado com ordem fixa
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0002, Decisions/0003]
---

## Contexto

A Fase 1 entrega seis subsistemas (`Core`, `Platform`, `Math`, `Memory`, `JobSystem`, `FileSystem`). Cada um tem estado global (logger, allocators, worker pool, watcher thread). Sem orquestração explícita de ordem de inicialização e destruição, surgem bugs de "use-after-shutdown" — particularmente o caso clássico: `JobSystem` ainda com worker vivo loga via spdlog que já foi destruído.

## Opções consideradas

- **A**: `EngineCore::Initialize(...)` / `EngineCore::Shutdown()` centrais, ordem fixa.
- **B**: Cada módulo se inicializa preguicosamente (singleton Meyer / init-on-first-use).

## Decisão

**Opção A — `EngineCore::Initialize` central com ordem fixa.**

Ordem de Initialize:
1. `Logging` (spdlog **síncrono** na Fase 1; troca para async sink será coberta por ADR própria quando renderer da Fase 4 introduzir log em hot path)
2. `Memory` (allocators padrão e `MemoryTag` registry)
3. `Platform` (timer, file primitives, thread primitives — `Window` entra a partir da Fase 2; `DynamicLibrary` adiada até existir cliente real)
4. `JobSystem` (N workers — ver ADR 0007 para política de N)
5. `FileSystem` (depende de JobSystem para async)

Shutdown na ordem inversa:
1. `FileSystem` (drena watcher e cancela async pendentes)
2. `JobSystem` (`Wait()` em todas as fences vivas + join workers)
3. `Platform`
4. `Memory` (relatório de leaks via `TrackingAllocator`)
5. `Logging` (último a morrer — para que tudo acima possa logar erros)

API:
```cpp
namespace VibeEngine {
    struct EngineCoreConfig {
        Vuint32 m_WorkerCount;       // 0 = auto (cores físicos − 1)
        Vuint64 m_FrameAllocatorSize; // 0 = default 1 MiB por worker
        bool    m_EnableTracking;    // ignorado em Shipping
    };
    VResult<void, EngineError> Initialize(const EngineCoreConfig& Config);
    void Shutdown();
}
```

## Consequências

- `main()` de `VibeTests.exe`, futuro `VibeEditor.exe` e `VibeGame.exe` começa com `EngineCore::Initialize(...)` e termina com `Shutdown()`.
- Nenhum módulo da Fase 1 pode ser usado antes de `Initialize` retornar com sucesso — `VASSERT` cobre tentativa.
- Quando fases posteriores adicionarem `RHI`, `Renderer`, `World`, etc., a ordem cresce — ADRs futuras estendem esta sequência sem inverter os 5 primeiros passos.
- Leaks de memória são detectados na hora certa (pós-shutdown de JobSystem, antes de destruir Memory).
