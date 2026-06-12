---
id: 09
nome: jobsystem-worker-queue
fase: Fase-01-foundation
formato: 2
status: Planejado
dependencies: [03, 07]
decisions: [0002, 0004, 0006, 0007, 0014, 0018, 0019]
especialistas_consultados: [vx-spec-memory-perf, vx-spec-architecture, vx-spec-testing]
risco_memoria: true
risco_frame: true
contexto:
  - Docs/Hardening/HARDENING.md
  - Docs/Decisions/0002-jobsystem-worker-pool.md
  - Docs/Decisions/0014-convencao-include-paths.md
  - Docs/Decisions/0018-jobsystem-api-ciclo-de-vida.md
  - Docs/Roadmap/Phases/Fase-01-foundation.md          # §Contratos entre tasks (T09/T10), §Comandos canônicos
  - Docs/Roadmap/Tasks/02-core-types-handles-result.md # apenas §Contratos (Vuint*/Vbyte/Vspan, VASSERT)
  - Docs/Roadmap/Tasks/03-core-logging-time-profile.md # apenas §Contratos (VLOG_*, VPROFILE_ZONE/PLOT)
  - Docs/Roadmap/Tasks/07-platform-windows-base.md     # apenas §Contratos (PlatformThread, PlatformPerformanceCounter)
files_create:
  - Engine/Source/Runtime/JobSystem/CMakeLists.txt
  - Engine/Source/Runtime/JobSystem/Public/JobSystem/JobHandle.h
  - Engine/Source/Runtime/JobSystem/Public/JobSystem/JobFence.h
  - Engine/Source/Runtime/JobSystem/Public/JobSystem/JobSystem.h
  - Engine/Source/Runtime/JobSystem/Private/Job.h
  - Engine/Source/Runtime/JobSystem/Private/JobQueue.h
  - Engine/Source/Runtime/JobSystem/Private/WorkerThread.h
  - Engine/Source/Runtime/JobSystem/Private/WorkerThread.cpp
  - Engine/Source/Runtime/JobSystem/Private/JobSystem.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_Smoke_Task09.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_Worker.cpp
  - Engine/Source/Runtime/JobSystem/Tests/Job_Schedule.cpp
files_modify:
  - Engine/Source/Runtime/CMakeLists.txt
  - Engine/Source/Tests/CMakeLists.txt
---

# 09 — JobSystem: WorkerThread, Job 64B, JobQueue work-stealing + facade

## Objetivo
Núcleo de agendamento do `JobSystem` (design-mvp.md §8.5, §5.4): pool de N workers sobre `PlatformThread`
(ADR 0002), `Job` de 64 B (payload inline de 48 B), `JobQueue` work-stealing por worker, e o facade
**estático** `JobSystem` (`Initialize`/`Shutdown`/`Schedule`/`Wait`) + `JobHandle`/`JobFence` (ADR 0018).
`ParallelFor`/`TaskGraph` são a T10. O executor NÃO precisa ler o design — os contratos abaixo são completos.

## Contexto obrigatório (orçamento de leitura)
O executor lê: (1) este doc, (2) `contexto:`, (3) `files_modify:`, (4) o que ele criar. **Nada além disso**
(HARDENING §14). T07 ainda não está implementada na criação → o pre-flight compara os contratos contra
`Tasks/07 §Contratos`; ao executar, o gate de dependências garante os headers reais.

| Arquivo | Por quê | Seções relevantes |
|---|---|---|
| Docs/Hardening/HARDENING.md | regras | §3, §4, §5, §7, §12, §13, §14 |
| Decisions/0002 | Job 64 B (48 B inline + fn ptr), backend PlatformThread, sem std::thread/std::function | inteiro |
| Decisions/0014 | include `<JobSystem/Foo.h>` / `<Core/Foo.h>`; Public/<Module>/ | inteiro |
| Decisions/0018 | facade estático, fronteira T09/T10, ScheduleImpl, JobFence por associação | inteiro |
| Phases/Fase-01 | contrato T09/T10 + comandos (asan) | §Contratos entre tasks, §Comandos canônicos |
| Tasks/02 | `Vuint8..Vuint64`/`Vbyte`/`Vspan`/`VASSERT` | §Contratos |
| Tasks/03 | `VLOG_WARN/ERROR`; `VPROFILE_ZONE`/`VPROFILE_PLOT` (no-op sem TRACY_ENABLE) | §Contratos |
| Tasks/07 | `PlatformThread` (EntryFn `void(*)(void*)`, Create/SetName/SetAffinity/Join/IsJoinable); `PlatformPerformanceCounter` | §Contratos |

