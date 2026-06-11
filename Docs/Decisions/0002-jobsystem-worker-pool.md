---
id: 0002
titulo: JobSystem worker pool — backend de thread e payload
data: 2026-05-27
status: Aceita
relacionada: [Phases/Fase-01-foundation, Decisions/0001]
---

## Contexto

§8.5 e §5.4 definem `JobSystem` com worker pool (N = núcleos físicos − 1, sem fibers no MVP). Duas decisões de implementação não estão no design:

1. Quem cria as threads de worker — `std::thread` direto, ou passamos pela camada `Platform`?
2. Tamanho máximo de payload inline em um `Job` antes de cair em heap-alloc.

A primeira impacta o slot multi-backend (§5.4 prevê troca de backend para fibers/console sob a mesma API). A segunda impacta perf em hot path: `std::function` com captura grande aloca em heap silenciosamente — anti-pattern documentado por `vx-spec-memory-perf`.

## Opções consideradas

- **Backend de thread**: (A) `PlatformThread`; (B) `std::thread` direto.
- **Job payload**: (A) 48 B inline + function pointer (`Job` ~64 B); (B) `std::function` sem limite; (C) 32 B inline (`Job` magro).

## Decisão

- **Backend de thread = `PlatformThread`**. `WorkerThread` em `Private/` chama `PlatformThread::Create`, `PlatformThread::SetName`, `PlatformThread::SetAffinity` (afinidade desabilitada no MVP). Mantém o slot trocável.
- **Job payload = 48 B inline + function pointer**. `sizeof(Job) ≈ 64 B` para caber em uma cache line. Captures maiores são alocados em `FrameAllocator` pelo submitter e referenciados por ponteiro de 8 B.

```cpp
// Esboço (apenas para fixar contrato; o código real vive em vx-task-create/execute)
struct alignas(64) Job {
    using EntryFn = void(*)(void* payload);
    EntryFn  m_Entry;
    Vbyte    m_Payload[48];
    Vuint32  m_PredecessorsCount;
    Vuint16  m_SuccessorsHead;
    Vuint16  m_Reserved;
};
static_assert(sizeof(Job) == 64);
```

## Consequências

- `JobSystem::Schedule(callable)` falha em compile-time se `sizeof(captures) > 48`. Mensagem clara orienta o usuário a alocar em `FrameAllocator` e passar ponteiro.
- `WorkerThread` não usa `std::thread` em lugar algum; qualquer uso direto é violação detectada por revisão de `Public/` (sem `<thread>` em Public).
- Quando/se um backend de fibers for adotado pós-MVP, somente `WorkerThread` e a parte de scheduling muda; API `Schedule`/`Wait`/`ParallelFor`/`TaskGraph` permanece.
- `PlatformThread` precisa expor `SetAffinity` mesmo desligada no MVP — placeholder não-op é aceitável.