## Escopo

### Dentro
- Alvo `Vibe::JobSystem` (STATIC) com 3 headers Public em `Public/JobSystem/`, 3 headers/TU Private, 3 arquivos de teste.
- Facade estático `JobSystem` sobre pool de N workers; `Schedule` enfileira; `Wait`/`JobFence` sincronizam.
- `JobQueue` work-stealing por worker (deque Chase-Lev, head/tail em cache lines separadas — ADR 0002, §13).
- Tracy: `VPROFILE_ZONE("JobSystem.Schedule")` e `VPROFILE_ZONE("WorkerThread.Run")` (ADR 0004).

### Fora (NÃO fazer, mesmo que pareça óbvio)
- `ParallelFor`, `TaskGraph` (T10). Link de `Vibe::Memory` ao alvo (entra na T10 — ADR 0018).
- Detecção de núcleos físicos (`EngineCore`/T13 faz o check do ADR 0007 e passa `WorkerCount`).
- `std::thread` (usar `PlatformThread`), `std::function` como payload de Job, captura > 48 B (ADR 0002, §13).
- Backend de fibers (§5.4 — pós-MVP). Override de `new` global.

### Semântica vinculante dos arquivos (HARDENING §14)
`files_create`/`files_modify` exaustivos. `files_modify`: raiz do Runtime (`add_subdirectory(JobSystem)`) +
`Engine/Source/Tests/CMakeLists.txt` (linkar `Vibe::JobSystem` em `VibeTests`; fontes auto-globadas — ADR 0016).
Exceções permanentes: este doc, README, Backlog. Outro arquivo → Protocolo de bloqueio.

### Protocolo de bloqueio (HARDENING §14)
PARE e acione `AskUserQuestion` quando: (a) contrato não compila vs `<Core/...>`/`<Platform/PlatformThread.h>`
reais; (b) precisar de arquivo fora das listas (ex.: `Vibe::Platform` não linkável → drift da T07); (c) seções
se contradizem; (d) teste impossível; (e) gate (incl. asan/perf) falha após 2 ciclos de `diagnose`. Relate:
**passo · trecho do doc · erro literal · repro mínimo · resolução sugerida**.

## Contratos (implementar exatamente como declarado; include canônico `#include <JobSystem/Foo.h>`. Doxygen §5 completo.)

### Public/JobSystem/JobHandle.h
```cpp
#include <Core/Types.h>   // Vuint32

namespace VibeEngine
{
/// @brief Resultado opaco e aguardável de um Schedule único; POD de 8 B (índice + geração — ADR 0001/0002).
struct JobHandle
{
    Vuint32 m_Index { 0xFFFFFFFFu };   ///< @brief Slot do job no pool; 0xFFFFFFFF quando inválido.
    Vuint32 m_Generation { 0 };        ///< @brief Geração do slot (detecta uso após reciclagem — §12).
    /// @brief @return true se referencia um slot real (m_Index != 0xFFFFFFFF).
    [[nodiscard]] bool IsValid() const noexcept { return m_Index != 0xFFFFFFFFu; }
};
}
```

### Public/JobSystem/JobFence.h
```cpp
#include <Core/Types.h>   // Vuint32

namespace VibeEngine
{
/**
 * @brief Barreira de conclusão para um conjunto de jobs (ADR 0018). Move-only; bloqueio cooperativo.
 * @details Construída vazia; associada a jobs via `JobSystem::Schedule(fn, fence)` (incrementa o contador);
 *          o worker decrementa ao completar. `Wait()` ajuda o pool até o contador zerar (não fica em spin
 *          ocioso). Reusada por `TaskGraph::Submit` (T10). Sem alocação própria.
 */
class JobFence
{
public:
    JobFence() noexcept;                                ///< @brief Fence vazia (sem slot de contador).
    ~JobFence();                                        ///< @brief Libera o slot de contador, se houver.
    JobFence(JobFence&& Other) noexcept;                ///< @brief Move: transfere o slot; Other fica vazia.
    JobFence& operator=(JobFence&& Other) noexcept;     ///< @brief Move-assign (libera o atual; self-assign seguro).
    JobFence(const JobFence&) = delete;
    JobFence& operator=(const JobFence&) = delete;

    /// @brief Bloqueia (cooperativamente) até todos os jobs associados completarem. No-op se vazia.
    void Wait() noexcept;
    /// @brief @return true se não há jobs pendentes (ou a fence está vazia).
    [[nodiscard]] bool IsComplete() const noexcept;

private:
    Vuint32 m_CounterIndex { 0xFFFFFFFFu };  ///< @brief Índice do contador atômico no pool; 0xFFFFFFFF = vazia.
};
}
```

### Public/JobSystem/JobSystem.h
```cpp
#include <Core/Types.h>            // Vuint32, Vbyte, Vspan
#include <JobSystem/JobFence.h>
#include <JobSystem/JobHandle.h>

#include <type_traits>             // std::decay_t, is_trivially_copyable/destructible

namespace VibeEngine
{
namespace Detail
{
/// @brief Trampolim: reinterpreta o payload inline do Job como Fn e o invoca (mantém Job em Private/).
template <typename Fn> void JobThunk(void* Payload) noexcept { (*static_cast<Fn*>(Payload))(); }
}

/**
 * @brief Facade ESTÁTICO sobre um pool global de N workers (ADR 0018). PlatformThread backend (ADR 0002).
 * @details `EngineCore` (T13) chama Initialize/Shutdown em ordem fixa/reversa (ADR 0006/0007). Não-instanciável.
 */
class JobSystem
{
public:
    JobSystem() = delete;

    /// @brief Sobe `WorkerCount` workers e suas filas (heap frio — §12; contagem decidida por EngineCore, ADR 0007).
    /// @param WorkerCount Número de threads de worker (>= 1). Idempotente: 2ª chamada sem Shutdown é no-op.
    static void Initialize(Vuint32 WorkerCount);
    /// @brief Junta todos os workers e libera o pool; idempotente; inverso de Initialize (ADR 0006).
    static void Shutdown();
    /// @brief @return true entre um Initialize e o Shutdown correspondente.
    [[nodiscard]] static bool IsInitialized() noexcept;
    /// @brief @return número de workers passado a Initialize (0 quando não inicializado).
    [[nodiscard]] static Vuint32 WorkerCount() noexcept;

    /// @brief Enfileira `Func`; copia o callable no payload inline de 48 B (ADR 0002). @return handle aguardável.
    template <typename Fn>
    [[nodiscard]] static JobHandle Schedule(Fn&& Func) noexcept
    {
        using D = std::decay_t<Fn>;
        static_assert(sizeof(D) <= 48, "Captura de Job > 48 B (ADR 0002): aloque em FrameAllocator e passe um ponteiro.");
        static_assert(std::is_trivially_copyable_v<D>, "Payload de Job deve ser trivialmente copiável (ADR 0002, §13).");
        static_assert(std::is_trivially_destructible_v<D>, "Payload de Job deve ser trivialmente destrutível (ADR 0002).");
        D Local = static_cast<Fn&&>(Func);
        return ScheduleImpl(&Detail::JobThunk<D>, Vspan<const Vbyte>{ reinterpret_cast<const Vbyte*>(&Local), sizeof(D) });
    }

    /// @brief Como Schedule, mas associa o job a `Fence` (incrementa seu contador). @param Func callable @param Fence barreira.
    template <typename Fn>
    static void Schedule(Fn&& Func, JobFence& Fence) noexcept
    {
        using D = std::decay_t<Fn>;
        static_assert(sizeof(D) <= 48 && std::is_trivially_copyable_v<D> && std::is_trivially_destructible_v<D>,
                      "Mesma guarda de payload da sobrecarga acima (ADR 0002, §13).");
        D Local = static_cast<Fn&&>(Func);
        ScheduleIntoImpl(&Detail::JobThunk<D>, Vspan<const Vbyte>{ reinterpret_cast<const Vbyte*>(&Local), sizeof(D) }, Fence);
    }

    /// @brief Bloqueia (cooperativamente) até o job de `Handle` completar. No-op se Handle inválido/obsoleto.
    static void Wait(JobHandle Handle) noexcept;
    /// @brief @return true se o job de `Handle` completou (ou Handle é inválido/obsoleto).
    [[nodiscard]] static bool IsComplete(JobHandle Handle) noexcept;

private:
    /// @brief Enfileira um job type-erased. @param Entry trampolim @param Payload bytes (≤ 48) @return handle.
    static JobHandle ScheduleImpl(void (*Entry)(void*), Vspan<const Vbyte> Payload) noexcept;
    /// @brief Como ScheduleImpl, associando o job a `Fence` (incrementa o contador da fence).
    static void ScheduleIntoImpl(void (*Entry)(void*), Vspan<const Vbyte> Payload, JobFence& Fence) noexcept;
};
}
```

**Notas de implementação (decididas na criação; não re-derivar):**
- **`Job` (Private/Job.h) é VERBATIM do ADR 0002**: `struct alignas(64) Job { using EntryFn = void(*)(void*); EntryFn m_Entry; Vbyte m_Payload[48]; Vuint32 m_PredecessorsCount; Vuint16 m_SuccessorsHead; Vuint16 m_Reserved; }; static_assert(sizeof(Job)==64);`. Não re-derivar os tamanhos. (Campos de predecessor/successor existem para a T10; em T09 ficam zerados.)
- **`JobQueue` (Private/JobQueue.h)** = deque work-stealing Chase-Lev de **capacidade fixa** `kQueueCapacity = 4096` (potência de 2; wrap por máscara `Idx & (Cap-1)`), uma por worker. `m_Bottom` (push/pop do dono) e `m_Top` (CAS dos ladrões) em structs `alignas(64)` separadas — sem false sharing (§13). Ordenação Chase-Lev: push = store relaxed do item + store release de bottom; pop = load acquire de top + fence no caminho do último item; steal = load acquire de top/bottom + CAS acq_rel em top. **Overflow (fila cheia) → executa o job inline na thread submissora** (nunca cresce heap, nunca bloqueia); `VPROFILE_PLOT("JobSystem.QueueOverflow", n)` opcional.
- **`WorkerThread` (Private)** sobre `PlatformThread::Create` (entry `void(*)(void*)`, ADR 0002). Loop: `VPROFILE_ZONE("WorkerThread.Run")`; tenta pop da própria fila, senão rouba de outra; ao completar um job, decrementa o contador da fence associada (se houver) e marca o slot do `JobHandle`. Encerra em `Shutdown` via flag atômica + Join. Sem `std::thread`.
- **`Schedule` mantém `Job` Private**: o template Public faz a guarda de 48 B e delega a `ScheduleImpl`/`ScheduleIntoImpl` (Private, em JobSystem.cpp), que copiam `Payload` (≤ 48 B) no `m_Payload` de um `Job` e setam `m_Entry`. Interface interna recebe `Vspan<const Vbyte>`, nunca ptr+size (§12). `VPROFILE_ZONE("JobSystem.Schedule")` em `ScheduleImpl`/`ScheduleIntoImpl`.
- **Pool global em heap frio** (`std::unique_ptr` de workers + filas + array de contadores de fence `alignas(64)`), em `JobSystem.cpp` (TU-local). Código frio de boot → §12 permite. **T09 NÃO usa `IAllocator`/`MemoryTag`** (ADR 0018) → o alvo linka só `Vibe::Core`+`Vibe::Platform`.
- **`JobFence`**: `m_CounterIndex` aponta um contador atômico no pool; ctor vazio; primeira associação adquire um slot; `Wait()` cooperativo (pop/executa enquanto espera); dtor libera o slot. `JobHandle::IsValid` inline no header.
- **`WorkerCount` automático**: NÃO aqui — `Initialize` recebe o número pronto. Sem detecção de HW (ADR 0007 é T13).
- **CMake** (`JobSystem/CMakeLists.txt`): `add_library(VibeJobSystem STATIC Private/WorkerThread.cpp Private/JobSystem.cpp)` + alias `Vibe::JobSystem`; `target_include_directories(... PUBLIC Public PRIVATE Private)`; `target_link_libraries(VibeJobSystem PUBLIC Vibe::Core Vibe::Platform)`. `Engine/Source/Runtime/CMakeLists.txt` ganha `add_subdirectory(JobSystem)`; `Engine/Source/Tests/CMakeLists.txt` ganha `Vibe::JobSystem` no link de `VibeTests` (fontes auto-globadas — ADR 0016).

## Plano de testes (lista RED — ordem de execução do `tdd`; fonte da política: ADR 0019)
Determinismo (§7): correção por invariante pós-`Wait`/`Fence::Wait` (a barreira do SUT É a barreira do teste);
sem `sleep_for`/wall-clock no caminho de correção; sem RNG. Wall-clock só no teste `[perf]` (teto suave).

| # | TEST_CASE (nome exato) | Tags | Arquivo | Asserções-chave |
|---|---|---|---|---|
| 1 | `Job_Smoke_Task09` | `[job][smoke]` | Job_Smoke_Task09.cpp | `Initialize(8)`; `IsInitialized()`; `WorkerCount()==8`; agenda 256 jobs numa `JobFence` cada `fetch_add(1)`; `Fence.Wait()` → contador==256; `Shutdown()`; `!IsInitialized()`; sem leak (asan); < 1 s |
| 2 | `Job_WorkerPool_SpawnsN` | `[job][unit]` | Job_Worker.cpp | `Initialize(N)` para N∈{1,2,8} → `WorkerCount()==N`; `Shutdown()`; re-`Initialize(4)` → `WorkerCount()==4` (§11 "N workers") |
| 3 | `Job_Lifecycle_IdempotentInitShutdown` | `[job][unit]` | Job_Worker.cpp | `Initialize` já-inicializado = no-op; `Shutdown()` 2× seguro; `IsInitialized()` coerente em cada passo |
| 4 | `Job_Schedule_RunsOnce` | `[job][unit]` | Job_Schedule.cpp | agenda K=10000 jobs (> kQueueCapacity, exercita overflow inline) numa fence, cada `fetch_add(1)`; `Wait()` → contador==K (cada job 1×, nenhum perdido/duplicado) |
| 5 | `Job_Schedule_HandleWaitObservesWrite` | `[job][unit]` | Job_Schedule.cpp | `JobHandle h = Schedule(...)` que escreve valor conhecido em `std::atomic`; `h.IsValid()`; `Wait(h)`; valor visível; `IsComplete(h)` |
| 6 | `Job_Fence_WaitBlocksUntilCompletion` | `[job][unit]` | Job_Schedule.cpp | agenda M jobs numa fence; `fence.Wait()`; imediatamente contador==M sem sync extra; `fence.IsComplete()` (prova que Wait não retornou cedo) |
| 7 | `Job_Perf_SchedulerThroughput` | `[job][perf]` | Job_Schedule.cpp | agenda P=100000 jobs triviais numa fence; `Wait()` → contador==P (exato) **E** wall-clock < `kPerfCeilingMs` (constexpr generoso provisório — calibrar por Tracy, ADR 0019); emite `VPROFILE_ZONE` |
| 8 | `Job_Stress_RepeatHappyPath` | `[job][stress]` | Job_Schedule.cpp | repete 64× { agenda 256 jobs numa fence local; `Wait()`; contador==256 }; determinístico, sem timing (anti-flaky in-repo — ADR 0019) |

**Smoke**: teste #1, < 30 s. **Gate de memória**: `risco_memoria: true` → preset **asan-debug** verde (sem
leak/UAF; o pool é RAII via `unique_ptr` + Join). TrackingAllocator global é a T14 (nada flui por ele aqui —
precedente T05). **Gate de frame**: `risco_frame: true` → teste #7 `[job][perf]`. **Captura > 48 B** é
`static_assert` de produção (não há teste runtime — §13). **Cobertura**: cada símbolo público dos contratos
aparece em ≥ 1 linha.

## Comandos (copiar e executar literalmente — fonte: Fase-01 §Comandos canônicos + gate asan)

```powershell
# Configurar + build (gate: /W4 /WX falham com warning novo)
cmake --preset debug; cmake --preset development
cmake --build --preset debug; cmake --build --preset development
# Testes da task (lista RED, inclui [perf] e [stress])
ctest --preset debug --output-on-failure -R "Job_"
# Smoke da task (com medição de duração)
Build\debug\bin\VibeTests.exe "[job][smoke]" --durations yes
# Suite completa (gate final)
ctest --preset debug --output-on-failure
ctest --preset development --output-on-failure
# Gate de memória (risco_memoria: true — HARDENING §12)
cmake --preset asan-debug
cmake --build --preset asan-debug
ctest --preset asan-debug --output-on-failure -R "Job_"
```

## Critério de aceitação
- [ ] 8 TEST_CASE do plano verdes — `ctest --preset debug -R "Job_"`
- [ ] Smoke `Job_Smoke_Task09` < 30 s (alvo < 1 s) — saída de `--durations yes`
- [ ] `[job][perf]` (correção exata + teto generoso) e `[job][stress]` verdes — `ctest -R "Job_Perf_SchedulerThroughput|Job_Stress_RepeatHappyPath"`
- [ ] Suite completa verde em debug e development (não regrediu)
- [ ] **asan-debug verde** — `ctest --preset asan-debug -R "Job_"` (gate de memória; pool RAII, sem UAF na corrida)
- [ ] Headers Public sem `<thread>`/`<windows.h>`/`PlatformThread`/Private/; `Job`/`JobQueue`/`WorkerThread` só em Private — inspeção
- [ ] Sem `std::thread`, sem `std::function` como payload de Job; interface interna em `Vspan` (não ptr+size) — inspeção
- [ ] `VPROFILE_ZONE("JobSystem.Schedule")` e `VPROFILE_ZONE("WorkerThread.Run")` presentes — inspeção (ADR 0004)
- [ ] Doxygen em toda declaração pública (HARDENING §5)
- [ ] vx-naming-style → OK · vx-hardening-guard → OK no diff

## Interface para tasks sucessoras
Fonte: Fase-01 §Contratos entre tasks (T09/T10), ADR 0018. **Mudar qualquer assinatura = Protocolo de bloqueio.**

| Símbolo | Consumido por |
|---|---|
| `JobHandle` (`m_Index`/`m_Generation`/`IsValid`) | T10, T12, T13 |
| `JobFence` (`Wait`/`IsComplete`; construção por associação) | T10 (ParallelFor/TaskGraph::Submit), T13 |
| `JobSystem` (`Initialize`/`Shutdown`/`IsInitialized`/`WorkerCount`/`Schedule`×2/`Wait`/`IsComplete`) | T10, T12, T13, T14 |

## Hardening aplicável (referência + concretização)
- §3/§4/§5 → aqui: PascalCase + `m_*`; headers em `Public/JobSystem/` (include `<JobSystem/Foo.h>`, ADR 0014), `Job`/`JobQueue`/`WorkerThread` em Private sem `<thread>`/Win32 em Public; Doxygen em toda decl. pública (inclusive templates).
- §7 → aqui: a tabela do plano É a aplicação; barreira = `Wait`/`Fence::Wait` do SUT; sem timing na correção.
- §12 → aqui: pool RAII (`unique_ptr` + Join); `Vspan` na interface interna; sem C-cast; **gate asan-debug**.
- §13 → aqui: `Job` 64 B; deque head/tail padded (`alignas(64)`); sem alloc/`std::function` no hot path; Tracy desde já; **teste `[perf]`**.

## Fluxo de execução (mecânico — detalhes no SKILL vx-task-execute)
Formato → orçamento → deps (T03/T07 = Implementado) → pre-flight (`PlatformThread`/`PerfCounter` vs Tasks/07;
`VPROFILE_ZONE`/`Vspan` vs Tasks/03/02) → Em-execução → baseline → branch `task/09-jobsystem-worker-queue` →
`tdd` sobre a lista RED → gate completo + asan-debug + perf → Implementado → commit `[task 09] jobsystem: worker pool, job, queue, facade`, push, PR.

## Desvios aprovados
(vazio na criação)

## Bloqueio
(vazio)
## Referências (proveniência — o executor não precisa ler)
- design-mvp.md §8.5, §5.4, §11 · Phases/Fase-01-foundation.md · Decisions/0002, 0004, 0006, 0007, 0014, 0018, 0019
